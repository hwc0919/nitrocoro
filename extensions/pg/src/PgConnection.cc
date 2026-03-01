/**
 * @file PgConnection.cc
 * @brief PostgreSQL async connection implementation
 */
#include <nitrocoro/pg/PgConnection.h>
#include <nitrocoro/utils/Debug.h>

#include <stdexcept>

namespace nitrocoro::pg
{

// ── PgResult ─────────────────────────────────────────────────────────────────

PgValue PgResult::get(size_t row, size_t col) const
{
    int r = static_cast<int>(row);
    int c = static_cast<int>(col);

    if (PQgetisnull(res_, r, c))
        return std::monostate{};

    const char * val = PQgetvalue(res_, r, c);
    Oid oid = PQftype(res_, c);

    switch (oid)
    {
        case 16: // bool
            return val[0] == 't';
        case 20: // int8
        case 21: // int2
        case 23: // int4
            return static_cast<int64_t>(std::stoll(val));
        case 700:  // float4
        case 701:  // float8
        case 1700: // numeric
            return std::stod(val);
        case 17: // bytea — libpq returns hex-escaped by default
        {
            size_t len = 0;
            unsigned char * decoded = PQunescapeBytea(reinterpret_cast<const unsigned char *>(val), &len);
            std::vector<uint8_t> bytes(decoded, decoded + len);
            PQfreemem(decoded);
            return bytes;
        }
        default:
            return std::string(val);
    }
}

// ── PgConnection ─────────────────────────────────────────────────────────────

PgConnection::PgConnection(std::shared_ptr<PGconn> conn, std::unique_ptr<IoChannel> channel)
    : pgConn_(std::move(conn))
    , channel_(std::move(channel))
{
}

PgConnection::~PgConnection()
{
    // channel_->cancelAll();
    channel_->disableAll();
}

Task<std::shared_ptr<PgConnection>> PgConnection::connect(std::string connStr, Scheduler * scheduler)
{
    auto pgConn = std::shared_ptr<PGconn>(PQconnectStart(connStr.c_str()), PQfinish);
    if (!pgConn)
        throw std::runtime_error("PQconnectStart: out of memory");

    if (PQstatus(pgConn.get()) == CONNECTION_BAD)
        throw std::runtime_error("PgConnection::connect: " + std::string(PQerrorMessage(pgConn.get())));

    co_await scheduler->switch_to();
    auto channel = std::make_unique<IoChannel>(PQsocket(pgConn.get()), TriggerMode::EdgeTriggered, scheduler);
    channel->enableReading();

    auto pgPollStr = [](PostgresPollingStatusType s) -> const char * {
        switch (s)
        {
            case PGRES_POLLING_READING:
                return "POLLING_READING";
            case PGRES_POLLING_WRITING:
                return "POLLING_WRITING";
            case PGRES_POLLING_OK:
                return "POLLING_OK";
            case PGRES_POLLING_FAILED:
                return "POLLING_FAILED";
            default:
                return "POLLING_UNKNOWN";
        }
    };

    PostgresPollingStatusType s = PQconnectPoll(pgConn.get());
    while (s != PGRES_POLLING_OK)
    {
        if (s == PGRES_POLLING_FAILED)
            throw std::runtime_error("PgConnection: handshake failed");

        if (s == PGRES_POLLING_WRITING)
        {
            channel->enableWriting();
            co_await channel->performWrite([&pgConn, &s, &pgPollStr](int, IoChannel *) -> IoChannel::IoResult {
                s = PQconnectPoll(pgConn.get());
                NITRO_TRACE("PgConnection: pollWrite PQconnectPoll=%s\n", pgPollStr(s));
                if (s == PGRES_POLLING_FAILED)
                    return IoChannel::IoResult::Error;
                if (s == PGRES_POLLING_WRITING)
                    return IoChannel::IoResult::NeedWrite;
                return IoChannel::IoResult::Success;
            });
            channel->disableWriting();
        }
        else
        {
            co_await channel->performRead([&pgConn, &s, &pgPollStr](int, IoChannel *) -> IoChannel::IoResult {
                s = PQconnectPoll(pgConn.get());
                NITRO_TRACE("PgConnection: pollRead PQconnectPoll=%s\n", pgPollStr(s));
                if (s == PGRES_POLLING_FAILED)
                    return IoChannel::IoResult::Error;
                if (s == PGRES_POLLING_READING)
                    return IoChannel::IoResult::NeedRead;
                return IoChannel::IoResult::Success;
            });
        }
    }
    NITRO_TRACE("PgConnection: connected (fd=%d)\n", PQsocket(pgConn.get()));
    if (PQsetnonblocking(pgConn.get(), 1) != 0)
        throw std::runtime_error("PQsetnonblocking: " + std::string(PQerrorMessage(pgConn.get())));
    co_return std::shared_ptr<PgConnection>(new PgConnection(std::move(pgConn), std::move(channel)));
}

bool PgConnection::isAlive() const
{
    return pgConn_ && PQstatus(pgConn_.get()) == CONNECTION_OK;
}

Task<std::unique_ptr<PgResult>> PgConnection::sendAndReceive(std::string_view sql, std::vector<PgValue> params)
{
    std::vector<std::string> strBufs;
    std::vector<const char *> paramValues;
    std::vector<int> paramLengths;
    std::vector<int> paramFormats;

    strBufs.reserve(params.size());
    paramValues.reserve(params.size());
    paramLengths.reserve(params.size());
    paramFormats.reserve(params.size());

    for (auto & v : params)
    {
        std::visit(
            [&](auto && arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::monostate>)
                {
                    paramValues.push_back(nullptr);
                    paramLengths.push_back(0);
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    strBufs.push_back(arg ? "t" : "f");
                    paramValues.push_back(strBufs.back().c_str());
                    paramLengths.push_back(static_cast<int>(strBufs.back().size()));
                }
                else if constexpr (std::is_same_v<T, int64_t>)
                {
                    strBufs.push_back(std::to_string(arg));
                    paramValues.push_back(strBufs.back().c_str());
                    paramLengths.push_back(static_cast<int>(strBufs.back().size()));
                }
                else if constexpr (std::is_same_v<T, double>)
                {
                    strBufs.push_back(std::to_string(arg));
                    paramValues.push_back(strBufs.back().c_str());
                    paramLengths.push_back(static_cast<int>(strBufs.back().size()));
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    paramValues.push_back(arg.c_str());
                    paramLengths.push_back(static_cast<int>(arg.size()));
                }
                else if constexpr (std::is_same_v<T, std::vector<uint8_t>>)
                {
                    // bytea: send as binary
                    paramValues.push_back(reinterpret_cast<const char *>(arg.data()));
                    paramLengths.push_back(static_cast<int>(arg.size()));
                    paramFormats.back() = 1; // binary
                }
                paramFormats.push_back(0);
            },
            v);
    }

