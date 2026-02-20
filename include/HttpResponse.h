#pragma once
#include <sys/mman.h>
#include <sys/stat.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "Buffer.h"

class HttpResponse {
public:
    HttpResponse() : code_(-1), path_(""), srcDir_(""), fileFd_(-1), isKeepAlive_(false) {}

    ~HttpResponse() {
        if (fileFd_ != -1) {
            close(fileFd_);
        }
    }

    void Init(const std::string& srcDir, std::string& path, bool isKeepAlive = false,
              int code = -1) {
        if (fileFd_ != -1) {
            close(fileFd_);
        }
        code_ = code;
        isKeepAlive_ = isKeepAlive;
        path_ = path;
        srcDir_ = srcDir;
        fileFd_ = -1;
        mmFileStat_ = {0};
    }

    // 核心: 构建响应报文写入 Buffer, Content(即html文件)传输到 clientFd
    void MakeResponse(Buffer& buf, int clientFd);

    // 获取文件的 Mime Type(如 .html -> text/html)
    std::string GetFileType(const std::string& name);

    // 生成默认错误页面
    void ErrorContent(Buffer& buf, std::string message);

    int getCode() const { return code_; }

    int getFileFd() const { return fileFd_; }

    off_t getFileSize() const { return mmFileStat_.st_size; }

private:
    void AddStateLine(Buffer& buf);
    void AddHeader(Buffer& buf);
    void AddContent(Buffer& buf, int clientFd);  // Content即为静态html资源,采用sendfile

    ssize_t SendFile(int inFd);  // 封装sendfile

    void ErrorHtml();  // 当文件找不到时,把path_改为 404.html

    int code_;  // 200,404 等
    std::string path_;
    std::string srcDir_;  // 静态资源根目录 /var/www/html

    int fileFd_;
    struct stat mmFileStat_;  // 文件状态信息

    bool isKeepAlive_;
};