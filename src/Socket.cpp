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

void Socket::Bind(const std::string& ip, const uint16_t port) {
    sockaddr_in addr{};  // sockaddr_in: ipv4专用地址结构体
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);  // 主机字节序 -> 网络字节序
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) == -1) {  // ip地址 -> 网络字节序
        LOG_ERROR("Invalid IP: {}", ip);
    }
    if (bind(fd_, (sockaddr*)&addr, sizeof(addr)) == -1) {
        LOG_ERROR("Bind error: {}", std::string(strerror(errno)));
    }
}

void Socket::Listen() {
    if (listen(fd_, SOMAXCONN) == -1)  // 监听最大连接数 4096
        LOG_ERROR("Listen error: {}", std::string(strerror(errno)));
}

Socket Socket::Accept() {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    int client_fd = accept(fd_, (sockaddr*)&addr, &len);
    if (client_fd == -1) {
        // 目前如果是非阻塞且无连接,会抛异常或返回无效Socket,我们在上层处理
        LOG_ERROR("Accept:invalid client_fd");
        return Socket(-1);
    }
    return Socket(client_fd);
}

void Socket::Connect(const std::string& ip, const uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) == -1) {  // ip地址 -> 网络字节序
        LOG_ERROR("Invalid IP: {}", ip);
    }
    if (connect(fd_, (sockaddr*)&addr, sizeof(addr)) == -1) {
        LOG_ERROR("Connect error: {}", std::string(strerror(errno)));
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