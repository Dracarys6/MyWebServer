#pragma once
#include <mysql/mysql.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <semaphore>  //C++20
#include <stdexcept>
#include <string>
#include <thread>

class SqlConnPool {
public:
    static SqlConnPool* getInstance() {
        static SqlConnPool connPool;
        return &connPool;
    }

    // 初始化连接池
    void Init(const char* host, int port, const char* user, const char* pwd, const char* dbName,
              int connSize = 10);

    // 获取连接
    MYSQL* GetConn();

    // 释放连接
    void FreeConn(MYSQL* conn);

    // 关闭连接池
    void ClosePool();

    // 获取空闲连接数
    int getFreeConnCount() { return freeCount_; }

private:
    SqlConnPool() : useCount_{0}, freeCount_{0} {}
    ~SqlConnPool() { ClosePool(); }

    int MAX_CONN_;
    int useCount_;
    int freeCount_;

    std::queue<MYSQL*> connQue_;
    std::mutex mutex_;
    std::unique_ptr<std::counting_semaphore<1000>> sem_;
};

class SqlConn {
public:
    SqlConn(MYSQL** sql, SqlConnPool* connpool) {
        if (connpool == nullptr) {
            throw std::runtime_error("connpool nullptr");
        }
        *sql = connpool->GetConn();
        sql_ = *sql;
        connpool_ = connpool;
    }

    ~SqlConn() {
        if (sql_ != nullptr) {
            connpool_->FreeConn(sql_);
        }
    }

private:
    MYSQL* sql_;
    SqlConnPool* connpool_;
};