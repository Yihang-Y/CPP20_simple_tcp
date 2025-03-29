#pragma once
#include <coroutine>
#include <stdexcept>
#include <liburing.h>

namespace detail{

    struct task_info{
        std::coroutine_handle<> coro;
        int32_t result;

        uint64_t as_user_data() const noexcept{
            return reinterpret_cast<uint64_t>(this);
        }
    };

    inline constexpr uintptr_t raw_io_info_mask = ~uintptr_t(alignof(task_info) - 1);

    // NOTE: put assert close to the definition of io_info, so that it can be easily found
    static_assert(alignof(task_info) == 8);

    inline constexpr task_info* as_task_info(uint64_t user_data) noexcept{
        return reinterpret_cast<task_info*>(user_data & raw_io_info_mask);
    }

    enum class user_data_type : uint8_t{
        task_info_ptr,
        coroutine_handle,
        msg_ring,
        none
    };

    class IO_Awaiter {
    public:
        IO_Awaiter() {
            if (!detail::this_thread.io_ctx) {
                throw std::runtime_error("no io context");
            }

            if (detail::this_thread.role != detail::context_role::io) {
                throw std::runtime_error("io awaiter must be used in io context");
            }

            sqe_ = detail::this_thread.io_ctx->worker.get_sqe();
            if (!sqe_) {
                throw std::runtime_error("no sqe");
            }
            io_uring_sqe_set_data(sqe_, reinterpret_cast<void*>(this));
        }

        ~IO_Awaiter() = default;

        bool await_ready() const noexcept {
            return false;
        }

        void await_suspend(std::coroutine_handle<> coro) {
            coro_ = coro;
            io_uring_submit(&detail::this_thread.io_ctx->worker.ring);
        }

        int await_resume() {
            return result_;
        }

        int result() const noexcept {
            return result_;
        }

    protected:
        io_uring_sqe* sqe;
        task_info* info;
    };
}
