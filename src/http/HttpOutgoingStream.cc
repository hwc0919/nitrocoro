/**
 * @file HttpOutgoingStream.cc
 * @brief HTTP outgoing stream implementations
 */
#include <nitro_coro/http/BodyWriter.h>
#include <nitro_coro/http/stream/HttpOutgoingStream.h>
#include <sstream>

namespace nitro_coro::http
{

static const char * toVersionString(Version version)
{
    switch (version)
    {
        case Version::kHttp10:
            return "HTTP/1.0";
        case Version::kHttp11:
            return "HTTP/1.1";
        default:
            return "HTTP/1.1";
    }
}

// ============================================================================
// HttpOutgoingStreamBase Implementation
// ============================================================================

template <typename DataType>
void HttpOutgoingStreamBase<DataType>::setHeader(std::string_view name, std::string value)
{
    HttpHeader header(name, std::move(value));
    data_.headers.insert_or_assign(header.name(), std::move(header));
}

template <typename DataType>
void HttpOutgoingStreamBase<DataType>::setHeader(HttpHeader::NameCode code, std::string value)
{
    HttpHeader header(code, std::move(value));
    data_.headers.insert_or_assign(header.name(), std::move(header));
}

template <typename DataType>
void HttpOutgoingStreamBase<DataType>::setHeader(HttpHeader header)
{
    data_.headers.insert_or_assign(header.name(), std::move(header));
}

template <typename DataType>
void HttpOutgoingStreamBase<DataType>::setCookie(const std::string & name, std::string value)
{
    data_.cookies[name] = std::move(value);
}

template <typename DataType>
void HttpOutgoingStreamBase<DataType>::decideTransferMode()
{
    if (bodyWriter_)
        return;

    auto it = data_.headers.find(HttpHeader::Name::ContentLength_L);
    if (it != data_.headers.end())
    {
        size_t contentLength = std::stoull(it->second.value());
        bodyWriter_ = BodyWriter::create(TransferMode::ContentLength, conn_, contentLength);
        return;
    }

    it = data_.headers.find(HttpHeader::Name::TransferEncoding_L);
    if (it != data_.headers.end() && it->second.value().find("chunked") != std::string::npos)
    {
        bodyWriter_ = BodyWriter::create(TransferMode::Chunked, conn_);
        return;
    }

    setHeader(HttpHeader::NameCode::TransferEncoding, "chunked");
    bodyWriter_ = BodyWriter::create(TransferMode::Chunked, conn_);
}

template <typename DataType>
Task<> HttpOutgoingStreamBase<DataType>::write(const char * data, size_t len)
{
    if (!bodyWriter_)
        decideTransferMode();

    if (!headersSent_)
        co_await writeHeaders();

    co_await bodyWriter_->write(std::string_view(data, len));
}

template <typename DataType>
Task<> HttpOutgoingStreamBase<DataType>::write(std::string_view data)
{
    co_await write(data.data(), data.size());
}

template <typename DataType>
Task<> HttpOutgoingStreamBase<DataType>::end()
{
    if (!headersSent_)
    {
        setHeader(HttpHeader::NameCode::ContentLength, "0");
        co_await writeHeaders();
    }

    if (bodyWriter_)
        co_await bodyWriter_->end();
}

template <typename DataType>
Task<> HttpOutgoingStreamBase<DataType>::end(std::string_view data)
{
    if (data.empty())
    {
        co_await end();
        co_return;
    }

    if (!bodyWriter_)
        decideTransferMode();

    if (!headersSent_)
        co_await writeHeaders();

    co_await bodyWriter_->write(data);
    co_await bodyWriter_->end();
}

template <typename DataType>
Task<> HttpOutgoingStreamBase<DataType>::writeHeaders()
{
    if (headersSent_)
        co_return;

    std::ostringstream oss;

    if constexpr (std::is_same_v<DataType, HttpRequest>)
    {
        oss << data_.method << " " << data_.path << " " << toVersionString(data_.version) << "\r\n";

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
        oss << toVersionString(data_.version) << " " << static_cast<int>(data_.statusCode) << " " << data_.statusReason << "\r\n";

        for (const auto & [name, header] : data_.headers)
        {
            oss << header.name() << ": " << header.value() << "\r\n";
        }

        for (const auto & [name, value] : data_.cookies)
        {
            oss << "Set-Cookie: " << name << "=" << value << "\r\n";
        }

        // Add Connection header if not already set
        if (data_.headers.find(HttpHeader::Name::Connection_L) == data_.headers.end())
        {
            if (data_.shouldClose)
            {
                oss << "Connection: close\r\n";
            }
            else if (data_.version == Version::kHttp10)
            {
                oss << "Connection: keep-alive\r\n";
            }
        }
    }

    oss << "\r\n";

    std::string headers = oss.str();
    co_await conn_->write(headers.c_str(), headers.size());
    headersSent_ = true;
}

// TODO: move to http helpers
template <typename DataType>
const char * HttpOutgoingStreamBase<DataType>::getDefaultReason(StatusCode code)
{
    switch (code)
    {
        case StatusCode::k100Continue:
            return "Continue";
        case StatusCode::k101SwitchingProtocols:
            return "Switching Protocols";
        case StatusCode::k102Processing:
            return "Processing";
        case StatusCode::k103EarlyHints:
            return "Early Hints";
        case StatusCode::k200OK:
            return "OK";
        case StatusCode::k201Created:
            return "Created";
        case StatusCode::k202Accepted:
            return "Accepted";
        case StatusCode::k203NonAuthoritativeInformation:
            return "Non-Authoritative Information";
        case StatusCode::k204NoContent:
            return "No Content";
        case StatusCode::k205ResetContent:
            return "Reset Content";
        case StatusCode::k206PartialContent:
            return "Partial Content";
        case StatusCode::k207MultiStatus:
            return "Multi-Status";
        case StatusCode::k208AlreadyReported:
            return "Already Reported";
        case StatusCode::k226IMUsed:
            return "IM Used";
        case StatusCode::k300MultipleChoices:
            return "Multiple Choices";
        case StatusCode::k301MovedPermanently:
            return "Moved Permanently";
        case StatusCode::k302Found:
            return "Found";
        case StatusCode::k303SeeOther:
            return "See Other";
        case StatusCode::k304NotModified:
            return "Not Modified";
        case StatusCode::k305UseProxy:
            return "Use Proxy";
        case StatusCode::k307TemporaryRedirect:
            return "Temporary Redirect";
        case StatusCode::k308PermanentRedirect:
            return "Permanent Redirect";
        case StatusCode::k400BadRequest:
            return "Bad Request";
        case StatusCode::k401Unauthorized:
            return "Unauthorized";
        case StatusCode::k402PaymentRequired:
            return "Payment Required";
        case StatusCode::k403Forbidden:
            return "Forbidden";
        case StatusCode::k404NotFound:
            return "Not Found";
        case StatusCode::k405MethodNotAllowed:
            return "Method Not Allowed";
        case StatusCode::k406NotAcceptable:
            return "Not Acceptable";
        case StatusCode::k407ProxyAuthenticationRequired:
            return "Proxy Authentication Required";
        case StatusCode::k408RequestTimeout:
            return "Request Timeout";
        case StatusCode::k409Conflict:
            return "Conflict";
        case StatusCode::k410Gone:
            return "Gone";
        case StatusCode::k411LengthRequired:
            return "Length Required";
        case StatusCode::k412PreconditionFailed:
            return "Precondition Failed";
        case StatusCode::k413RequestEntityTooLarge:
            return "Request Entity Too Large";
        case StatusCode::k414RequestURITooLarge:
            return "Request-URI Too Large";
        case StatusCode::k415UnsupportedMediaType:
            return "Unsupported Media Type";
        case StatusCode::k416RequestedRangeNotSatisfiable:
            return "Requested Range Not Satisfiable";
        case StatusCode::k417ExpectationFailed:
            return "Expectation Failed";
        case StatusCode::k418ImATeapot:
            return "I'm a teapot";
        case StatusCode::k421MisdirectedRequest:
            return "Misdirected Request";
        case StatusCode::k422UnprocessableEntity:
            return "Unprocessable Entity";
        case StatusCode::k423Locked:
            return "Locked";
        case StatusCode::k424FailedDependency:
            return "Failed Dependency";
        case StatusCode::k425TooEarly:
            return "Too Early";
        case StatusCode::k426UpgradeRequired:
            return "Upgrade Required";
        case StatusCode::k428PreconditionRequired:
            return "Precondition Required";
        case StatusCode::k429TooManyRequests:
            return "Too Many Requests";
        case StatusCode::k431RequestHeaderFieldsTooLarge:
            return "Request Header Fields Too Large";
        case StatusCode::k451UnavailableForLegalReasons:
            return "Unavailable For Legal Reasons";
        case StatusCode::k500InternalServerError:
            return "Internal Server Error";
        case StatusCode::k501NotImplemented:
            return "Not Implemented";
        case StatusCode::k502BadGateway:
            return "Bad Gateway";
        case StatusCode::k503ServiceUnavailable:
            return "Service Unavailable";
        case StatusCode::k504GatewayTimeout:
            return "Gateway Timeout";
        case StatusCode::k505HTTPVersionNotSupported:
            return "HTTP Version Not Supported";
        case StatusCode::k506VariantAlsoNegotiates:
            return "Variant Also Negotiates";
        case StatusCode::k507InsufficientStorage:
            return "Insufficient Storage";
        case StatusCode::k508LoopDetected:
            return "Loop Detected";
        case StatusCode::k510NotExtended:
            return "Not Extended";
        case StatusCode::k511NetworkAuthenticationRequired:
            return "Network Authentication Required";
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

void HttpOutgoingStream<HttpResponse>::setStatus(StatusCode code, const std::string & reason)
{
    data_.statusCode = code;
    data_.statusReason = reason.empty() ? getDefaultReason(code) : reason;
}

} // namespace nitro_coro::http
