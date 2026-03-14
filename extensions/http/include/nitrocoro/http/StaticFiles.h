/**
 * @file StaticFiles.h
 * @brief Static file serving handler for HttpRouter
 */
#pragma once

#include <nitrocoro/http/HttpHandler.h>

#include <string>

namespace nitrocoro::http
{

struct StaticFilesOptions
{
    std::string index_file = "index.html"; // served when path resolves to a directory
    bool enable_etag = true;               // ETag based on file mtime + size
    int max_age = 3600;                    // Cache-Control max-age in seconds, 0 = no-cache
};

/**
 * @brief Returns a handler that serves static files from @p root.
 *
 * Intended for use with a wildcard route:
 * @code
 * server.route("/static/*path", {"GET", "HEAD"}, staticFiles("./public"));
 * @endcode
 *
 * The captured `path` param is resolved relative to @p root.
 * Path traversal attempts (e.g. `../../etc/passwd`) are rejected with 403.
 * Unknown file extensions are served as `application/octet-stream`.
 */
HttpHandlerPtr staticFiles(std::string_view root, StaticFilesOptions opts = {});

} // namespace nitrocoro::http
