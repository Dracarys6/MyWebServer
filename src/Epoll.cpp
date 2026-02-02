#include "Epoll.h"

void Epoll::Control(int fd, int events, int op) {
    epoll_event ev{};
    ev.data.fd = fd;
    ev.events = events;
    if (epoll_ctl(epoll_fd_, op, fd, &ev) == -1) {
        throw std::runtime_error("Epoll control error: " + std::string(strerror(errno)));
    }
}

// Control之Add 默认加上 EPOLLET(边缘触发)
void Epoll::Add(int fd, uint32_t events) { Control(fd, events | EPOLLET, EPOLL_CTL_ADD); }

// Control之Mod
void Epoll::Mod(int fd, uint32_t events) { Control(fd, events | EPOLLET, EPOLL_CTL_MOD); }

// Control之Del
void Epoll::Del(int fd) { Control(fd, 0, EPOLL_CTL_DEL); }

std::vector<epoll_event> Epoll::Wait(int timeout) {
    // 传入events.data() 作为缓冲区接收就绪时间
    int nfds = epoll_wait(epoll_fd_, events_.data(), MAX_EVENTS, timeout);
    if (nfds == -1) {
        if (errno == EINTR) {  // 被信号中断,不是错误
            return {};
        }
        throw std::runtime_error("Epoll wait error: " + std::string(strerror(errno)));
    }
    // 返回前 nfds 个有效事件
    // 为了性能，我们其实可以直接返回 span 或者引用，但为了简单，这里构造新 vector 返回
    // C++20 实际上可以用 std::span 避免拷贝，这是优化点，目前先用 vector 拷贝实现
    return std::vector<epoll_event>(events_.begin(), events_.begin() + nfds);
}