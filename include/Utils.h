#pragma once
#include <sys/resource.h>
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
                if (errno == EINTR) continue;
                if (errno == EPIPE || errno == ECONNRESET) {
                    LOG_WARN("Client disconnected (EPIPE)");
                } else {
                    LOG_ERROR("sendfile failed: {}", strerror(errno));
                }
                return false;
            } else if (sent == 0) {
                return true;
            }
            remaining -= sent;
            LOG_INFO("[Sendfile]已传输Body: {}B, 剩余{}B", sent, remaining);
        }
        return true;
    }

    static void setRlimit() {
        // 将文件描述符限制提高到65535
        struct rlimit rlim;
        if (getrlimit(RLIMIT_NOFILE, &rlim) != 0) {
            LOG_ERROR("getrlimit failed");
        }
        rlim.rlim_cur = rlim.rlim_max;
        if (setrlimit(RLIMIT_NOFILE, &rlim) != 0) {
            LOG_ERROR("setrlimit failed");
        }
        LOG_INFO("Set fd limit to 65535");
    }
};
