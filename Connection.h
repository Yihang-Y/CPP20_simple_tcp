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
            RecvAttr attr{{ring}, fd, readBuf, 4096};
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
        WriteAttr attr{{ring}, fd, readBuf, size};
        int res = co_await attr;
        std::cout << "Sent: " << res << " bytes" << " message: " << readBuf << std::endl;
        co_return State::Close;
    };

    Task write(const char* writeBuf, size_t size){
        WriteAttr attr{{ring}, fd, writeBuf, size};
        int res = co_await attr;
        std::cout << "Sent: " << res << " bytes" << " message: " << writeBuf << std::endl;
        co_return State::Close;
    };
};