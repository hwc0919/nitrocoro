/**
 * @file PgConnectionImpl.h
 * @brief Internal libpq implementation of PgConnection
 */
#pragma once

#include <nitrocoro/io/IoChannel.h>
#include <nitrocoro/pg/PgConnection.h>

#include <memory>
#include <string_view>
#include <vector>

namespace nitrocoro::pg
{

class PgConnectionImpl : public PgConnection
{
    friend class PgConnection;
    struct PgConnWrapper;

public:
    PgConnectionImpl(std::shared_ptr<PgConnWrapper> conn, std::unique_ptr<io::IoChannel> channel);
    ~PgConnectionImpl() override = default;

    PgConnectionImpl(const PgConnectionImpl &) = delete;
    PgConnectionImpl & operator=(const PgConnectionImpl &) = delete;
    PgConnectionImpl(PgConnectionImpl &&) = delete;
    PgConnectionImpl & operator=(PgConnectionImpl &&) = delete;

    Scheduler * scheduler() const override;
    bool isAlive() const override;

    Task<PgResult> query(std::string_view sql, std::vector<PgValue> params = {}) override;
    Task<> execute(std::string_view sql, std::vector<PgValue> params = {}) override;

private:
    Task<PgResult> sendAndReceive(std::string_view sql, std::vector<PgValue> params);

    std::shared_ptr<PgConnWrapper> pgConn_;
    std::unique_ptr<io::IoChannel> channel_;
};

} // namespace nitrocoro::pg
