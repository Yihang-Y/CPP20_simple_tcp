#pragma once
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
    TaskBase(promise_type& p) : coro(handle_type::from_promise(p)) {}
    TaskBase(const TaskBase&) = delete;
    TaskBase(TaskBase&& t) : coro(t.coro) { t.coro = nullptr; }

    // HACK: the lifetime of the coroutine is intriguing, and should be take more care 
    // ~TaskBase() { if (coro) coro.destroy(); }
    ~TaskBase() {}

    void resume(){
        // FIXME: if(coro.done()), might be a bug
        if (!coro.done())
        {
            std::cout << "RESUME: " << &coro << std::endl;
            coro.resume();
        }
    }

    void then(std::coroutine_handle<>& nextTask) {
        if(coro.promise().caller){
            // FIXME: should find another way to implement this feat
            std::cout << "ERROR: set nextTask for a coroutine that already has a caller" << std::endl;
        }else{
            std::cout << "SET CALLER: " << &nextTask << std::endl;
            coro.promise().caller = nextTask;
        }
        return this->resume();
    }

    // NOTE: it will be meaningless as you can't get its return_type inside the coroutine, the only way to get
    // a coro inside the coroutine is to use the co_await
    // std::coroutine_handle<> get_this_coro(){
    //     return coro;
    // }

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

        template<typename AwaitableAttr>
        auto await_transform(AwaitableAttr&& attr){
            using real_type = std::remove_reference_t<AwaitableAttr>;
            if constexpr(std::is_same_v<real_type, typename awaitable_traits<real_type>::type>){
                #pragma message("AwaitableAttr is the same as its type")
                // FIXME: should be more explicit here
                std::cout << "call await_transform with the same type" << std::endl;
                // NOTE: only when you co_awiat a Task, you need to store the callee
                callee = attr.coro;
                return attr.operator co_await();
            } else if constexpr (std::is_same_v<typename ::DoAsOriginal, typename awaitable_traits<AwaitableAttr>::type>){
                // #pragma message("AwaitableAttr is DoAsOriginal")
                return attr;
            }
            else {
                using awaitable_type = typename awaitable_traits<AwaitableAttr>::type;
                return awaitable_type{attr};
            }
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
    // auto this_coro = std::coroutine_handle<>::from_address(&coro);
    auto state = when_any_state<retutn_type>();

    auto start_task = [&state, &completed](auto& task) -> ::Task<void> {
        std::cout << std::is_same_v<std::remove_reference_t<decltype(task)>, ::Task<int>> << std::endl;
        auto result = co_await task;
        if (!completed.exchange(true)) {
            state.result = retutn_type{std::in_place_index<0>, result};
        }
        else {
            state.error = std::current_exception();
        }
        co_return;
    };
    auto tasks_tuple = [&]<size_t... idx>(std::index_sequence<idx...>) {
        return std::make_tuple( start_task.operator()(tasks)... );
    }(std::index_sequence_for<Task...>{});
    // start all the tasks
    [&]<size_t... idx>(std::index_sequence<idx...>) {
        (std::get<idx>(tasks_tuple).then(this_coro), ...);
    }(std::index_sequence_for<Task...>{});
    co_await ForgetAwaitable{};
    // FIXME: cancel all the other tasks, and the IO_uring cancle part should be done in the Task itself
    // 获取当前所有的task，在单线程的情况下，task肯定是在某处co_await, 所以cancel的目的是从得到的Task向下找
    [&]<size_t... idx>(std::index_sequence<idx...>) {
        (std::get<idx>(tasks_tuple).coro.promise().cancel(), ...);
    }(std::index_sequence_for<Task...>{});
    co_return state;
};