#pragma once
#include <coroutine>
#include <thread>
#include <array>
#include <liburing.h>

namespace::detail {

enum class context_role {
    unknown = 0,
    io,
    compute,
};

struct worker_meta{
    io_uring ring;
    int ring_fd;

    std::array<std::coroutine_handle<>, 32> handles;
    spsc_cursor<32> cursor;

    io_uring_sqe* get_sqe() {
        return io_uring_get_sqe(&ring);
    }
};

class ContextBase {
public:
    ContextBase(){
        int ret = io_uring_queue_init(256, &worker.ring, 0);
        if (ret < 0) {
            throw std::runtime_error("io_uring init failed");
        }
        worker.ring_fd = worker.ring.ring_fd;
    }
    virtual ~ContextBase() {
        io_uring_queue_exit(&worker.ring);
        ring_fd = -1;

        for (auto& handle : worker.handles) {
            if (handle) {
                handle.destroy();
            }
        }
    }

    virtual context_role role() const noexcept = 0;

protected:
    worker_meta worker;
    std::thread worker_thread;
};

template<typename Derived>
class Context : public ContextBase {
public:
    Context() = default;
    ~Context() = default;

    context_role role() const noexcept override {
        return Derived::role;
    }
};


struct thread_meta {
    ContextBase* io_ctx = nullptr;
};

inline thread_local thread_meta this_thread = nullptr;

}
