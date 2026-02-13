#pragma once
#include <sys/sendfile.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>

class Utils {
public:
    static bool SendFile(int outFd, int inFd, size_t size, off_t offset = 0) {
        size_t remaining = size;
        ssize_t sent = 0;
        std::cout << "开始传输, 总大小: " << remaining << "B" << std::endl;

        while (remaining > 0) {
            sent = sendfile(outFd, inFd, &offset, remaining);
            // 处理错误: EINTR 是被信号中断,可重试; EPIPE 是客户端断开
            if (sent == -1) {
                if (errno == EINTR)
                    continue;
                else if (errno == EPIPE) {
                    std::cerr << "客户端断开连接" << std::endl;
                    break;
                } else {
                    std::cerr << "传输失败: " << strerror(errno) << std::endl;
                    return false;
                }
            } else if (sent == 0) {
                return true;
            }
            remaining -= sent;
            std::cout << "已传输: " << sent << "B, 剩余" << remaining << "B" << std::endl;
        }
        return true;
    }
};
