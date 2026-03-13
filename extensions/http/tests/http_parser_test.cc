/**
 * @file http_parser_test.cc
 * @brief Tests for HttpParser
 */
#include "../src/HttpParser.h"
#include <nitrocoro/testing/Test.h>

using namespace nitrocoro::http;

// ── Request Parser Tests ──────────────────────────────────────────────────────

NITRO_TEST(http_parser_request_basic)
{
    HttpParser<HttpRequest> parser;

    auto state = parser.parseLine("GET /hello HTTP/1.1");
    NITRO_CHECK_EQ(state, HttpParserState::ExpectHeader);

    state = parser.parseLine("Host: example.com");
    NITRO_CHECK_EQ(state, HttpParserState::ExpectHeader);

    state = parser.parseLine("");
    NITRO_CHECK_EQ(state, HttpParserState::HeaderComplete);

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.method, methods::Get);
    NITRO_CHECK_EQ(result.message.path, "/hello");
    NITRO_CHECK_EQ(result.message.version, Version::kHttp11);
    co_return;
}

NITRO_TEST(http_parser_request_with_query)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("GET /search?q=hello+world&page=1 HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.path, "/search");
    NITRO_CHECK_EQ(result.message.query, "q=hello+world&page=1");
    NITRO_CHECK(result.message.queries.contains("q"));
    NITRO_CHECK_EQ(result.message.queries.at("q"), "hello world");
    NITRO_CHECK(result.message.queries.contains("page"));
    NITRO_CHECK_EQ(result.message.queries.at("page"), "1");
    co_return;
}

NITRO_TEST(http_parser_request_malformed_line)
{
    HttpParser<HttpRequest> parser;

    auto state = parser.parseLine("GET /hello"); // Missing HTTP version
    NITRO_CHECK_EQ(state, HttpParserState::Error);

    auto result = parser.extractResult();
    NITRO_CHECK(result.error());
    NITRO_CHECK_EQ(result.errorCode, HttpParseError::MalformedRequestLine);
    co_return;
}

NITRO_TEST(http_parser_request_missing_space)
{
    HttpParser<HttpRequest> parser;

    auto state = parser.parseLine("GET"); // Missing path and version
    NITRO_CHECK_EQ(state, HttpParserState::Error);

    auto result = parser.extractResult();
    NITRO_CHECK(result.error());
    NITRO_CHECK_EQ(result.errorCode, HttpParseError::MalformedRequestLine);
    co_return;
}

NITRO_TEST(http_parser_request_content_length)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("POST /data HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("Content-Length: 100");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.transferMode, TransferMode::ContentLength);
    NITRO_CHECK_EQ(result.message.contentLength, 100);
    co_return;
}

NITRO_TEST(http_parser_request_chunked)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("POST /data HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("Transfer-Encoding: chunked");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.transferMode, TransferMode::Chunked);
    co_return;
}

NITRO_TEST(http_parser_request_unsupported_encoding)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("POST /data HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("Transfer-Encoding: gzip");
    auto state = parser.parseLine("");

    NITRO_CHECK_EQ(state, HttpParserState::Error);
    auto result = parser.extractResult();
    NITRO_CHECK(result.error());
    NITRO_CHECK_EQ(result.errorCode, HttpParseError::UnsupportedTransferEncoding);
    co_return;
}

NITRO_TEST(http_parser_request_keep_alive_http11)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("GET /hello HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK(result.message.keepAlive); // HTTP/1.1 default
    co_return;
}

NITRO_TEST(http_parser_request_keep_alive_http10)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("GET /hello HTTP/1.0");
    parser.parseLine("Host: example.com");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK(!result.message.keepAlive); // HTTP/1.0 default
    co_return;
}

NITRO_TEST(http_parser_request_connection_close)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("GET /hello HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("Connection: close");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK(!result.message.keepAlive);
    co_return;
}

// ── Response Parser Tests ─────────────────────────────────────────────────────

NITRO_TEST(http_parser_response_basic)
{
    HttpParser<HttpResponse> parser;

    auto state = parser.parseLine("HTTP/1.1 200 OK");
    NITRO_CHECK_EQ(state, HttpParserState::ExpectHeader);

    state = parser.parseLine("Content-Type: text/plain");
    NITRO_CHECK_EQ(state, HttpParserState::ExpectHeader);

    state = parser.parseLine("");
    NITRO_CHECK_EQ(state, HttpParserState::HeaderComplete);

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.version, Version::kHttp11);
    NITRO_CHECK_EQ(result.message.statusCode, StatusCode::k200OK);
    NITRO_CHECK_EQ(result.message.statusReason, "OK");
    co_return;
}

