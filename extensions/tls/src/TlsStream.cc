/**
 * @file TlsStream.cc
 * @brief Coroutine TLS stream implementation
 */
#include <nitrocoro/tls/TlsStream.h>

#include <cstring>
#include <stdexcept>

namespace nitrocoro::tls
{

TlsStream::TlsStream(net::TcpConnectionPtr conn, std::unique_ptr<TlsProvider> provider)
    : conn_(std::move(conn))
    , provider_(std::move(provider))
{
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

Task<bool> TlsStream::flushEncrypted()
{
    if (encryptedOutBuf_.empty())
        co_return true;
    std::vector<char> toSend;
    toSend.swap(encryptedOutBuf_);
    co_return co_await conn_->write(toSend.data(), toSend.size()) > 0;
}

Task<FeedResult> TlsStream::feedOnce()
{
    char raw[16384];
    size_t n = co_await conn_->read(raw, sizeof(raw));
    if (n == 0)
    {
        eof_ = true;
        co_return FeedResult::Eof;
    }

    auto result = provider_->feedEncrypted(raw, n, plainBuf_, encryptedOutBuf_);

    if (!co_await flushEncrypted())
    {
        eof_ = true;
        co_return FeedResult::Eof;
    }

    if (result == FeedResult::Eof)
        eof_ = true;

    co_return result;
}

// ---------------------------------------------------------------------------
// accept / connect
// ---------------------------------------------------------------------------

Task<TlsStreamPtr> TlsStream::accept(net::TcpConnectionPtr conn, TlsContextPtr ctx)
{
    auto stream = std::shared_ptr<TlsStream>(
        new TlsStream(std::move(conn), ctx->newProvider()));

    stream->provider_->startHandshake(stream->encryptedOutBuf_);
    co_await stream->flushEncrypted();

    while (true)
    {
        auto result = co_await stream->feedOnce();
        if (result == FeedResult::HandshakeDone)
            co_return stream;
        if (result == FeedResult::Error)
            throw std::runtime_error(stream->provider_->lastError());
        if (result == FeedResult::Eof)
            throw std::runtime_error("Connection closed during TLS handshake");
    }
}

Task<TlsStreamPtr> TlsStream::connect(net::TcpConnectionPtr conn, TlsContextPtr ctx)
{
    auto stream = std::shared_ptr<TlsStream>(
        new TlsStream(std::move(conn), ctx->newProvider()));

    stream->provider_->startHandshake(stream->encryptedOutBuf_);
    co_await stream->flushEncrypted();

    while (true)
    {
        auto result = co_await stream->feedOnce();
        if (result == FeedResult::HandshakeDone)
            co_return stream;
        if (result == FeedResult::Error)
            throw std::runtime_error(stream->provider_->lastError());
        if (result == FeedResult::Eof)
            throw std::runtime_error("Connection closed during TLS handshake");
    }
}

// ---------------------------------------------------------------------------
// read
// ---------------------------------------------------------------------------

Task<size_t> TlsStream::read(void * buf, size_t len)
{
    while (plainBuf_.empty() && !eof_)
    {
        auto result = co_await feedOnce();
        if (result == FeedResult::Error)
            throw std::runtime_error(provider_->lastError());
    }

    if (plainBuf_.empty())
        co_return 0;

    size_t copy = std::min(len, plainBuf_.size());
    std::memcpy(buf, plainBuf_.data(), copy);
    plainBuf_.erase(plainBuf_.begin(),
                    plainBuf_.begin() + static_cast<ptrdiff_t>(copy));
    co_return copy;
}

// ---------------------------------------------------------------------------
// write
// ---------------------------------------------------------------------------

Task<size_t> TlsStream::write(const void * buf, size_t len)
{
    if (len == 0)
        co_return 0;
    [[maybe_unused]] auto lock = co_await writeMutex_.scoped_lock();

    size_t sent = 0;
    while (sent < len)
    {
        ssize_t n = provider_->sendPlain(
            static_cast<const char *>(buf) + sent, len - sent, encryptedOutBuf_);
        if (n < 0)
            throw std::runtime_error(provider_->lastError());
        if (n == 0)
            throw std::runtime_error("TLS write returned 0 unexpectedly");
        if (!co_await flushEncrypted())
            co_return sent; // peer closed
        sent += static_cast<size_t>(n);
    }
    co_return sent;
}

// ---------------------------------------------------------------------------
// shutdown
// ---------------------------------------------------------------------------

Task<> TlsStream::shutdown()
{
    provider_->close(encryptedOutBuf_);
    co_await flushEncrypted();
    co_await conn_->shutdown();
}

std::string TlsStream::sniName() const
{
    return provider_->sniName();
}
std::string TlsStream::negotiatedAlpn() const
{
    return provider_->negotiatedAlpn();
}

} // namespace nitrocoro::tls
