/**
 * @file HttpOutgoingStream.h
 * @brief HTTP outgoing stream for writing requests and responses
 */
#pragma once
#include <nitrocoro/http/HttpMessage.h>
#include <nitrocoro/http/HttpTypes.h>
#include <nitrocoro/http/stream/HttpOutgoingStreamBase.h>

namespace nitrocoro::http
{

// Forward declaration
template <typename T>
class HttpOutgoingStream;

// ============================================================================
// HttpOutgoingStream<HttpRequest> - Write HTTP Request
// ============================================================================

template <>
class HttpOutgoingStream<HttpRequest>
    : public HttpOutgoingStreamBase<HttpRequest>
{
public:
    explicit HttpOutgoingStream(net::TcpConnectionPtr conn)
        : HttpOutgoingStreamBase(std::move(conn)) {}

    void setMethod(const std::string & method) { data_.method = method; }
    void setPath(const std::string & path) { data_.path = path; }
    void setVersion(Version version) { data_.version = version; }
};

// ============================================================================
// HttpOutgoingStream<HttpResponse> - Write HTTP Response
// ============================================================================

template <>
class HttpOutgoingStream<HttpResponse>
    : public HttpOutgoingStreamBase<HttpResponse>
{
public:
    using HttpOutgoingStreamBase::HttpOutgoingStreamBase;

    void setStatus(StatusCode code, const std::string & reason = "");
    void setVersion(Version version) { data_.version = version; }
    void setCloseConnection(bool shouldClose) { data_.shouldClose = shouldClose; }
};

} // namespace nitrocoro::http
