#pragma once

namespace detail{


    class ComputeContext : public Context<ComputeContext> {
    public:
        static constexpr context_role role = context_role::compute;

        ComputeContext() = default;

        ~ComputeContext() {
            stop();
        }

        void start() {
            // start the compute thread
            running_ = true;
            worker_thread = std::thread([this]{
                detail::this_thread.role = detail::context_role::compute;
                detail::this_thread.io_ctx = this;

                this->run_loop();
            });
        }

        // stop and release the resources
        void stop() {
            // stop the compute thread
            if (running_) {
                running_ = false;
                if (worker_thread.joinable()) {
                    worker_thread.join();
                }
            }
        }

        void co_spawn(Task<void>&& task) {
            // spawn the task to the compute thread
            co_spawn_unsafe(std::move(task.coroHandle()));
        }

        void co_spawn_unsafe(std::coroutine_handle<> coro) {
            // spawn the coroutine to the compute thread
            if (worker.cursor.available() == 0) {
                throw std::runtime_error("worker queue full");
            }
            const size_t idx = worker.cursor.load_raw_tail() & worker.cursor.mask;
            worker.handles[idx] = coro;
            worker.cursor.push_notify();
        }
    private:
        
        void run_loop() {
            // the main loop of the compute thread
            while (running_) {
                // process the tasks in the local queue
                if (!worker.cursor.empty()) {
                    const size_t idx = worker.cursor.load_head() & worker.cursor.mask;
                    auto coro = worker.handles[idx];
                    worker.cursor.pop_notify();
                    if (coro && !coro.done()) {
                        coro.resume();
                    }
                }
            }
        }

        bool running_ = false;
    };

} // namespace detail
