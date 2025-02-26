#pragma once
#include <coroutine>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <any>
#include "Promise.h"

// NOTE: Attr, Awaitable and awaitable_traits used to foward the parameters to the io_uring functions
struct Attr{
    io_uring_sqe* sqe;
};

class Awaitable{
};

template<typename TaskType>
class TaskAwaitable : public Awaitable{
public:
    explicit TaskAwaitable(TaskType& task) : task(task) {}
    // HACK: didn't really think about why use task.coro.done() here
    bool await_ready() noexcept {
        return task.coro.done();
    }

    /**
     * @brief we will pass the control to the task, and setting the caller of the task to the current coroutine
     * if the task is canceled, we will just go forward to the resume function
     * 
     * @param awaiting 
     */
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> awaiting) noexcept {
        task.coro.promise().caller = awaiting;
        // task.resume();
        return task.coro;
    }
    TaskType::return_type await_resume() {
        if (task.coro.promise().canceled){
            throw CancelledException{};
        }
        if constexpr(std::is_same_v<typename TaskType::return_type, void>){
                return;
        }
        else{
            auto coro = std::coroutine_handle<typename TaskType::promise_type>::from_address(task.coro.address());
            return std::any_cast<typename TaskType::return_type>(coro.promise().data);
        }
    }
private:
    // HACK: and here the awaiter store the reference of the Task object, why not promise_type? or?
    // TODO: the lifetime of this, should be considered more carefully
    TaskType& task;
};

class CoroAwaitable : public Awaitable{
public:
    bool await_ready() noexcept { return false; }
    bool await_suspend(std::coroutine_handle<> handle){
        coro = handle;
        return false;
    }
    std::coroutine_handle<> await_resume(){
        return coro;
    }
private:
    // std::coroutine_handle<promise_base> coro;
    std::coroutine_handle<> coro;
};

class ForgetAwaitable : public Awaitable{
public:
    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle){
        // HACK: just forget the coroutine
        // FIXME: should be destroy the coroutine somehow
    }
    void await_resume(){}
};

// HACK: just a tag here, to distinguish in @PromiseType::await_transform
struct DoAsOriginal{};

template<typename T>
struct awaitable_traits{
    using type = T;
};

template<>
struct awaitable_traits<CoroAwaitable>{
    using type = typename ::DoAsOriginal;
};

template<>
struct awaitable_traits<ForgetAwaitable>{
    using type = typename ::DoAsOriginal;
};