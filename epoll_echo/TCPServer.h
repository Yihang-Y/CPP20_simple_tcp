#pragma once
#include "utils.h"
#include "Socket.h"
#include "Connection.h"
#include "Acceptor.h"
#include "EventLoop.h"

#include <functional>
#include <map>
#include <memory>
#include <string>

class TCPServer : noncopyable {
public:
    TCPServer(EventLoop* loop, const std::string& port) 
        : loop_(loop), 
          acceptor_(new Acceptor(loop, port)) 
    {
        acceptor_->addNewConnectionCallback(
            [this](int sockfd, const InetAddr& peerAddr) {
                newConnection(sockfd, peerAddr);
            });
    }

    ~TCPServer() {
        for (auto& conn : connections_) {
            conn.second.reset();
        }
    }

    void start() {
        loop_->runInLoop([this]() { acceptor_->listen(); });
    }

private:
    void newConnection(int sockfd, const InetAddr& peerAddr) {
        auto conn = std::make_shared<Connection>(loop_, sockfd, peerAddr);
        connections_[sockfd] = conn;
        conn->setReadCallback([](const std::string& msg) {
            // 读回调在Connection中已处理回显逻辑
        });
        conn->enableReading();
    }

    EventLoop* loop_;
    std::unique_ptr<Acceptor> acceptor_;
    using ConnectionMap = std::map<int, std::shared_ptr<Connection>>;
    ConnectionMap connections_;
};