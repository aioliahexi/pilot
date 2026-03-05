#include "sensors/temperature_sensor.h"
#include "sensors/rpm_sensor.h"
#include "sensors/battery_soc_sensor.h"
#include "sensors/imu_sensor.h"
#include "engine/data_stream.h"
#include "engine/condition_engine.h"
#include "control/vehicle_controller.h"
#include "db/pg_client.h"
#include "server/api_server.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running.store(false);
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    int         port    = 8080;
    // DB credentials loaded from environment variables or --db CLI flag.
    // Defaults use standard PostgreSQL env-var names as a fallback.
    const char* pg_host = std::getenv("PGHOST");
    const char* pg_db   = std::getenv("PGDATABASE");
    const char* pg_user = std::getenv("PGUSER");
    const char* pg_pass = std::getenv("PGPASSWORD");
    std::string db_conn;
    if (pg_host || pg_db || pg_user) {
        if (pg_host) db_conn += std::string("host=")     + pg_host + " ";
        if (pg_db)   db_conn += std::string("dbname=")   + pg_db   + " ";
        if (pg_user) db_conn += std::string("user=")     + pg_user + " ";
        if (pg_pass) db_conn += std::string("password=") + pg_pass + " ";
    }

    for (int i = 1; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "--port") {
            try {
                port = std::stoi(argv[i + 1]);
            } catch (const std::exception& ex) {
                std::cerr << "[main] Invalid port value '" << argv[i + 1] << "': " << ex.what() << "\n";
                return 1;
            }
            ++i;
        } else if (std::string(argv[i]) == "--db") {
            db_conn = argv[i + 1];
            ++i;
        }
    }

    std::cout << "[main] Starting pilot server on port " << port << "\n";

    // DB client (optional)
    auto db_client = std::make_shared<pilot::db::PgClient>(db_conn);
    bool db_ok     = db_client->connect();
    if (db_ok) {
        db_client->ensure_schema();
        std::cout << "[main] PostgreSQL connected\n";
    } else {
        std::cout << "[main] Running without PostgreSQL\n";
    }

    // Sensors
    auto temp_sensor = std::make_shared<pilot::sensors::TemperatureSensor>("temp_0", 85.0);
    auto rpm_sensor  = std::make_shared<pilot::sensors::RpmSensor>("rpm_0", 3000.0);
    auto soc_sensor  = std::make_shared<pilot::sensors::BatterySocSensor>("soc_0", 75.0);
    auto imu_sensor  = std::make_shared<pilot::sensors::ImuSensor>("imu_0");

    // Condition engine
    auto condition_engine = std::make_shared<pilot::engine::ConditionEngine>(std::chrono::seconds(10));

    // Vehicle controller
    auto controller = std::make_shared<pilot::control::VehicleController>(db_ok ? db_client : nullptr);
    controller->set_profile(pilot::control::VehicleProfile::normal());
    controller->register_with_engine(*condition_engine);

    // Data stream
    pilot::engine::DataStream data_stream(std::chrono::milliseconds(200));
    data_stream.add_sensor(temp_sensor);
    data_stream.add_sensor(rpm_sensor);
    data_stream.add_sensor(soc_sensor);
    data_stream.add_sensor(imu_sensor);

    // Data stream -> condition engine
    data_stream.add_callback([&](const pilot::sensors::SensorReading& r) {
        condition_engine->process(r);
    });

    // API server
    auto api_server = std::make_shared<pilot::server::ApiServer>(controller, condition_engine, port);

    // Data stream -> SSE push
    data_stream.add_callback([&](const pilot::sensors::SensorReading& r) {
        api_server->push_reading(r);
    });

    data_stream.start();
    api_server->start();

    std::cout << "[main] Server running at http://localhost:" << port << "\n";
    std::cout << "[main] Press Ctrl+C to stop\n";

    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[main] Shutting down...\n";
    data_stream.stop();
    api_server->stop();
    db_client->disconnect();

    return 0;
}
