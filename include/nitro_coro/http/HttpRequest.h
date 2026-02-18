/**
 * @file HttpRequest.h
 * @brief HTTP request parser and representation
 */
#pragma once

#include <map>
#include <string>
#include <string_view>

namespace nitro_coro::http
{

class HttpRequest
{
public:
    HttpRequest() = default;

    // Parse HTTP request from buffer, returns bytes consumed or -1 on error
    int parse(const char * data, size_t len);

    bool isComplete() const { return complete_; }

    const std::string & method() const { return method_; }
    const std::string & path() const { return path_; }
    const std::string & version() const { return version_; }
    const std::string & body() const { return body_; }

    std::string_view header(const std::string & name) const;
    const std::map<std::string, std::string> & headers() const { return headers_; }

    std::string_view query(const std::string & name) const;

private:
    enum class State
    {
        RequestLine,
        Headers,
        Body,
        Complete
    };

    void parseRequestLine(std::string_view line);
    void parseHeader(std::string_view line);
    void parseQueryString();

    State state_ = State::RequestLine;
    bool complete_ = false;

    std::string method_;
    std::string path_;
    std::string version_;
    std::map<std::string, std::string> headers_;
    std::map<std::string, std::string> queries_;
    std::string body_;

    std::string buffer_;
    size_t bodyBytesRead_ = 0;
    size_t contentLength_ = 0;
};

} // namespace nitro_coro::http