    std::string sqlStr(sql);
    int ok = PQsendQueryParams(pgConn_.get(),
                               sqlStr.c_str(),
                               static_cast<int>(params.size()),
                               nullptr,
                               params.empty() ? nullptr : paramValues.data(),
                               params.empty() ? nullptr : paramLengths.data(),
                               params.empty() ? nullptr : paramFormats.data(),
                               0);
    if (!ok)
    {
        throw std::runtime_error(std::string("PQsendQueryParams: ") + PQerrorMessage(pgConn_.get()));
    }

    auto flushResult = co_await channel_->performWrite([this](int, IoChannel * c) {
        int r = PQflush(pgConn_.get());
        NITRO_TRACE("PgConnection: PQflush=%d\n", r);
        if (r == 0)
        {
            c->disableWriting();
            return IoChannel::IoResult::Success;
        }
        if (r > 0)
        {
            c->enableWriting();
            return IoChannel::IoResult::NeedWrite;
        }
        return IoChannel::IoResult::Error;
    });
    if (flushResult == IoChannel::IoResult::Error)
    {
        throw std::runtime_error(std::string("PQflush: ") + PQerrorMessage(pgConn_.get()));
    }
    if (flushResult != IoChannel::IoResult::Success)
    {
        throw std::runtime_error("PQflush: canceled");
    }

    PGresult * res = nullptr;
    auto readResult = co_await channel_->performRead([this, &res](int, IoChannel *) {
        if (!PQconsumeInput(pgConn_.get()))
            return IoChannel::IoResult::Error;
        NITRO_TRACE("PgConnection: PQisBusy=%d\n", PQisBusy(pgConn_.get()));
        if (PQisBusy(pgConn_.get()))
            return IoChannel::IoResult::NeedRead;
        while (PGresult * r = PQgetResult(pgConn_.get()))
        {
            if (res)
            {
                NITRO_TRACE("PgConnection: dropping extra result status=%s rows=%d\n",
                            PQresStatus(PQresultStatus(res)),
                            PQntuples(res));
                PQclear(res);
            }
            res = r;
        }
        return IoChannel::IoResult::Success;
    });
    if (readResult == IoChannel::IoResult::Error)
    {
        throw std::runtime_error(std::string("PQconsumeInput: ") + PQerrorMessage(pgConn_.get()));
    }
    if (readResult != IoChannel::IoResult::Success)
    {
        throw std::runtime_error("PgConnection: read canceled");
    }
    NITRO_TRACE("PgConnection: result received, res=%p\n", (void *)res);

    if (!res)
        throw std::runtime_error("PgConnection: no result returned");

    ExecStatusType status = PQresultStatus(res);
    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK)
    {
        std::string err = PQresultErrorMessage(res);
        PQclear(res);
        throw std::runtime_error("PgConnection query error: " + err);
    }

    co_return std::make_unique<PgResult>(res);
}

Task<std::unique_ptr<PgResult>> PgConnection::query(std::string_view sql, std::vector<PgValue> params)
{
    co_return co_await sendAndReceive(sql, std::move(params));
}

Task<> PgConnection::execute(std::string_view sql, std::vector<PgValue> params)
{
    co_await sendAndReceive(sql, std::move(params));
}

Task<> PgConnection::begin()
{
    co_await execute("BEGIN");
}

Task<> PgConnection::commit()
{
    co_await execute("COMMIT");
}

Task<> PgConnection::rollback()
{
    co_await execute("ROLLBACK");
}

} // namespace nitrocoro::pg
