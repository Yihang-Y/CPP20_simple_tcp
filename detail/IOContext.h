#pragma once

#include <liburing.h>
#include <system_error>

#include "Context.h"

namespace::detail {

    class IOContext : public Context<IOContext> {
    public:
        static constexpr context_role role = context_role::io;

        IOContext() = default;

        ~IOContext() {
            stop();
        }

        void start() {
            // start the io thread
            running_ = true;
            worker_thread = std::thread([this]{
                detail::this_thread.role = detail::context_role::io;
                detail::this_thread.io_ctx = this;

                this->run_loop();
            });
        }

        void stop() {
            // stop the io thread
            if (running_) {
                running_ = false;
                if (worker_thread.joinable()) {
                    worker_thread.join();
                }
            }
        }

    private:
        void run_loop() {
            // the main loop of the io thread
            while (running_) {
                // process the tasks in the local queue
                if (!worker.cursor.empty()) {
                    const size_t idx = worker.cursor.load_head() & worker.cursor.mask;
                    auto coro = worker.handles[idx];
                    coro.resume();
                    worker.handles[idx] = nullptr;
                    worker.cursor.pop_notify();
                }

                // process the io events
                io_uring_cqe *cqe;
                int ret = io_uring_peek_cqe(&worker.ring, &cqe);
                if (ret == 0 && cqe) {
                    uint64_t id = cqe->user_data;
                    auto it = handles.find(id);
                    if (it != handles.end()) {
                        auto task = it->second;
                        task->resume();
                        handles.erase(it);
                    }
                    io_uring_cqe_seen(&worker.ring, cqe);
                }
            }
        }
        std::unordered_map<uint64_t, Task<void>*> handles;
        bool running_ = false;
        std::atomic<uint64_t> next_id{0};

    };

} // namespace detail


