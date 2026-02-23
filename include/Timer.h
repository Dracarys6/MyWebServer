#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <queue>
#include <unordered_map>

using TimeoutCallBack = std::function<void()>;
using Clock = std::chrono::high_resolution_clock;
using TimeStamp = std::chrono::high_resolution_clock::time_point;
using MS = std::chrono::milliseconds;

/**
 * @brief 定时器节点
 */
struct TimerNode {
    int id;
    TimeStamp expire_time;
    TimeoutCallBack callback;
    // 重载 > 和 < 比较运算符,越早过期越小
    bool operator<(const TimerNode& t) { return expire_time < t.expire_time; }
    bool operator>(const TimerNode& t) { return expire_time > t.expire_time; }
};

/**
 * @brief 最小堆定时器
 */
class Timer {
public:
    Timer() { heap_.reserve(64); }
    ~Timer() { clear(); }

    // 调整 id 对应的定时器,延长 timeout 毫秒
    void adjust(int id, int timeout, const TimeoutCallBack& callback);

    // 添加新的定时器
    void add(int id, int timeout, const TimeoutCallBack& callback = {});

    // 手动触发 id 对应的回调并删除
    void doWork(int id);

    // 核心函数：检查并处理所有超时节点
    void tick();

    void pop() {
        if (heap_.empty()) return;
        del(heap_.front().id);
    }

    // 获取下一次超时的毫秒数，用于 epoll_wait
    int GetNextTick();

    void clear() {
        heap_.clear();
        id2Index.clear();
    }

    // 删除指定 id(fd) 的定时器
    void del(int id);

private:
    /**
     * @brief 上浮
     * 不断将该节点与其父节点比较
     * 如果它比父节点小,就和父节点交换,直到它到达根节点,或者父节点比它小为止
     */
    bool siftup(size_t index);

    /**
     * @brief 下沉
     * 不断将该节点与其两个孩子中较小的那个比较，
     * 如果它比那个孩子大，就交换，直到它是叶子节点，或者比两个孩子都小为止
     */
    void siftdown(size_t index, size_t n);

    // 交换两个节点，并更新 id2Index 映射
    void swapNode(size_t i, size_t j) {
        std::swap(heap_[i], heap_[j]);
        id2Index[heap_[i].id] = i;
        id2Index[heap_[j].id] = j;
    }

    /**
     *父节点下标：(i - 1) / 2
     *左孩子下标：2 * i + 1
     *右孩子下标：2 * i + 2
     */
    std::vector<TimerNode> heap_;

    // 快速查找 id 对应的堆索引
    std::unordered_map<int, size_t> id2Index;
};
