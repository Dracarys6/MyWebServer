#include <iostream>
#include <sstream>

#include "Buffer.h"
#include "EventLoop.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Result.h"
#include "Socket.h"
#include "Worker.h"

thread_local EventLoop* t_loop = nullptr;  // 线程局部变量,每个线程有一份独立的,全局可访问
std::vector<std::unique_ptr<Worker>> workers;  // 线程池

// 处理客户端连接的协程
Task<void> HandleClient(Socket client) {
    // !必须用 std::move 接管 client,否则析构会关闭fd
    Buffer readBuffer;
    HttpRequest request;
    HttpResponse response;
    while (true) {
        // *协程挂起,等待数据读入 Buffer
        ssize_t n = co_await client.Read(readBuffer);

        if (n <= 0) break;  // 简化处理,关闭连接
        if (request.Parse(readBuffer)) {
            // 1. 初始化响应
            std::string path = request.getPath();
            if (path == "/") path = "/index.html";  // 默认页

            // 设置资源根目录为 resources目录
            response.Init("../resources", path, false, 200);

            // 2. 构建响应 Buffer
            Buffer writeBuffer;
            response.MakeResponse(writeBuffer);

            // 3. 发送
            // 暂时用同步write
            // TODO:改用co_await client.Write(writeBuffer)
            write(client.getFd(), writeBuffer.Peek(), writeBuffer.ReadableBytes());
            // 重置 request 状态，准备处理下一个请求 (Keep-Alive)
            request.Init();
        } else {
            std::cout << "解析失败" << std::endl;
        }
        // *协程结束，Task 析构，client 析构，连接关闭。
    }
}

// 接收连接的协程
Task<void> Acceptor(Socket& server) {
    int next_worker{0};
    std::cout << "Acceptor started." << std::endl;
    while (true) {
        Buffer buf;
        auto awaiter = server.Read(buf);  // 只要等待事件,不读数据
        co_await awaiter;                 // 挂起 awaiter是封装的可等待对象(read版)
        // 从 co_await 醒来(调度器EventLoop调用resume)说明有连接
        Socket client = server.Accept();
        if (client.getFd() >= 0) {
            std::cout << "New Connection: " << client.getFd() << " -> Dispatch to Worker "
                      << next_worker << std::endl;
            client.SetNonBlocking();
            // 关键：把 Socket 移动到 Worker 线程去处理
            // 这里不能直接 HandleClient(client)，因为那会在主线程跑。
            // 启动处理协程
            auto client_ptr = std::make_shared<Socket>(std::move(client));
            workers[next_worker]->getLoop()->RunInLoop([client_ptr]() {
                // 这个 lambda 会在 Worker 线程里执行
                // 1. 设置 TLS (Worker 线程自己已经设了，双保险)
                // 2. 启动协程
                // 重新把指针转回对象
                Socket client(std::move(*client_ptr));

                HandleClient(std::move(client));
            });

            // 轮询下一个
            next_worker = (next_worker + 1) % workers.size();
        }
    }
}

int main() {
    // 启动 4 个 Worker
    for (int i = 0; i < 4; ++i) {
        workers.push_back(std::make_unique<Worker>());
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
    std::cout << "MainLoop is ready." << std::endl;
    main_loop.Loop();

    return 0;
}