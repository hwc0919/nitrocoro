/**
 * @file TlsConnection.h
 * @brief TLS connection wrapping a raw fd via OpenSSL
 */
#pragma once

#include <nitrocoro/core/Mutex.h>
#include <nitrocoro/core/Task.h>
#include <nitrocoro/io/IoChannel.h>

#include <openssl/ssl.h>
#include <memory>
#include <string>

namespace nitrocoro::tls
{

using nitrocoro::Mutex;
using nitrocoro::Task;
using nitrocoro::io::IoChannel;

class TlsContext
{
public:
    static std::shared_ptr<TlsContext> forServer(const std::string & cert, const std::string & key);

    TlsContext(const TlsContext &) = delete;
    TlsContext & operator=(const TlsContext &) = delete;
    ~TlsContext() { SSL_CTX_free(ctx_); }

    SSL_CTX * ctx() const { return ctx_; }

private:
    explicit TlsContext(SSL_CTX * ctx) : ctx_(ctx) {}
    SSL_CTX * ctx_;
};

using TlsContextPtr = std::shared_ptr<TlsContext>;

class TlsConnection;
using TlsConnectionPtr = std::shared_ptr<TlsConnection>;

class TlsConnection
{
public:
    // Takes ownership of fd; performs SSL_accept (server-side handshake)
    static Task<TlsConnectionPtr> accept(int fd, TlsContextPtr ctx);

    TlsConnection(const TlsConnection &) = delete;
    TlsConnection & operator=(const TlsConnection &) = delete;
    TlsConnection(TlsConnection &&) = delete;
    TlsConnection & operator=(TlsConnection &&) = delete;

    ~TlsConnection();

    Task<size_t> read(void * buf, size_t len);
    Task<> write(const void * buf, size_t len);
    Task<> close();

    bool isOpen() const { return fd_ >= 0; }

private:
    TlsConnection(int fd, SSL * ssl, std::unique_ptr<IoChannel> channel);

    int fd_;
    SSL * ssl_;
    std::unique_ptr<IoChannel> channel_;
    Mutex writeMutex_;
};

} // namespace nitrocoro::tls
