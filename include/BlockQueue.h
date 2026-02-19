#pragma once
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>

template <typename T>
class BlockQueue {
public:
    explicit BlockQueue(size_t maxCapacity = 1000) : capacity_(maxCapacity), shutdown_(false) {
        if (maxCapacity <= 0) {
            throw std::runtime_error("MaxCapacity must > 0");
        }
    }

    // 禁止拷贝
    BlockQueue(const BlockQueue&) = delete;
    BlockQueue& operator=(const BlockQueue&) = delete;

    ~BlockQueue() { shutdown(); }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        dq_.clear();
    }

    void shutdown() {
        clear();
        shutdown_ = true;
        condConsumer_.notify_all();  // 唤醒所有消费者
        condProducer_.notify_all();  // 唤醒所有生产者
    }

    bool full() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return dq_.size() == capacity_;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return dq_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return dq_.size();
    }

    size_t capacity() const { return capacity_; }

    T front() {
        std::lock_guard<std::mutex> lock(mutex_);
        return dq_.front();
    }

    T back() {
        std::lock_guard<std::mutex> lock(mutex_);
        return dq_.back();
    }

    // 生产者接口:往队列里放数据
    // 拷贝版本
    void push_back(const T& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        //* 等待队列非满
        while (dq_.size() >= capacity_) {
            condProducer_.wait(lock);  // 生产者等待
        }

        if (shutdown_) {
            throw std::runtime_error("BlockQueue is shutdown");
        }

        dq_.push_back(item);
        condConsumer_.notify_one();  // 通知一个消费者: 队列非空了
    }

    // 移动版本
    void push_back(T&& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        //* 等待队列非满
        while (dq_.size() >= capacity_) {
            condProducer_.wait(lock);  // 生产者等待
        }

        if (shutdown_) {
            throw std::runtime_error("BlockQueue is shutdown");
        }

        dq_.push_back(std::move(item));
        condConsumer_.notify_one();  // 通知一个消费者: 队列非空了
    }

    // 阻塞版本: 队列为空时一直等待
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);

        //* 等待队列非空
        while (dq_.empty()) {
            if (shutdown_) {
                return false;
            }
            condConsumer_.wait(lock);  // 消费者等待
        }
        item = std::move(dq_.front());  // 移动语义
        dq_.pop_front();
        condProducer_.notify_one();  // 通知一个生产者: 队列非满了
        return true;
    }

    // 超时弹出
    bool pop(T& item, int timeout) {
        std::unique_lock<std::mutex> lock(mutex_);

        //* 等待队列非空
        while (dq_.empty()) {
            if (condConsumer_.wait_for(lock, std::chrono::milliseconds(timeout)) ==
                std::cv_status::timeout) {
                return false;
            }
            if (shutdown_) {
                return false;
            }
            condConsumer_.wait(lock);  // 消费者等待
        }
        item = std::move(dq_.front());
        dq_.pop_front();
        condProducer_.notify_one();  // 通知一个生产者: 队列非满了
        return true;
    }

    void flush() { condConsumer_.notify_one(); }

private:
    std::deque<T> dq_;          // 底层队列
    size_t capacity_;           // 最大容量
    mutable std::mutex mutex_;  // mutable 允许const 成员函数加锁
    bool shutdown_;
    std::condition_variable condConsumer_;
    std::condition_variable condProducer_;
};