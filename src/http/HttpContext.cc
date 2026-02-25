/**
 * @file HttpContext.cc
 * @brief HTTP context implementations
 */
#include <nitrocoro/http/HttpContext.h>
#include <nitrocoro/http/HttpMessage.h>
#include <stdexcept>

namespace nitrocoro::http
{

template <typename MessageType>
Task<MessageType> HttpContext<MessageType>::receiveMessage()
{
    HttpParser<MessageType> parser;

    while (!parser.isHeaderComplete())
    {
        size_t pos = buffer_->find("\r\n");
        if (pos == std::string::npos)
        {
            char * writePtr = buffer_->prepareWrite(4096);
            size_t n = co_await conn_->read(writePtr, 4096);
            buffer_->commitWrite(n);
            if (n == 0)
                throw std::runtime_error("Connection closed before headers complete");
            continue;
        }

        std::string_view line = buffer_->view().substr(0, pos);
        buffer_->consume(pos + 2);
        parser.parseLine(line);
    }

    co_return parser.extractMessage();
}

// Explicit instantiations
template class HttpContext<HttpRequest>;
template class HttpContext<HttpResponse>;

} // namespace nitrocoro::http
