#pragma once
#include <liburing.h>
#include <system_error>
#include <iostream>
#include <map>
#include <atomic>
#include <unistd.h>
#include <coroutine>
#include "Promise.h"

// forward declaration
template<typename T>
class Task;

template<>
class Task<void>;

class IoUringScheduler{
public:
    IoUringScheduler(){
        init();
    }
    ~IoUringScheduler(){
        io_uring_queue_exit(&ring);
    }
    void init(){
        if (io_uring_queue_init(32, &ring, 0) < 0) {
            throw std::system_error(errno, std::system_category(), "io_uring_queue_init");
        }
    }

    io_uring* getRing(){
        return &ring;
    }

    uint64_t getNewId(){
        // HACK: maybe should consider more about the memory order
        return next_id.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief the event-loop of the server
     * 
     * @note 
     * - // NOTE: just like the event-loop mechanism in the framework using epoll, 
     * - // Instead of using channel and registered callbacks, here we use the suspend and resume mechanism of the coroutine
     * - // in the loop, the only blocking point is the io_uring_submit_and_wait, no other function should block the thread
     */
    void run() {
        while (true){
            io_uring_cqe *cqe;
            // NOTE: just put all submit work here, all xxxAwaitable will just prepare the sqe
            int ret = io_uring_submit_and_wait(&ring, 1);
            if(ret < 0){
                std::cout << "ERROR in wait_cqe: "<< strerror(-ret) << std::endl;
                break;
            }
            while (io_uring_peek_cqe(&ring, &cqe) == 0 && cqe){
                uint64_t id = cqe->user_data;
                auto it = handles.find(id);
                if (it != handles.end()) {
                    void* addr = reinterpret_cast<void*>(it->second.address());
                    auto& promise = std::coroutine_handle<promise_base>::from_address(reinterpret_cast<void*>(addr)).promise();
                    promise.data = cqe->res;
                    if (it->second && !it->second.done())
                        it->second.resume();
                    handles.erase(it);
                }
                io_uring_cqe_seen(&ring, cqe);
            }

            if (handles.empty()) {
                // break;
            }
        }
    }

    std::map<uint64_t, std::coroutine_handle<>> handles;
private:
    /* data */
    io_uring ring;
    std::atomic<uint64_t> next_id{0};

};

IoUringScheduler& getScheduler(){
    static IoUringScheduler scheduler;
    return scheduler;
}