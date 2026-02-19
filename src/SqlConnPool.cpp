#include "SqlConnPool.h"

#include <iostream>

#include "Log.h"

void SqlConnPool::Init(const char* host, int port, const char* user, const char* pwd,
                       const char* dbName, int connSize) {
    if (connSize <= 0) {
        throw std::runtime_error("ConnSize must > 0");
    }
    MAX_CONN_ = connSize;
    sem_ = std::make_unique<std::counting_semaphore<1000>>(connSize);
    for (int i = 0; i < connSize; ++i) {
        MYSQL* sql = nullptr;
        sql = mysql_init(sql);
        if (sql == nullptr) {
            LOG_ERROR("Mysql Init Error: {}", std::string(strerror(errno)));
        }
        sql = mysql_real_connect(sql, host, user, pwd, dbName, port, nullptr, 0);
        if (sql == nullptr) {
            LOG_ERROR("Mysql Connect Error: {}", std::string(strerror(errno)));
        }
        connQue_.push(sql);
    }
}

MYSQL* SqlConnPool::GetConn() {
    MYSQL* sql = nullptr;
    if (connQue_.empty()) {
        LOG_WARN("SqlConnPool Busy!");
        return nullptr;
    }

    // P操作: 请求资源,计数减一,如果为0则阻塞
    sem_->acquire();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        sql = connQue_.front();
        connQue_.pop();
    }
    return sql;
}

void SqlConnPool::FreeConn(MYSQL* sql) {
    if (sql == nullptr) return;
    std::lock_guard<std::mutex> lock(mutex_);
    connQue_.push(sql);

    // V操作: 释放资源,计数加1,唤醒等待者
    sem_->release();
}

void SqlConnPool::ClosePool() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!connQue_.empty()) {
        auto conn = connQue_.front();
        connQue_.pop();
        mysql_close(conn);
    }
    mysql_library_end();
}