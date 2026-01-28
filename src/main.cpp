#include <exception>
#include <iostream>

#include "Epoll.h"
#include "Result.h"
#include "Socket.h"

// 一个简单的协程
Task<int> simple_coroutinue() {
    std::cout << "==========1. Coroutinue started !!!==========" << std::endl;
    co_return 42;
}

int main() {
    std::cout << "==========0. Main started !!!================" << std::endl;
    // 调用协程
    Task<int> task = simple_coroutinue();
    try {
        Socket server;
        server.SetReuseAddr();
        // 1.socket设置为非阻塞
        server.SetNonBlocking();
        server.Bind("127.0.0.1", 8080);
        server.Listen();

        Epoll epoll;
        // 2.将socket注册到epoll,监听EPOLLIN
        epoll.Add(server.Fd(), EPOLLIN);

        while (true) {
            // 3.阻塞等待事件,直到有动静
            auto events = epoll.Wait();
            for (auto& event : events) {
                if (event.data.fd == server.Fd()) {
                    // CASE A: Server Socket 有动静 -> 说明有新连接！
                    Socket client = server.Accept();
                    if (client.Fd() == -1) continue;
                    std::cout << "New Client Connected: " << client.Fd() << std::endl;

                    // 设为非阻塞并添加到Epoll
                    client.SetNonBlocking();
                    epoll.Add(client.Fd(), EPOLLIN);
                    // 注意：Socket client 在这里析构会关闭 fd！
                    // 为了演示，我们需要把 client 的 fd "偷" 出来，避免被析构关闭
                    // 在正式项目中，我们会把 client 移动到一个 Map 或者 Session 对象里保存
                    // 这里我们暂时用一种 Hack 的方式：
                    // Socket 暂时没办法"释放"所有权而不析构 fd (除了移动)。
                    // 今天的练习有点尴尬，因为我们没有 Session 类来持有 client。
                    //
                    // *** 临时解决方案 ***：
                    // 我们为了演示，只能让 client 析构时不 close。
                    // 但 Socket 类不支持。
                    // 所以：今天的测试代码只能演示 Accept，演示完 client 析构连接就断了。
                    // 只要你看到 "New Client Connected" 且没有崩溃，就算成功。
                } else {
                    // CASE B: Client Socket 有动静 -> 说明发数据来了
                    // 今天先不处理读写，只打印
                    std::cout << "Data received from client: " << event.data.fd << std::endl;
                    // 如果读缓冲区空了，epoll 下次还会通知（如果不是 ET）
                    // 但我们用了 ET，所以如果不读完，下次就不通知了。
                }
            }
        }
    } catch (std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
    }
    std::cout << "==========2. Back to main !!!================" << std::endl;
    std::cout << "==========3. Result: " << task.get_result()
              << " !!!==================" << std::endl;
    std::cout << "==========4. Main ended !!!==================" << std::endl;
    return 0;
}