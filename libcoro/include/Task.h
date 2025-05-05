#pragma once
#include <coroutine>
#include <iostream>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include "Awaitable.h"
#include "Promise.h"


template <typename Derived>
class TaskBase{
public:
    using promise_type = typename ::promise_base;
    using handle_type = std::coroutine_handle<promise_type>;

    /**
     * @brief Construct a new Task Base object
     * 
     * @param p 
     *
     * @note
     * - // NOTE: as template class will not mantain the inheritance relationship
     * - // so just use the inheritance of promise_type to initialize the coro
     * - // and the original promise_type can be retrieved by using CRTP technique if needed
     * - // but for now it is not utilized
     */
    TaskBase(promise_type& p) : coro(handle_type::from_promise(p)) {}
    TaskBase(const TaskBase&) = delete;
    TaskBase(TaskBase&& t) : coro(t.coro) { t.coro = nullptr; }

    ~TaskBase() { 
        if (coro && !coro.promise().is_detached()) { 
            coro.destroy();
        }
    }

    void resume(){
        // FIXME: if(coro.done()), might be a bug
        if (!coro.done())
        {
            // std::cout << "RESUME: " << &coro << std::endl;
            coro.resume();
        }
    }

    void then(std::coroutine_handle<> nextTask) {
        if(coro.promise().caller){
            // FIXME: should find another way to implement this feat
            std::cout << "ERROR: set nextTask for a coroutine that already has a caller" << std::endl;
        }else{
            coro.promise().caller = nextTask;
        }
        return this->resume();
    }

    // NOTE: it will be meaningless as you can't get its return_type inside the coroutine, the only way to get
    // a coro inside the coroutine is to use the co_await
    // std::coroutine_handle<> get_this_coro(){
    //     return coro;
    // }

    // 将协程标记为分离状态，此后生命周期由调度器管理
    void detach() {
        if (coro) {
            coro.promise().detach();
        }
    }

    // 查询协程是否已完成
    bool isCompleted() const {
        return coro == nullptr || coro.done();
    }

// protected:
    handle_type coro;
};

template<typename T>
class Task : public TaskBase<Task<T>>{
public:
    using Base = TaskBase<Task<T>>;
    using promise_type = typename ::promise_type<T>;
    using return_type = T;
    friend TaskAwaitable<Task<T>>;

    Task(promise_type& p) : TaskBase<Task<T>>(static_cast<promise_base&>(p)) {}

    // NOTE: here I come accross a problem that arises from the & after the function declaration
    // when there is a & after the function declaration, the function can only be call by a lvalue
    auto operator co_await() noexcept {
        return TaskAwaitable<Task<T>>(*this);
    }
};

template<>
class Task<void> : public TaskBase<Task<void>>{
public:
    using Base = TaskBase<Task<void>>;
    using return_type = void;
    // NOTE: use friend to make the TaskAwaitable<Task<void>> access the private members, as it used to be the inner class
    friend TaskAwaitable<Task<void>>;
    
    Task(promise_type& p) : TaskBase<Task<void>>(static_cast<promise_base&>(p)) {}

    struct promise_type : public promise_base{
        void return_void() {}
        
        auto get_return_object() {
            return Task<void>{ *this };
        }
    };

    auto operator co_await() noexcept {
        return TaskAwaitable<Task<void>>(*this);
    }
};

template<typename T>
class when_any_state {
public:
    std::optional<T> result;
    std::exception_ptr error;
};

template<typename... Task>
auto when_any(Task&&... tasks) -> ::Task<when_any_state<std::variant<typename std::decay_t<Task>::return_type...>>>
{
    (std::cout << ... << &tasks ) << std::endl;
    std::atomic<bool> completed{false};
    using retutn_type = std::variant<typename std::decay_t<Task>::return_type...>;

    auto this_coro = co_await CoroAwaitable{};
    auto state = when_any_state<retutn_type>();

    auto start_task = [&state, &completed](auto& task) -> ::Task<void> {
        auto result = co_await task;
        if (!completed.exchange(true)) {
            state.result = retutn_type{std::in_place_index<0>, result};
        }
        else {
            state.error = std::current_exception();
        }
        co_return;
    };
    
    (start_task(tasks).then(this_coro), ...);
    co_await ForgetAwaitable{};
    // FIXME: cancel all the other tasks, and the IO_uring cancle part should be done in the Task itself

    co_return state;
};