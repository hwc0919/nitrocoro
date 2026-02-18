/**
 * @file HttpRequest.cc
 * @brief HTTP request parser implementation
 */
#include <nitro_coro/http/HttpRequest.h>
#include <algorithm>
#include <sstream>

namespace nitro_coro::http
{

int HttpRequest::parse(const char * data, size_t len)
{
    buffer_.append(data, len);

    while (!complete_)
    {
        if (state_ == State::RequestLine || state_ == State::Headers)
        {
            size_t pos = buffer_.find("\r\n");
            if (pos == std::string::npos)
                return len; // Need more data

            std::string line = buffer_.substr(0, pos);
            buffer_.erase(0, pos + 2);

            if (state_ == State::RequestLine)
            {
                parseRequestLine(line);
                state_ = State::Headers;
            }
            else if (line.empty())
            {
                // Empty line marks end of headers
                auto it = headers_.find("content-length");
                if (it != headers_.end())
                {
                    contentLength_ = std::stoul(it->second.value());
                    state_ = State::Body;
                }
                else
                {
                    state_ = State::Complete;
                    complete_ = true;
                }
            }
            else
            {
                parseHeader(line);
            }
        }
        else if (state_ == State::Body)
        {
            size_t needed = contentLength_ - bodyBytesRead_;
            size_t available = buffer_.size();
            size_t toRead = std::min(needed, available);

            body_.append(buffer_.data(), toRead);
            buffer_.erase(0, toRead);
            bodyBytesRead_ += toRead;

            if (bodyBytesRead_ >= contentLength_)
            {
                state_ = State::Complete;
                complete_ = true;
            }
            else
            {
                return len; // Need more data
            }
        }
    }

    return len;
}

void HttpRequest::parseRequestLine(std::string_view line)
{
    size_t pos1 = line.find(' ');
    size_t pos2 = line.find(' ', pos1 + 1);

    method_ = line.substr(0, pos1);
    std::string fullPath(line.substr(pos1 + 1, pos2 - pos1 - 1));
    version_ = line.substr(pos2 + 1);

    // Split path and query string
    size_t qpos = fullPath.find('?');
    if (qpos != std::string::npos)
    {
        path_ = fullPath.substr(0, qpos);
        parseQueryString();
    }
    else
    {
        path_ = fullPath;
    }
}

void HttpRequest::parseHeader(std::string_view line)
{
    size_t pos = line.find(':');
    if (pos == std::string::npos)
        return;

    std::string name(line.substr(0, pos));
    std::string value(line.substr(pos + 1));

    HttpHeader header(std::move(name), std::move(value));
    
    // Special handling for Cookie header
    if (header.name() == "cookie")
    {
        parseCookies(header.value());
    }
    else
    {
        // Only add non-cookie headers to headers map
        headers_.emplace(header.name(), std::move(header));
    }
}

void HttpRequest::parseQueryString()
{
    size_t qpos = path_.find('?');
    if (qpos == std::string::npos)
        return;

    std::string queryStr = path_.substr(qpos + 1);
    path_ = path_.substr(0, qpos);

    std::istringstream iss(queryStr);
    std::string pair;
    while (std::getline(iss, pair, '&'))
    {
        size_t eqPos = pair.find('=');
        if (eqPos != std::string::npos)
        {
            queries_[pair.substr(0, eqPos)] = pair.substr(eqPos + 1);
        }
    }
}

std::string_view HttpRequest::header(const std::string & name) const
{
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    auto it = headers_.find(lowerName);
    return it != headers_.end() ? std::string_view(it->second.value()) : std::string_view();
}

std::string_view HttpRequest::query(const std::string & name) const
{
    auto it = queries_.find(name);
    return it != queries_.end() ? std::string_view(it->second) : std::string_view();
}

std::string_view HttpRequest::cookie(const std::string & name) const
{
    auto it = cookies_.find(name);
    return it != cookies_.end() ? std::string_view(it->second) : std::string_view();
}

void HttpRequest::parseCookies(const std::string & cookieHeader)
{
    // TODO: Implement cookie parsing
    // Format: name1=value1; name2=value2; name3=value3
}

} // namespace nitro_coro::http
