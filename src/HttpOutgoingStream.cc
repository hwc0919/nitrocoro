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

template <typename DataType>
void HttpOutgoingStreamBase<DataType>::setHeader(const std::string & name, const std::string & value)
{
    HttpHeader header(name, value);
    data_.headers.insert_or_assign(header.name(), std::move(header));
}

template <typename DataType>
void HttpOutgoingStreamBase<DataType>::setHeader(HttpHeader header)
{
    data_.headers.insert_or_assign(header.name(), std::move(header));
}

template <typename DataType>
void HttpOutgoingStreamBase<DataType>::setCookie(const std::string & name, const std::string & value)
{
    data_.cookies[name] = value;
}

template <typename DataType>
Task<> HttpOutgoingStreamBase<DataType>::write(const char * data, size_t len)
{
    co_await writeHeaders();
    co_await conn_->write(data, len);
}

template <typename DataType>
Task<> HttpOutgoingStreamBase<DataType>::write(std::string_view data)
{
    co_await write(data.data(), data.size());
}

template <typename DataType>
Task<> HttpOutgoingStreamBase<DataType>::end()
{
    co_await writeHeaders();
}

template <typename DataType>
Task<> HttpOutgoingStreamBase<DataType>::end(std::string_view data)
{
    co_await write(data);
}

template <typename DataType>
Task<> HttpOutgoingStreamBase<DataType>::writeHeaders()
{
    if (headersSent_)
        co_return;

    std::ostringstream oss;

    if constexpr (std::is_same_v<DataType, HttpRequest>)
    {
        oss << data_.method << " " << data_.path << " " << data_.version << "\r\n";

        for (const auto & [name, header] : data_.headers)
        {
            oss << header.name() << ": " << header.value() << "\r\n";
        }

        for (const auto & [name, value] : data_.cookies)
        {
            oss << "Cookie: " << name << "=" << value << "\r\n";
        }
    }
    else // HttpResponse
    {
        oss << data_.version << " " << data_.statusCode << " " << data_.statusReason << "\r\n";

        for (const auto & [name, header] : data_.headers)
        {
            oss << header.name() << ": " << header.value() << "\r\n";
        }

        for (const auto & [name, value] : data_.cookies)
        {
            oss << "Set-Cookie: " << name << "=" << value << "\r\n";
        }
    }

    oss << "\r\n";

    std::string headers = oss.str();
    co_await conn_->write(headers.c_str(), headers.size());
    headersSent_ = true;
}

template <typename DataType>
const char * HttpOutgoingStreamBase<DataType>::getDefaultReason(int code)
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

// Explicit instantiations
template class HttpOutgoingStreamBase<HttpRequest>;
template class HttpOutgoingStreamBase<HttpResponse>;

// ============================================================================
// HttpOutgoingStream<HttpRequest> Implementation
// ============================================================================

// ============================================================================
// HttpOutgoingStream<HttpResponse> Implementation
// ============================================================================

void HttpOutgoingStream<HttpResponse>::setStatus(int code, const std::string & reason)
{
    data_.statusCode = code;
    data_.statusReason = reason.empty() ? getDefaultReason(code) : reason;
}

} // namespace nitro_coro::http
