#pragma once
#include <iostream>
#include <string>
#include <unordered_map>

#include "Buffer.h"

class HttpRequest {
public:
    enum ParseState {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,
    };

    void Init() {
        state_ = REQUEST_LINE;
        method_.clear();
        path_.clear();
        version_.clear();
        body_.clear();
        headers_.clear();
    }

    // 状态机: 解析 Buffer 中的数据,返回 true 表示解析成功（至少完成了一个请求）
    bool Parse(Buffer& buf);

    // Getters
    std::string path() const { return path_; }
    std::string method() const { return method_; }
    std::string body() const { return body_; }
    std::string header(const std::string& key) const {
        if (auto it = headers_.find(key); it != headers_.end()) {
            return it->second;
        }
        return "";
    }

private:
    // http报文
    bool ParseRequestLine(const std::string& line);
    void ParseHeaders(const std::string& line);
    bool ParseBody(Buffer& buf);

    void ParsePath() { return; }

    ParseState state_{REQUEST_LINE};
    std::string method_{};
    std::string path_{};
    std::string version_{};
    std::string body_{};
    std::unordered_map<std::string, std::string> headers_{};
};