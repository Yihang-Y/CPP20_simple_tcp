#pragma once
#include <sys/epoll.h>
#include <vector>
#include <unistd.h>
#include <iostream>
#include "utils.h"  // 确保包含 noncopyable

class EventLoop;
class Channel;

class EPoller : noncopyable {
public:
    using ChannelList = std::vector<Channel*>;
    
    EPoller(EventLoop* loop);
    ~EPoller();

    void poll(int timeoutMs, ChannelList* activeChannels);
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

private:
    EventLoop* loop;
    int epollfd;
    using EventList = std::vector<struct epoll_event>;
    EventList events{16};  // 初始大小为16
}; 