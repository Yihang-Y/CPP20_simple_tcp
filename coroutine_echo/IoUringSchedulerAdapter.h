#pragma once
#include "IoUringScheduler.h"

// 全局单例
namespace {
    IoUringScheduler& getGlobalScheduler() {
        static IoUringScheduler scheduler;
        return scheduler;
    }
}

// 兼容层
inline IoUringScheduler& getScheduler() {
    return getGlobalScheduler();
}

// 全局co_spawn函数
template<typename T>
void co_spawn(Task<T> task) {
    getScheduler().co_spawn(std::move(task));
}