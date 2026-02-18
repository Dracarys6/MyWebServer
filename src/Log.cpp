#include "Log.h"

#include <assert.h>

Log::Log() {}

Log::~Log() {
    if (writeThread_ != nullptr && writeThread_->joinable()) {
        while (!deque_->empty()) {
            deque_->flush();  // 唤醒消费者把剩下的写完
        }
        deque_->shutdown();
        writeThread_->join();
    }
    if (fp_ != nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        Flush();
        fclose(fp_);
    }
}

void Log::Init(int level, const char* path, const char* suffix, int maxQueueCapacity) {
    isOpen_ = true;
    level_ = level;
    if (maxQueueCapacity > 0) {
        isAsync_ = true;
        // 创建阻塞队列
        if (deque_ == nullptr) {
            deque_ = std::make_unique<BlockQueue<std::string>>(maxQueueCapacity);
            // 启动后台线程
            writeThread_ = std::make_unique<std::thread>(FlushLogThread);
        }
    } else {
        isAsync_ = false;
    }

    lineCount_ = 0;
    time_t t = time(nullptr);
    struct tm* sysTime = localtime(&t);
    struct tm t_now = *sysTime;
    path_ = path;
    suffix_ = suffix;

    // 创建日志文件名: path/YYYY_MM_DD.log
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", path_, t_now.tm_year + 1900,
             t_now.tm_mon + 1, t_now.tm_mday, suffix_);
    toDay_ = t_now.tm_mday;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.RetrieveAll();
        if (fp_ != nullptr) {
            Flush();
            fclose(fp_);
        }

        fp_ = fopen(fileName, "a");
        if (fp_ == nullptr) {
            mkdir(path_, 0777);  // 尝试创建目录
            fp_ = fopen(fileName, "a");
        }
        assert(fp_ != nullptr);
    }
}

// 异步写线程入口
void Log::FlushLogThread() { Log::getInstance()->AsyncWrite(); }

// 真正的写盘逻辑(在后台线程跑)
void Log::AsyncWrite() {
    std::string str;
    // 循环从队列取日志
    int count = 0;
    while (deque_->pop(str)) {
        std::lock_guard<std::mutex> lock(mutex_);
        fputs(str.c_str(), fp_);
        ++count;

        // 每 10 条刷一次盘，或缓冲区满了自动刷
        if (count >= 10) {
            fflush(fp_);
            count = 0;
        }
    }
    // 退出前最后刷一次
    fflush(fp_);
}

void Log::Flush() {
    if (isAsync_) {
        deque_->flush();
    }
    fflush(fp_);
}
