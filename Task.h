#pragma once
#include <coroutine>
#include <iostream>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <netinet/in.h>
#include <sys/socket.h>

// NOTE: Attr, Awaitable and awaitable_traits used to foward the parameters to the io_uring functions
struct Attr{
    io_uring_sqe* sqe;
};

class Awaitable{
public:
    bool await_ready(){
        return false;
    }
    void await_suspend(std::coroutine_handle<> handle){
        sqe->user_data = reinterpret_cast<uint64_t>(handle.address());
    }
    int await_resume(){
        return *res;
    }
protected:
    Awaitable(io_uring_sqe* sqe, int* res) : sqe(sqe), res(res){}
    io_uring_sqe* sqe;
    int* res;
};

template<typename T>
struct awaitable_traits{
    using type = T;
};

// forward declaration
template<typename T>
class Task;

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
    std::coroutine_handle<> caller = nullptr;
    void unhandled_exception() {}

    std::suspend_always initial_suspend() noexcept { return {}; }
    // TODO: further investigate the lifetime of the coroutine, should we destroy it in the final_suspend?
    // HACK: I just destory the coroutine here, but it might be a bug
    std::suspend_never final_suspend() noexcept {
        if(caller){
            caller.resume();
        }
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
    T res;

    template<typename U>
    void return_value(U&& v) { res = std::forward<U>(v); }

    auto get_return_object() {
        return Task<T>{ *this };
    }

    template<typename AwaitableAttr>
    auto await_transform(AwaitableAttr&& attr){
        using real_type = std::remove_reference_t<AwaitableAttr>;
        if constexpr(std::is_same_v<real_type, typename awaitable_traits<real_type>::type>){
            // #pragma message("AwaitableAttr is the same as its type")
            return attr.operator co_await();
        }
        else {
            using awaitable_type = typename awaitable_traits<AwaitableAttr>::type;
            return awaitable_type{attr, &res};
        }
    }
};

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

    Task(promise_type& p) : TaskBase<Task<T>>(static_cast<promise_base&>(p)) {}

    // NOTE: here I come accross a problem that arises from the & after the function declaration
    // when there is a & after the function declaration, the function can only be call by a lvalue
    auto operator co_await() noexcept {
        struct awaiter {
            Task& task;
            bool await_ready() noexcept {
                return task.coro.done();
            }
            void await_suspend(std::coroutine_handle<> awaiting) noexcept {
                task.coro.promise().caller = awaiting;
                task.coro.resume();
            }
            T await_resume() {
                return static_cast<promise_type&>(task.coro.promise()).res;
            }
        };
        return awaiter{ *this };
    }
};

template<>
class Task<void> : public TaskBase<Task<void>>{
public:
    using Base = TaskBase<Task<void>>;
    struct promise_type;
    Task(promise_type& p) : TaskBase<Task<void>>(static_cast<promise_base&>(p)) {}

    struct promise_type : public promise_base{
        void return_void() {}
        
        auto get_return_object() {
            return Task<void>{ *this };
        }
    };

    auto operator co_await() noexcept {
        struct awaiter {
            Task& task;
            bool await_ready() noexcept {
                return task.coro.done();
            }
            void await_suspend(std::coroutine_handle<> awaiting) noexcept {
                task.coro.promise().caller = awaiting;
                task.coro.resume();
            }
            void await_resume() {}
        };
        return awaiter{ *this };
    }
};