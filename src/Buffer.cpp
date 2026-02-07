#include "Buffer.h"

#include <sys/uio.h>  //readv
#include <unistd.h>   //read

#include <cerrno>

void Buffer::MakeSpace(size_t len) {
    // 不够用,扩容
    if (WritableBytes() + readerIndex_ - kCheapPrepend < len) {
        buffer_.resize(writerIndex_ + len);
    }
    // 够用,但碎片化,把可读的数据挪到前面去
    else {
        size_t readable = ReadableBytes();
        std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
        readerIndex_ = kCheapPrepend;
        writerIndex_ = readerIndex_ + readable;
    }
}

// 利用栈空间临时缓冲区 (iovec),避免每次都分配大内存,又能一次性读完 socket 数据
ssize_t Buffer::ReadFd(int fd, int *saveErrno) {
    char extrabuf[INT16_MAX];  // 64KB的栈空间
    struct iovec vec[2];  // scatter/gather I/O,一次系统调用读写多块不连续的内存

    const size_t writable = WritableBytes();

    // 第一块缓冲区: Buffer 内部的剩余空间
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    // 第二块缓冲区: 栈上的临时空间
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;  // 如果 Buffer 够大,就不需要第二块
    const ssize_t n = readv(fd, vec, iovcnt);  // readv 分散读:自动填满第一块,再填第二块

    if (n < 0) {
        *saveErrno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        // 数据很少,第一块就装下了
        writerIndex_ += n;
    } else {
        // 数据很多,溢出到了 extrabuf
        writerIndex_ = buffer_.size();  // 第一块填满了
        // 把 extrabuf 里的数据加入 Buffer (此时会自动扩容)
        Append(extrabuf, n - writable);
    }

    return n;
}