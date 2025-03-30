#pragma once
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <thread>
#include <sys/eventfd.h>
#include <unistd.h>
#include <iostream>
#include <cstdlib>

class Channel;
class EPoller;

// 声明为内联函数以避免多重定义
inline int createEventfd() {
    int fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (fd < 0) {
        perror("eventfd");
        exit(1);
    }
    return fd;
}

class EventLoop {
public:
    using Functor = std::function<void()>;
    using ChannelList = std::vector<Channel*>;
    
    EventLoop();
    ~EventLoop();

    void loop();
    void quit() { quit_ = true; wakeup(); }

    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

    void runInLoop(const Functor& cb);
    void doPendingFunctors();
    void wakeup();
    void handleRead();

    bool isInLoopThread() const { return threadId == std::this_thread::get_id(); }

private:
    ChannelList activeChannels_;
    bool looping_;
    bool quit_;
    const std::thread::id threadId;
    int wakeupFd_;
    std::unique_ptr<EPoller> poller_;
    std::unique_ptr<Channel> wakeupChannel_;
    std::mutex mutex_;
    std::vector<Functor> pendingFunctors_;
};