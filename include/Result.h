#pragma once
#include <coroutine>
#include <exception>

/**
 * @brief Task: 协程函数的返回类型
 *  C++20 协程任何使用 co_await/co_return 的函数,其返回类型必须包含名为 promise_type的嵌套类型
 */
template <typename T = void>
struct Task {
    // 1. 嵌套的promise_type(C++20强制要求的名称)
    struct promise_type {
        T value;                           // 存储协程返回值
        std::exception_ptr exception_ptr;  // 存储异常

        // 作用：协程创建时，返回一个Task对象给调用者(外部操作协程的句柄)
        Task get_return_object() {
            // from_promise：从promise对象创建对应的coroutine_handle
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        // 协程初始化行为 std::suspend_never 立即执行,不挂起
        std::suspend_never initial_suspend() { return {}; }

        // 协程结束时行为 std::suspend_always 结束后挂起,确保Task可以读取返回值
        std::suspend_always final_suspend() noexcept { return {}; }

        // 处理 co_return
        void return_value(T v) { value = v; }

        // 处理未捕获异常
        void unhandled_exception() { exception_ptr = std::current_exception(); }
    };

    // 2. 协程句柄(外部操作协程的核心: 恢复、销毁、获取promise)
    std::coroutine_handle<promise_type> handle;
    // 构造函数
    explicit Task(std::coroutine_handle<promise_type> hd) : handle(hd) {}

    // 禁用拷贝,支持移动(避免析构重复销毁)
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&& other) noexcept {
        // 转移所有权
        handle = other.handle;
        other.handle = nullptr;
    }
    Task& operator=(Task&& other) noexcept {
        if (this != other) {
            if (handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    //  析构函数：释放协程句柄
    ~Task() {
        if (handle) handle.destroy();
    }

    // 获取返回值
    T get_result() {
        // 如果协程内有未处理异常，重新抛出(让外部捕获)
        if (handle.promise().exception_ptr) {
            std::rethrow_exception(handle.promise().exception_ptr);
        }
        return handle.promise().value;
    }
};

/**
 * @brief void特化版本
 */
template <>
struct Task<void> {
    struct promise_type {
        std::exception_ptr exception_ptr;
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { exception_ptr = std::current_exception(); }
    };
    std::coroutine_handle<promise_type> handle;
    explicit Task(std::coroutine_handle<promise_type> hd) : handle(hd) {}
    // 同样禁用拷贝,支持移动
    Task(const Task&&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&& other) {
        handle = other.handle;
        other.handle = nullptr;
    }
    Task& operator=(Task&& other) {
        if (this != &other) {
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    ~Task() {
        //! Task析构不销毁句柄,不然会段错误
        // if (handle) handle.destroy();
    }
    void get_result() {
        if (handle.promise().exception_ptr) {
            std::rethrow_exception(handle.promise().exception_ptr);
        }
    }
};