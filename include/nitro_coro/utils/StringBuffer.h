/**
 * @file StringBuffer.h
 * @brief High-performance string buffer with zero-copy read operations
 */
#pragma once

#include <cstring>
#include <string>
#include <string_view>

namespace nitro_coro::utils
{

/**
 * @brief String buffer with read/write offsets - memory only grows, never shrinks
 */
class StringBuffer
{
public:
    explicit StringBuffer(size_t autoCompactThreshold = 8192)
        : autoCompactThreshold_(autoCompactThreshold) {}

    // Get view of unconsumed data
    std::string_view view() const { return std::string_view(buffer_.data() + readOffset_, writeOffset_ - readOffset_); }

    // Find pattern in unconsumed data
    size_t find(std::string_view pattern, size_t pos = 0) const
    {
        std::string_view unconsumed = view();
        size_t result = unconsumed.find(pattern, pos);
        return result;
    }

    // Mark n bytes as consumed
    void consume(size_t n)
    {
        readOffset_ += n;
        if (autoCompactThreshold_ > 0 && readOffset_ > autoCompactThreshold_)
            compact();
    }

    // Get view of n bytes and consume them
    std::string_view consumeView(size_t n)
    {
        readOffset_ += n;
        if (autoCompactThreshold_ > 0 && readOffset_ > autoCompactThreshold_)
            compact();
        return { buffer_.data() + readOffset_ - n, n };
    }

    // Prepare space for writing, returns pointer to write position
    char * prepareWrite(size_t len)
    {
        // TODO: improve
        if (buffer_.size() < writeOffset_ + len)
            buffer_.resize(writeOffset_ + len);
        return buffer_.data() + writeOffset_;
    }

    // Commit actual written bytes
    void commitWrite(size_t len) { writeOffset_ += len; }

    // Compact buffer by removing consumed data
    void compact()
    {
        if (readOffset_ > 0)
        {
            size_t remaining = writeOffset_ - readOffset_;
            if (remaining > 0)
                std::memmove(buffer_.data(), buffer_.data() + readOffset_, remaining);
            writeOffset_ = remaining;
            readOffset_ = 0;
        }
    }

    // Get size of unconsumed data
    size_t remainSize() const { return writeOffset_ - readOffset_; }
    bool hasRemaining() const { return readOffset_ < writeOffset_; }

    // Reset offsets (keep buffer capacity)
    void reset()
    {
        readOffset_ = 0;
        writeOffset_ = 0;
    }

private:
    std::string buffer_;
    size_t readOffset_ = 0;       // Start of unconsumed data
    size_t writeOffset_ = 0;      // End of written data
    size_t autoCompactThreshold_; // Auto compact when readOffset exceeds this (0 = disabled)
};

} // namespace nitro_coro::utils
