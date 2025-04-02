#include "EventLoop.h"
#include "Channel.h"
#include "EPoller.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <iostream>

EventLoop::EventLoop() 
    : looping_(false), 
      quit_(false), 
      threadId(std::this_thread::get_id()),
      wakeupFd_(createEventfd()),
      poller_(std::make_unique<EPoller>(this)),
      wakeupChannel_(std::make_unique<Channel>(this, wakeupFd_))
{
    wakeupChannel_->setReadCallback([this] { handleRead(); });
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    close(wakeupFd_);
}

void EventLoop::loop() {
    looping_ = true;
    quit_ = false;

    while (!quit_) {
        activeChannels_.clear();
        poller_->poll(10000, &activeChannels_);
        for (auto channel : activeChannels_) {
            channel->handleEvent();
        }

        doPendingFunctors();
    }
    
    looping_ = false;
}

void EventLoop::updateChannel(Channel* channel) {
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel) {
    poller_->removeChannel(channel);
}

void EventLoop::runInLoop(const Functor& cb) {
    if (isInLoopThread()) {
        cb();
    } else {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pendingFunctors_.push_back(cb);
        }
        wakeup();
    }
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }
    for (const auto& functor : functors) {
        functor();
    }
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        std::cerr << "EventLoop::wakeup() writes " << n << " bytes instead of 8" << std::endl;
    }
}

void EventLoop::handleRead() {
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one) {
        std::cerr << "EventLoop::handleRead() reads " << n << " bytes instead of 8" << std::endl;
    }
} 