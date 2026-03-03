/**
 * @file UntilCloseWriter.h
 * @brief Body writer for close-delimited transfer (HTTP/1.0 fallback)
 */
#pragma once
#include <nitrocoro/http/BodyWriter.h>
#include <nitrocoro/io/AnyStream.h>

namespace nitrocoro::http
{

class UntilCloseWriter : public BodyWriter
{
public:
    explicit UntilCloseWriter(io::AnyStreamPtr stream)
        : stream_(std::move(stream)) {}

    Task<> write(std::string_view data) override;
    Task<> end() override;

private:
    io::AnyStreamPtr stream_;
};

} // namespace nitrocoro::http
