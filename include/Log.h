#pragma once
#include <assert.h>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <stdarg.h>  //for va_start
#include <string.h>
#include <sys/stat.h>  //for mkdir
#include <sys/time.h>

#include <mutex>
#include <string>
#include <thread>

#include "BlockQueue.h"
#include "Buffer.h"

class Log {
public:
    // 初始化日志实例
    // level:  0=DEBUG  1=INFO  2=WARN  3=ERROR
    void Init(int level = 1, const char* path = "./log", const char* suffix = ".log",
              int maxQueueCapacity = 1024);

    static Log* getInstance() {
        static Log instance;
        return &instance;
    }

    static void FlushLogThread();  // 后台线程入口

    //! 模板函数必须在头文件中定义
    // 业务线程调用的接口
    template <typename... Args>
    void Write(int level, fmt::format_string<Args...> format, Args&&... args) {
        struct timeval now = {0, 0};
        gettimeofday(&now, nullptr);
        time_t tSec = now.tv_sec;
        struct tm* sysTime = localtime(&tSec);

        std::string levelStr = LevelToString(level);

        std::string logLine{};
        try {
            //* 1. 格式化用户内容
            std::string content = fmt::format(format, std::forward<Args>(args)...);

            //* 2. 加上前缀
            logLine = fmt::format("[{}] {}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:06d} {}\n",
                                  levelStr, sysTime->tm_year + 1900, sysTime->tm_mon + 1,
                                  sysTime->tm_mday, sysTime->tm_hour, sysTime->tm_min,
                                  sysTime->tm_sec, (int)now.tv_usec, content);
        } catch (const std::exception& e) {
            logLine = fmt::format("[ERROR] Log Format Error: {}\n", e.what());
        }

        //* 3. 推入异步队列
        if (isAsync_ && deque_ != nullptr && !deque_->full()) {
            deque_->push_back(std::move(logLine));
        } else {
            // 同步写 (如果没开异步或队列满)
            std::lock_guard<std::mutex> lock(mutex_);
            fputs(logLine.c_str(), fp_);
            // 同步模式下强制刷盘
            fflush(fp_);
        }
    }

    // 把缓冲区里的数据刷到磁盘
    void Flush();

    int getLevel() { return level_; }
    void setLevel(int level) { level_ = level; }

    bool isOpen() { return isOpen_; }

private:
    Log();
    virtual ~Log();

    const char* LevelToString(int level) {
        switch (level) {
            case 0:
                return "DEBUG";
            case 1:
                return "INFO";
            case 2:
                return "WARN";
            case 3:
                return "ERROR";
            default:
                return "INFO";
        }
    }

    void AsyncWrite();  // 真正的写盘逻辑

    int level_;
    bool isAsync_{false};
    bool isOpen_;

    // 日志路径和文件名
    const char* path_;
    const char* suffix_;

    static const int LOG_NAME_LEN{256};
    static const int LOG_PATH_LEN{256};
    static const int MAX_LINES_{50000};

    int maxLines{0};
    int lineCount_{0};
    int toDay_{0};

    Buffer buffer_;  // 日志缓冲

    // 更高效：双缓冲区 (Double Buffering)
    // 今天先用简单的 BlockQueue<std::string>
    std::unique_ptr<BlockQueue<std::string>> deque_{nullptr};
    std::unique_ptr<std::thread> writeThread_{nullptr};
    std::mutex mutex_;

    FILE* fp_{nullptr};
};

// 宏定义简化调用
#define LOG_DEBUG(format, ...) \
    if (0 <= Log::getInstance()->getLevel()) Log::getInstance()->Write(0, format, ##__VA_ARGS__)

#define LOG_INFO(format, ...) \
    if (1 <= Log::getInstance()->getLevel()) Log::getInstance()->Write(1, format, ##__VA_ARGS__);

#define LOG_WARN(format, ...) \
    if (2 <= Log::getInstance()->getLevel()) Log::getInstance()->Write(2, format, ##__VA_ARGS__);

#define LOG_ERROR(format, ...) \
    if (3 <= Log::getInstance()->getLevel()) Log::getInstance()->Write(3, format, ##__VA_ARGS__);
