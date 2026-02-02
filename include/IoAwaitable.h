#pragma once
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <coroutine>

#include "EventLoop.h"

/**
 * @brief 等待体：用于 co_await socket.read()
 *
 */
struct IoAwaitable {
    int fd;
    uint32_t event_type;  // EPOLLIN 或 EPOLLOUT
    // 暂存 IO 操作的结果
    ssize_t result{0L};

    // 1. 总是挂起(简单起见,我们假设总是需要等 Epoll,除非是 Reactor 优化)
    bool await_ready() { return false; }

    // 2. 挂起时的操作
    void await_suspend(std::coroutine_handle<> hd) {
        // 将 fd 和当前协程句柄 hd 注册到调度器
        if (t_loop) {
            t_loop->WaitFor(fd, hd);

            // 将 fd 注册到 Epoll
            try {
                t_loop->GetEpoll().Mod(fd, event_type | EPOLLET);
            } catch (...) {
                // 如果时第一次加,Mod 会失败,改为 Add
                t_loop->GetEpoll().Add(fd, event_type | EPOLLET);
            }
        }
    }
    // 3. 唤醒后的操作
    // 这里我们不做实际的 read/write，只返回 io 结果的占位符
    // 或者，更常见的做法：await_resume 里不做 IO，
    // 而是让协程醒来后自己在 Socket 方法里做 IO。
    // 我们采用后者：只负责唤醒，不负责读写数据。
    void await_resume() {}
};
