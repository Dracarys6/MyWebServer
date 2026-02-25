#pragma once
#include <sys/eventfd.h>

#include <coroutine>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "Epoll.h"
#include "Timer.h"

/**
 * @brief EventLoop: 每个线程持有一个
 * 负责：
 * 1. 运行 Epoll Wait 循环
 * 2. 存储 <fd, 协程句柄> 的映射
 * 3. 当 Epoll 有事件时,恢复对应的协程
 */
class EventLoop {
public:
    EventLoop() {
        wakeup_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);  // 非阻塞和执行时自动关闭
        if (wakeup_fd_ == -1) {
            LOG_ERROR("eventfd error: {}", std::string(strerror(errno)));
            exit(1);
        }
        timer_ = std::make_unique<Timer>();
    }

    ~EventLoop() { close(wakeup_fd_); }

    // 禁止拷贝
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    Epoll& GetEpoll() { return epoll_; }

    // 注册等待: 当 fd 有 event 事件时,恢复 handle
    void WaitFor(int fd, std::coroutine_handle<> handle) {
        // 简单粗暴：每个 fd 同一时刻只能有一个协程在等
        // 实际项目中这里需要区分 ReadHandle 和 WriteHandle
        waiting_coroutines_[fd] = handle;
    }

    // 核心循环
    void Loop();

    void Stop() { stop_ = true; }

    // 添加任务到队列,并唤醒 Loop
    void RunInLoop(std::function<void()> task);

    // 唤醒 epoll_wait
    void WakeUp() {
        uint64_t one{1UL};
        write(wakeup_fd_, &one, sizeof(one));
    }

    // 添加定时任务接口
    void AddTimer(int id, int timeout, const TimeoutCallBack& callback) {
        if (timer_ != nullptr) timer_->add(id, timeout, callback);
    }

    // 更新定时任务
    void UpdateTimer(int id, int timeout, const TimeoutCallBack& callback) {
        if (timer_ != nullptr) timer_->adjust(id, timeout, callback);
    }

    // 删除指定 id(fd) 的定时器
    void DelTimer(int id) {
        if (timer_ != nullptr) timer_->del(id);
    }

private:
    void ExecuteTasks() {
        std::vector<std::function<void()>> temp_tasks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            temp_tasks.swap(tasks_);  // 快速交换,减小锁粒度
        }
        for (auto& task : temp_tasks) {
            task();
        }
    }

    Epoll epoll_;
    // 存储 fd -> 挂起的协程
    std::map<int, std::coroutine_handle<>> waiting_coroutines_;
    std::atomic<bool> stop_{false};
    int wakeup_fd_;
    std::mutex mutex_;
    std::vector<std::function<void()>> tasks_;
    std::atomic<bool> is_sleeping_{false};
    std::unique_ptr<Timer> timer_;
};

extern thread_local EventLoop* t_loop;  // TLS指针,要写在EventLoop定义之后,不然会报错