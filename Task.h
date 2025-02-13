#include <coroutine>
#include <iostream>
#include <liburing.h>
#include <netinet/in.h>
#include <sys/socket.h>

struct Attr{
    io_uring* ring;
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

class RecvAwaitable{
public:
    RecvAwaitable(RecvAttr attr, int* res) : ring(attr.ring), res(res){
        sqe = io_uring_get_sqe(ring);
        io_uring_prep_recv(sqe, attr.fd, attr.buf, attr.size, 0);
    }
    bool await_ready(){
        return false;
    }
    void await_suspend(std::coroutine_handle<> handle){
        sqe->user_data = reinterpret_cast<uint64_t>(handle.address());
        if (int err = io_uring_submit(ring) < 0) {
            std::cout << "ERROR: "<< strerror(-err) << std::endl;
            handle.resume();
        }
        io_uring_submit(ring);
    }
    int await_resume(){
        return *res;
    }
private:
    io_uring* ring;
    io_uring_sqe* sqe;
    int* res;
};


class AcceptAwaitable{
public:
    AcceptAwaitable(AcceptAttr attr, int* res) : ring(attr.ring), res(res){
        sqe = io_uring_get_sqe(ring);
        io_uring_prep_accept(sqe, attr.fd, (sockaddr*)attr.clientAddr, attr.len, 0);
    }
    bool await_ready(){
        return false;
    }
    void await_suspend(std::coroutine_handle<> handle){
        sqe->user_data = reinterpret_cast<uint64_t>(handle.address());
        if (int err = io_uring_submit(ring) < 0) {
            std::cout << "ERROR: "<< strerror(-err) << std::endl;
            handle.resume();
        }
        io_uring_submit(ring);
    }
    int await_resume(){
        return *res;
    }
private:
    io_uring* ring;
    io_uring_sqe* sqe;
    int* res;
};

class WriteAwaitable{
public:
    WriteAwaitable(WriteAttr attr, int* res) : ring(attr.ring), res(res){
        sqe = io_uring_get_sqe(ring);
        io_uring_prep_write(sqe, attr.fd, attr.buf, attr.size, 0);
    }
    bool await_ready(){
        return false;
    }
    void await_suspend(std::coroutine_handle<> handle){
        sqe->user_data = reinterpret_cast<uint64_t>(handle.address());
        if (int err = io_uring_submit(ring) < 0) {
            std::cout << "ERROR: "<< strerror(-err) << std::endl;
            handle.resume();
        }
        io_uring_submit(ring);
    }
    int await_resume(){
        return *res;
    }
private:
    io_uring* ring;
    io_uring_sqe* sqe;
    int* res;
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