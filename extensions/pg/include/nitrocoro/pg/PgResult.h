/**
 * @file PgResult.h
 * @brief PostgreSQL query result
 */
#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <libpq-fe.h>

namespace nitrocoro::pg
{

using PgValue = std::variant<std::monostate, bool, int64_t, double, std::string, std::vector<uint8_t>>;

class PgResult
{
public:
    explicit PgResult(PGresult * res)
        : res_(res) {}
    ~PgResult()
    {
        if (res_)
            PQclear(res_);
    }

    PgResult(const PgResult &) = delete;
    PgResult & operator=(const PgResult &) = delete;

    size_t rowCount() const { return static_cast<size_t>(PQntuples(res_)); }
    size_t colCount() const { return static_cast<size_t>(PQnfields(res_)); }
    std::string_view colName(size_t col) const { return PQfname(res_, static_cast<int>(col)); }

    PgValue get(size_t row, size_t col) const;

private:
    PGresult * res_;
};

} // namespace nitrocoro::pg
