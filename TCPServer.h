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

class AcceptAwaitable : public SubmitAwaitable{
public:
    AcceptAwaitable(AcceptAttr attr, int* res) : SubmitAwaitable{attr.sqe, res}{
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

    Task<int> accept(InetAddr* clientAddr) {
        io_uring_sqe *sqe = io_uring_get_sqe(ring.getRing());
        auto len = clientAddr->get_size();
        int res = co_await AcceptAttr{{sqe}, serverSocket.getFd(), clientAddr->getAddr(), &len};
        std::cout << "ACCEPTED: " << res << " FROM: " << clientAddr->get_sin_addr() << std::endl;
        if (res < 0) {
            std::cout << "ERROR: "<< strerror(-res) << std::endl;
            co_return -1;
        }
        co_return res;
    }

    Task<void> echo(){
        while (true){
            InetAddr clientAddr;
            auto clientFd = co_await accept(&clientAddr);
            if (clientFd < 0) {
                std::cout << "ERROR: "<< strerror(-clientFd) << std::endl;
                continue;
            }
            connections.emplace(std::piecewise_construct_t{}, std::forward_as_tuple(clientFd), std::forward_as_tuple(clientFd, ring.getRing()));
            // spawn(handle_client(clientFd));
            handle_client(clientFd).resume();
        }
    }

    Task<void> handle_client(int clientFd){ 
        while (true){
            auto res = co_await connections[clientFd].read();
            if (res <= 0) {
                connections.erase(clientFd);
                co_return;
            }

            connections[clientFd].writeBuf.append(connections[clientFd].readBuf.peek(), res);

            auto writeRes = co_await connections[clientFd].write(res);
            if (writeRes < 0) {
                connections.erase(clientFd);
                co_return;
            }
        }
    }


    void run(){
        serverSocket.listen(5);
        ring.init();
        
        Task t = echo();
        t.resume();
        while (true){
            io_uring_cqe *cqe;
            int ret = io_uring_submit_and_wait(ring.getRing(), 1);
            if(ret < 0){
                std::cout << "ERROR in wait_cqe: "<< strerror(-ret) << std::endl;
            }
            io_uring_peek_cqe(ring.getRing(), &cqe);
            auto handle = std::coroutine_handle<promise_type<int>>::from_address(reinterpret_cast<void*>(cqe->user_data));
            (handle.promise().res) = cqe->res;
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