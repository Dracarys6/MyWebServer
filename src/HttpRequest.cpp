#include "HttpRequest.h"

#include <regex>

#include "Log.h"

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
                ParsePath();       // TODO:处理 URL 里的参数
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
        // 检查数据够不够
        if (buf.ReadableBytes() >= len) {
            body_ = buf.RetrieveToStr(len);  // 够了,全部取走
            ParsePost();                     // 解析 body_ 内容存入post_ map
            state_ = FINISH;
        }
        //  else 不够,等下一次 read
    }
    // else 没有Content-Length,视作请求体为空
}

void HttpRequest::ParsePost() {
    LOG_DEBUG("method = {}", getMethod());
    LOG_DEBUG("Content-Type = {}", headers_["Content-Type"]);
    LOG_DEBUG("body = {}", getBody());
    if (method_ == "POST" && headers_["Content-Type"] == "application/x-www-form-urlencoded") {
        if (body_.empty()) return;

        // 解析 key=value & key2=value2
        std::string key{}, value{};
        int num{0};
        int n = body_.size();
        int i = 0, j = 0;

        for (; i < n; ++i) {
            char ch = body_[i];
            switch (ch) {
                case '=':
                    key = body_.substr(j, i - j);
                    j = i + 1;
                    break;
                case '+':
                    body_[i] = ' ';  // 空格替换
                    break;
                case '%':
                    // URL Decode: %20 -> ' '(空格)
                    num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
                    body_[i + 2] = num % 10 + '0';
                    body_[i + 1] = num / 10 + '0';
                    i += 2;
                    break;
                case '&':
                    value = body_.substr(j, i - j);
                    j = i + 1;
                    post_[key] = value;
                default:
                    break;
            }
        }
        // 最后一个参数
        value = body_.substr(j, i - j);
        post_[key] = value;
    }

    LOG_DEBUG("user = {}", post_["user"]);
    LOG_DEBUG("pwd = {}", post_["pwd"]);
}

int HttpRequest::ConverHex(char ch) {
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return ch;
}