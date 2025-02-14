#include <coroutine>
#include <iostream>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <netinet/in.h>
#include <sys/socket.h>

struct Attr{
    io_uring_sqe* sqe;
};

struct RecvAttr : Attr{
    int fd;
    char* buf;
    size_t size;
};

struct AcceptAttr : Attr{
    int fd;
    sockaddr_in* clientAddr;
    socklen_t* len;
};

struct WriteAttr : Attr{
    int fd;
    const char* buf;
    size_t size;
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

class RecvAwaitable : public Awaitable{
public:
    RecvAwaitable(RecvAttr attr, int* res) : Awaitable{attr.sqe, res}{
        io_uring_prep_recv(attr.sqe, attr.fd, attr.buf, attr.size, 0);
    }
};


class AcceptAwaitable : public Awaitable{
public:
    AcceptAwaitable(AcceptAttr attr, int* res) : Awaitable{attr.sqe, res} {
        io_uring_prep_accept(attr.sqe, attr.fd, reinterpret_cast<sockaddr*>(attr.clientAddr), attr.len, 0);
    }
};

class WriteAwaitable : public Awaitable{
public:
    WriteAwaitable(WriteAttr attr, int* res) : Awaitable{attr.sqe, res}{
        io_uring_prep_send(attr.sqe, attr.fd, attr.buf, attr.size, 0);
    }
};

struct awaitable_traits{
    template<typename T>
    struct awaitable_type{
        using type = T;
    };

    template<>
    struct awaitable_type<RecvAttr>{
        using type = RecvAwaitable;
    };

    template<>
    struct awaitable_type<AcceptAttr>{
        using type = AcceptAwaitable;
    };

    template<>
    struct awaitable_type<WriteAttr>{
        using type = WriteAwaitable;
    };
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
            using awaitable_type = typename awaitable_traits::awaitable_type<AwaitableAttr>::type;
            return awaitable_type{attr, &res};
        }
    };
    Task(std::coroutine_handle<promise_type> handle) : handle(handle){}
    std::coroutine_handle<promise_type> handle;
};