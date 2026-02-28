/**
 * @file Socket.cc
 * @brief Implementation of Socket
 */
#include <nitrocoro/io/Socket.h>

#include <unistd.h>

namespace nitrocoro::io
{

Socket::~Socket() noexcept
{
    if (fd_ >= 0)
        ::close(fd_);
}

Socket & Socket::operator=(Socket && other) noexcept
{
    if (this != &other)
    {
        if (fd_ >= 0)
            ::close(fd_);
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

} // namespace nitrocoro::io
