/**
 * @file ChunkedWriter.h
 * @brief Body writer for chunked transfer encoding
 */
#pragma once
#include <nitrocoro/http/BodyWriter.h>
#include <nitrocoro/io/AnyStream.h>

namespace nitrocoro::http
{

class ChunkedWriter : public BodyWriter
{
public:
    explicit ChunkedWriter(io::AnyStreamPtr stream)
        : stream_(std::move(stream)) {}

    Task<> write(std::string_view data) override;
    Task<> end() override;

private:
    io::AnyStreamPtr stream_;
};

} // namespace nitrocoro::http
