#include "Socket.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

Socket::Socket() {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ == -1) {
        throw std::runtime_error("Socket create error: " + std::string(strerror(errno)));
    }
}

Socket::Socket(int fd) {
    fd_ = fd;
    if (fd_ == -1) {
        throw std::runtime_error("Socket create error: " + std::string(strerror(errno)));
    }
}

Socket::~Socket() {
    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
}

// 移动构造
Socket::Socket(Socket&& other) noexcept {
    if (other.fd_ != -1) {
        fd_ = other.fd_;
        other.fd_ = -1;
    }
}

// 移动赋值
Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        if (other.fd_ != -1) {
            if (fd_ != -1) close(fd_);  // 释放当前资源
            fd_ = other.fd_;            // 接管新资源
            other.fd_ = -1;
        }
    }
    return *this;
}

void Socket::Bind(const std::string& ip, const uint16_t port) {
    sockaddr_in addr{};  // sockaddr_in: ipv4专用地址结构体
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);                                 // 主机字节序 -> 网络字节序
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) == -1) {  // ip地址 -> 网络字节序
        throw std::runtime_error("Invalid IP: " + ip);
    }
    if (bind(fd_, (sockaddr*)&addr, sizeof(addr)) == -1) {
        throw std::runtime_error("Bind error: " + std::string(strerror(errno)));
    }
}

void Socket::Listen() {
    if (listen(fd_, SOMAXCONN) == -1)  // 监听最大连接数 128
        throw std::runtime_error("Listen error" + std::string(strerror(errno)));
}

Socket Socket::Accept() {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    int client_fd = accept(fd_, (sockaddr*)&addr, &len);
    if (client_fd == -1) {
        // todo:
        // 后续配合协程修改
        // 目前如果是非阻塞且无连接,会抛异常或返回无效Socket,我们在上层处理
        return Socket(-1);
    }
    return Socket(client_fd);
}

void Socket::Connect(const std::string& ip, const uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) == -1) {  // ip地址 -> 网络字节序
        throw std::runtime_error("Invalid IP" + ip);
    }
    if (connect(fd_, (sockaddr*)&addr, sizeof(addr)) == -1) {
        throw std::runtime_error("Connect error: " + std::string(strerror(errno)));
    }
}

void Socket::SetNonBlocking() {
    // File Control系统调用: 修改fd的属性
    // F_GETFL: 获取 fd 的状态标志
    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags == -1) return;
    // F_SETFL: 设置 fd 的状态标志
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
}

void Socket::SetReuseAddr() {
    // 设置选项值为1（开启),0为关闭
    int opt = 1;
    // SO_REUSEADDR: 允许新进程绑定到处于TIME_WAIT状态的端口
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}