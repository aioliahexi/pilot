#include "db/pg_client.h"
#include <iostream>
#include <sstream>

#ifdef PILOT_HAVE_POSTGRESQL
#  include <libpq-fe.h>
#endif

namespace pilot::db {

PgClient::PgClient(const std::string& connection_string)
    : connection_string_(connection_string)
{}

PgClient::~PgClient() {
    disconnect();
}

bool PgClient::connect() {
#ifdef PILOT_HAVE_POSTGRESQL
    conn_ = PQconnectdb(connection_string_.c_str());
    if (PQstatus(conn_) != CONNECTION_OK) {
        std::cerr << "[PgClient] Connection failed: " << PQerrorMessage(conn_) << "\n";
        PQfinish(conn_);
        conn_       = nullptr;
        connected_  = false;
        return false;
    }
    connected_ = true;
    return true;
#else
    std::cerr << "[PgClient] Built without PostgreSQL support.\n";
    return false;
#endif
}

void PgClient::disconnect() {
#ifdef PILOT_HAVE_POSTGRESQL
    if (conn_) {
        PQfinish(conn_);
        conn_      = nullptr;
        connected_ = false;
    }
#endif
}

bool PgClient::ensure_schema() {
    const std::string sql = R"(
        CREATE TABLE IF NOT EXISTS sensor_events (
            id          SERIAL PRIMARY KEY,
            sensor_id   TEXT,
            sensor_name TEXT,
            value       DOUBLE PRECISION,
            unit        TEXT,
            timestamp   TIMESTAMPTZ,
            event_type  TEXT,
            metadata    JSONB,
            created_at  TIMESTAMPTZ DEFAULT NOW()
        );
    )";
    return execute(sql);
}

bool PgClient::insert_sensor_event(const SensorEvent& event) {
    if (!connected_) return false;

    std::ostringstream sql;
    sql << "INSERT INTO sensor_events "
           "(sensor_id, sensor_name, value, unit, timestamp, event_type, metadata) VALUES ("
        << "'" << escape_string(event.sensor_id)   << "',"
        << "'" << escape_string(event.sensor_name) << "',"
        << event.value                              << ","
        << "'" << escape_string(event.unit)         << "',"
        << "'" << escape_string(event.timestamp_iso)<< "',"
        << "'" << escape_string(event.event_type)   << "',"
        << "'" << escape_string(event.metadata)     << "'"
        << ");";

    return execute(sql.str());
}

bool PgClient::execute(const std::string& sql) {
#ifdef PILOT_HAVE_POSTGRESQL
    if (!connected_ || !conn_) return false;
    PGresult* res = PQexec(conn_, sql.c_str());
    ExecStatusType status = PQresultStatus(res);
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        std::cerr << "[PgClient] Query error: " << PQerrorMessage(conn_) << "\n";
        PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
#else
    (void)sql;
    return false;
#endif
}

std::string PgClient::escape_string(const std::string& s) {
#ifdef PILOT_HAVE_POSTGRESQL
    if (!conn_) return s;
    std::string result(s.size() * 2 + 1, '\0');
    int error = 0;
    std::size_t len = PQescapeStringConn(conn_, &result[0], s.c_str(), s.size(), &error);
    if (error) return s;
    result.resize(len);
    return result;
#else
    // Simple escaping without libpq
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        if (c == '\'') result += "''";
        else           result += c;
    }
    return result;
#endif
}

} // namespace pilot::db
