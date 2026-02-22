/**
 * @file ChunkedReader.cc
 * @brief Implementation of ChunkedReader
 */
#include "ChunkedReader.h"
#include <cstring>

namespace nitro_coro::http
{

Task<bool> ChunkedReader::parseChunkSize()
{
    while (true)
    {
        size_t pos = buffer_.find("\r\n");
        if (pos == std::string::npos)
        {
            char * writePtr = buffer_.prepareWrite(128);
            size_t n = co_await conn_->read(writePtr, 128);
            if (n == 0)
                co_return false;
            buffer_.commitWrite(n);
            continue;
        }

        std::string_view line = buffer_.view().substr(0, pos);
        buffer_.consume(pos + 2);
        
        currentChunkSize_ = std::stoul(std::string(line), nullptr, 16);
        currentChunkRead_ = 0;
        
        if (currentChunkSize_ == 0)
        {
            complete_ = true;
            state_ = State::Complete;
        }
        else
        {
            state_ = State::ReadData;
        }
        
        co_return true;
    }
}

Task<> ChunkedReader::skipCRLF()
{
    while (buffer_.remainSize() < 2)
    {
        char * writePtr = buffer_.prepareWrite(2);
        size_t n = co_await conn_->read(writePtr, 2);
        if (n == 0)
            co_return;
        buffer_.commitWrite(n);
    }
    buffer_.consume(2);
}

Task<std::string_view> ChunkedReader::read(size_t maxSize)
{
    if (complete_)
        co_return std::string_view();

    if (state_ == State::ReadSize)
    {
        if (!co_await parseChunkSize())
            co_return std::string_view();
        if (complete_)
            co_return std::string_view();
    }

    size_t available = buffer_.remainSize();
    size_t remaining = currentChunkSize_ - currentChunkRead_;
    
    if (available > 0)
    {
        size_t toRead = std::min({available, remaining, maxSize});
        currentChunkRead_ += toRead;
        auto result = buffer_.consumeView(toRead);
        
        if (currentChunkRead_ >= currentChunkSize_)
        {
            co_await skipCRLF();
            state_ = State::ReadSize;
        }
        
        co_return result;
    }

    size_t toRead = std::min(maxSize, remaining);
    char * writePtr = buffer_.prepareWrite(toRead);
    size_t n = co_await conn_->read(writePtr, toRead);
    buffer_.commitWrite(n);
    currentChunkRead_ += n;
    auto result = buffer_.consumeView(n);

    if (currentChunkRead_ >= currentChunkSize_)
    {
        co_await skipCRLF();
        state_ = State::ReadSize;
    }

    co_return result;
}

Task<size_t> ChunkedReader::readTo(char * buf, size_t len)
{
    if (complete_)
        co_return 0;

    if (state_ == State::ReadSize)
    {
        if (!co_await parseChunkSize())
            co_return 0;
        if (complete_)
            co_return 0;
    }

    size_t available = buffer_.remainSize();
    size_t remaining = currentChunkSize_ - currentChunkRead_;
    
    if (available > 0)
    {
        size_t toRead = std::min({available, remaining, len});
        std::memcpy(buf, buffer_.view().data(), toRead);
        buffer_.consume(toRead);
        currentChunkRead_ += toRead;
        
        if (currentChunkRead_ >= currentChunkSize_)
        {
            co_await skipCRLF();
            state_ = State::ReadSize;
        }
        
        co_return toRead;
    }

    size_t toRead = std::min(len, remaining);
    size_t n = co_await conn_->read(buf, toRead);
    currentChunkRead_ += n;

    if (currentChunkRead_ >= currentChunkSize_)
    {
        co_await skipCRLF();
        state_ = State::ReadSize;
    }

    co_return n;
}

Task<std::string_view> ChunkedReader::readAll()
{
    while (!complete_)
    {
        if (state_ == State::ReadSize)
        {
            if (!co_await parseChunkSize())
                break;
            if (complete_)
                break;
        }

        size_t remaining = currentChunkSize_ - currentChunkRead_;
        char * writePtr = buffer_.prepareWrite(remaining);
        size_t n = co_await conn_->read(writePtr, remaining);
        buffer_.commitWrite(n);
        currentChunkRead_ += n;

        if (currentChunkRead_ >= currentChunkSize_)
        {
            co_await skipCRLF();
            state_ = State::ReadSize;
        }
    }

    co_return buffer_.view();
}

} // namespace nitro_coro::http