NITRO_TEST(http_parser_response_content_length)
{
    HttpParser<HttpResponse> parser;

    parser.parseLine("HTTP/1.1 200 OK");
    parser.parseLine("Content-Length: 50");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.transferMode, TransferMode::ContentLength);
    NITRO_CHECK_EQ(result.message.contentLength, 50);
    co_return;
}

NITRO_TEST(http_parser_response_chunked)
{
    HttpParser<HttpResponse> parser;

    parser.parseLine("HTTP/1.1 200 OK");
    parser.parseLine("Transfer-Encoding: chunked");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.transferMode, TransferMode::Chunked);
    co_return;
}

NITRO_TEST(http_parser_response_until_close)
{
    HttpParser<HttpResponse> parser;

    parser.parseLine("HTTP/1.1 200 OK");
    parser.parseLine(""); // No Content-Length or Transfer-Encoding

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.transferMode, TransferMode::UntilClose);
    co_return;
}

NITRO_TEST(http_parser_response_connection_close)
{
    HttpParser<HttpResponse> parser;

    parser.parseLine("HTTP/1.1 200 OK");
    parser.parseLine("Connection: close");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK(result.message.shouldClose);
    co_return;
}

NITRO_TEST(http_parser_response_http10_default_close)
{
    HttpParser<HttpResponse> parser;

    parser.parseLine("HTTP/1.0 200 OK");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK(result.message.shouldClose); // HTTP/1.0 default
    co_return;
}

NITRO_TEST(http_parser_response_unsupported_encoding)
{
    HttpParser<HttpResponse> parser;

    parser.parseLine("HTTP/1.1 200 OK");
    parser.parseLine("Transfer-Encoding: deflate");
    auto state = parser.parseLine("");

    NITRO_CHECK_EQ(state, HttpParserState::Error);
    auto result = parser.extractResult();
    NITRO_CHECK(result.error());
    NITRO_CHECK_EQ(result.errorCode, HttpParseError::UnsupportedTransferEncoding);
    co_return;
}

NITRO_TEST(http_parser_response_status_code)
{
    // normal: with reason phrase
    {
        HttpParser<HttpResponse> parser;
        parser.parseLine("HTTP/1.1 200 OK");
        parser.parseLine("");
        auto result = parser.extractResult();
        NITRO_CHECK(!result.error());
        NITRO_CHECK_EQ(result.message.statusCode, StatusCode::k200OK);
        NITRO_CHECK_EQ(result.message.statusReason, "OK");
    }
    // normal: without reason phrase
    {
        HttpParser<HttpResponse> parser;
        parser.parseLine("HTTP/1.1 204 ");
        parser.parseLine("");
        auto result = parser.extractResult();
        NITRO_CHECK(!result.error());
        NITRO_CHECK_EQ(result.message.statusCode, StatusCode::k204NoContent);
        NITRO_CHECK(result.message.statusReason.empty());
    }
    // normal: without reason phrase and trailing space
    {
        HttpParser<HttpResponse> parser;
        parser.parseLine("HTTP/1.1 204");
        parser.parseLine("");
        auto result = parser.extractResult();
        NITRO_CHECK(!result.error());
        NITRO_CHECK_EQ(result.message.statusCode, StatusCode::k204NoContent);
        NITRO_CHECK(result.message.statusReason.empty());
    }
    // non-standard code
    {
        HttpParser<HttpResponse> parser;
        parser.parseLine("HTTP/1.1 999 Custom");
        parser.parseLine("");
        auto result = parser.extractResult();
        NITRO_CHECK(!result.error());
        NITRO_CHECK_EQ(result.message.statusCode, 999);
    }
    // invalid version
    {
        HttpParser<HttpResponse> parser;
        auto state = parser.parseLine("HTTP/2.0 200 OK");
        NITRO_CHECK_EQ(state, HttpParserState::Error);
        NITRO_CHECK(parser.extractResult().error());
    }
    // missing first space
    {
        HttpParser<HttpResponse> parser;
        auto state = parser.parseLine("HTTP/1.1");
        NITRO_CHECK_EQ(state, HttpParserState::Error);
        NITRO_CHECK(parser.extractResult().error());
    }
    // non-numeric status code
    {
        HttpParser<HttpResponse> parser;
        auto state = parser.parseLine("HTTP/1.1 abc OK");
        NITRO_CHECK_EQ(state, HttpParserState::Error);
        NITRO_CHECK_EQ(parser.extractResult().errorCode, HttpParseError::MalformedRequestLine);
    }
    // status code too short (2 digits)
    {
        HttpParser<HttpResponse> parser;
        auto state = parser.parseLine("HTTP/1.1 99 OK");
        NITRO_CHECK_EQ(state, HttpParserState::Error);
        NITRO_CHECK_EQ(parser.extractResult().errorCode, HttpParseError::MalformedRequestLine);
    }
    // status code too long (4 digits) but in range
    {
        HttpParser<HttpResponse> parser;
        auto state = parser.parseLine("HTTP/1.1 1000 OK");
        NITRO_CHECK_EQ(state, HttpParserState::Error);
        NITRO_CHECK_EQ(parser.extractResult().errorCode, HttpParseError::MalformedRequestLine);
    }
    co_return;
}

