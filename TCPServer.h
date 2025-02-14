#pragma once
#include <cstddef>
#include <cstring>
#include <liburing/io_uring.h>
#include <map>
#include <liburing.h>
#include "IOUring.h"
#include <string>
#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include "Connection.h"
#include "Socket.h"


struct AcceptAttr : Attr{
    int fd;
    sockaddr_in* clientAddr;
    socklen_t* len;
};

class AcceptAwaitable : public Awaitable{
public:
    AcceptAwaitable(AcceptAttr attr, int* res) : Awaitable{attr.sqe, res} {
        io_uring_prep_accept(attr.sqe, attr.fd, reinterpret_cast<sockaddr*>(attr.clientAddr), attr.len, 0);
    }
};

template<>
struct awaitable_traits<AcceptAttr>{
    using type = AcceptAwaitable;
};

class TCPServer{

public:
    TCPServer() : serverSocket("8080"){
        serverSocket.setResusePort();
    }
    TCPServer(const std::string& ip_port) : serverSocket(ip_port){
    }
    ~TCPServer(){}

    Task accept(InetAddr* clientAddr){
        while (true) {
            io_uring_sqe *sqe = io_uring_get_sqe(ring.getRing());
            auto len = clientAddr->get_size();
            AcceptAttr attr{{sqe}, serverSocket.getFd(), clientAddr->getAddr(), &len};
            int res = co_await attr;
            std::cout << "ACCEPTED: " << res << " FROM: " << clientAddr->get_sin_addr() << std::endl;
            if (res < 0) {
                std::cout << "ERROR: "<< strerror(-res) << std::endl;
                co_return State::Close;
            }
            connections.emplace(std::piecewise_construct_t{}, std::forward_as_tuple(res), std::forward_as_tuple(res, ring.getRing()));
            auto t = connections[res].read();
            if(t.handle.promise().state == State::Close){
                connections.erase(res);
            }
        }
    }

    void run(){
        serverSocket.listen(5);
        ring.init();
        
        InetAddr clientAddr;
        accept(&clientAddr);
        while (true){
            io_uring_cqe *cqe;
            int ret = io_uring_submit_and_wait(ring.getRing(), 1);
            if(ret < 0){
                std::cout << "ERROR in wait_cqe: "<< strerror(-ret) << std::endl;
            }
            io_uring_peek_cqe(ring.getRing(), &cqe);
            auto handle = std::coroutine_handle<promise_res>::from_address(reinterpret_cast<void*>(cqe->user_data));
            handle.promise().res = cqe->res;
            handle.resume();
            io_uring_cqe_seen(ring.getRing(), cqe);
        }
    }

    IOUring ring;
private:
    Socket serverSocket;
 
    using ConnectionMap = std::map<int, Connection>;
    ConnectionMap connections;
};