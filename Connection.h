#pragma once
#include <cstddef>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <iostream>
#include "Task.h"
#include "utils.h"
#include "Buffer.h"

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

class RecvAwaitable : public Awaitable{
public:
    RecvAwaitable(RecvAttr attr, int* res) : Awaitable{attr.sqe, res}{
        io_uring_prep_readv(attr.sqe, attr.fd, attr.buf, 2, 0);
    }
};

class WriteAwaitable : public Awaitable{
public:
    WriteAwaitable(WriteAttr attr, int* res) : Awaitable{attr.sqe, res}{
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

class TCPServer;


class Connection : noncopyable{
public:

    Connection(): fd(-1), ring(nullptr) {}
    Connection(int fd, io_uring* ring): fd(fd), ring(ring) {}
    ~Connection(){
        close(fd);
        std::cout << "Connection closed" << std::endl;
    }

    // add the task to the ring, await for the completion, then return the result
    Task read(){
        // 64KB extra buffer in stack
        char extraBuf[65536];
        while (true) {
            io_uring_sqe *sqe = io_uring_get_sqe(ring);
            struct iovec vec[2];
            vec[0].iov_base = readBuf.begin() + readBuf.writerIndex;
            vec[0].iov_len = readBuf.writableBytes();
            vec[1].iov_base = extraBuf;
            vec[1].iov_len = sizeof(extraBuf);

            std::cout << "Read: " << readBuf.writableBytes() << std::endl;

            RecvAttr attr{{sqe}, fd, vec, 2};

            int res = co_await attr;
            std::cout << "Received: " << res << " bytes" << std::endl;
            if (res == 0) {
                std::cout << "Connection closed" << std::endl;
                std::string goodbye = "Goodbye!";
                writeBuf.append(goodbye.c_str(), 8);
                write(8);
                // need to pass information
                co_return State::Close;
            }
            else if (res < 0) {
                std::cout << "ERROR: "<< strerror(-res) << std::endl;
                co_return State::Close;
            }
            else if (res <= readBuf.writableBytes()) {
                readBuf.writerIndex += res;
            }
            else {
                size_t writableBytes = readBuf.writableBytes();
                readBuf.writerIndex = readBuf.size();
                readBuf.append(extraBuf, res - writableBytes);
            }
            std::string message = readBuf.retrieveAsString(res);
            std::cout << "Message: " << message << std::endl;
            writeBuf.append(message.c_str(), res);
            write(res);
        }
    };

    Task write(size_t size){
        while (size > 0){
            io_uring_sqe *sqe = io_uring_get_sqe(ring);
            char* buf = writeBuf.begin() + writeBuf.readerIndex;
            WriteAttr attr{{sqe}, fd, buf, size};
            int res = co_await attr;
            std::cout << "Sent: " << res << " bytes" << std::endl;
            writeBuf.readerIndex += res;
            size -= res;
        }
        co_return State::Close;
    };

private:
    int fd;
    Buffer readBuf;
    Buffer writeBuf;
    io_uring* ring;
};