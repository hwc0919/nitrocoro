/**
 * @file TlsPolicy.h
 * @brief TLS configuration policy
 */
#pragma once

#include <string>
#include <vector>

namespace nitrocoro::tls
{

struct TlsPolicy
{
    std::string certPath;
    std::string keyPath;
    std::string caPath;
    std::string hostname;           // client: SNI + cert validation
    std::vector<std::string> alpn;  // e.g. {"h2", "http/1.1"}
    bool validate = true;
    bool useSystemCertStore = true;

    static TlsPolicy defaultServer(std::string cert, std::string key)
    {
        TlsPolicy p;
        p.certPath = std::move(cert);
        p.keyPath = std::move(key);
        p.validate = false;
        p.useSystemCertStore = false;
        return p;
    }

    static TlsPolicy defaultClient(std::string host = "")
    {
        TlsPolicy p;
        p.hostname = std::move(host);
        p.validate = true;
        p.useSystemCertStore = true;
        return p;
    }
};

} // namespace nitrocoro::tls
