#include "Timer.h"

#include "Log.h"

void Timer::adjust(int id, int timeout, const TimeoutCallBack& callback) {
    if (id2Index.count(id) == 0) {
        // 不存在,直接添加
        add(id, timeout, callback);
        return;
    }
    size_t index = id2Index[id];
    heap_[index].expire_time = Clock::now() + MS(timeout);
    siftdown(index, heap_.size());
}

void Timer::add(int id, int timeout, const TimeoutCallBack& callback) {
    if (id2Index.count(id)) {
        // 如果id已存在,直接调整
        adjust(id, timeout, callback);
        return;
    }
    // 追加到堆数组末尾
    heap_.push_back({id, Clock::now() + MS(timeout), callback});
    id2Index[id] = heap_.size() - 1;
    siftup(heap_.size() - 1);
}

void Timer::doWork(int id) {
    if (id2Index.count(id) == 0) {
        return;
    }
    size_t index = id2Index[id];
    TimerNode node = heap_[index];
    node.callback();  // 执行回调
    del(index);       // 删除节点
}

void Timer::tick() {
    if (heap_.empty()) return;
    TimeStamp now = Clock::now();
    while (!heap_.empty()) {
        TimerNode& node = heap_.front();  // 堆顶
        if (node.expire_time > now) {     // 没超时
            break;
        }
        // 超时了,执行回调
        node.callback();
        pop();
    }
}

int Timer::GetNextTick() {
    if (heap_.empty()) return -1;
    TimeStamp now = Clock::now();
    auto next = heap_.front().expire_time;
    if (next <= now) return 0;
    auto duration = std::chrono::duration_cast<MS>(next - now);
    return static_cast<int>(duration.count());
}

void Timer::del(int id) {
    if (id2Index.count(id) == 0) return;
    size_t index = id2Index[id];
    // 将要删除的节点和最后一个节点交换
    size_t last = heap_.size() - 1;
    swapNode(index, last);
    // 删除最后一个节点
    id2Index.erase(heap_.back().id);
    heap_.pop_back();
    // 调整堆,因为换上来的元素可能比父节点大,也可能比子节点小,所以需要尝试上浮或下沉
    if (!heap_.empty() && index < heap_.size()) {
        siftup(index);
        siftdown(index, heap_.size());
    }
}

bool Timer::siftup(size_t index) {
    if (index == 0) {
        return false;
    }
    ssize_t i = index;
    size_t parent = (i - 1) / 2;
    while (parent >= 0 && heap_[i] < heap_[parent]) {
        swapNode(i, parent);
        i = parent;
        parent = (i - 1) / 2;
        if (i == 0) break;
    }
    return true;
}

void Timer::siftdown(size_t index, size_t n) {
    size_t i = index;
    size_t child = 2 * i + 1;
    while (child < n) {
        // 找两个孩子中较小的那个
        if (child + 1 < n && heap_[child + 1] < heap_[child]) {
            ++child;
        }
        if (heap_[i] > heap_[child]) {
            swapNode(i, child);
            i = child;
            child = 2 * i + 1;
        } else {  // 如果当前节点比两孩子都小,就不用动了
            break;
        }
    }
}