#pragma once

#include <cstdio>

#define NITRO_ERROR(...) printf(__VA_ARGS__)
#define NITRO_INFO(...) printf(__VA_ARGS__)

#ifndef NDEBUG
#define NITRO_DEBUG(...) printf(__VA_ARGS__)
#define NITRO_TRACE(...) printf(__VA_ARGS__)
#else
#define NITRO_DEBUG(...) ((void)0)
#define NITRO_TRACE(...) ((void)0)
#endif
