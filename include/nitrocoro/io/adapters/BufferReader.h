#pragma once

#include <nitrocoro/io/IoChannel.h>
#include <unistd.h>

namespace nitrocoro::io::adapters
{

using nitrocoro::io::IoChannel;

// BufferReader never calls enable/disableReading, it assumes that
// the reading event is enabled by user and won't be disabled.
struct BufferReader
{
    BufferReader(void * buf, size_t len)
        : buf_(buf), len_(len) {}
    size_t readLen() const { return readLen_; }

    IoChannel::IoResult read(int fd, IoChannel *)
    {
        if (!buf_ || len_ == 0)
        {
            return IoChannel::IoResult::Success;
        }

        ssize_t ret = ::read(fd, buf_, len_);
        if (ret > 0)
        {
            readLen_ = ret;
            return IoChannel::IoResult::Success;
        }
        else if (ret == 0)
        {
            return IoChannel::IoResult::Eof;
        }
        else
        {
            switch (errno)
            {
                case EAGAIN:
#if EAGAIN != EWOULDBLOCK
                case EWOULDBLOCK:
#endif
                    return IoChannel::IoResult::WouldBlock;
                case EINTR:
                    return IoChannel::IoResult::Retry;
                default:
                    return IoChannel::IoResult::Error;
            }
        }
    }

private:
    void * buf_;
    size_t len_;
    size_t readLen_{ 0 };
};

} // namespace nitrocoro::io::adapters
