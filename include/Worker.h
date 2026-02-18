#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "EventLoop.h"

// 引用 TLS 变量
extern thread_local EventLoop* t_loop;

class Worker {
public:
    Worker() {
        // 启动线程
        thread_ = std::thread([this]() {
            //* 1. 在线程内创建 EventLoop
            EventLoop loop;
            //* 2. 设置 TLS
            t_loop = &loop;
            //* 3. 保存指针供外部调用 (简化 先直接赋值)
            this->loop_ = &loop;
            //* 4. 通知主线程: 我准备好了
            {
                std::unique_lock<std::mutex> lock(mutex_);
                ready_ = true;
                cv_.notify_one();
            }
            // 5. 开启循环
            loop.Loop();
        });

        // 等待线程启动完毕
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return ready_.load(); });
    }

    ~Worker() {
        if (loop_ != nullptr) {
            loop_->Stop();    // 停止循环
            loop_->WakeUp();  // 醒来退出循环
        }
        if (thread_.joinable()) thread_.join();
    }

    EventLoop* getLoop() { return loop_; }

private:
    std::thread thread_;
    EventLoop* loop_{nullptr};
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> ready_{false};
};