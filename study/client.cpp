#include <Log.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    const char* message = "Hello from Client";

    // 1.socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG_ERROR("Socket creation error!");
        return -1;
    }

    // 2.配置服务器地址
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // 3.将IPv4地址从文本转换成二进制格式
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        LOG_ERROR("Invalid address/ Address not supported!");
        return -1;
    }

    // 4.connect 连接到服务器
    if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr))) {
        LOG_ERROR("Connection Failed!");
        return -1;
    }

    // 5.write 向服务器发送数据
    write(sock, message, sizeof(message));
    LOG_INFO("Message sent to server: {}", message);

    // 6.read 读取服务器响应
    ssize_t valread = read(sock, buffer, BUFFER_SIZE);
    LOG_INFO("Response from server: {}", buffer);

    // 7.close
    close(sock);
    return 0;
}