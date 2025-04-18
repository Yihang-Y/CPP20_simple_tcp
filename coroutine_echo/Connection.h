#pragma once
#include <cstddef>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <iostream>
#include "Task.h"
#include "utils.h"
#include "Buffer.h"
#include "BufferOperations.h"

class TCPServer;


class Connection : noncopyable{
public:

    Connection(): fd(-1) {}
    Connection(int fd): fd(fd) {}
    ~Connection(){
        close(fd);
        // std::cout << "Deconstruct Connection" << std::endl;
    }

    Task<int> read(){
        auto res = co_await recv(readBuf, fd);
        if (res == 0) {
            // std::cout << "Connection closed" << std::endl;
            co_return 0;
        }
        else if (res < 0) {
            std::cout << "ERROR: "<< strerror(-res) << std::endl;
            co_return -1;
        }
        // std::cout << "Read " << res << " bytes" << std::endl;
        co_return res;
    }

    Task<int> write(size_t len){
        auto res = co_await send(writeBuf, fd, len);
        if (res < 0) {
            std::cout << "ERROR: "<< strerror(-res) << std::endl;
            co_return -1;
        }
        // std::cout << "Write " << len << " bytes" << std::endl;
        co_return len;
    }


    Buffer readBuf;
    Buffer writeBuf;

private:
    int fd;
    // io_uring* ring;
};