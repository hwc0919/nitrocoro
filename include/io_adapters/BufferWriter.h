#pragma once

#include "IoChannel.h"
#include <unistd.h>

namespace my_coro
{

// BufferWriter calls enable/disableWriting only when needed, it assumes
// that user don't enable/disable write event manually.
// If situation is different, user should implement their own Writer.
struct BufferWriter
{
    BufferWriter(const void * buf, size_t len) : buf_(buf), len_(len)
    {
    }

    IoChannel::IoResult write(int fd, IoChannel * channel)
    {
        if (!buf_ || len_ == 0)
        {
            return IoChannel::IoResult::Success;
        }

        ssize_t ret = ::write(fd, static_cast<const char *>(buf_) + wroteLen_, len_ - wroteLen_);
        if (ret > 0)
        {
            wroteLen_ += ret;
            if (wroteLen_ >= static_cast<ssize_t>(len_))
            {
                channel->disableWriting();
                return IoChannel::IoResult::Success;
            }
            else
            {
                return IoChannel::IoResult::Retry;
            }
        }
        else // ret <= 0
        {
            switch (errno)
            {
                case EAGAIN:
#if EAGAIN != EWOULDBLOCK
                case EWOULDBLOCK:
#endif
                    channel->enableWriting();
                    return IoChannel::IoResult::WouldBlock;
                case EINTR:
                    return IoChannel::IoResult::Retry;
                case EPIPE:
                case ECONNRESET:
                    return IoChannel::IoResult::Disconnect;
                default:
                    return IoChannel::IoResult::Error;
            }
        }
    }

private:
    const void * buf_;
    size_t len_;
    ssize_t wroteLen_{ 0 };
};

} // namespace my_coro
