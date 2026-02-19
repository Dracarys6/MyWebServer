#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <ostream>

#include "Log.h"

#define PORT 8080         // 监听端口
#define BUFFER_SIZE 1024  // 缓冲区大小

int main() {
    int server_fd, new_socket;
    sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    const char* response = "Hello From Server";

    // 1.socket 创建socket fd
    // IPv4 TCP
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 2.设置socket 选项
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsocket failed");
        exit(EXIT_FAILURE);
    }

    // 3.配置地址结构体
    address.sin_family = AF_INET;          // IPv4
    address.sin_addr.s_addr = INADDR_ANY;  // 监听所有网卡
    address.sin_port = htons(PORT);        // 端口转换成网络字节序

    // 4.bind 绑定socket到端口
    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 5.listen 监听连接(最大等待队列数为3)
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
    LOG_INFO("Server listening on port {}", PORT);

    // 6.accept 接受客户端连接(阻塞等待)
    if ((new_socket = accept(server_fd, (sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }

    // 7.read 读取客户端发送的数据
    size_t valread = read(new_socket, buffer, BUFFER_SIZE);
    LOG_INFO("Received from client: {}", buffer);

    // 8.write 向客户端发送响应
    write(new_socket, response, strlen(response));
    LOG_INFO("Response sent to client");

    // 9.close 关闭socket
    close(new_socket);
    close(server_fd);

    return 0;
}
