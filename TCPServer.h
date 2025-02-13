#include <cstddef>
#include <cstdint>
#include <cstring>
#include <future>
#include <liburing/io_uring.h>
#include <map>
#include <liburing.h>
#include "IOUring.h"
#include <string>
#include <sys/socket.h>
#include <system_error>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <utility>
#include "Connection.h"


class Socket{

public:
    Socket(const std::string& ip_port){
        uint16_t port = std::stoi(ip_port);
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            throw std::system_error(errno, std::system_category(), "socket");
        }
        serverAddr = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
            .sin_addr = {
                .s_addr = INADDR_ANY
                }
        };
        // reuse port
        int opt = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
            throw std::system_error(errno, std::system_category(), "setsockopt");
        }
        if (bind(fd, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            throw std::system_error(errno, std::system_category(), "bind");
        }
    }
    ~Socket(){
        close(fd);
    }

    void listen(int backlog){
        if (::listen(fd, backlog) < 0) {
            throw std::system_error(errno, std::system_category(), "listen");
        }
    }

    int fd;        
private:
    sockaddr_in serverAddr;
};

class Task;
class TCPServer;


class TCPServer{

public:
    TCPServer() : serverSocket("8080"){
        // ring.init();
    }
    TCPServer(const std::string& ip_port) : serverSocket(ip_port){
        // ring.init();
    }
    ~TCPServer(){}

    Task accept(sockaddr_in* clientAddr, socklen_t* len){
        while (true) {
            AcceptAttr attr{{ring.getRing()}, serverSocket.fd, clientAddr, len};
            int res = co_await attr;
            std::cout << "ACCEPTED: " << res << " FROM: " << inet_ntoa(clientAddr->sin_addr) << std::endl;
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
        
        sockaddr_in clientAddr;
        socklen_t len = sizeof(sockaddr_in);
        accept(&clientAddr, &len);
        while (true){
            io_uring_cqe *cqe;
            int ret = io_uring_wait_cqe(ring.getRing(), &cqe);
            if(ret < 0){
                std::cout << "ERROR in wait_cqe: "<< strerror(-ret) << std::endl;
            }
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