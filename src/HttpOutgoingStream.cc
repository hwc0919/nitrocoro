/**
 * @file HttpOutgoingStream.cc
 * @brief HTTP outgoing stream implementation
 */
#include <nitro_coro/http/HttpOutgoingStream.h>
#include <sstream>

namespace nitro_coro::http
{

void HttpOutgoingStream::setStatus(int code, const std::string & reason)
{
    statusCode_ = code;
    statusReason_ = reason.empty() ? getDefaultReason(code) : reason;
}

void HttpOutgoingStream::setHeader(const std::string & name, const std::string & value)
{
    HttpHeader header(name, value);
    headers_.insert_or_assign(header.name(), std::move(header));
}

void HttpOutgoingStream::setCookie(const std::string & name, const std::string & value)
{
    cookies_.emplace_back(name, value);
}

Task<> HttpOutgoingStream::writeHeaders()
{
    if (headersSent_)
        co_return;

    std::ostringstream oss;
    oss << "HTTP/1.1 " << statusCode_ << " " << statusReason_ << "\r\n";

    for (const auto & [_, h] : headers_)
    {
        oss << h.serialize();
    }

    for (const auto & [name, value] : cookies_)
    {
        oss << "Set-Cookie: " << name << "=" << value << "\r\n";
    }

    oss << "\r\n";

    std::string headers = oss.str();
    co_await conn_->write(headers.c_str(), headers.size());
    headersSent_ = true;
}

Task<> HttpOutgoingStream::write(const char * data, size_t len)
{
    if (!headersSent_)
    {
        co_await writeHeaders();
    }

    if (len > 0)
    {
        co_await conn_->write(data, len);
    }
}

Task<> HttpOutgoingStream::write(std::string_view data)
{
    co_await write(data.data(), data.size());
}

Task<> HttpOutgoingStream::end()
{
    if (!headersSent_)
    {
        setHeader("Content-Length", "0");
        co_await writeHeaders();
    }
}

Task<> HttpOutgoingStream::end(std::string_view data)
{
    if (!headersSent_)
    {
        setHeader("Content-Length", std::to_string(data.size()));
        co_await writeHeaders();
    }

    if (!data.empty())
    {
        co_await conn_->write(data.data(), data.size());
    }
}

const char * HttpOutgoingStream::getDefaultReason(int code)
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
