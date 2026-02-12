#pragma once
#include <netinet/in.h>
#include <sys/socket.h>

#include <string>
#include <vector>

#include "Buffer.h"
#include "IoAwaitable.h"

/**
 * @brief 封装Socket的fd,提供RAII机制
 */
class Socket {
public:
    // 默认构造: 创造一个TCP Socket
    Socket() {
        fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ == -1) {
            throw std::runtime_error("Socket create error: " + std::string(strerror(errno)));
        }
    }

    // 包装构造: 接管已有的fd
    explicit Socket(int fd) {
        fd_ = fd;
        if (fd_ == -1) {
            throw std::runtime_error("Socket create error: " + std::string(strerror(errno)));
        }
    }

    // 析构: 自动close fd
    ~Socket() noexcept {
        if (fd_ != -1) {
            close(fd_);
            fd_ = -1;
        }
    }

    // 禁止拷贝
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // 移动构造
    Socket(Socket&& other) noexcept {
        if (other.fd_ != -1) {
            fd_ = other.fd_;
            other.fd_ = -1;
        }
    }

    // 移动赋值
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            if (other.fd_ != -1) {
                if (fd_ != -1) close(fd_);  // 释放当前资源
                fd_ = other.fd_;            // 接管新资源
                other.fd_ = -1;
            }
        }
        return *this;
    }

    // 网络操作封装
    void Bind(const std::string& ip, const uint16_t port);
    void Listen();

    // 获取原始fd(仅用于 epoll注册)
    int getFd() const { return fd_; }

    // Accept 返回一个Socket对象,具备RAII特性
    Socket Accept();

    void Connect(const std::string& ip, const uint16_t port);

    // 关键配置
    void SetNonBlocking();  // 设置非阻塞
    void SetReuseAddr();    // 设置地址复用

    auto Read(Buffer& buffer) {
        // 定义内部结构体来实现 Read 的 Awaitable
        struct ReadAwaitable {
            int fd;
            Buffer& buf;

            bool await_ready() { return false; }  // 总是挂起

            void await_suspend(std::coroutine_handle<> hd) {
                // 注册 EPOLLIN | EPOLLET
                if (t_loop) {
                    t_loop->WaitFor(fd, hd);
                    try {
                        t_loop->GetEpoll().Mod(fd, EPOLLIN | EPOLLET);
                    } catch (...) {
                        t_loop->GetEpoll().Add(fd, EPOLLIN | EPOLLET);
                    }
                }
            }

            ssize_t await_resume() {
                // 使用 Buffer::ReadFd 进行分散读
                int savedErrno = 0;
                ssize_t n = buf.ReadFd(fd, &savedErrno);
                if (n < 0 && savedErrno == EAGAIN) {
                    // ET 模式下没数据了,不算错
                    return 0;
                }
                return n;
            }
        };
        return ReadAwaitable{fd_, buffer};
    }

    // TODO: 待完善封装 Write 方法
    auto Write(Buffer& buffer) {
        // 定义内部结构体来实现 Write 的 Awaitable
        struct WriteAwaitable {
            int fd;
            Buffer& buf;

            bool await_ready() { return false; }  // 总是挂起

            void await_suspend(std::coroutine_handle<> hd) {
                // 注册 EPOLLIN | EPOLLET
                if (t_loop) {
                    t_loop->WaitFor(fd, hd);
                    try {
                        t_loop->GetEpoll().Mod(fd, EPOLLIN | EPOLLET);
                    } catch (...) {
                        t_loop->GetEpoll().Add(fd, EPOLLIN | EPOLLET);
                    }
                }
            }

            ssize_t await_resume() {
                ssize_t n = write(fd, buf.Writable(), buf.WritableBytes());
                if (n < 0) {
                    return 0;
                }
                return n;
            }
        };
        return WriteAwaitable{fd_, buffer};
    }

private:
    int fd_;
};