#include <iostream>

#include "EventLoop.h"
#include "Result.h"  //Task
#include "Socket.h"

thread_local EventLoop* t_loop = nullptr;  // 线程局部变量,每个线程有一份独立的,全局可访问

// 处理客户端连接的协程
Task<void> HandleClient(Socket client) {
    // 必须用 std::move 接管 client,否则析构会关闭fd
    char buf[1024];
    while (true) {
        // 1. 等待数据(挂起)
        // 当有数据时,resume执行read,返回读取字节数
        std::cout << "Waiting for data..." << std::endl;
        ssize_t n = co_await client.Read(buf, sizeof(buf));
        if (n > 0) {
            buf[n] = '\0';  // 末尾加字符串结束符
            std::cout << "Recv: " << buf << std::endl;
            // 回显 暂时用同步write
            write(client.Fd(), buf, n);
        } else if (n == 0) {
            std::cout << "Client closed." << std::endl;
            break;
        } else {
            std::cout << "Read error." << std::endl;
            break;
        }
    }
}

// 接收连接的协程
Task<void> Acceptor(Socket& server) {
    std::cout << "Acceptor started." << std::endl;
    while (true) {
        auto awaiter = server.Read(nullptr, 0);  // 只要等待事件,不读数据
        co_await awaiter;  // 挂起 awaiter是封装的可等待对象(read版)
        // 从 co_await 醒来(调度器调用resume)说明有连接
        Socket client = server.Accept();
        if (client.Fd() >= 0) {
            std::cout << "New Connection: " << client.Fd() << std::endl;
            client.SetNonBlocking();
            // 启动处理协程
            HandleClient(std::move(client));
            // 这一行结束,Task<void>临时对象被析构了
        }
    }
}

int main() {
    Socket server;
    server.SetReuseAddr();
    server.SetNonBlocking();
    server.Bind("0.0.0.0", 8080);
    server.Listen();

    // 1.创建主线程的 Loop
    EventLoop main_loop;
    // 2.设置 TLS 指针,让该线程内的协程能找到他
    t_loop = &main_loop;
    // 3.启动 Acceptor 协程
    Acceptor(server);
    // 4.运行 Loop
    main_loop.Loop();
    return 0;
}