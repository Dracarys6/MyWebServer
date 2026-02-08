#include "HttpRequest.h"

#include <regex>

// todo: post 似乎会死循环
bool HttpRequest::Parse(Buffer& buf) {
    const char* crlf = "\r\n";
    if (buf.ReadableBytes() <= 0) return false;

    // 循环处理每一行
    while (buf.ReadableBytes() > 0 && state_ != FINISH) {  // 1. 获取一行数据的结束位置
        const char* lineEnd =
                std::search(buf.Peek(), buf.Peek() + buf.ReadableBytes(), crlf, crlf + 2);
        std::string line;

        // 没找到换行符 -> 数据不完整，等下次 read
        if (lineEnd == buf.Peek() + buf.ReadableBytes()) {
            return false;
        }

        line = std::string(buf.Peek(), lineEnd);  // 提取这一行
        buf.Retrieve(line.length() + 2);          // buf 指针后移(跳过\r\n)

        // 状态机
        switch (state_) {
            case REQUEST_LINE:
                if (!ParseRequestLine(line))
                    return false;  // 请求头不对,提前退出状态机,避免继续循环
                ParsePath();       // todo:处理 URL 里的参数
                break;
            case HEADERS:
                ParseHeaders(line);
                if (buf.ReadableBytes() <= 0 && state_ != FINISH) {
                    // Buffer 空了但还没 FINISH (还在 Head 中) -> return false 等更多数据
                    return false;
                }
                break;
            case BODY:
                if (!ParseBody(buf)) return false;
                break;
            default:
                break;
        }
    }
    return state_ == FINISH;
}

bool HttpRequest::ParseRequestLine(const std::string& line) {
    // 正则匹配: GET /index.html HTTP/1.1
    std::regex pattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch subMatch;
    if (std::regex_match(line, subMatch, pattern)) {
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = HEADERS;
        return true;
    }
    return false;
}

void HttpRequest::ParseHeaders(const std::string& line) {
    std::regex pattern("^([^:]*): ?(.*)$");
    std::smatch subMatch;
    if (std::regex_match(line, subMatch, pattern)) {
        headers_[subMatch[1]] = subMatch[2];
    } else if (line.empty()) {
        // 遇到空行,Headers结束
        if (method_ == "POST")
            state_ = BODY;
        else
            state_ = FINISH;
    }
}

bool HttpRequest::ParseBody(Buffer& buf) {
    size_t len = 0;
    if (auto it = headers_.find("Content-Length"); it != headers_.end()) {
        len = std::stoull(it->second);
        // 检查数据够不够
        if (buf.ReadableBytes() >= len) {
            body_ = buf.RetrieveToStr(len);  // 够了,全部取走
            state_ = FINISH;
            return true;
        } else
            return false;  // 不够,啥也不做,等下一次 read
    }
    return false;  // 没有Content-Length,视作请求体为空
}
