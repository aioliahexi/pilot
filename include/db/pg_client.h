#pragma once
#include <string>
#include <memory>
#include <optional>

// Forward-declare libpq types so the header is usable without libpq installed.
struct pg_conn;
typedef struct pg_conn PGconn;

namespace pilot::db {

struct SensorEvent {
    std::string sensor_id;
    std::string sensor_name;
    double      value;
    std::string unit;
    std::string timestamp_iso; // ISO 8601
    std::string event_type;    // "reading", "condition_triggered", etc.
    std::string metadata;      // JSON string
};

class PgClient {
public:
    explicit PgClient(const std::string& connection_string);
    ~PgClient();

    bool connect();
    void disconnect();
    bool is_connected() const { return connected_; }

    bool insert_sensor_event(const SensorEvent& event);
    bool ensure_schema();

    PgClient(const PgClient&)            = delete;
    PgClient& operator=(const PgClient&) = delete;

private:
    bool        execute(const std::string& sql);
    std::string escape_string(const std::string& s);

    std::string connection_string_;
    PGconn*     conn_{nullptr};
    bool        connected_{false};
};

} // namespace pilot::db
