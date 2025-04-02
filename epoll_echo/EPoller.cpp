#include "EPoller.h"
#include "Channel.h"
#include "EventLoop.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <iostream>

EPoller::EPoller(EventLoop* loop) : loop(loop) {
    epollfd = ::epoll_create1(EPOLL_CLOEXEC);
    if (epollfd < 0) {
        std::cerr << "EPoller::EPoller - epoll_create1 error" << std::endl;
        exit(1);
    }
}

EPoller::~EPoller() {
    ::close(epollfd);
}

void EPoller::poll(int timeoutMs, ChannelList* activeChannels) {
    int numEvents = ::epoll_wait(epollfd, events.data(), events.size(), timeoutMs);
    if (numEvents == 0) {
        return;
    }
    else if (numEvents < 0) {
        if (errno == EINTR) {
            return;
        }
        else {
            std::cerr << "EPoller::poll - epoll_wait error" << std::endl;
            return;
        }
    }
    else { 
        if (numEvents == static_cast<int>(events.size())) {
            events.resize(events.size() * 2);
        }
        for(int i = 0; i < numEvents; i++){
            Channel* channel = static_cast<Channel*>(events[i].data.ptr);
            channel->setRevents(events[i].events);
            activeChannels->push_back(channel);
        }
    }
}

void EPoller::updateChannel(Channel* channel) {
    struct epoll_event event;
    event.events = channel->getEvents();
    event.data.ptr = channel;
    int fd = channel->getFd();
    if(channel->getIndex() == -1){
        int ret = ::epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
        if (ret < 0) {
            std::cerr << "EPoller::updateChannel - epoll_ctl add error" << std::endl;
        }
        channel->setIndex(1);
    } else {
        int ret = ::epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
        if (ret < 0) {
            std::cerr << "EPoller::updateChannel - epoll_ctl mod error" << std::endl;
        }
    }
}

void EPoller::removeChannel(Channel* channel) {
    struct epoll_event event;
    event.events = channel->getEvents();
    event.data.ptr = channel;
    int fd = channel->getFd();
    int ret = ::epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &event);
    if (ret < 0) {
        std::cerr << "EPoller::removeChannel - epoll_ctl del error" << std::endl;
    }
    channel->setIndex(-1);
} 