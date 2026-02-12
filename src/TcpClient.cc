/**
 * @file TcpClient.cc
 * @brief Implementation of coroutine-based TCP client
 */
#include "TcpClient.h"
#include "Scheduler.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace my_coro
{

TcpClient::TcpClient() : fd_(-1)
{
}

TcpClient::~TcpClient()
{
    close();
}

struct Connector : public IoChannel::IoWriter
{
    Connector(sockaddr * addr, size_t addrLen) : addr_(addr), addrLen_(addrLen)
    {
    }

    IoChannel::IoResult write(int fd) override
    {
        if (connecting_)
        {
            // 第二次调用：检查连接结果
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
            {
                return IoChannel::IoResult::Error;
            }
            if (error == 0)
            {
                return IoChannel::IoResult::Success; // 连接成功
            }
            else if (error == EINPROGRESS || error == EALREADY)
            {
                return IoChannel::IoResult::WouldBlock; // 继续等待
            }
            else
            {
                return IoChannel::IoResult::Error; // 连接失败
            }
        }

        int ret = ::connect(fd, addr_, addrLen_);
        if (ret == 0)
        {
            return IoChannel::IoResult::Success; // Connected immediately
        }
        int lastErrno = errno;
        switch (lastErrno)
        {
            case EISCONN:
                return IoChannel::IoResult::Success;
            case EINPROGRESS:
            case EALREADY:
                connecting_ = true;
                return IoChannel::IoResult::WouldBlock;
            case EINTR:
                return IoChannel::IoResult::Retry;  // 立即重试

            default:
                return IoChannel::IoResult::Error;
        }
    }

private:
    sockaddr * addr_;
    size_t addrLen_;

    bool connecting_{ false };
};

Task<> TcpClient::connect(const char * host, int port)
{
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0)
    {
        throw std::runtime_error("Failed to create socket");
    }

    int flags = fcntl(fd_, F_GETFL, 0);
    fcntl(fd_, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);
    Connector connector((sockaddr *)&addr, sizeof(addr));

    ioChannelPtr_ = std::make_unique<IoChannel>(fd_, Scheduler::current());
    co_await ioChannelPtr_->performWrite(&connector);
}

Task<ssize_t> TcpClient::read(void * buf, size_t len)
{
    BufferReader reader(buf, len);
    co_await ioChannelPtr_->performRead(&reader);
    co_return reader.readLen();
}

Task<> TcpClient::write(const void * buf, size_t len)
{
    BufferWriter writer(buf, len);
    co_await ioChannelPtr_->performWrite(&writer);
}

void TcpClient::close()
{
    if (fd_ >= 0)
    {
        fd_ = -1;
    }
}

} // namespace my_coro
