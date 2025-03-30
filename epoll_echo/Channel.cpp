#include "Channel.h"
#include "EventLoop.h"

void Channel::update() {
    loop->updateChannel(this);
}

void Channel::remove() {
    loop->removeChannel(this);
} 