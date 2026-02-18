/**
 * @file HttpClient.cc
 * @brief HTTP client implementation
 */
#include <nitro_coro/http/HttpClient.h>
#include <nitro_coro/net/Dns.h>
#include <sstream>
#include <stdexcept>

namespace nitro_coro::http
{

std::string_view HttpClientResponse::header(const std::string & name) const
{
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    auto it = headers_.find(lowerName);
    return it != headers_.end() ? std::string_view(it->second.value()) : std::string_view();
}

std::string_view HttpClientResponse::cookie(const std::string & name) const
{
    auto it = cookies_.find(name);
    return it != cookies_.end() ? std::string_view(it->second) : std::string_view();
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
    // Resolve hostname
    auto addresses = co_await net::resolve(url.host);
    if (addresses.empty())
        throw std::runtime_error("DNS resolution returned no addresses");

    // Try to connect to first address
    auto addr = addresses[0];
    auto conn = co_await net::TcpConnection::connect(addr.toIp().c_str(), url.port);

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
            else if (!line.empty())
            {
                // Parse header
                size_t colonPos = line.find(':');
                if (colonPos != std::string::npos)
                {
                    std::string name = line.substr(0, colonPos);
                    std::string value = line.substr(colonPos + 1);

                    HttpHeader header(std::move(name), std::move(value));

                    // Special handling for Set-Cookie header
                    if (header.name() == "set-cookie")
                    {
                        // TODO: Parse Set-Cookie header properly
                        // For now, just extract name=value
                        size_t eqPos = header.value().find('=');
                        if (eqPos != std::string::npos)
                        {
                            size_t endPos = header.value().find(';');
                            std::string cookieName = header.value().substr(0, eqPos);
                            std::string cookieValue = (endPos != std::string::npos)
                                                          ? header.value().substr(eqPos + 1, endPos - eqPos - 1)
                                                          : header.value().substr(eqPos + 1);
                            response.cookies_[cookieName] = cookieValue;
                        }
                    }
                    else
                    {
                        // Only add non-cookie headers to headers map
                        response.headers_.emplace(header.name(), std::move(header));
                    }
                }
            }
            else
            {
                // End of headers
                static const std::string contentLengthKey{ HttpHeader::codeToName(HttpHeader::NameCode::ContentLength) };
                auto it = response.headers_.find(contentLengthKey);
                if (it != response.headers_.end())
                {
                    contentLength = std::stoul(it->second.value());
                    hasContentLength = true;
                }
                state = State::Body;
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
