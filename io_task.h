#pragma once
#include "Awaitable.h"
#include "IoUringScheduler.h"
#include <liburing.h>
#include <liburing/io_uring.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "Promise.h"
/**
 * @brief used to forward the parameters to the io_uring event-loop
 * 
 * @details Now, the event-loop is implemented by the io_uring, we can use co_await xxxAttr to prepare the sqe
 * it will automatically transform into an SubmitAwaitable, which will warp the current coroutine into the sqe->user_data
 * This class controls the lifetime of the coroutine have co_await Attr.
 */
template<typename ResType>
class SubmitAwaitable : public Awaitable{
public:
    using type = ResType;
    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle){
        sqe->user_data = getScheduler().getNewId();
        getScheduler().handles[sqe->user_data] = handle;
        // auto coro = std::coroutine_handle<promise_base>::from_address(handle.address());
        // coro.promise().callee = sqe->user_data;
    }
    ResType await_resume(){
        return *res;
    }
protected:
    SubmitAwaitable(io_uring_sqe* sqe, ResType* res) : sqe(sqe), res(res){}
    io_uring_sqe* sqe;
    ResType* res;
};

template<>
class SubmitAwaitable<void> : public Awaitable{
public:
    using type = void;
    bool await_ready() noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle){
        sqe->user_data = getScheduler().getNewId();
        getScheduler().handles[sqe->user_data] = handle;
    }
    void await_resume(){}
protected:
    SubmitAwaitable(io_uring_sqe* sqe) : sqe(sqe){}
    io_uring_sqe* sqe;
};


struct AcceptAttr : Attr{
    int fd;
    sockaddr_in* clientAddr;
    socklen_t* len;
};

class AcceptAwaitable : public SubmitAwaitable<int>{
public:
    AcceptAwaitable(AcceptAttr attr, int* res) : SubmitAwaitable{attr.sqe, res}{
        io_uring_prep_accept(attr.sqe, attr.fd, reinterpret_cast<sockaddr*>(attr.clientAddr), attr.len, 0);
    }
};

template<>
struct awaitable_traits<AcceptAttr>{
    using type = AcceptAwaitable;
};

struct CancelAttr : Attr{
    int data;
};

class CancelAwaitable : public SubmitAwaitable<void>{
public:
    CancelAwaitable(CancelAttr attr) : SubmitAwaitable{attr.sqe}{
        io_uring_prep_cancel64(attr.sqe, attr.data, 0);
    }
    bool await_suspend(std::coroutine_handle<> handle){
        sqe->user_data = getScheduler().getNewId();
        getScheduler().handles[sqe->user_data] = nullptr;
        return false;
    }
};

template<>
struct awaitable_traits<CancelAttr>{
    using type = CancelAwaitable;
};

struct RecvAttr : Attr{
    int fd;
    struct iovec* buf;
    size_t size;
};

struct WriteAttr : Attr{
    int fd;
    const char* buf;
    size_t size;
};

class RecvAwaitable : public SubmitAwaitable<int>{
public:
    RecvAwaitable(RecvAttr attr, int* res) : SubmitAwaitable{attr.sqe, res}{
        io_uring_prep_readv(attr.sqe, attr.fd, attr.buf, 2, 0);
    }
};

class WriteAwaitable : public SubmitAwaitable<int>{
public:
    WriteAwaitable(WriteAttr attr, int* res) : SubmitAwaitable{attr.sqe, res}{
        io_uring_prep_send(attr.sqe, attr.fd, attr.buf, attr.size, 0);
    }
};

template<>
struct awaitable_traits<RecvAttr>{
    using type = RecvAwaitable;
};

template<>
struct awaitable_traits<WriteAttr>{
    using type = WriteAwaitable;
};