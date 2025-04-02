#pragma once
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <functional>

class Acceptor {

public:
    using NewConnectionCallback = std::function<void(int, const InetAddr&)>;
    Acceptor(EventLoop* loop, const std::string& port) : acceptSocket_(port), 
                                                   acceptChannel_(loop, acceptSocket_.getFd()),
                                                   listenning_(false),
                                                   idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC)) {
        loop_ = loop;
        acceptSocket_.setReusePort();
        acceptSocket_.setNonBlocking();
        acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
    }
    ~Acceptor() {
        acceptChannel_.disableAll();
        acceptChannel_.remove();
        ::close(idleFd_);
    }

    void listen() {
        listenning_ = true;
        acceptSocket_.listen(5);
        acceptChannel_.enableReading();
    }

    void handleRead() {
        InetAddr clientAddr;
        int connfd = acceptSocket_.accept(&clientAddr);
        if (connfd >= 0) {
            if (newConnectionCallback_) {
                newConnectionCallback_(connfd, clientAddr);
            } else {
                close(connfd);
            }
        } else {
            // 处理接受连接失败情况
            if (errno == EMFILE) {
                ::close(idleFd_);
                idleFd_ = ::accept(acceptSocket_.getFd(), NULL, NULL);
                ::close(idleFd_);
                idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
            }
        }
    }

    void addNewConnectionCallback(const NewConnectionCallback& cb) {
        newConnectionCallback_ = cb;
    }

private:
    EventLoop* loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;
    int idleFd_;  // 用于处理文件描述符用尽的情况
};