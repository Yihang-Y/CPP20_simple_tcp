#pragma once
#include "utils.h"
#include "Channel.h"
#include "Socket.h"
#include "Buffer.h"
#include <functional>
#include <memory>
#include <string>

class Connection : noncopyable {
public:
    using ReadCallback = std::function<void(const std::string&)>;
    
    Connection(EventLoop* loop, int sockfd, const InetAddr& peerAddr)
        : loop_(loop),
          channel_(new Channel(loop, sockfd))
    {
        channel_->setReadCallback([this]() { handleRead(); });
        channel_->setWriteCallback([this]() { handleWrite(); });
        channel_->setCloseCallback([this]() { handleClose(); });
        channel_->setErrorCallback([this]() { handleError(); });
    }
    
    ~Connection() {
        channel_->disableAll();
        channel_->remove();
    }
    
    void setReadCallback(ReadCallback cb) { readCallback_ = std::move(cb); }
    
    void enableReading() { channel_->enableReading(); }
    void disableReading() { channel_->disableReading(); }
    
    int fd() const { return channel_->getFd(); }
    
private:
    void handleRead() {
        int savedErrno = 0;
        ssize_t n = buffer_.readFromFd(channel_->getFd(), &savedErrno);
        if (n > 0) {
            if (readCallback_) {
                std::string msg = buffer_.retrieveAsString(n);
                readCallback_(msg);
                // 回显
                ::write(channel_->getFd(), msg.c_str(), msg.size());
            }
        } else if (n == 0) {
            handleClose();
        } else {
            errno = savedErrno;
            handleError();
        }
    }
    
    void handleWrite() {
        // 简单实现，无需写缓冲区
    }
    
    void handleClose() {
        channel_->disableAll();
    }
    
    void handleError() {
        // 错误处理
    }
    
    EventLoop* loop_;
    std::unique_ptr<Channel> channel_;
    ReadCallback readCallback_;
    Buffer buffer_;
}; 