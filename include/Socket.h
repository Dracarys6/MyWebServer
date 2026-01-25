#pragma once
#include <netinet/in.h>
#include <sys/socket.h>

#include <string>

// Socket类: 封装fd,提供RAII机制
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

    // Accept 返回一个 Socket对象,而不是int
    // 这样client socket 也自动具备 RAII特性
    Socket Accept();

    void Connect(const std::string& ip, const uint16_t port);

    // 关键配置
    void SetNonBlocking();  // 设置非阻塞
    void SetReuseAddr();    // 设置地址复用

    // 获取原始fd (仅用于 epoll注册)
    int Fd() const { return fd_; }

    private:
    int fd_;
};