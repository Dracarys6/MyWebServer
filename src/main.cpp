#include <iostream>

#include "Buffer.h"
#include "EventLoop.h"
#include "Result.h"
#include "Socket.h"
#include "Worker.h"
thread_local EventLoop* t_loop = nullptr;  // 线程局部变量,每个线程有一份独立的,全局可访问
std::vector<Worker*> workers;              // 线程池

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
    int next_worker{0};
    std::cout << "Acceptor started." << std::endl;
    while (true) {
        auto awaiter = server.Read(nullptr, 0);  // 只要等待事件,不读数据
        co_await awaiter;  // 挂起 awaiter是封装的可等待对象(read版)
        // 从 co_await 醒来(调度器EventLoop调用resume)说明有连接
        Socket client = server.Accept();
        if (client.Fd() >= 0) {
            std::cout << "New Connection: " << client.Fd() << " -> Dispatch to Worker "
                      << next_worker << std::endl;
            client.SetNonBlocking();
            // 关键：把 Socket 移动到 Worker 线程去处理
            // 这里不能直接 HandleClient(client)，因为那会在主线程跑。
            // 我们要把它包装成 task 扔给 worker。

            // 注意：std::function 难以捕获 move-only 的 Socket。
            // 这是一个经典的 C++ 痛点。
            // 解决方案：用 shared_ptr 包装 Socket，或者用 C++20 的 move-only function (尚未普及)。
            // 这里我们用 new 出来的指针传递 (Raw Pointer)，所有权转移。
            // 启动处理协程
            Socket* client_ptr = new Socket(std::move(client));
            workers[next_worker]->GetLoop()->RunInLoop([client_ptr]() {
                // 这个 lambda 会在 Worker 线程里执行
                // 1. 设置 TLS (Worker 线程自己已经设了，双保险)
                // 2. 启动协程
                // 重新把指针转回对象
                Socket c(std::move(*client_ptr));
                delete client_ptr;

                HandleClient(std::move(c));
            });

            // 轮询下一个
            next_worker = (next_worker + 1) % workers.size();
        }
    }
}

int main() {
    Buffer buffer;  // 测试Buffer
    buffer.Append("Hello World!");
    std::cout << "ReadableBytes: " << buffer.ReadableBytes() << std::endl;
    // 启动 4 个 Worker
    for (int i = 0; i < 4; ++i) {
        workers.push_back(new Worker());
    }
    // 启动 server
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