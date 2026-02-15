#pragma once
#include <iostream>
#include <string>
#include <unordered_map>

#include "Buffer.h"

class HttpRequest {
public:
    HttpRequest() {}

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

    std::string getPath() const { return path_; }
    std::string getMethod() const { return method_; }
    std::string getBody() const { return body_; }
    std::string getHeader(const std::string& key) const {
        if (auto it = headers_.find(key); it != headers_.end()) {
            return it->second;
        }
        return "";
    }

    // 获取 POST 参数
    std::string getPost(const std::string& key) const {
        if (auto it = post_.find(key); it != post_.end()) {
            return it->second;
        }
        return "";
    }

    bool IsKeepAlive() const {
        if (auto it = headers_.find("Connection"); it != headers_.end()) {
            return it->second == "keep-alive" && version_ == "1.1";
        }
        return false;
    }

private:
    // http报文
    bool ParseRequestLine(const std::string& line);
    void ParseHeaders(const std::string& line);
    void ParseBody(Buffer& buf);

    void ParsePath() { return; }

    // 解析POST (当Content-Type 为 "application/x-www-form-urlencoded" 时说明是 POST 请求)
    void ParsePost();

    // 把十六进制转字符
    static int ConverHex(char ch);

    ParseState state_{REQUEST_LINE};
    std::string method_{};
    std::string path_{};
    std::string version_{};
    std::string body_{};
    std::unordered_map<std::string, std::string> headers_{};
    std::unordered_map<std::string, std::string> post_{};
};