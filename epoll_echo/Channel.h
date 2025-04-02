#pragma once
#include <sys/epoll.h>
#include <vector>
#include <functional>
#include <unistd.h>
#include <iostream>
#include "utils.h"  // 确保包含 noncopyable

// 前向声明
class EventLoop;
class Channel;

// EPoller定义移至单独的头文件
class EPoller;

class Channel : noncopyable {
public:
    using EventCallback = std::function<void()>;
    Channel(EventLoop* loop, int fd) : loop(loop), fd(fd), events(0), revents(0), index(-1) {
    }

    ~Channel(){
    }

    void setFd(int fd) {
        this->fd = fd;
    }

    void setEvents(int events) {
        this->events = events;
    }

    void setReadCallback(EventCallback cb){
        readCallback = std::move(cb);
    }

    void setWriteCallback(EventCallback cb){
        writeCallback = std::move(cb);
    }

    void setCloseCallback(EventCallback cb){
        closeCallback = std::move(cb);
    }

    void setErrorCallback(EventCallback cb){
        errorCallback = std::move(cb);
    }

    void setRevents(int revents){
        this->revents = revents;
    }

    void update();

    void remove();

    void enableReading(){
        events |= EPOLLIN |EPOLLPRI;
        update();
    }

    void disableReading(){
        events &= ~(EPOLLIN | EPOLLPRI);
        update();
    }

    void enableWriting(){
        events |= EPOLLOUT;
        update();
    }

    void disableWriting(){
        events &= ~EPOLLOUT;
        update();
    }

    void disableAll(){
        events = 0;
        update();
    }

    void handleEvent(){
        if(revents & (EPOLLIN | EPOLLPRI)){
            if(readCallback){
                readCallback();
            }
        }
        if(revents & EPOLLOUT){
            if(writeCallback){
                writeCallback();
            }
        }
        if(revents & EPOLLERR){
            if(errorCallback){
                errorCallback();
            }
        }
        if(revents & EPOLLHUP){
            if(closeCallback){
                closeCallback();
            }
        }
    }

    int getFd() const { return fd; }
    int getEvents() const { return events; }
    int getIndex() const { return index; }
    void setIndex(int idx) { index = idx; }

private:
    EventCallback readCallback;
    EventCallback writeCallback;
    EventCallback closeCallback;
    EventCallback errorCallback;
    EventLoop* loop;
    int fd;
    int events;
    int revents;
    int index;
};