#include "HttpResponse.h"

#include <fcntl.h>     //open
#include <sys/mman.h>  //mmap,munmap
#include <unistd.h>    //close

#include <iostream>

const std::unordered_map<std::string, std::string> SUFFIX_TYPE = {
        {".html", "text/html"},
        {".xml", "text/xml"},
        {".xhtml", "application/xhtml+xml"},
        {".txt", "text/plain"},
        {".rtf", "application/rtf"},
        {".pdf", "application/pdf"},
        {".word", "application/nsword"},
        {".png", "image/png"},
        {".gif", "image/gif"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".au", "audio/basic"},
        {".mpeg", "video/mpeg"},
        {".avi", "video/x-msvideo"},
        {".gz", "application/x-gzip"},
        {".tar", "application/x-tar"},
        {".css", "text/css"},
        {".js", "text/javascript"},
};

void HttpResponse::MakeResponse(Buffer& buf) {
    std::string finalPath{srcDir_ + path_};
    if (stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
        code_ = 404;  // 文件不存在
    } else if (!(mmFileStat_.st_mode & S_IROTH)) {
        code_ = 403;  // 无读取权限
    } else if (code_ == -1) {
        code_ = 200;
    }
    // 如果是404,加载404.html
    if (code_ == 404) {
        path_ = "/404.html";
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }

    AddStateLine(buf);
    AddHeader(buf);
    AddContent(buf);
}

void HttpResponse::AddStateLine(Buffer& buf) {
    std::string status;
    switch (code_) {
        case 200:
            status = "OK";
            break;
        case 400:
            status = "Bad Request";
            break;
        case 403:
            status = "Forbidden";
            break;
        case 404:
            status = "Not Found";
            break;
        default:
            status = "Unknown";
            break;
    }
    buf.Append("HTTP/1.1 " + std::to_string(code_) + " " + status + "\r\n");
}

void HttpResponse::AddHeader(Buffer& buf) {
    buf.Append("Connection: ");
    if (isKeepAlive_) {
        buf.Append("keep-alive\r\n");
        buf.Append("Keep-Alive: timeout=10, max=500\r\n");
    } else {
        buf.Append("close\r\n");
    }

    // Content-Type
    // 查找后缀
    std::string::size_type idx = path_.find_last_of('.');
    std::string suffix{};
    if (idx != std::string::npos) suffix = path_.substr(idx);

    std::string type = "text/plain";
    if (auto it = SUFFIX_TYPE.find(suffix); it != SUFFIX_TYPE.end()) {
        type = it->second;
    }

    buf.Append("Content-Type: " + type + "\r\n");
    buf.Append("Content-Length: " + std::to_string(mmFileStat_.st_size) + "\r\n");
    buf.Append("\r\n");  // 头部结束
}

// 先采用 mmap + write
// TODO:采用 sendfile 零拷贝
void HttpResponse::AddContent(Buffer& buf) {
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if (srcFd < 0) {
        ErrorContent(buf, "File NotFound!");
        return;
    }

    // *将文件映射到内存
    // TODO:理解这些内容 MAP_PRIVATE 建立写时拷贝的私有映射
    int* mmRet = (int*)(mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0));
    if (*mmRet == -1) {
        ErrorContent(buf, "File NotFound!");  // mmap失败
        return;
    }

    mmFile_ = (char*)mmRet;
    close(srcFd);

    // 将 mmap 的内存追加到 buffer
    // 注意：Buffer::Append 会执行 memcpy。
    // 如果想要极致性能，可以使用 writev 直接发送 mmap 的指针 (sendfile 更好)。
    // 但为了架构统一，我们先 copy 到 buffer。
    // (进阶作业：改用 sendfile 实现零拷贝)
    buf.Append(mmFile_, mmFileStat_.st_size);
}

void HttpResponse::ErrorContent(Buffer& buf, std::string message) {
    std::string body{};
    std::string status{};
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if (code_ == 404) {
        status = "404 Not Found";
        body += "404 Not Found";
    } else {
        status = "400 Bad Request";
        body += "Bad Request";
    }
    body += "<p>" + message + "</p>";
    body += "</body></html>";

    buf.Append("Content-Length: " + std::to_string(body.size()) + "\r\n\r\n");
    buf.Append(body);
}
