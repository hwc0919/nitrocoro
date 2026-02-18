/**
 * @file HttpMessage.h
 * @brief HTTP message data structures
 */
#pragma once

#include <nitro_coro/http/HttpHeader.h>
#include <map>
#include <string>

namespace nitro_coro::http
{

struct HttpRequest
{
    std::string method;
    std::string path;
    std::string version = "HTTP/1.1";
    std::map<std::string, HttpHeader> headers;
    std::map<std::string, std::string> cookies;
    std::map<std::string, std::string> queries;
};

struct HttpResponse
{
    int statusCode = 200;
    std::string statusReason;
    std::string version = "HTTP/1.1";
    std::map<std::string, HttpHeader> headers;
    std::map<std::string, std::string> cookies;
};

} // namespace nitro_coro::http
