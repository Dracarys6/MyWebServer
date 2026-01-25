#include <exception>
#include <iostream>

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
        server.Bind("127.0.0.1", 8080);
        server.Listen();
        std::cout << "Socket test: " << std::endl;
        std::cout << "Listening on 8080..." << std::endl;
        // 临时测试
        Socket client = server.Accept();
        if (client.Fd() != -1) {
            std::cout << "Client connected! FD=" << client.Fd() << std::endl;
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    // std::cout << "==========2. Back to main !!!================" << std::endl;
    // std::cout << "==========3. Result: " << task.get_result()
    //           << " !!!==================" << std::endl;
    // std::cout << "==========4. Main ended !!!==================" << std::endl;
    return 0;
}