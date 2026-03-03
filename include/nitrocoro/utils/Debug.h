#pragma once

#include <cstdio>
#include <cstring>

namespace nitrocoro
{

enum class LogLevel
{
    Trace = 0,
    Debug = 1,
    Info = 2,
    Error = 3,
    Off = 4
};

void setLogLevel(LogLevel level);
LogLevel getLogLevel();

// Helper to extract filename from path
inline const char * extractFilename(const char * path)
{
    const char * file = strrchr(path, '/');
    if (!file)
        file = strrchr(path, '\\');
    return file ? file + 1 : path;
}

} // namespace nitrocoro

#define NITRO_ERROR(...)                                                            \
    do                                                                              \
    {                                                                               \
        if (::nitrocoro::getLogLevel() <= ::nitrocoro::LogLevel::Error)             \
        {                                                                           \
            printf("[ERROR] ");                                                     \
            printf(__VA_ARGS__);                                                    \
            printf(" - %s:%d\n", ::nitrocoro::extractFilename(__FILE__), __LINE__); \
        }                                                                           \
    } while (0)

#define NITRO_INFO(...)                                                             \
    do                                                                              \
    {                                                                               \
        if (::nitrocoro::getLogLevel() <= ::nitrocoro::LogLevel::Info)              \
        {                                                                           \
            printf("[INFO] ");                                                      \
            printf(__VA_ARGS__);                                                    \
            printf(" - %s:%d\n", ::nitrocoro::extractFilename(__FILE__), __LINE__); \
        }                                                                           \
    } while (0)

#ifndef NDEBUG
#define NITRO_DEBUG(...)                                                            \
    do                                                                              \
    {                                                                               \
        if (::nitrocoro::getLogLevel() <= ::nitrocoro::LogLevel::Debug)             \
        {                                                                           \
            printf("[DEBUG] ");                                                     \
            printf(__VA_ARGS__);                                                    \
            printf(" - %s:%d\n", ::nitrocoro::extractFilename(__FILE__), __LINE__); \
        }                                                                           \
    } while (0)

#define NITRO_TRACE(...)                                                            \
    do                                                                              \
    {                                                                               \
        if (::nitrocoro::getLogLevel() <= ::nitrocoro::LogLevel::Trace)             \
        {                                                                           \
            printf("[TRACE] ");                                                     \
            printf(__VA_ARGS__);                                                    \
            printf(" - %s:%d\n", ::nitrocoro::extractFilename(__FILE__), __LINE__); \
        }                                                                           \
    } while (0)
#else
#define NITRO_DEBUG(...) ((void)0)
#define NITRO_TRACE(...) ((void)0)
#endif
