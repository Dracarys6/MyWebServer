#pragma once
#include <sys/mman.h>
#include <sys/stat.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "Buffer.h"

class HttpResponse {
public:
    HttpResponse() : code_(-1), path_(""), srcDir_(""), mmFile_(nullptr), isKeepAlive_(false) {}

    ~HttpResponse() {
        if (mmFile_) {
            // *内核分配的内存由内核回收(这里是munmap),不用delete
            munmap(mmFile_, mmFileStat_.st_size);
            mmFile_ = nullptr;  // 指针悬空,防止悬空指针
        }
    }

    void Init(const std::string& srcDir, std::string& path, bool isKeepAlive = false,
              int code = -1) {
        if (mmFile_) {
            munmap(mmFile_, mmFileStat_.st_size);
            mmFile_ = nullptr;
        }
        code_ = code;
        isKeepAlive = isKeepAlive;
        path_ = path;
        srcDir_ = srcDir;
        mmFile_ = nullptr;
        mmFileStat_ = {0};
    }

    // 核心: 构建响应报文写入 Buffer
    void MakeResponse(Buffer& buf);

    // 获取文件的 Mime Type(如 .html -> text/html)
    std::string GetFileType(const std::string& name);

    // 生成默认错误页面
    void ErrorContent(Buffer& buf, std::string message);

    int getCode() const { return code_; }

private:
    void AddStateLine(Buffer& buf);
    void AddHeader(Buffer& buf);
    void AddContent(Buffer& buf);

    void ErrorHtml();  // 当文件找不到时,把path_改为 404.html

    int code_;  // 200,404 等
    std::string path_;
    std::string srcDir_;  // 静态资源根目录 /var/www/html

    char* mmFile_;            // mmap映射的文件指针
    struct stat mmFileStat_;  // 文件状态信息

    bool isKeepAlive_;
};