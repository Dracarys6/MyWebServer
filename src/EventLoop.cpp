#include "EventLoop.h"

#include <sstream>

#include "Log.h"

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
        auto events = epoll_.Wait(-1);  // 阻塞等待,直到有fd就绪,释放CPU,不空转

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
    }
}

// 添加任务到队列,并唤醒 Loop
void EventLoop::RunInLoop(std::function<void()> task) {  // 包装成统一对象
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push_back(task);
    }
    WakeUp();
}