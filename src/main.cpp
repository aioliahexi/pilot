#include "config/config_loader.h"
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

    // ------------------------------------------------------------------
    // Parse CLI arguments
    // ------------------------------------------------------------------
    std::string config_path;
    int         port_override = 0;
    std::string db_conn_override;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            try {
                port_override = std::stoi(argv[++i]);
            } catch (const std::exception& ex) {
                std::cerr << "[main] Invalid port value: " << ex.what() << "\n";
                return 1;
            }
        } else if (arg == "--db" && i + 1 < argc) {
            db_conn_override = argv[++i];
        }
    }

    // ------------------------------------------------------------------
    // Load configuration
    // ------------------------------------------------------------------
    pilot::config::PilotConfig cfg;
    if (!config_path.empty()) {
        try {
            cfg = pilot::config::load_config(config_path);
            std::cout << "[main] Loaded config from " << config_path << "\n";
        } catch (const std::exception& ex) {
            std::cerr << "[main] Failed to load config: " << ex.what() << "\n";
            return 1;
        }
    } else {
        std::cout << "[main] No config file specified, using defaults\n";
    }

    // CLI and environment overrides
    int port = port_override > 0 ? port_override : cfg.server.port;

    std::string db_conn;
    if (!db_conn_override.empty()) {
        db_conn = db_conn_override;
    } else {
        // Environment variables take precedence over config file
        const char* pg_host = std::getenv("PGHOST");
        const char* pg_db   = std::getenv("PGDATABASE");
        const char* pg_user = std::getenv("PGUSER");
        const char* pg_pass = std::getenv("PGPASSWORD");
        if (pg_host || pg_db || pg_user) {
            if (pg_host) db_conn += std::string("host=")     + pg_host + " ";
            if (pg_db)   db_conn += std::string("dbname=")   + pg_db   + " ";
            if (pg_user) db_conn += std::string("user=")     + pg_user + " ";
            if (pg_pass) db_conn += std::string("password=") + pg_pass + " ";
        } else {
            db_conn = cfg.database.build_connection_string();
        }
    }

    std::cout << "[main] Device: " << cfg.device.name
              << " (" << cfg.device.id << ")\n";
    std::cout << "[main] Starting pilot server on port " << port << "\n";

    // ------------------------------------------------------------------
    // DB client (optional)
    // ------------------------------------------------------------------
    auto db_client = std::make_shared<pilot::db::PgClient>(db_conn);
    bool db_ok     = db_client->connect();
    if (db_ok) {
        db_client->ensure_schema();
        std::cout << "[main] PostgreSQL connected\n";
    } else {
        std::cout << "[main] Running without PostgreSQL\n";
    }

    // ------------------------------------------------------------------
    // Sensors (configured from config file)
    // ------------------------------------------------------------------
    auto temp_sensor = std::make_shared<pilot::sensors::TemperatureSensor>(
        cfg.sensors.temp_id, cfg.sensors.temp_base);
    auto soc_sensor  = std::make_shared<pilot::sensors::BatterySocSensor>(
        cfg.sensors.soc_id, cfg.sensors.soc_initial);
    auto imu_sensor  = std::make_shared<pilot::sensors::ImuSensor>(
        cfg.sensors.imu_id);

    // Create thruster RPM sensors
    std::vector<std::shared_ptr<pilot::sensors::RpmSensor>> rpm_sensors;
    for (const auto& tc : cfg.sensors.thrusters) {
        if (tc.enabled) {
            auto s = std::make_shared<pilot::sensors::RpmSensor>(tc.id, tc.base_rpm);
            rpm_sensors.push_back(s);
        }
    }

    // ------------------------------------------------------------------
    // Condition engine
    // ------------------------------------------------------------------
    auto condition_engine = std::make_shared<pilot::engine::ConditionEngine>(
        std::chrono::seconds(cfg.sensors.condition_window_seconds));

    // ------------------------------------------------------------------
    // Vehicle controller
    // ------------------------------------------------------------------
    auto controller = std::make_shared<pilot::control::VehicleController>(db_ok ? db_client : nullptr);
    auto default_mode = pilot::control::mode_from_string(cfg.control.default_mode);
    controller->set_profile_by_mode(default_mode);
    controller->register_with_engine(*condition_engine);

    // ------------------------------------------------------------------
    // Data stream
    // ------------------------------------------------------------------
    pilot::engine::DataStream data_stream(
        std::chrono::milliseconds(cfg.sensors.poll_interval_ms));

    if (cfg.sensors.temp_enabled) data_stream.add_sensor(temp_sensor);
    for (auto& rs : rpm_sensors)  data_stream.add_sensor(rs);
    if (cfg.sensors.soc_enabled)  data_stream.add_sensor(soc_sensor);
    if (cfg.sensors.imu_enabled)  data_stream.add_sensor(imu_sensor);

    // Data stream -> condition engine
    data_stream.add_callback([&](const pilot::sensors::SensorReading& r) {
        condition_engine->process(r);
    });

    // ------------------------------------------------------------------
    // API server
    // ------------------------------------------------------------------
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
        std::this_thread::sleep_for(
            std::chrono::milliseconds(cfg.control.main_loop_interval_ms));
    }

    std::cout << "[main] Shutting down...\n";
    data_stream.stop();
    api_server->stop();
    db_client->disconnect();

    return 0;
}
