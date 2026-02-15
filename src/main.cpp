#include <netinet/tcp.h>
#include <sys/sendfile.h>  //sendfile

#include <iostream>
#include <sstream>

#include "Buffer.h"
#include "EventLoop.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Result.h"
#include "Socket.h"
#include "Utils.h"
#include "Worker.h"

thread_local EventLoop* t_loop = nullptr;  // 线程局部变量,每个线程有一份独立的,全局可访问
std::vector<std::unique_ptr<Worker>> workers;  // 线程池

// 处理客户端连接的协程
Task<void> HandleClient(Socket client) {
    // !必须用 std::move 接管 client,否则析构会关闭fd
    Buffer readBuffer;
    HttpRequest request;
    HttpResponse response;

    // TODO:增加超时机制
    // client.SetTimeOut(5000);  //5秒无数据自动断开

    while (true) {
        // *协程挂起,等待数据读入 Buffer
        ssize_t n = co_await client.Read(readBuffer);
        if (n <= 0) break;  // 简化处理,对端关闭或超时 直接关闭连接

        //*循环处理 Buffer 中的请求
        while (request.Parse(readBuffer)) {
            //*处理业务逻辑
            std::string path = request.getPath();
            //! 拦截API请求
            if (path == "/login" && request.getMethod() == "POST") {
                // 不是静态文件
                std::string user = request.getPost("user");
                std::string pwd = request.getPost("pwd");

                std::cout << "[Debug]user = " << request.getPost("user") << std::endl;
                std::cout << "[Debug]pwd = " << request.getPost("pwd") << std::endl;

                if (user == "root" && pwd == "123") {
                    path = "/welcome.html";  // 登录成功,显示欢迎页
                } else {
                    path = "/error.html";
                }
            } else if (path == "/") {
                path = "/index.html";  // 默认页
            }

            //*初始化响应
            bool keepAlive = request.IsKeepAlive();
            response.Init("../resources", path, keepAlive, 200);

            //*生成响应数据
            Buffer headerBuffer;
            response.MakeResponse(headerBuffer, client.getFd());

            //*发送响应
            // 发送前开启,TCP_CORK优化,避免 Header 和 Body 分成两个 TCP 包发
            int on = 1;
            setsockopt(client.getFd(), IPPROTO_TCP, TCP_CORK, &on, sizeof(on));

            // 先发送Header
            // TODO: 改为异步write
            write(client.getFd(), headerBuffer.Peek(), headerBuffer.ReadableBytes());

            // 如果是静态文件文件,使用 sendfile 发送 Body
            if (response.getFileFd() != -1 && response.getCode() == 200) {
                if (Utils::SendFile(client.getFd(), response.getFileFd(), response.getFileSize()))
                    std::cout << "传输成功!" << std::endl;
            }

            // 发送完关闭 CORK,强制刷出数据
            int off = 0;
            setsockopt(client.getFd(), IPPROTO_TCP, TCP_CORK, &off, sizeof(off));

            if (!keepAlive) {  // 如果是短连接,发完就关
                break;
            }

            // 重置 request 状态，准备处理下一个请求 (Keep-Alive)
            request.Init();
        }
        // *协程结束，Task 析构，client 析构，连接关闭
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