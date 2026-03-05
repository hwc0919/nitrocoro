#pragma once

#include <libpq-fe.h>

namespace nitrocoro::pg
{

struct PgResultWrapper
{
    PGresult * raw;

    explicit PgResultWrapper(PGresult * raw)
        : raw(raw) {}
    ~PgResultWrapper() noexcept
    {
        PQclear(raw);
    }
};

} // namespace nitrocoro::pg
