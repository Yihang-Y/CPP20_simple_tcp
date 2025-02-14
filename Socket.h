#pragma once
#include <cstddef>
#include <string>
#include <system_error>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "utils.h"


class InetAddr{
public:
    explicit InetAddr(const std::string& ip_port){
        uint16_t port = std::stoi(ip_port);
        addr = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
            .sin_addr = {
                .s_addr = INADDR_ANY
                }
        };
    }
    InetAddr() = default;

    sockaddr_in* getAddr(){
        return &addr;
    }

    std::string get_sin_addr(){
        return inet_ntoa(addr.sin_addr);
    }
    
    socklen_t get_size(){
        socklen_t len = sizeof(sockaddr_in);
        return len;   
    }

private:
    sockaddr_in addr;
};


class Socket : public noncopyable {

public:
    Socket(const std::string& ip_port) : serverAddr(ip_port){
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            throw std::system_error(errno, std::system_category(), "socket");
        }

        if (bind(fd, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            throw std::system_error(errno, std::system_category(), "bind");
        }
    }

    ~Socket(){
        close(fd);
    }

    void setResusePort(){
        int opt = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
            throw std::system_error(errno, std::system_category(), "setsockopt");
        }
    }

    void listen(int backlog){
        if (::listen(fd, backlog) < 0) {
            throw std::system_error(errno, std::system_category(), "listen");
        }
    }

    int getFd(){
        return fd;
    }   
private:
    InetAddr serverAddr;
    int fd;
};