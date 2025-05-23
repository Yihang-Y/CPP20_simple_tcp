#pragma once
// #include "Awaitable.h"
#include <coroutine>
#include <any>
#include <cstddef>
#include <iostream>

// forward declaration
template<typename T>
class Task;
template<typename T>
struct awaitable_traits;
struct DoAsOriginal;


struct final_awaiter {
    bool await_ready() noexcept { return false; }
    template<typename PromiseType>
    auto await_suspend(std::coroutine_handle<PromiseType> handle) noexcept {
        auto& promise = handle.promise();
        return promise.caller ? promise.caller : std::noop_coroutine();
    }
    void await_resume() noexcept {}
};

/**
 * @brief used to implement the chains of coroutines
 * 
 * @details store a coroutine_handle to the caller coroutine,
 *  when finished, resume the caller coroutine in the final_suspend.
 * and the initial_suspend should be always suspend, as it cooperates with @function operator co_await
 * when you await a coroutine in a coroutine, i,e, co_await Task<int>{}, operator co_await will be called, 
 * it will use coro.resume() to explicitly resume the coroutine.
 */
struct promise_base{
    promise_base() = default;
    promise_base(std::coroutine_handle<> caller, std::any data) : caller(caller), data(std::move(data)), detached_(false) {}
    std::coroutine_handle<> caller = nullptr;
    std::any data;
    bool detached_ = false;

    void detach() { detached_ = true; }
    bool is_detached() const { return detached_; }

    void unhandled_exception() {
        std::cout <<"ERROR: unhandled exception in coroutine" << std::endl;
        std::terminate();
    }

    std::suspend_always initial_suspend() noexcept { return {}; }
    // TODO: further investigate the lifetime of the coroutine, should we destroy it in the final_suspend?
    // HACK: I just destory the coroutine here, but it might be a bug
    // std::suspend_never final_suspend() noexcept {
    //     if(caller){
    //         caller.resume();
    //     }
    //     return {};
    // }

    // NOTE: A better way to implement the final_suspend
    final_awaiter final_suspend() noexcept {
        return {};
    }
};

/**
 * @brief Store the result of the coroutine, cooresponding to the Task<T>
 * 
 * @tparam T 
 * 
 * @note 
 * - // NOTE: maybe should consider more about if Task and promise_type correspond to each other so well ?
 * - // TODO: maybe promise_type should store more stuffs?
 */
template<typename T>
struct promise_type : public promise_base{
    // T res;
    promise_type() : promise_base(nullptr, T{}){}
    using value_type = std::remove_reference_t<T>;

    template<typename U>
    void return_value(U&& v) { 
        data = static_cast<value_type>(std::forward<U>(v));
    }

    auto get_return_object() {
        return Task<T>{ *this };
    }

    template<typename AwaitableAttr>
    auto await_transform(AwaitableAttr&& attr){
        using real_type = std::remove_reference_t<AwaitableAttr>;
        if constexpr(std::is_same_v<real_type, typename awaitable_traits<real_type>::type>){
            // #pragma message("AwaitableAttr is the same as its type")
            return attr.operator co_await();
        } else if constexpr (std::is_same_v<typename ::DoAsOriginal, typename awaitable_traits<AwaitableAttr>::type>){
            #pragma message("AwaitableAttr is DoAsOriginal")
            return attr;
        }
        else {
            using awaitable_type = typename awaitable_traits<AwaitableAttr>::type;
            int* d = nullptr;
            try {
                d = &std::any_cast<value_type&>(data);
            } catch (const std::bad_any_cast& e) {
                std::cout << "ERROR: " << e.what() << std::endl;
                throw;
            }
            return awaitable_type{attr, d};
        }
    }
};