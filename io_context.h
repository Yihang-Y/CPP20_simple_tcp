#pragma once
#include <liburing.h>
#include <system_error>
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

#include "Task.h"
#include "spsc.h"

class io_context;

namespace detail {

    struct io_context_meta {
        std::mutex mtx;
        std::condition_variable cv;

        std::vector<io_context*> contexts;
        std::atomic<bool> stop_all{false};
    };

    // global io_context_meta, use inline since C++17
    inline io_context_meta g_meta;

} // namespace detail

struct worker_meta{
    io_uring ring;
    int ring_fd;

    std::array<std::coroutine_handle<>, 32> handles;
    spsc_cursor<32> cursor;
};

namespace detail {
    enum class thread_role {
        unknown = 0,
        io_thread,
        compute_thread,
    };

    struct thread_meta {
        thread_role role = thread_role::unknown;
        io_context* io_ctx = nullptr;
    };

    inline thread_local thread_meta this_thread;
}


class io_context {
public:
    io_context() {
        // initialize io_uring
        if (io_uring_queue_init(256, &worker.ring, 0) < 0) {
            throw std::runtime_error("io_uring init failed");
        }
        worker.ring_fd = worker.ring.ring_fd;

        {
            std::lock_guard<std::mutex> lock(detail::g_meta.mtx);
            detail::g_meta.contexts.push_back(this);
        }

        // each io_context corresponds to a worker thread
        worker_thread = std::thread([this]{
            detail::this_thread.role = detail::thread_role::io_thread;
            detail::this_thread.io_ctx = this;

            this->run_loop();
        });
    }

    ~io_context() {
        stop = true;
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
        io_uring_queue_exit(&worker.ring);

        {
            std::lock_guard<std::mutex> lock(detail::g_meta.mtx);
            auto it = std::find(detail::g_meta.contexts.begin(), detail::g_meta.contexts.end(), this);
            if (it != detail::g_meta.contexts.end()) {
                detail::g_meta.contexts.erase(it);
            }
        }
    }

    
    void co_spawn(Task<void>&& task) {
        auto coro = std::move(task).coroHandle();
        if (detail::this_thread.io_ctx == this) {
            co_spawn_unsafe(coro);
        } else {
            co_spawn_msg_ring(coro);
        }
    }

    void co_spawn_unsafe(std::coroutine_handle<> coro) {
        if (worker.cursor.available() == 0) {
            throw std::runtime_error("worker queue full");
        }
        const size_t idx = worker.cursor.load_raw_tail() & worker.cursor.mask;
        worker.handles[idx] = coro;
        worker.cursor.push_notify();
    }

    void co_spawn_msg_ring(std::coroutine_handle<> coro) {
        // io_context* from = detail::this_thread.io_ctx;
        // std::cout << "co_spawn_msg_ring from " << from << " to " << this << std::endl;
        // auto *const sqe = io_uring_get_sqe(&worker.ring);
        // auto user_data = reinterpret_cast<uint64_t>(coro.address()) | uint8_t(::msg_ring);
        // // sqe->prep_msg_ring(worker., 0, user_data, 0);
        // io_uring_prep_msg_ring(sqe, this->worker.ring_fd, 
        //                        reinterpret_cast<unsigned long>(msg), 
        //                        /*flags*/0);
        // int ret = io_uring_submit(&worker.ring);
        // if (ret < 0) {
        //     delete msg;
        //     throw std::runtime_error("io_uring_submit failed in msg_ring");
        // }

        // std::cout << "[co_spawn_msg_ring] send cross-thread msg to ctx("
        //           << (void*)this << "), ring_fd=" << this->worker.ring_fd 
        //           << std::endl;
    }

private:
    bool stop = false;
    worker_meta worker;
    std::thread worker_thread;

private:
    void run_loop() {
        std::cout << "[io_context::run_loop] start on thread "
                  << std::this_thread::get_id() 
                  << ", ring_fd=" << worker.ring_fd << std::endl;

        while (!stop) {
            // (1) 先处理本地队列里已有的协程
            if (!worker.cursor.empty()) {
                const size_t idx = worker.cursor.load_head() & worker.cursor.mask;
                auto coro = worker.handles[idx];
                worker.cursor.pop_notify();
                if (coro && !coro.done()) {
                    coro.resume();
                }
            }

            // // (2) 处理 io_uring completion
            // io_uring_cqe* cqe = nullptr;
            // // peek or wait cqe
            // int ret = io_uring_peek_cqe(&worker.ring, &cqe);
            // if (ret == -EAGAIN) {
            //     // 没有 cqe，就稍微睡一下 (demo 简化)
            //     std::this_thread::sleep_for(std::chrono::milliseconds(1));
            //     continue;
            // }
            // if (ret < 0) {
            //     // 出错
            //     std::cerr << "[io_context] io_uring_peek_cqe error: " 
            //               << ret << std::endl;
            //     break;
            // }
            // if (cqe) {
            //     // 如果是 MSG_RING 事件
            //     // cqe->flags 可能包含 IORING_CQE_F_MORE，或 cqe->res >= 0
            //     // 这里简化处理
            //     // user_data 就是我们发送的msg指针
            //     auto* raw_ptr = reinterpret_cast<detail::msg_ring_spawn_data*>(cqe->user_data);
            //     if (raw_ptr) {
            //         // 取出协程句柄
            //         auto h = raw_ptr->coro;
            //         // 这里要确保是投递给自己的
            //         assert(raw_ptr->target_ctx == this);

            //         // 投递到本地队列
            //         // 可能也可以直接 resume
            //         const size_t idx = worker.cursor.load_raw_tail() & worker.cursor.mask;
            //         if (worker.cursor.available() > 0) {
            //             worker.handles[idx] = h;
            //             worker.cursor.push_notify();
            //         } else {
            //             std::cerr << "[msg_ring] local worker queue full, ignoring" << std::endl;
            //             // 正常应有更好的错误处理
            //         }

            //         // 清理
            //         delete raw_ptr;
            //     }
            //     io_uring_cqe_seen(&worker.ring, cqe);
            // }
        }

        std::cout << "[io_context::run_loop] stop." << std::endl;
    }
};