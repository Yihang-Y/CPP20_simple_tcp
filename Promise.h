#pragma once
// #include "Awaitable.h"
#include <atomic>
#include <coroutine>
#include <any>
#include <cstddef>
#include <iostream>
#include <variant>

// forward declaration
template<typename T>
class Task;
template<typename T>
struct awaitable_traits;
struct DoAsOriginal;

class CancelledException : public std::exception{
public:
    const char* what() const noexcept override {
        return "Task canceled";
    }
};

struct cancel_tag{};

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
    promise_base(std::coroutine_handle<> caller, std::any data) : caller(caller), data(std::move(data)){}
    std::coroutine_handle<> caller = nullptr;
    std::coroutine_handle<> callee = nullptr;
    // std::variant<std::any, typename ::cancel_tag> data;
    std::any data;
    // 这样的话所有的promise中都会有一个这个值，但是除了最里面需要进行提交的部分需要这个判断，其他都是返回一个Task<cancelled>用来表示？
    // 不对co_await resume了之后得到的是一个返回值，这个只能通过
    std::atomic_bool canceled = false;

    void unhandled_exception() {
        try{
            throw;
        } catch (const CancelledException& e) {
            std::cout << "Caught exception: " << e.what() << std::endl;
        }
    }

    std::suspend_always initial_suspend() noexcept { return {}; }
    // TODO: further investigate the lifetime of the coroutine, should we destroy it in the final_suspend?
    // HACK: I just destory the coroutine here, but it might be a bug
    std::suspend_always final_suspend() noexcept {
        auto coro = std::coroutine_handle<>::from_address(this);
        std::cout << "SUSPEND: " << &(coro) << " " << this << std::endl;
        if(!canceled && caller){
            caller.resume();
        }
        std::cout << "back to suspend" << std::endl;
        return {};
    }

    // HACK: use this to resume the suspended task in the end of the coroutine
    void cancel() {
        if(callee) {
            canceled = true;
            std::cout << "cancel layer" << std::endl;
            std::coroutine_handle<promise_base>::from_address(callee.address()).promise().cancel();
        }else {
            canceled = true;
            auto coro = std::coroutine_handle<promise_base>::from_promise(*this);
            if (coro.done()) {
                coro.destroy();
            } else {
                std::cout << "cancel RESUME: " << this << std::endl;
                coro.resume();
            }
        }
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

            if constexpr(std::is_same_v<typename awaitable_type::type, void>){
                return awaitable_type{attr};
            }
            else{
                value_type* d = nullptr;
                try {
                    d = std::any_cast<value_type>(&data);
                } catch (const std::bad_any_cast& e) {
                    std::cout << "ERROR: " << e.what() << std::endl;
                    throw;
                }
                return awaitable_type{attr, d};
            }
        }
    }
};