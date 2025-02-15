#pragma once
#include <coroutine>
#include <iostream>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <netinet/in.h>
#include <sys/socket.h>

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

enum class State{
    Working,
    Close
};

struct promise_base{
    std::coroutine_handle<> caller = nullptr;
    void unhandled_exception() {}
};

struct promise_always : public promise_base{
    std::suspend_always initial_suspend() noexcept { return {}; }
};

struct promise_res_int : public promise_always{
    int res;
};

template<typename T>
class Task;

template<>
class Task<void>;

template<>
class Task<int>;

template<>
struct awaitable_traits<Task<int>>{
    using type = Task<int>;
};

template<typename Task>
struct TaskTraits;


template <typename Derived>
class TaskBase{
public:
    using promise_type = typename ::promise_base;
    using handle_type = std::coroutine_handle<promise_type>;

    TaskBase(promise_type& p) : coro(handle_type::from_promise(p)) {}
    TaskBase(const TaskBase&) = delete;
    TaskBase(TaskBase&& t) : coro(t.coro) { t.coro = nullptr; }
    // ~TaskBase() { if (coro) coro.destroy(); }
    ~TaskBase() {}

    void resume(){
        // if(coro.done())
        coro.resume();
    }


protected:
    handle_type coro;
};

template<typename T>
class Task : public TaskBase<Task<T>>{
public:
    using Base = TaskBase<Task<T>>;
    struct promise_type;
    Task(promise_type& p) : TaskBase<Task<T>>(static_cast<promise_base&>(p)) {}

    struct promise_type : public promise_always{
        T res;

        template<typename U>
        void return_value(U&& v) { res = std::forward<U>(v); }

        auto get_return_object() {
            return Task{ *this };
        }

        std::suspend_never final_suspend() noexcept {
            if(caller){
                caller.resume();
            }
            return {};
        }

        template<typename AwaitableAttr>
        auto await_transform(AwaitableAttr&& attr){
            if constexpr(std::is_same_v<AwaitableAttr, typename awaitable_traits<AwaitableAttr>::type>){
                return attr;
            }
            using awaitable_type = typename awaitable_traits<AwaitableAttr>::type;
            return awaitable_type{attr, &res};
        }
    };

    auto operator co_await() & noexcept {
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

    struct promise_type : public promise_always{
        auto get_return_object() {
            return Task{ *this };
        }

        std::suspend_never final_suspend() noexcept { 
            if (caller) {
                caller.resume();
            }    
            return {}; 
        }

        template<typename AwaitableAttr>
        auto await_transform(AwaitableAttr&& attr){
            using real_type = std::remove_reference_t<AwaitableAttr>;
            if constexpr(std::is_same_v<real_type, typename awaitable_traits<real_type>::type>){
                #pragma message("AwaitableAttr is the same as its type")
                return attr.operator co_await();
            }
            else{
                using awaitable_type = typename awaitable_traits<AwaitableAttr>::type;
                return awaitable_type{attr, nullptr};
            }
        }
        void return_void() {}
    };

    auto operator co_await() & noexcept {
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

template<>
class Task<int> : public TaskBase<Task<int>>{
public:
    using Base = TaskBase<Task<int>>;
    struct promise_type;
    Task(promise_type& p) : TaskBase<Task<int>>(static_cast<promise_base&>(p)) {}

    struct promise_type : public promise_res_int{

        void return_value(int v) { res = v; }
        
        auto get_return_object() {
            return Task{ *this };
        }

        std::suspend_never final_suspend() noexcept {
            if(caller){
                caller.resume();
            }
            return {};
        }

        template<typename AwaitableAttr>
        auto await_transform(AwaitableAttr&& attr){
            using real_type = std::remove_reference_t<AwaitableAttr>;
            if constexpr(std::is_same_v<real_type, typename awaitable_traits<real_type>::type>){
                #pragma message("AwaitableAttr is the same as its type")
                return attr.operator co_await();
            }
            else {
                using awaitable_type = typename awaitable_traits<AwaitableAttr>::type;
                return awaitable_type{attr, &res};
            }
        }
    };

    auto operator co_await() & noexcept {
        struct awaiter {
            Task& task;
            bool await_ready() noexcept {
                return task.coro.done();
            }
            void await_suspend(std::coroutine_handle<> awaiting) noexcept {
                task.coro.promise().caller = awaiting;
                task.coro.resume();
            }
            int await_resume() {
                return static_cast<promise_type&>(task.coro.promise()).res;
            }
        };
        return awaiter{ *this };
    }
};