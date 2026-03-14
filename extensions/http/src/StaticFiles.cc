/**
 * @file StaticFiles.cc
 * @brief Static file serving handler implementation
 */
#include <nitrocoro/http/StaticFiles.h>

#include <nitrocoro/http/HttpStream.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <string_view>
#include <sys/stat.h>
#include <unordered_map>

namespace nitrocoro::http
{

std::unordered_map<std::string, std::string> StaticFilesOptions::defaultMimeTypes()
{
    return {
        { ".html", "text/html; charset=utf-8" },
        { ".htm", "text/html; charset=utf-8" },
        { ".css", "text/css; charset=utf-8" },
        { ".js", "text/javascript; charset=utf-8" },
        { ".mjs", "text/javascript; charset=utf-8" },
        { ".json", "application/json" },
        { ".xml", "application/xml" },
        { ".txt", "text/plain; charset=utf-8" },
        { ".md", "text/markdown; charset=utf-8" },
        { ".svg", "image/svg+xml" },
        { ".png", "image/png" },
        { ".jpg", "image/jpeg" },
        { ".jpeg", "image/jpeg" },
        { ".gif", "image/gif" },
        { ".ico", "image/x-icon" },
        { ".webp", "image/webp" },
        { ".woff", "font/woff" },
        { ".woff2", "font/woff2" },
        { ".ttf", "font/ttf" },
        { ".otf", "font/otf" },
        { ".pdf", "application/pdf" },
        { ".wasm", "application/wasm" },
    };
}

std::unordered_map<std::string, std::string> StaticFilesOptions::defaultAcceptEncodings()
{
    return { { "br", "br" }, { "gzip", "gz" } };
}

namespace
{

static constexpr size_t kChunkSize = 65536;

static std::string_view mimeType(const std::string & ext,
                                 const std::unordered_map<std::string, std::string> & mime_types)
{
    auto it = mime_types.find(ext);
    return it != mime_types.end() ? std::string_view(it->second) : "application/octet-stream";
}

static std::vector<std::string_view> splitTokens(std::string_view sv)
{
    std::vector<std::string_view> tokens;
    while (!sv.empty())
    {
        auto comma = sv.find(',');
        std::string_view token = sv.substr(0, comma);
        while (!token.empty() && token.front() == ' ')
            token.remove_prefix(1);
        auto semi = token.find(';');
        token = token.substr(0, semi);
        while (!token.empty() && token.back() == ' ')
            token.remove_suffix(1);
        if (!token.empty())
            tokens.push_back(token);
        if (comma == std::string_view::npos)
            break;
        sv.remove_prefix(comma + 1);
    }
    return tokens;
}

static std::string makeETag(time_t mtime, off_t size)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "\"%lx-%lx\"",
                  static_cast<unsigned long>(mtime),
                  static_cast<unsigned long>(size));
    return buf;
}

} // namespace

HttpHandlerPtr staticFiles(std::string_view root, StaticFilesOptions opts)
{
    namespace fs = std::filesystem;

    auto canonicalRoot = fs::weakly_canonical(fs::path(root));

    return makeHttpHandler(
        [root = std::move(canonicalRoot),
         opts = std::move(opts)](HttpIncomingStream<HttpRequest> && req,
                                 HttpOutgoingStream<HttpResponse> && resp,
                                 Params params) -> Task<> {
            // Resolve path
            auto it = params.find("path");
            std::string relPath = (it != params.end()) ? it->second : "";

            // Strip leading slash if present
            if (!relPath.empty() && relPath.front() == '/')
            {
                relPath = relPath.substr(1);
            }

            fs::path filePath = fs::weakly_canonical(root / relPath);

            // Directory → index file
            if (fs::is_directory(filePath))
                filePath /= fs::path(opts.index_file).filename();

            // Path traversal check
            auto rel = filePath.lexically_relative(root);
            if (rel.empty() || *rel.begin() == "..")
            {
                resp.setStatus(403);
                co_await resp.end();
                co_return;
            }

            // Stat
            struct stat st{};
            if (::stat(filePath.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
            {
                resp.setStatus(404);
                co_await resp.end();
                co_return;
            }

            // Last-Modified / 304
            {
                struct tm tm{};
#ifdef _WIN32
                gmtime_s(&tm, &st.st_mtime);
#else
                gmtime_r(&st.st_mtime, &tm);
#endif
                char lm[32]; // strftime ensure ends with \0
                std::strftime(lm, sizeof(lm), "%a, %d %b %Y %H:%M:%S GMT", &tm);

                const auto & ims = req.getHeader(HttpHeader::NameCode::IfModifiedSince);
                if (!ims.empty() && ims == lm)
                {
                    resp.setStatus(304);
                    co_await resp.end();
                    co_return;
                }
                resp.setHeader(HttpHeader::NameCode::LastModified, lm);
            }

            // ETag / 304
            if (opts.enable_etag)
            {
                std::string etag = makeETag(st.st_mtime, st.st_size);
                const auto & ifNoneMatch = req.getHeader(HttpHeader::NameCode::IfNoneMatch);
                if (!ifNoneMatch.empty() && ifNoneMatch == etag)
                {
                    resp.setStatus(304);
                    co_await resp.end();
                    co_return;
                }
                resp.setHeader("ETag", etag);
            }

            // Pre-compressed static file: iterate Accept-Encoding in order
            fs::path actualPath = filePath;
            const auto & ae = req.getHeader(HttpHeader::NameCode::AcceptEncoding);
            if (!ae.empty() && !opts.accept_encodings.empty())
            {
                for (auto token : splitTokens(ae))
                {
                    auto encIt = opts.accept_encodings.find(std::string(token));
                    if (encIt == opts.accept_encodings.end())
                        continue;
                    fs::path candidate(filePath.string() + "." + encIt->second);
                    struct stat cst{};
                    if (::stat(candidate.c_str(), &cst) == 0 && S_ISREG(cst.st_mode))
                    {
                        actualPath = candidate;
                        st = cst;
                        resp.setHeader(HttpHeader::NameCode::ContentEncoding, std::string(token));
                        break;
                    }
                }
            }

            // Headers
            resp.setStatus(200);
            resp.setHeader(HttpHeader::NameCode::ContentType, std::string(mimeType(filePath.extension().string(), opts.mime_types)));
            resp.setHeader(HttpHeader::NameCode::ContentLength, std::to_string(st.st_size));

            std::string cacheControlValue;
            if (opts.max_age > 0)
            {
                cacheControlValue = "public, max-age=" + std::to_string(opts.max_age);
            }
            else
            {
                cacheControlValue = "no-cache";
            }
            resp.setHeader(HttpHeader::NameCode::CacheControl, std::move(cacheControlValue));

            // HEAD: headers only
            if (req.method() == methods::Head)
            {
                co_await resp.end();
                co_return;
            }

            // Stream file body
            std::unique_ptr<FILE, decltype(&std::fclose)> fp(
                std::fopen(actualPath.c_str(), "rb"), &std::fclose);
            if (!fp)
            {
                resp.setStatus(500);
                co_await resp.end();
                co_return;
            }

            // TODO: send file
            char buf[kChunkSize];
            size_t remaining = static_cast<size_t>(st.st_size);
            while (remaining > 0)
            {
                size_t n = std::fread(buf, 1, std::min(remaining, kChunkSize), fp.get());
                if (n == 0)
                    break;
                co_await resp.write(buf, n);
                remaining -= n;
            }

            // TODO: cache
            co_await resp.end();
        });
}

} // namespace nitrocoro::http
