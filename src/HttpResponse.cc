/**
 * @file HttpResponse.cc
 * @brief HTTP response builder implementation
 */
#include <nitro_coro/http/HttpResponse.h>
#include <sstream>

namespace nitro_coro::http
{

void HttpResponse::setStatus(int code, const std::string & reason)
{
    statusCode_ = code;
    statusReason_ = reason.empty() ? getDefaultReason(code) : reason;
}

void HttpResponse::setHeader(const std::string & name, const std::string & value)
{
    HttpHeader header(name, value);
    headers_.insert_or_assign(header.name(), std::move(header));
}

void HttpResponse::setCookie(const std::string & name, const std::string & value)
{
    // TODO: Support cookie attributes (path, domain, expires, httponly, secure, etc.)
    cookies_.emplace_back(name, value);
}

Task<> HttpResponse::writeHeaders()
{
    if (headersSent_)
        co_return;

    std::ostringstream oss;
    oss << "HTTP/1.1 " << statusCode_ << " " << statusReason_ << "\r\n";

    for (const auto & [_, h] : headers_)
    {
        oss << h.serialize();
    }
    
    // Write Set-Cookie headers
    for (const auto & [name, value] : cookies_)
    {
        oss << "Set-Cookie: " << name << "=" << value << "\r\n";
    }
    
    oss << "\r\n";

    std::string headers = oss.str();
    co_await conn_->write(headers.c_str(), headers.size());
    headersSent_ = true;
}

Task<> HttpResponse::write(const std::string & body)
{
    if (!headersSent_)
    {
        setHeader("Content-Length", std::to_string(body.size()));
        co_await writeHeaders();
    }

    if (!body.empty())
    {
        co_await conn_->write(body.c_str(), body.size());
    }
}

const char * HttpResponse::getDefaultReason(int code)
{
    switch (code)
    {
    case 200:
        return "OK";
    case 201:
        return "Created";
    case 204:
        return "No Content";
    case 400:
        return "Bad Request";
    case 404:
        return "Not Found";
    case 500:
        return "Internal Server Error";
    default:
        return "Unknown";
    }
}

} // namespace nitro_coro::http
