#pragma once
#include <coroutine>
#include <liburing.h>
#include <liburing/io_uring.h>
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

    // HACK: the lifetime of the coroutine is intriguing, and should be take more care 
    // ~TaskBase() { if (coro) coro.destroy(); }
    ~TaskBase() {}

    void resume(){
        // FIXME: if(coro.done()), might be a bug
        coro.resume();
    }

protected:
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