#pragma once
#include <atomic>
#include <coroutine>
#include <cstddef>
#include <iostream>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <memory>
#include <optional>
#include <ostream>
#include <tuple>
#include <type_traits>
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
    TaskBase(promise_type& p) : coro(handle_type::from_promise(p))
    {
    }
    TaskBase(const TaskBase&) = delete;
    TaskBase(TaskBase&& t) : coro(t.coro) { t.coro = nullptr; }

    // HACK: the lifetime of the coroutine is intriguing, and should be take more care 
    // ~TaskBase() { if (coro) coro.destroy(); }
    ~TaskBase() {
        std::cout << "Deconstruct TaskBase" << " " << std::endl;
        if (coro) {
            coro.destroy();
        }
    }

    void resume(){
        // FIXME: if(coro.done()), might be a bug
        if (!coro.done())
        {
            coro.resume();
        }
    }

    void then(std::coroutine_handle<>& nextTask) {
        if(coro.promise().caller){
            // FIXME: should find another way to implement this feat
            std::cout << "ERROR: set nextTask for a coroutine that already has a caller" << std::endl;
        }else{
            coro.promise().caller = nextTask;
        }
        return this->resume();
    }

    void cancel(){
        coro.promise().cancel();
    }

    handle_type coroHandle() {
        return coro;
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

    Task(promise_type& p) : TaskBase<Task<T>>(static_cast<promise_base&>(p)) {
    }

    // NOTE: here I come accross a problem that arises from the & after the function declaration
    // when there is a & after the function declaration, the function can only be call by a lvalue
    auto operator co_await() noexcept {
        return TaskAwaitable<Task<T>>(*this);
    }

    void cancel(){
        this->coro.promise().cancel();
    }
};

template<>
class Task<void> : public TaskBase<Task<void>>{
public:
    using Base = TaskBase<Task<void>>;
    using return_type = void;
    // using promise_type = typename ::promise_type<void>;
    // NOTE: use friend to make the TaskAwaitable<Task<void>> access the private members, as it used to be the inner class
    friend TaskAwaitable<Task<void>>;
    
    Task(promise_type& p) : TaskBase<Task<void>>(static_cast<promise_base&>(p)) {
    }

    auto operator co_await() noexcept {
        return TaskAwaitable<Task<void>>(*this);
    }

    struct promise_type : public ::promise_type<void>{
        auto get_return_object() {
            return Task<void>{ *this };
        }
    };
};

template<typename T>
class when_any_state {
public:
    std::optional<T> result;
    std::exception_ptr error;
    std::atomic<bool> completed{false};
    size_t index = -1;
    when_any_state() = default;
    when_any_state(const when_any_state& state){
        result = state.result;
        error = state.error;
        completed.store(state.completed.load());
        index = state.index;
    }
    when_any_state(when_any_state&&) = default;
};

template<typename Task, typename T>
auto start_task(Task& task, size_t id,  std::shared_ptr<when_any_state<T>> state) -> ::Task<void> {
    auto result = co_await task;
    if (!state->completed.exchange(true)) {
        state->result = T{std::in_place_index<0>, result};
        state->index = id;
    }
    else {
        state->error = std::current_exception();
    }
    co_return;
}

template<typename... Task>
auto when_any(Task&&... tasks) -> ::Task<when_any_state<std::variant<typename std::decay_t<Task>::return_type...>>>
{
    using retutn_type = std::variant<typename std::decay_t<Task>::return_type...>;

    auto this_coro = co_await CoroAwaitable{};
    // auto this_coro = std::coroutine_handle<>::from_address(&coro);
    auto state = std::make_shared<when_any_state<retutn_type>>();

    auto tasks_tuple = [&]<size_t... idx>(std::index_sequence<idx...>) {
        return std::make_tuple( start_task(tasks, idx, state)...);
    }(std::index_sequence_for<Task...>{});
    // start all the tasks
    [&]<size_t... idx>(std::index_sequence<idx...>) {
        (std::get<idx>(tasks_tuple).then(this_coro), ...);
    }(std::index_sequence_for<Task...>{});
    co_await ForgetAwaitable{};
    // FIXME: cancel all the other tasks, and the IO_uring cancle part should be done in the Task itself
    std::cout << state->index << std::endl;
    // 应该是这里cancel的问题，这里cancel没有co_await，但是中间又会交出控制权，函数直接执行到co_return
    [&]<size_t... idx>(std::index_sequence<idx...>) {
        ((idx != state->index ? std::get<idx>(tasks_tuple).cancel() : void()), ...);
    }(std::index_sequence_for<Task...>{});
    co_return (*(state));
};