NITRO_TEST(http_parser_empty_header_value)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("GET /hello HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("X-Empty:"); // Empty header value
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK(result.message.headers.contains("x-empty"));
    NITRO_CHECK_EQ(result.message.headers.at("x-empty").value(), "");
    co_return;
}

NITRO_TEST(http_parser_header_with_spaces)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("GET /hello HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("  X-Spaced  :  value with spaces  ");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK(result.message.headers.contains("x-spaced"));
    NITRO_CHECK_EQ(result.message.headers.at("x-spaced").value(), "value with spaces");
    co_return;
}

NITRO_TEST(http_parser_identity_encoding)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("POST /data HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("Transfer-Encoding: identity");
    parser.parseLine("Content-Length: 10");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.transferMode, TransferMode::ContentLength);
    NITRO_CHECK_EQ(result.message.contentLength, 10);
    co_return;
}
NITRO_TEST(http_parser_request_duplicate_content_length_same)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("POST /data HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("Content-Length: 42");
    parser.parseLine("Content-Length: 42");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.contentLength, 42);
    co_return;
}

NITRO_TEST(http_parser_request_duplicate_content_length_conflict)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("POST /data HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("Content-Length: 42");
    parser.parseLine("Content-Length: 99");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(result.error());
    NITRO_CHECK_EQ(result.errorCode, HttpParseError::AmbiguousContentLength);
    co_return;
}

NITRO_TEST(http_parser_request_cookie)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("GET /hello HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("Cookie: session=abc123; user=john");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.cookies.at("session"), "abc123");
    NITRO_CHECK_EQ(result.message.cookies.at("user"), "john");
    co_return;
}

NITRO_TEST(http_parser_response_set_cookie)
{
    HttpParser<HttpResponse> parser;

    parser.parseLine("HTTP/1.1 200 OK");
    parser.parseLine("Set-Cookie: session=abc123; Path=/; HttpOnly");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.cookies.at("session"), "abc123");
    co_return;
}

NITRO_TEST(http_parser_request_encoded_path)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("GET /user/john%20doe HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.rawPath, "/user/john%20doe");
    NITRO_CHECK_EQ(result.message.path, "/user/john doe");
    co_return;
}

NITRO_TEST(http_parser_request_keep_alive_http10_explicit)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("GET /hello HTTP/1.0");
    parser.parseLine("Host: example.com");
    parser.parseLine("Connection: keep-alive");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK(result.message.keepAlive);
    co_return;
}

NITRO_TEST(http_parser_response_keep_alive_http10_explicit)
{
    HttpParser<HttpResponse> parser;

    parser.parseLine("HTTP/1.0 200 OK");
    parser.parseLine("Connection: keep-alive");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK(!result.message.shouldClose);
    co_return;
}

NITRO_TEST(http_parser_transfer_encoding_overrides_content_length)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("POST /data HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("Content-Length: 100");
    parser.parseLine("Transfer-Encoding: chunked");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error());
    NITRO_CHECK_EQ(result.message.transferMode, TransferMode::Chunked);
    co_return;
}

NITRO_TEST(http_parser_invalid_header_no_colon)
{
    HttpParser<HttpRequest> parser;

    parser.parseLine("GET /hello HTTP/1.1");
    parser.parseLine("Host: example.com");
    parser.parseLine("InvalidHeaderWithoutColon");
    parser.parseLine("");

    auto result = parser.extractResult();
    NITRO_CHECK(!result.error()); // silently ignored
    co_return;
}

int main(int argc, char ** argv)
{
    return nitrocoro::test::run_all(argc, argv);
}
