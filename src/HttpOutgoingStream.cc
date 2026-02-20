/**
 * @file HttpOutgoingStream.cc
 * @brief HTTP outgoing stream implementations
 */
#include <nitro_coro/http/stream/HttpOutgoingStream.h>
#include <sstream>

namespace nitro_coro::http
{

// ============================================================================
// HttpOutgoingStreamBase Implementation
// ============================================================================

template <typename Derived, typename DataType>
void HttpOutgoingStreamBase<Derived, DataType>::setHeader(const std::string & name, const std::string & value)
{
    HttpHeader header(name, value);
    data_.headers.insert_or_assign(header.name(), std::move(header));
}

template <typename Derived, typename DataType>
void HttpOutgoingStreamBase<Derived, DataType>::setHeader(HttpHeader header)
{
    data_.headers.insert_or_assign(header.name(), std::move(header));
}

template <typename Derived, typename DataType>
void HttpOutgoingStreamBase<Derived, DataType>::setCookie(const std::string & name, const std::string & value)
{
    data_.cookies[name] = value;
}

template <typename Derived, typename DataType>
Task<> HttpOutgoingStreamBase<Derived, DataType>::write(const char * data, size_t len)
{
    co_await static_cast<Derived *>(this)->writeHeaders();
    co_await conn_->write(data, len);
}

template <typename Derived, typename DataType>
Task<> HttpOutgoingStreamBase<Derived, DataType>::write(std::string_view data)
{
    co_await write(data.data(), data.size());
}

template <typename Derived, typename DataType>
Task<> HttpOutgoingStreamBase<Derived, DataType>::end()
{
    co_await static_cast<Derived *>(this)->writeHeaders();
}

template <typename Derived, typename DataType>
Task<> HttpOutgoingStreamBase<Derived, DataType>::end(std::string_view data)
{
    co_await write(data);
}

// Explicit instantiations
template class HttpOutgoingStreamBase<HttpOutgoingStream<HttpRequest>, HttpRequest>;
template class HttpOutgoingStreamBase<HttpOutgoingStream<HttpResponse>, HttpResponse>;

// ============================================================================
// HttpOutgoingStream<HttpRequest> Implementation
// ============================================================================

Task<> HttpOutgoingStream<HttpRequest>::writeHeaders()
{
    if (headersSent_)
        co_return;

    std::ostringstream oss;
    oss << data_.method << " " << data_.path << " " << data_.version << "\r\n";

    for (const auto & [name, header] : data_.headers)
    {
        oss << header.name() << ": " << header.value() << "\r\n";
    }

    for (const auto & [name, value] : data_.cookies)
    {
        oss << "Cookie: " << name << "=" << value << "\r\n";
    }

    oss << "\r\n";

    std::string headers = oss.str();
    co_await conn_->write(headers.c_str(), headers.size());
    headersSent_ = true;
}

// ============================================================================
// HttpOutgoingStream<HttpResponse> Implementation
// ============================================================================

void HttpOutgoingStream<HttpResponse>::setStatus(int code, const std::string & reason)
{
    data_.statusCode = code;
    data_.statusReason = reason.empty() ? getDefaultReason(code) : reason;
}

Task<> HttpOutgoingStream<HttpResponse>::writeHeaders()
{
    if (headersSent_)
        co_return;

    std::ostringstream oss;
    oss << data_.version << " " << data_.statusCode << " " << data_.statusReason << "\r\n";

    for (const auto & [name, header] : data_.headers)
    {
        oss << header.name() << ": " << header.value() << "\r\n";
    }

    for (const auto & [name, value] : data_.cookies)
    {
        oss << "Set-Cookie: " << name << "=" << value << "\r\n";
    }

    oss << "\r\n";

    std::string headers = oss.str();
    co_await conn_->write(headers.c_str(), headers.size());
    headersSent_ = true;
}

const char * HttpOutgoingStream<HttpResponse>::getDefaultReason(int code)
{
    switch (code)
    {
        case 200:
            return "OK";
        case 201:
            return "Created";
        case 204:
            return "No Content";
        case 301:
            return "Moved Permanently";
        case 302:
            return "Found";
        case 304:
            return "Not Modified";
        case 400:
            return "Bad Request";
        case 401:
            return "Unauthorized";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 500:
            return "Internal Server Error";
        case 502:
            return "Bad Gateway";
        case 503:
            return "Service Unavailable";
        default:
            return "";
    }
}

} // namespace nitro_coro::http
