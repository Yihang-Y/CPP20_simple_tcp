#include <cstddef>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <iostream>
#include "Task.h"
#include "utils.h"

class TCPServer;


class Connection : noncopyable{
public:
    int fd;
    char readBuf[4096];
    char writeBuf[4096];
    io_uring* ring;


    Connection(): fd(-1), ring(nullptr) {}
    Connection(int fd, io_uring* ring): fd(fd), ring(ring) {}
    ~Connection(){
        close(fd);
        std::cout << "Connection closed" << std::endl;
    }

    // add the task to the ring, await for the completion, then return the result
    Task read(){
        while (true) {
            io_uring_sqe *sqe = io_uring_get_sqe(ring);
            RecvAttr attr{{sqe}, fd, readBuf, 4096};
            int res = co_await attr;
            std::cout << "Received: " << res << " bytes" << " message: " << readBuf << std::endl;
            if (res == 0) {
                std::cout << "Connection closed" << std::endl;
                std::string goodbye = "Goodbye!";
                write(goodbye.c_str(), 8);
                // need to pass information
                co_return State::Close;
            }
            write(res);
        }
    };

    Task write(size_t size){
        io_uring_sqe *sqe = io_uring_get_sqe(ring);
        WriteAttr attr{{sqe}, fd, readBuf, size};
        int res = co_await attr;
        std::cout << "Sent: " << res << " bytes" << " message: " << readBuf << std::endl;
        co_return State::Close;
    };

    Task write(const char* writeBuf, size_t size){
        io_uring_sqe *sqe = io_uring_get_sqe(ring);
        WriteAttr attr{{sqe}, fd, writeBuf, size};
        int res = co_await attr;
        std::cout << "Sent: " << res << " bytes" << " message: " << writeBuf << std::endl;
        co_return State::Close;
    };
};