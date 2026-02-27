/**
 * @file HttpIncomingStream.h
 * @brief HTTP incoming stream for reading requests and responses
 */
#pragma once
#include <nitrocoro/http/BodyReader.h>
#include <nitrocoro/http/HttpContext.h>
#include <nitrocoro/http/HttpDataAccessor.h>
#include <nitrocoro/http/HttpMessage.h>
#include <nitrocoro/http/stream/HttpIncomingStreamBase.h>

namespace nitrocoro::http
{

// Forward declaration
template <typename T>
class HttpIncomingStream;

class HttpCompleteResponse;

// ============================================================================
// HttpIncomingStream<HttpRequest> - Read HTTP Request
// ============================================================================

template <>
class HttpIncomingStream<HttpRequest>
    : public HttpRequestAccessor<HttpIncomingStream<HttpRequest>>,
      public HttpIncomingStreamBase<HttpRequest>
{
public:
    using HttpIncomingStreamBase::HttpIncomingStreamBase;

    // Task<HttpCompleteRequest> toCompleteRequest();
};

// ============================================================================
// HttpIncomingStream<HttpResponse> - Read HTTP Response
// ============================================================================

template <>
class HttpIncomingStream<HttpResponse>
    : public HttpResponseAccessor<HttpIncomingStream<HttpResponse>>,
      public HttpIncomingStreamBase<HttpResponse>
{
public:
    using HttpIncomingStreamBase::HttpIncomingStreamBase;

    Task<HttpCompleteResponse> toCompleteResponse();
};

} // namespace nitrocoro::http
