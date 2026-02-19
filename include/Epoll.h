#pragma once
#include <sys/epoll.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "Log.h"

// 哪怕是高并发,一次处理1024个就绪事件也够了
inline const int MAX_EVENTS = 1024;

/**
 * @brief 封装 Linux epoll 机制
 */
class Epoll {
public:
    Epoll() : events_(MAX_EVENTS) {
        // epoll_create1(0)是更新的API,比epoll_create更推荐
        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ == -1) {
            LOG_ERROR("Epoll create error: {}", std::string(strerror(errno)));
        }
    }

    ~Epoll() {
        if (epoll_fd_ != -1) close(epoll_fd_);
    }

    // 禁止拷贝
    Epoll(const Epoll&) = delete;
    Epoll& operator=(const Epoll&) = delete;

    // 注册/修改/删除 事件
    // op: EPOLL_CTL_ADD / MOD /DEL
    // events: EPOLLIN | EPOLLET 等
    void Control(int fd, int events, int op);

    // 封装 Add 方法 (默认开启 ET 模式 边缘触发)
    void Add(int fd, uint32_t events);

    // 封装 Mod 方法
    void Mod(int fd, uint32_t events);

    // 封装 Del 方法
    void Del(int fd);

    // 2.等待事件
    // 返回活跃的事件列表
    // timeout: -1 表示永久阻塞, 0 表示立即返回
    std::vector<epoll_event> Wait(int timeout = -1);

private:
    int epoll_fd_;
    std::vector<epoll_event> events_;  // 用于接收 epoll_wait 返回的事件
};
