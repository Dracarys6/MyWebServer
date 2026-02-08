#pragma once
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

/**
 * @brief Buffer：自动扩容、支持追加、支持部分读取的缓冲区类
 * @details readerIndex: 数据开始读的地方
 * writerIndex: 数据写进去的地方
 * +-------------------+------------------+------------------+
   | prependable bytes |  readable bytes  |  writable bytes  |
   |                   |     (CONTENT)    |                  |
   +-------------------+------------------+------------------+
   |                   |                  |                  |
   0      <=      readerIndex   <=   writerIndex    <=     size
 */
class Buffer {
public:
    static const size_t kCheapPrepend{8UL};  // 前缀头
    static const size_t kInitialSize{1024UL};

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize),
          readerIndex_(kCheapPrepend),
          writerIndex_(kCheapPrepend) {}

    // 可读字节数
    size_t ReadableBytes() const { return writerIndex_ - readerIndex_; }

    // 可写字节数
    size_t WritableBytes() const { return buffer_.size() - writerIndex_; }

    // 返回可读数据的起始指针
    const char* Peek() const {
        return begin() + readerIndex_;  // 等价于 &buffer_[readerIndex_]
    }

    // 取出 len 长度的数据(移动 readerIndex)
    void Retrieve(size_t len) {
        if (len < ReadableBytes()) {
            readerIndex_ += len;
        } else {
            RetrieveAll();
        }
    }

    // 取出所有数据
    void RetrieveAll() {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    // 取出指定长数据转为string
    std::string RetrieveToStr(size_t len) {
        if (len > ReadableBytes()) len = ReadableBytes();
        std::string str(Peek(), len);
        Retrieve(len);
        return str;
    }

    // 取出所有数据转为 string
    std::string RetrieveAllToStr() {
        std::string str(Peek(), ReadableBytes());
        RetrieveAll();
        return str;
    }

    // 写入数据
    void Append(const std::string& str) { Append(str.data(), str.length()); }

    void Append(const char* data, size_t len) {
        EnsureWritableBytes(len);
        std::copy(data, data + len, begin() + writerIndex_);
        writerIndex_ += len;
    }

    // 确保有足够空间写
    void EnsureWritableBytes(size_t len) {
        if (WritableBytes() < len) {
            MakeSpace(len);
        }
    }

    // 从 fd 读取数据 (处理 ET 模式的关键)
    ssize_t ReadFd(int fd, int* saveErrno);

private:
    char* begin() { return &*buffer_.begin(); }

    const char* begin() const {
        return buffer_.data();  // 等价于 &*buffer.begin()
    }

    void MakeSpace(size_t len);

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};