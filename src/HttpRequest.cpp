#include "HttpRequest.h"

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
    // GET /index.html HTTP/1.1
    size_t pos1 = line.find(' ');
    if (pos1 == std::string::npos) return false;
    method_ = line.substr(0, pos1);

    size_t pos2 = line.find(' ', pos1 + 1);
    if (pos2 == std::string::npos) return false;
    path_ = line.substr(pos1 + 1, pos2 - pos1 - 1);

    version_ = line.substr(pos2 + 1);
    state_ = HEADERS;
    return true;
}

void HttpRequest::ParseHeaders(const std::string& line) {
    if (line.empty()) {
        state_ = (method_ == "POST") ? BODY : FINISH;
        return;
    }
    size_t pos = line.find(':');
    if (pos == std::string::npos) return;

    std::string key = line.substr(0, pos);
    // 去除 value 前面的空格
    size_t val_start = pos + 1;
    while (val_start < line.size() && line[val_start] == ' ') val_start++;

    headers_[key] = line.substr(val_start);
}

void HttpRequest::ParseBody(Buffer& buf) {
    size_t len = 0;
    if (auto it = headers_.find("Content-Length"); it != headers_.end()) {
        len = std::stoull(it->second);
        // 检查数据够不够
        if (buf.ReadableBytes() >= len) {
            body_ = buf.RetrieveToStr(len);  // 够了,全部取走

            if (headers_["Content-Type"] == "application/json") {
                ParseJson();
            } else {
                ParsePost();  // 解析 body_ 内容存入post_ map
            }
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
}

void HttpRequest::ParseJson() {
    try {
        if (body_.empty()) {
            LOG_WARN("Body is empty!");
            return;
        }
        json j = json::parse(body_);  // 利用json库解析

        // 遍历json对象,把第一层 key-value 存入 post_ map
        for (auto& [key, val] : j.items()) {
            if (val.is_string()) {
                post_[key] = val.get<std::string>();
            } else if (val.is_number()) {
                post_[key] = std::to_string(val.get<int>());
            } else if (val.is_boolean()) {
                post_[key] = val.get<bool>() ? "true" : "false";
            }
        }
    } catch (const json::parse_error& e) {
        LOG_ERROR("JSON Parse Error: {}", e.what());
    }
}

int HttpRequest::ConverHex(char ch) {
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return ch;
}