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

struct promise_res{
    // store the result of the async iouring operation
    int res;
    // store the state of the coroutine task
    State state = State::Working;
};


class Task{
public:
    // inherit from promise_res, so we can get this from coroutine_handle
    struct promise_type : promise_res{
        Task get_return_object(){
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend(){
            return {};
        }
        std::suspend_never final_suspend() noexcept{
            return {};
        }

        void return_value(State state){
            this->state = state;
        }

        void unhandled_exception(){
            std::terminate();
        }

        // using traits to get the awaitable type
        template<typename AwaitableAttr>
        auto await_transform(AwaitableAttr& attr){
            using awaitable_type = typename awaitable_traits<AwaitableAttr>::type;
            return awaitable_type{attr, &res};
        }
    };
    Task(std::coroutine_handle<promise_type> handle) : handle(handle){}
    std::coroutine_handle<promise_type> handle;
};