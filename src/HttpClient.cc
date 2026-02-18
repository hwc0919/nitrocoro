/**
 * @file HttpClient.cc
 * @brief HTTP client implementation
 */
#include <nitro_coro/http/HttpClient.h>
#include <sstream>
#include <stdexcept>

namespace nitro_coro::http
{

std::string_view HttpClientResponse::header(const std::string & name) const
{
    auto it = headers_.find(name);
    return it != headers_.end() ? std::string_view(it->second) : std::string_view();
}

Task<HttpClientResponse> HttpClient::get(const std::string & url)
{
    co_return co_await request("GET", url);
}

Task<HttpClientResponse> HttpClient::post(const std::string & url, const std::string & body)
{
    co_return co_await request("POST", url, body);
}

Task<HttpClientResponse> HttpClient::request(const std::string & method, const std::string & url, const std::string & body)
{
    auto parts = parseUrl(url);
    co_return co_await sendRequest(method, parts, body);
}

HttpClient::UrlParts HttpClient::parseUrl(const std::string & url)
{
    UrlParts parts;

    // Parse http://host:port/path
    size_t pos = url.find("://");
    if (pos == std::string::npos)
        throw std::invalid_argument("Invalid URL: missing protocol");

    std::string rest = url.substr(pos + 3);

    // Find path separator
    size_t pathPos = rest.find('/');
    std::string hostPort = (pathPos != std::string::npos) ? rest.substr(0, pathPos) : rest;
    parts.path = (pathPos != std::string::npos) ? rest.substr(pathPos) : "/";

    // Parse host:port
    size_t colonPos = hostPort.find(':');
    if (colonPos != std::string::npos)
    {
        parts.host = hostPort.substr(0, colonPos);
        parts.port = std::stoi(hostPort.substr(colonPos + 1));
    }
    else
    {
        parts.host = hostPort;
        parts.port = 80;
    }

    return parts;
}

Task<HttpClientResponse> HttpClient::sendRequest(const std::string & method, const UrlParts & url, const std::string & body)
{
    auto conn = co_await net::TcpConnection::connect(url.host.c_str(), url.port);

    // Build request
    std::ostringstream oss;
    oss << method << " " << url.path << " HTTP/1.1\r\n";
    oss << "Host: " << url.host << "\r\n";
    oss << "Connection: close\r\n";

    if (!body.empty())
    {
        oss << "Content-Length: " << body.size() << "\r\n";
    }

    oss << "\r\n";

    if (!body.empty())
    {
        oss << body;
    }

    std::string request = oss.str();
    co_await conn->write(request.c_str(), request.size());

    co_return co_await readResponse(conn);
}

Task<HttpClientResponse> HttpClient::readResponse(net::TcpConnectionPtr conn)
{
    HttpClientResponse response;
    char buf[4096];
    std::string buffer;

    enum class State
    {
        StatusLine,
        Headers,
        Body
    };
    State state = State::StatusLine;
    size_t contentLength = 0;
    bool hasContentLength = false;

    while (true)
    {
        ssize_t n = co_await conn->read(buf, sizeof(buf));
        if (n <= 0)
            break;

        buffer.append(buf, n);

        while (state == State::StatusLine || state == State::Headers)
        {
            size_t pos = buffer.find("\r\n");
            if (pos == std::string::npos)
                break;

            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 2);

            if (state == State::StatusLine)
            {
                // Parse "HTTP/1.1 200 OK"
                size_t sp1 = line.find(' ');
                size_t sp2 = line.find(' ', sp1 + 1);
                response.statusCode_ = std::stoi(line.substr(sp1 + 1, sp2 - sp1 - 1));
                response.statusReason_ = line.substr(sp2 + 1);
                state = State::Headers;
            }
            else if (line.empty())
            {
                // End of headers
                auto it = response.headers_.find("Content-Length");
                if (it != response.headers_.end())
                {
                    contentLength = std::stoul(it->second);
                    hasContentLength = true;
                }
                state = State::Body;
            }
            else
            {
                // Parse header
                size_t colonPos = line.find(':');
                if (colonPos != std::string::npos)
                {
                    std::string name = line.substr(0, colonPos);
                    std::string value = line.substr(colonPos + 1);
                    size_t start = value.find_first_not_of(' ');
                    if (start != std::string::npos)
                        value = value.substr(start);
                    response.headers_[name] = value;
                }
            }
        }

        if (state == State::Body)
        {
            response.body_.append(buffer);
            buffer.clear();

            if (hasContentLength && response.body_.size() >= contentLength)
            {
                response.body_.resize(contentLength);
                break;
            }
        }
    }

    co_return response;
}

} // namespace nitro_coro::http
