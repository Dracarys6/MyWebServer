#pragma once
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Buffer.h"

using json = nlohmann::json;

/**
 * @brief Http请求类
 */
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
        method_ = "";
        path_ = "";
        version_ = "";
        body_.clear();
        headers_.clear();
    }

    // 状态机: 解析 Buffer 中的数据,返回 true 表示解析成功（至少完成了一个请求）
    bool Parse(Buffer& buf);

    std::string_view getPath() const { return path_; }
    std::string_view getMethod() const { return method_; }
    std::string_view getHeader(const std::string& key) const {
        if (auto it = headers_.find(key); it != headers_.end()) {
            return it->second;
        }
        return "";
    }
    std::string getBody() const { return body_; }

    // 获取 POST 参数
    std::string getPost(const std::string& key) const {
        if (auto it = post_.find(key); it != post_.end()) {
            return it->second;
        }
        return "";
    }

    bool IsKeepAlive() const {
        if (auto it = headers_.find("Connection"); it != headers_.end()) {
            return it->second == "keep-alive";
        }
        // HTTP/1.1 默认 true，HTTP/1.0 默认 false
        return version_ == "1.1";
    }

private:
    // http报文
    bool ParseRequestLine(const std::string_view& line);
    void ParseHeaders(const std::string& line);
    void ParseBody(Buffer& buf);

    void ParsePath() { return; }

    // 解析POST (当Content-Type 为 "application/x-www-form-urlencoded" 时说明是 POST 请求)
    // 或Content-Type 为 "application/json"时 也可能是POST请求,这时使用ParseJson()逻辑
    void ParsePost();

    void ParseJson();

    // 把十六进制转字符
    static int ConverHex(char ch);

    ParseState state_{REQUEST_LINE};
    std::string_view method_{};
    std::string_view path_{};
    std::string_view version_{};
    std::string body_{};
    std::unordered_map<std::string_view, std::string_view> headers_{};
    std::unordered_map<std::string, std::string> post_{};
};