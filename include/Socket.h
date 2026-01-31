#pragma once
#include <netinet/in.h>
#include <sys/socket.h>

#include <string>
#include <vector>

#include "IoAwaitable.h"

/**
 * @brief 封装Socket的fd,提供RAII机制
 */
class Socket {
public:
    // 默认构造: 创造一个TCP Socket
    Socket();

    // 包装构造: 接管已有的fd
    explicit Socket(int fd);

    // 析构: 自动close fd
    ~Socket();

    // 禁止拷贝
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // 允许移动
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    // 网络操作封装
    void Bind(const std::string& ip, const uint16_t port);
    void Listen();

    // 获取原始fd(仅用于 epoll注册)
    int Fd() const { return fd_; }

    // Accept 返回一个Socket对象,具备RAII特性
    Socket Accept();

    void Connect(const std::string& ip, const uint16_t port);

    // 关键配置
    void SetNonBlocking();  // 设置非阻塞
    void SetReuseAddr();    // 设置地址复用

    auto Read(char* buf, size_t len) {
        // 定义内部结构体来实现 Read 的 Awaitable
        struct ReadAwaitable {
            int fd;
            char* buf;
            size_t len;

            bool await_ready() { return false; }  // 总是挂起

            void await_suspend(std::coroutine_handle<> hd) {
                // 注册 EPOLLIN
                Scheduler::Get().WaitFor(fd, hd);
                try {
                    Scheduler::Get().GetEpoll().Mod(fd, EPOLLIN | EPOLLET);
                } catch (...) {
                    Scheduler::Get().GetEpoll().Add(fd, EPOLLIN | EPOLLET);
                }
            }

            ssize_t await_resume() {
                // 醒来时,说明 epoll 通知可读了,立即执行非阻塞 read
                // 注意: 这里没有循环读，,只读一次。
                // 如果是 ET 模式,用户层协程最好用循环读,或者我们在这里循环读完。
                return ::read(fd, buf, len);
            }
        };
        return ReadAwaitable{fd_, buf, len};
    }

    // todo: 待实现封装 Write 方法
    auto Write() {}

private:
    int fd_;
};