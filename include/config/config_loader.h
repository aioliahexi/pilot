#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace pilot::config {

struct DeviceConfig {
    std::string id   = "uuv-001";
    std::string name = "UUV Alpha";
    std::string type = "UUV";
    std::string description = "Unmanned Underwater Vehicle main controller";
};

struct ServerConfig {
    int         port = 8080;
    std::string host = "0.0.0.0";
};

struct DatabaseConfig {
    std::string host              = "localhost";
    int         port              = 5432;
    std::string name              = "pilot_db";
    std::string user              = "pilot";
    std::string password          = "pilot";
    std::string connection_string;  // if set, overrides individual fields

    // Build a libpq connection string from individual fields
    std::string build_connection_string() const;
};

struct SensorConfig {
    int poll_interval_ms        = 200;
    int buffer_size             = 1024;
    int condition_window_seconds = 10;

    // Temperature
    std::string temp_id         = "temp_0";
    bool        temp_enabled    = true;
    double      temp_base       = 85.0;
    double      temp_noise      = 0.5;

    // IMU
    std::string imu_id          = "imu_0";
    bool        imu_enabled     = true;
    double      imu_accel_noise = 0.05;
    double      imu_gyro_noise  = 0.1;

    // GPS
    std::string gps_id          = "gps_0";
    bool        gps_enabled     = true;

    // Depth
    std::string depth_id        = "depth_0";
    bool        depth_enabled   = true;

    // DVL
    std::string dvl_id          = "dvl_0";
    bool        dvl_enabled     = true;

    // Battery SOC
    std::string soc_id          = "soc_0";
    bool        soc_enabled     = true;
    double      soc_initial     = 75.0;
    double      soc_discharge   = 0.05;
    double      soc_charge      = 0.10;

    // Thruster RPM (multiple thrusters)
    struct ThrusterConfig {
        std::string id;
        bool        enabled  = true;
        double      base_rpm = 3000.0;
    };
    std::vector<ThrusterConfig> thrusters;
};

struct ControlConfig {
    std::string default_mode        = "NORMAL";
    double      soc_low_threshold   = 10.0;
    int         main_loop_interval_ms = 100;
};

struct PilotConfig {
    DeviceConfig   device;
    ServerConfig   server;
    DatabaseConfig database;
    SensorConfig   sensors;
    ControlConfig  control;
};

// Load configuration from a JSON file.
// Falls back to defaults for any missing fields.
PilotConfig load_config(const std::string& filepath);

// Load configuration from a JSON object.
PilotConfig load_config_from_json(const nlohmann::json& j);

} // namespace pilot::config
