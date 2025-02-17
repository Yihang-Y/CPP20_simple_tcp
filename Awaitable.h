#pragma once
#include <coroutine>
#include <liburing.h>
#include <liburing/io_uring.h>
#include "IoUringScheduler.h"


// NOTE: Attr, Awaitable and awaitable_traits used to foward the parameters to the io_uring functions
struct Attr{
    io_uring_sqe* sqe;
};

class Awaitable{
};

/**
 * @brief used to forward the parameters to the io_uring event-loop
 * 
 * @details Now, the event-loop is implemented by the io_uring, we can use co_await xxxAttr to prepare the sqe
 * it will automatically transform into an SubmitAwaitable, which will warp the current coroutine into the sqe->user_data
 * This class controls the lifetime of the coroutine have co_await Attr.
 */
class SubmitAwaitable : public Awaitable{
public:
    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle){
        sqe->user_data = getScheduler().getNewId();
        getScheduler().handles[sqe->user_data] = handle;
    }
    int await_resume(){
        return *res;
    }
protected:
    SubmitAwaitable(io_uring_sqe* sqe, int* res) : sqe(sqe), res(res){}
    io_uring_sqe* sqe;
    int* res;
};

template<typename TaskType>
class TaskAwaitable : public Awaitable{
public:
    explicit TaskAwaitable(TaskType& task) : task(task) {}
    // HACK: didn't really think about why use task.coro.done() here
    bool await_ready() noexcept {
        return task.coro.done();
    }
    void await_suspend(std::coroutine_handle<> awaiting) noexcept {
        task.coro.promise().caller = awaiting;
        task.coro.resume();
    }
    TaskType::return_type await_resume() {
        auto promise = static_cast<TaskType::promise_type&>(task.coro.promise());
        return std::any_cast<typename TaskType::return_type>(promise.data);
    }
private:
    // HACK: and here the awaiter store the reference of the Task object, why not promise_type? or?
    // TODO: the lifetime of this, should be considered more carefully
    TaskType& task;
};

template<typename T>
struct awaitable_traits{
    using type = T;
};