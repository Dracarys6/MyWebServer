#include <iostream>

#include "Result.h"

// 一个简单的协程
Task<int> simple_coroutinue() {
    std::cout << "==========1. Coroutinue started !!!==========" << std::endl;
    co_return 42;
}

int main() {
    std::cout << "==========0. Main started !!!================" << std::endl;
    // 调用协程
    Task<int> task = simple_coroutinue();
    std::cout << "==========2. Back to main !!!================" << std::endl;
    std::cout << "==========3. Result: " << task.get_result()
              << " !!!==================" << std::endl;
    std::cout << "==========4. Main ended !!!==================" << std::endl;
    return 0;
}