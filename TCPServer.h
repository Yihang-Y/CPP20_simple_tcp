#pragma once
#include <cstddef>
#include <cstring>
#include <liburing/io_uring.h>
#include <map>
#include <liburing.h>
#include "IoUringScheduler.h"
#include <string>
#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <variant>
#include "Connection.h"
#include "Socket.h"
#include "Task.h"


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

    /**
     * @brief warp the accept function with coroutine
     * 
     * @param clientAddr 
     * @return Task<int> 
     */
    Task<int> accept(InetAddr* clientAddr) {
        io_uring_sqe *sqe = io_uring_get_sqe(getScheduler().getRing());
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
            connections.emplace(std::piecewise_construct_t{}, std::forward_as_tuple(clientFd), std::forward_as_tuple(clientFd));
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

    Task<void> wait_one_accept(){
        auto clientAddr1 = new InetAddr();
        auto clientAddr2 = new InetAddr();

        auto task1 = accept(clientAddr1);
        auto task2 = accept(clientAddr2);

        auto res = co_await when_any(task1, task2);
        if (res.result.has_value()){
            auto v = res.result.value();
            std::visit([](auto&& arg){
                std::cout << "ACCEPTED: " << arg << std::endl;
            }, v);
        }
        else {
            throw res.error;
        }
    }


    void run(){
        serverSocket.listen(5);
        // ring.init();
        
        Task t = wait_one_accept();
        // FIXME: as I implement all Task as initial_suspend always, so I have to resume it here
        t.resume();

        getScheduler().run();
    }

    // IOUring ring;
private:
    Socket serverSocket;
 
    using ConnectionMap = std::map<int, Connection>;
    ConnectionMap connections;
};