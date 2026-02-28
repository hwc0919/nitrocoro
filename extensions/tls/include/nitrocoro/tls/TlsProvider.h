/**
 * @file TlsProvider.h
 * @brief Abstract TLS provider interface
 */
#pragma once

#include <string>
#include <vector>

namespace nitrocoro::tls
{

enum class FeedResult
{
    Ok,            // handshake in progress or application data received
    HandshakeDone, // handshake just completed (may also have plaintext in plainOut)
    Eof,           // peer sent close_notify
    Error          // fatal error; call lastError() for message
};

class TlsProvider
{
public:
    virtual ~TlsProvider() = default;

    TlsProvider(const TlsProvider &) = delete;
    TlsProvider & operator=(const TlsProvider &) = delete;

    /** Initiate handshake; may append initial records to encOut (e.g. ClientHello). */
    virtual void startHandshake(std::vector<char> & encOut) = 0;

    /**
     * Feed raw ciphertext from the network.
     * Decrypted plaintext is appended to plainOut.
     * Encrypted output (handshake records, session tickets) is appended to encOut.
     */
    virtual FeedResult feedEncrypted(const void * data, size_t len,
                                     std::vector<char> & plainOut,
                                     std::vector<char> & encOut)
        = 0;

    /**
     * Encrypt plaintext for sending.
     * Encrypted output is appended to encOut.
     * @return bytes of plaintext consumed, or -1 on fatal error.
     */
    virtual ssize_t sendPlain(const void * data, size_t len, std::vector<char> & encOut) = 0;

    /** Send TLS close_notify; appends the alert record to encOut. */
    virtual void close(std::vector<char> & encOut) = 0;

    virtual std::string sniName() const = 0;
    virtual std::string negotiatedAlpn() const = 0;
    virtual std::string lastError() const = 0;

protected:
    TlsProvider() = default;
};

} // namespace nitrocoro::tls
