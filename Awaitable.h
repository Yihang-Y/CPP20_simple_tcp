#pragma once
#include <coroutine>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <any>
#include <variant>
#include "IoUringScheduler.h"
#include "Promise.h"

// NOTE: Attr, Awaitable and awaitable_traits used to foward the parameters to the io_uring functions
struct Attr{
    io_uring_sqe* sqe;
};

class Awaitable{
protected:
    bool cancel = false;
};

/**
 * @brief used to forward the parameters to the io_uring event-loop
 * 
 * @details Now, the event-loop is implemented by the io_uring, we can use co_await xxxAttr to prepare the sqe
 * it will automatically transform into an SubmitAwaitable, which will warp the current coroutine into the sqe->user_data
 * This class controls the lifetime of the coroutine have co_await Attr.
 */
template<typename ResType>
class SubmitAwaitable : public Awaitable{
public:
    using type = ResType;
    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle){
        sqe->user_data = getScheduler().getNewId();
        getScheduler().handles[sqe->user_data] = handle;
    }
    ResType await_resume(){
        return *res;
    }
protected:
    SubmitAwaitable(io_uring_sqe* sqe, ResType* res) : sqe(sqe), res(res){}
    io_uring_sqe* sqe;
    ResType* res;
};

template<>
class SubmitAwaitable<void> : public Awaitable{
public:
    using type = void;
    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle){
        sqe->user_data = getScheduler().getNewId();
        getScheduler().handles[sqe->user_data] = handle;
    }
    void await_resume(){}
protected:
    SubmitAwaitable(io_uring_sqe* sqe) : sqe(sqe){}
    io_uring_sqe* sqe;
};

template<typename TaskType>
class TaskAwaitable : public Awaitable{
public:
    explicit TaskAwaitable(TaskType& task) : task(task) {}
    // HACK: didn't really think about why use task.coro.done() here
    bool await_ready() noexcept {
        // // 这里得到的task是当前co_await的task
        // if(task.coro.promise().canceled){
        //     // just go forward to resume function
        //     return true;
        // }
        return task.coro.done();
    }

    /**
     * @brief we will pass the control to the task, and setting the caller of the task to the current coroutine
     * if the task is canceled, we will just go forward to the resume function
     * 
     * @param awaiting 
     */
    void await_suspend(std::coroutine_handle<> awaiting) noexcept {
        if(task.coro.promise().canceled){
            cancel = true;
            awaiting.resume();
        }else{
            task.coro.promise().caller = awaiting;
            task.coro.resume();
        }
    }
    TaskType::return_type await_resume() {
        if (cancel) {
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
    void await_suspend(std::coroutine_handle<> handle){
        coro = handle;
        handle.resume();
        // coro = std::coroutine_handle<promise_base>::from_address(handle.address());
        // handle.resume();
    }
    // std::coroutine_handle<promise_base> await_resume(){
    //     return coro;
    // }
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