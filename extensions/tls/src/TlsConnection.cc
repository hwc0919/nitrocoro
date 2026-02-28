/**
 * @file TlsConnection.cc
 * @brief TLS connection implementation using OpenSSL
 */
#include <nitrocoro/tls/TlsConnection.h>

#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

namespace nitrocoro::tls
{

TlsContextPtr TlsContext::forServer(const std::string & cert, const std::string & key)
{
    SSL_CTX * ctx = SSL_CTX_new(TLS_method());
    if (!ctx)
        throw std::runtime_error("SSL_CTX_new failed");
    if (SSL_CTX_use_certificate_chain_file(ctx, cert.c_str()) <= 0)
    {
        SSL_CTX_free(ctx);
        throw std::runtime_error("Failed to load certificate: " + cert);
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, key.c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        SSL_CTX_free(ctx);
        throw std::runtime_error("Failed to load private key: " + key);
    }
    return std::shared_ptr<TlsContext>(new TlsContext(ctx));
}

TlsConnection::TlsConnection(int fd, SSL * ssl, std::unique_ptr<IoChannel> channel)
    : fd_(fd)
    , ssl_(ssl)
    , channel_(std::move(channel))
{
}

TlsConnection::~TlsConnection()
{
    if (ssl_)
        SSL_free(ssl_);
}

Task<TlsConnectionPtr> TlsConnection::accept(int fd, TlsContextPtr ctx)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    SSL * ssl = SSL_new(ctx->ctx());
    if (!ssl)
        throw std::runtime_error("SSL_new failed");
    SSL_set_fd(ssl, fd);

    auto channel = std::make_unique<IoChannel>(fd);
    channel->enableReading();

    while (!SSL_is_init_finished(ssl))
    {
        int ret = SSL_accept(ssl);
        if (ret == 1)
            break;

        int err = SSL_get_error(ssl, ret);
        if (err == SSL_ERROR_WANT_READ)
        {
            co_await channel->performRead([ssl](int, IoChannel *) -> IoChannel::IoResult {
                int r = SSL_accept(ssl);
                if (r == 1)
                    return IoChannel::IoResult::Success;
                int e = SSL_get_error(ssl, r);
                if (e == SSL_ERROR_WANT_READ)
                    return IoChannel::IoResult::WouldBlock;
                if (e == SSL_ERROR_WANT_WRITE)
                    return IoChannel::IoResult::Success; // outer loop will call performWrite
                return IoChannel::IoResult::Error;
            });
        }
        else if (err == SSL_ERROR_WANT_WRITE)
        {
            channel->enableWriting();
            co_await channel->performWrite([ssl](int, IoChannel * c) -> IoChannel::IoResult {
                int r = SSL_accept(ssl);
                if (r == 1)
                {
                    c->disableWriting();
                    return IoChannel::IoResult::Success;
                }
                int e = SSL_get_error(ssl, r);
                if (e == SSL_ERROR_WANT_WRITE)
                    return IoChannel::IoResult::WouldBlock;
                if (e == SSL_ERROR_WANT_READ)
                {
                    c->disableWriting();
                    return IoChannel::IoResult::Success; // outer loop will call performRead
                }
                return IoChannel::IoResult::Error;
            });
        }
        else
        {
            SSL_free(ssl);
            throw std::runtime_error("TLS handshake failed");
        }
    }

    co_return std::shared_ptr<TlsConnection>(new TlsConnection(fd, ssl, std::move(channel)));
}

Task<size_t> TlsConnection::read(void * buf, size_t len)
{
    size_t readLen = 0;
    while (true)
    {
        bool needWrite = false;

        auto result = co_await channel_->performRead([&](int, IoChannel *) -> IoChannel::IoResult {
            int n = SSL_read(ssl_, buf, static_cast<int>(len));
            if (n > 0)
            {
                readLen = static_cast<size_t>(n);
                return IoChannel::IoResult::Success;
            }
            if (n == 0)
                return IoChannel::IoResult::Eof;
            int err = SSL_get_error(ssl_, n);
            if (err == SSL_ERROR_WANT_READ)
                return IoChannel::IoResult::WouldBlock;
            if (err == SSL_ERROR_WANT_WRITE)
            {
                needWrite = true;
                return IoChannel::IoResult::Success;
            }
            return IoChannel::IoResult::Error;
        });

        if (result == IoChannel::IoResult::Eof)
            co_return 0;
        if (result != IoChannel::IoResult::Success)
            throw std::runtime_error("TLS read error");

        if (needWrite)
        {
            channel_->enableWriting();
            co_await channel_->performWrite([&](int, IoChannel * c) -> IoChannel::IoResult {
                int n = SSL_read(ssl_, buf, static_cast<int>(len));
                if (n > 0)
                {
                    readLen = static_cast<size_t>(n);
                    c->disableWriting();
                    return IoChannel::IoResult::Success;
                }
                int err = SSL_get_error(ssl_, n);
                if (err == SSL_ERROR_WANT_WRITE)
                    return IoChannel::IoResult::WouldBlock;
                if (err == SSL_ERROR_WANT_READ)
                {
                    c->disableWriting();
                    return IoChannel::IoResult::Success; // outer loop re-enters performRead
                }
                return IoChannel::IoResult::Error;
            });
        }

        if (readLen > 0)
            co_return readLen;
    }
}

Task<> TlsConnection::write(const void * buf, size_t len)
{
    [[maybe_unused]] auto lock = co_await writeMutex_.scoped_lock();

    size_t written = 0;
    while (written < len)
    {
        const char * ptr = static_cast<const char *>(buf) + written;
        size_t remaining = len - written;
        size_t thisWrite = 0;
        bool needRead = false;

        channel_->enableWriting();
        auto result = co_await channel_->performWrite([&](int, IoChannel * c) -> IoChannel::IoResult {
            int n = SSL_write(ssl_, ptr, static_cast<int>(remaining));
            if (n > 0)
            {
                thisWrite = static_cast<size_t>(n);
                c->disableWriting();
                return IoChannel::IoResult::Success;
            }
            int err = SSL_get_error(ssl_, n);
            if (err == SSL_ERROR_WANT_WRITE)
                return IoChannel::IoResult::WouldBlock;
            if (err == SSL_ERROR_WANT_READ)
            {
                needRead = true;
                c->disableWriting();
                return IoChannel::IoResult::Success;
            }
            return IoChannel::IoResult::Error;
        });

        if (result != IoChannel::IoResult::Success)
            throw std::runtime_error("TLS write error");

        if (needRead)
        {
            co_await channel_->performRead([&](int, IoChannel *) -> IoChannel::IoResult {
                int n = SSL_write(ssl_, ptr, static_cast<int>(remaining));
                if (n > 0)
                {
                    thisWrite = static_cast<size_t>(n);
                    return IoChannel::IoResult::Success;
                }
                int err = SSL_get_error(ssl_, n);
                if (err == SSL_ERROR_WANT_READ)
                    return IoChannel::IoResult::WouldBlock;
                if (err == SSL_ERROR_WANT_WRITE)
                    return IoChannel::IoResult::Success; // outer loop re-enters performWrite
                return IoChannel::IoResult::Error;
            });
        }

        if (thisWrite == 0)
            throw std::runtime_error("TLS write error");
        written += thisWrite;
    }
}

Task<> TlsConnection::close()
{
    co_await channel_->scheduler()->switch_to();
    if (fd_ < 0)
        co_return;
    SSL_shutdown(ssl_);
    channel_->disableAll();
    channel_->cancelAll();
    ::close(fd_);
    fd_ = -1;
}

} // namespace nitrocoro::tls
