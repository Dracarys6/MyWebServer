#pragma once
#include <coroutine>
#include <iostream>
#include <map>

#include "Epoll.h"

/**
 * @brief 调度器（单例模式）
 * 负责：
 * 1. 运行 Epoll Wait 循环
 * 2. 存储 <fd, 协程句柄> 的映射
 * 3. 当 Epoll 有事件时,恢复对应的协程
 */
class Scheduler {
public:
    static Scheduler& Get() {
        static Scheduler instance;  // 局部静态变量，第一次调用时初始化
        return instance;
    }

    Epoll& GetEpoll() { return epoll_; }

    // 注册等待: 当 fd 有 event 事件时,恢复 handle
    void WaitFor(int fd, std::coroutine_handle<> handle) {
        // 简单粗暴：每个 fd 同一时刻只能有一个协程在等
        // 实际项目中这里需要区分 ReadHandle 和 WriteHandle
        waiting_coroutines_[fd] = handle;
    }

    // 核心循环
    void Loop() {
        std::cout << "Scheduler Loop Started..." << std::endl;
        while (true) {
            auto events = epoll_.Wait(-1);  // 阻塞等待
            for (auto& ev : events) {
                int fd = ev.data.fd;
                // 查找在这个 fd 上等待的协程
                if (auto it = waiting_coroutines_.find(fd);
                    it != waiting_coroutines_.end()) {  // C++17 if初始化
                    auto handle = it->second;
                    waiting_coroutines_.erase(it);
                    handle.resume();  // 唤醒协程
                }
            }
        }
    }

private:
    Scheduler() = default;
    Epoll epoll_;
    // 存储 fd -> 挂起的协程
    std::map<int, std::coroutine_handle<>> waiting_coroutines_;
};
