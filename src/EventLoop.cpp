#include "EventLoop.h"

#include <sstream>

// 核心循环
void EventLoop::Loop() {
    // 把 wakeup_fd 加入 epoll
    epoll_.Add(wakeup_fd_, EPOLLIN);

    auto pid = std::this_thread::get_id();
    std::ostringstream oss;
    oss << pid;
    std::string id_str = oss.str();
    LOG_INFO("EventLoop Started in thread {}", id_str);

    while (!stop_) {
        is_sleeping_ = true;  // 睡前标记
        //* 1. 获取下一个超时时间 (ms)
        // 如果没有定时任务，timeout = -1 (无限等待)
        int timeout = -1;
        if (timer_ != nullptr) {
            timeout = timer_->GetNextTick();
        }

        //* 2. 阻塞等待 IO 事件,最多等 timeout 毫秒
        auto events = epoll_.Wait(timeout);  // 阻塞等待,直到有fd就绪,释放CPU,不空转

        is_sleeping_ = false;  // 醒来标记

        //* 3. 处理 IO 事件
        for (auto& ev : events) {
            if (ev.data.fd == wakeup_fd_) {
                // 如果是唤醒事件,读一下 buffer 清空
                uint64_t zero{0UL};
                read(wakeup_fd_, &zero, sizeof(zero));
                // 然后执行队列任务
                ExecuteTasks();
            } else {
                // 普通 socket 事件
                // ... 处理协程 resume ...
                // 查找在这个 fd 上等待的协程
                int fd = ev.data.fd;
                if (auto it = waiting_coroutines_.find(fd);
                    it != waiting_coroutines_.end()) {  // C++17 if初始化
                    auto handle = it->second;
                    waiting_coroutines_.erase(it);
                    handle.resume();  // 唤醒协程
                }
            }
        }

        //* 4. 处理定时器超时
        if (timer_ != nullptr) {
            timer_->tick();
        }
    }
}

// 添加任务到队列,并唤醒 Loop
void EventLoop::RunInLoop(std::function<void()> task) {  // 包装成统一对象
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push_back(task);
    }
    // 只有子线程在睡觉时,才需要叫醒!
    if (is_sleeping_.load(std::memory_order_relaxed)) {
        WakeUp();
    }
}