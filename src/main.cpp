#include <netinet/tcp.h>
#include <signal.h>
#include <sys/sendfile.h>  //sendfile

#include <iostream>
#include <sstream>

#include "Buffer.h"
#include "EventLoop.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Log.h"
#include "Result.h"
#include "Socket.h"
#include "SqlConnPool.h"
#include "Utils.h"
#include "Worker.h"

thread_local EventLoop* t_loop = nullptr;  // 线程局部变量,每个线程有一份独立的,全局可访问
std::vector<std::unique_ptr<Worker>> workers;  // 线程池

// 处理客户端连接的协程
Task<void> HandleClient(Socket client) {
    //! 必须用 std::move 接管 client,否则析构会关闭fd
    Buffer readBuffer;
    HttpRequest request;
    HttpResponse response;

    // TODO:增加超时机制
    // client.SetTimeOut(5000);  //5秒无数据自动断开

    while (true) {
        //* 协程挂起,等待数据读入 Buffer
        ssize_t n = co_await client.Read(readBuffer);
        if (n <= 0) break;  // 简化处理,对端关闭或超时 直接关闭连接

        //* 循环处理 Buffer 中的请求
        while (request.Parse(readBuffer)) {
            //* 处理业务逻辑
            std::string path = request.getPath();
            //! 拦截API请求
            //* Mysql 登录
            if (path == "/login" && request.getMethod() == "POST") {
                std::string user = request.getPost("user");
                std::string pwd = request.getPost("pwd");

                LOG_DEBUG("user = {}", user);
                LOG_DEBUG("pwd = {}", pwd);

                // 获取连接
                MYSQL* sql = nullptr;
                SqlConn sqlConn(&sql, SqlConnPool::getInstance());

                LOG_INFO("Mysql Connect Success");

                // 执行查询
                char order[256] = {0};
                snprintf(order, 256,
                         "SELECT username,password FROM user WHERE username='%s' LIMIT 1",
                         user.c_str());

                if (mysql_query(sql, order)) {
                    // 查询失败...
                }

                MYSQL_RES* result = mysql_store_result(sql);
                // 解析结果对比密码...
                mysql_free_result(result);

                if (user == "root" && pwd == "123") {
                    path = "/welcome.html";  // 登录成功,显示欢迎页
                } else {
                    path = "/error.html";
                }
            } else if (path == "/") {
                path = "/index.html";  // 默认页
            }

            //* 初始化响应
            bool keepAlive = request.IsKeepAlive();
            response.Init("../resources", path, keepAlive, 200);

            //* 生成响应数据
            Buffer headerBuffer;
            response.MakeResponse(headerBuffer, client.getFd());

            //* 发送响应
            // 发送前开启,TCP_CORK优化,避免 Header 和 Body 分成两个 TCP 包发
            int on = 1;
            setsockopt(client.getFd(), IPPROTO_TCP, TCP_CORK, &on, sizeof(on));

            // 先发送Header
            // TODO: 改为异步write
            write(client.getFd(), headerBuffer.Peek(), headerBuffer.ReadableBytes());

            // 如果是静态文件文件,使用 sendfile 发送 Body
            if (response.getFileFd() != -1 && response.getCode() == 200) {
                if (Utils::SendFile(client.getFd(), response.getFileFd(), response.getFileSize()))
                    LOG_INFO("SendFile 传输成功");
            }

            // 发送完关闭 CORK,强制刷出数据
            int off = 0;
            setsockopt(client.getFd(), IPPROTO_TCP, TCP_CORK, &off, sizeof(off));

            if (!keepAlive) {  // 如果是短连接,发完就关
                //* 以下是webbench需要:
                // 关闭写端，告诉客户端：我数据发完了
                // 这会向客户端发送 FIN 包
                shutdown(client.getFd(), SHUT_WR);

                // 尝试读取客户端可能发来的剩余数据（虽然通常没有）
                // 这是为了让内核完成正常的四次挥手，避免 RST
                char dummy[1024];
                while (true) {
                    int n = read(client.getFd(), dummy, sizeof(dummy));
                    if (n <= 0) {
                        // n == 0 代表客户端也关闭了连接 (FIN)
                        // n == -1 代表出错，不管怎样都可以结束了
                        break;
                    }
                }
            }

            // 重置 request 状态，准备处理下一个请求 (Keep-Alive)
            request.Init();
        }
        //* 协程结束，Task 析构，client 析构，连接关闭
    }
}

// 接收连接的协程
Task<void> Acceptor(Socket& server) {
    int next_worker{0};
    LOG_INFO("Acceptor started");
    while (true) {
        Buffer buf;
        auto awaiter = server.Read(buf);  // 只要等待事件,不读数据
        co_await awaiter;                 // 挂起 awaiter是封装的可等待对象(read版)
        // 从 co_await 醒来(调度器EventLoop调用resume)说明有连接
        Socket client = server.Accept();
        if (client.getFd() >= 0) {
            LOG_INFO("New Connection: {} -> Dispatch to Worker {}", client.getFd(), next_worker);
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
    signal(SIGPIPE, SIG_IGN);  // webbench需要: 忽略 SIGPIPE 信号，防止进程意外退出

    // 初始化日志(开启异步,队列长度 1024)
    Log::getInstance()->Init(0, "./log", ".log", 1024);

    LOG_INFO("========== Server Start ==========");
    LOG_INFO("Log System Init Success");
    LOG_INFO("Server Start Port: {}", 8080);
    Log::getInstance()->Flush();  // 刷盘

    // 将 fd 限制增加为65535
    Utils::setRlimit();

    // 初始化 Mysql 连接池
    SqlConnPool::getInstance()->Init("localhost", 3306, "root", "20050430", "webserver", 16);

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
    LOG_INFO("MainLoop is ready");
    main_loop.Loop();

    LOG_INFO("========== Server End ==========");
    return 0;
}