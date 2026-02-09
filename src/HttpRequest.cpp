#include "HttpRequest.h"

#include <regex>

bool HttpRequest::Parse(Buffer& buf) {
    const char* crlf = "\r\n";
    if (buf.ReadableBytes() <= 0) return false;

    // 循环处理每一行
    while (buf.ReadableBytes() > 0 && state_ != FINISH) {
        // 1. 处理 BODY (特殊：不需要找 \r\n，而是看长度)
        if (state_ == BODY) {
            ParseBody(buf);
            // 如果 ParseBody 还没读够数据，它会直接 return，state_ 保持 BODY
            // 如果读够了，它会把 state_ 改为 FINISH
            continue;  // 继续循环，检查是否 FINISH
        }

        // 2. 获取一行数据的结束位置
        const char* lineEnd = std::search(buf.Peek(), buf.Writable(), crlf, crlf + 2);
        std::string line;

        // 没找到换行符 -> 数据不完整，等下次 read
        if (lineEnd == buf.Writable()) {
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
        std::cout << "请求行: " << subMatch[0] << std::endl;
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
        std::cout << "请求头: " << subMatch[0] << std::endl;
        headers_[subMatch[1]] = subMatch[2];
    } else if (line.empty()) {
        // 遇到空行,Headers结束
        // 关键点：根据 Method 和 Content-Length 决定下一个状态
        if (method_ == "POST" && headers_.count("Content-Length"))
            state_ = BODY;
        else
            state_ = FINISH;
    }
}

void HttpRequest::ParseBody(Buffer& buf) {
    size_t len = 0;
    if (auto it = headers_.find("Content-Length"); it != headers_.end()) {
        len = std::stoull(it->second);
    }
    // 检查数据够不够
    if (buf.ReadableBytes() >= len) {
        body_ = buf.RetrieveToStr(len);  // 够了,全部取走
        state_ = FINISH;
        std::cout << "请求体: " << body_ << std::endl;
    } else {  // 不够,啥也不做,等下一次 read
    }  // 没有Content-Length,视作请求体为空
}
