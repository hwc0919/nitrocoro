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
    explicit StringBuffer() = default;

    // Get view of unconsumed data
    std::string_view view() const { return { buffer_.data() + readOffset_, writeOffset_ - readOffset_ }; }

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
    }

    // Get view of n bytes and consume them
    std::string_view consumeView(size_t n)
    {
        readOffset_ += n;
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

    // Get write begin position (without growing)
    char * beginWrite() { return buffer_.data() + writeOffset_; }

    // Get writable size (without growing)
    size_t writableSize() const { return buffer_.size() - writeOffset_; }

    // Commit actual written bytes
    void commitWrite(size_t len) { writeOffset_ += len; }

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
    size_t readOffset_ = 0;  // Start of unconsumed data
    size_t writeOffset_ = 0; // End of written data
};

} // namespace nitro_coro::utils
