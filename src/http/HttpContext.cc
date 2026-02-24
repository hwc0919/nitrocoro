/**
 * @file HttpContext.cc
 * @brief HTTP context implementations
 */
#include <nitro_coro/http/HttpContext.h>
#include <nitro_coro/http/HttpMessage.h>
#include <stdexcept>

namespace nitro_coro::http
{

template <typename MessageType>
Task<ParsedMessage<MessageType>> HttpContext<MessageType>::receiveMessage()
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

    co_return ParsedMessage<MessageType>{
        parser.extractMessage(),
        parser.transferMode(),
        parser.contentLength()
    };
}

// Explicit instantiations
template class HttpContext<HttpRequest>;
template class HttpContext<HttpResponse>;

} // namespace nitro_coro::http
