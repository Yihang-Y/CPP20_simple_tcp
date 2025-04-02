#pragma once
#include <liburing.h>
#include <system_error>
#include <iostream>
#include <map>
#include <atomic>
#include <unistd.h>
#include <coroutine>
#include <memory>
#include <vector>
#include <mutex>
#include <sys/eventfd.h>
#include <thread>
#include "Promise.h"

// forward declaration
template<typename T>
class Task;

template<>
class Task<void>;

class IoUringScheduler {
public:
    IoUringScheduler() : threadId_(std::this_thread::get_id()) {
        init();
    }
    
    ~IoUringScheduler() {
        // 清理所有跟踪的协程句柄
        for (auto handle : managedCoroutines_) {
            if (handle && !handle.done()) {
                handle.destroy();
            }
        }
        
        io_uring_queue_exit(&ring);
    }
    
    void init() {
        io_uring_params params{};
        params.flags |= IORING_SETUP_SQPOLL;
        if (io_uring_queue_init_params(512, &ring, &params) < 0) {
            throw std::system_error(errno, std::system_category(), "io_uring_queue_init_params");
        }
    }

    io_uring* getRing() {
        return &ring;
    }

    uint64_t getNewId() {
        return next_id.fetch_add(1, std::memory_order_relaxed);
    }

    // 使用NOP操作唤醒事件循环
    void wakeup() {
        // 提交一个NOP操作到io_uring队列
        io_uring_sqe* sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            std::cerr << "Failed to get SQE for wakeup" << std::endl;
            return;
        }
        
        // NOP操作不执行任何I/O，但会产生一个完成事件
        io_uring_prep_nop(sqe);
        
        // 使用特殊ID标记这是一个唤醒操作
        static uint64_t wakeupId = std::numeric_limits<uint64_t>::max();
        sqe->user_data = wakeupId;
        
        // 提交操作
        int ret = io_uring_submit(&ring);
        if (ret <= 0) {
            std::cerr << "Failed to submit NOP operation: " << strerror(-ret) << std::endl;
        }
    }

    // 跟踪和管理协程生命周期的方法
    template<typename T>
    void co_spawn(Task<T> task) {
        // 将任务标记为分离状态，生命周期由调度器管理
        task.detach();
        
        auto handle = task.coro;
        
        bool needWakeup = false;
        {
            std::lock_guard<std::mutex> lock(coroutinesMutex_);
            managedCoroutines_.push_back(handle);
            pendingCoroutines_.push_back(handle);
            needWakeup = true;
        }
        
        wakeup();
    }
    
    // 清理已完成的协程任务
    void cleanupCompletedTasks() {
        std::lock_guard<std::mutex> lock(coroutinesMutex_);
        
        auto it = managedCoroutines_.begin();
        while (it != managedCoroutines_.end()) {
            if (*it && (*it).done()) {
                // 销毁已完成的协程
                (*it).destroy();
                it = managedCoroutines_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 事件循环
    void run() {
        threadId_ = std::this_thread::get_id(); // 记录事件循环线程ID
        
        while (true) {
            // 处理IO事件
            processIOEvents();
            
            // 恢复待处理的协程
            resumePendingCoroutines();
            
            // 定期清理已完成任务
            cleanupCompletedTasks();
        }
    }
    
    // 恢复待处理的协程
    void resumePendingCoroutines() {
        std::vector<std::coroutine_handle<>> coroutines;
        {
            std::lock_guard<std::mutex> lock(coroutinesMutex_);
            coroutines.swap(pendingCoroutines_);
        }
        
        for (auto& handle : coroutines) {
            if (handle && !handle.done()) {
                handle.resume();
            }
        }
    }
    
    // 处理IO事件
    void processIOEvents() {
        // 提交挂起的请求
        io_uring_submit(&ring);
        
        // 尝试获取尽可能多的完成事件
        constexpr unsigned MAX_BATCH = 512;
        io_uring_cqe* cqes[MAX_BATCH];
        unsigned completed = io_uring_peek_batch_cqe(&ring, cqes, MAX_BATCH);

        // 如果没有可用的完成事件，则等待至少一个
        if (completed == 0) {
            int ret = io_uring_wait_cqe(&ring, &cqes[0]);
            if (ret < 0) {
                std::cout << "ERROR in wait_cqe: " << strerror(-ret) << std::endl;
                return;
            }
            completed = io_uring_peek_batch_cqe(&ring, cqes, MAX_BATCH);
        }

        // 处理所有可用的完成事件
        for (unsigned i = 0; i < completed; i++) {
            uint64_t id = cqes[i]->user_data;
            
            // 对于NOP唤醒操作，不需要特殊处理
            // NOP操作只是为了唤醒事件循环
            if (id != std::numeric_limits<uint64_t>::max()) {
                auto it = handles.find(id);
                if (it != handles.end()) {
                    void* addr = reinterpret_cast<void*>(it->second.address());
                    auto& promise = std::coroutine_handle<promise_base>::from_address(addr).promise();
                    promise.data = cqes[i]->res;
                    it->second.resume();
                    handles.erase(it);
                }
            }
            io_uring_cqe_seen(&ring, cqes[i]);
        }
    }

    std::map<uint64_t, std::coroutine_handle<>> handles;

private:
    io_uring ring;
    std::atomic<uint64_t> next_id{0};
    
    // 使用直接的协程句柄集合替代shared_ptr集合
    std::vector<std::coroutine_handle<>> managedCoroutines_;
    std::vector<std::coroutine_handle<>> pendingCoroutines_;
    
    std::mutex coroutinesMutex_;
    
    // 记录事件循环线程ID
    std::thread::id threadId_;
};