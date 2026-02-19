#pragma once
#include <sys/sendfile.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>

#include "Log.h"

class Utils {
public:
    static bool SendFile(int outFd, int inFd, size_t size, off_t offset = 0) {
        size_t remaining = size;
        ssize_t sent = 0;
        LOG_INFO("开始传输, 总大小: {}B", remaining);

        while (remaining > 0) {
            sent = sendfile(outFd, inFd, &offset, remaining);
            // 处理错误: EINTR 是被信号中断,可重试; EPIPE 是客户端断开
            if (sent == -1) {
                if (errno == EINTR)
                    continue;
                else if (errno == EPIPE) {
                    LOG_ERROR("客户端断开连接");
                    break;
                } else {
                    LOG_ERROR("传输失败: {}", strerror(errno));
                    return false;
                }
            } else if (sent == 0) {
                return true;
            }
            remaining -= sent;
            LOG_INFO("已传输: {}B, 剩余{}B", sent, remaining);
        }
        return true;
    }
};
