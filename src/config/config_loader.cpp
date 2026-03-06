#include "config/config_loader.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace pilot::config {

std::string DatabaseConfig::build_connection_string() const {
    if (!connection_string.empty()) {
        return connection_string;
    }
    std::string conn;
    if (!host.empty())     conn += "host=" + host + " ";
    if (port > 0)          conn += "port=" + std::to_string(port) + " ";
    if (!name.empty())     conn += "dbname=" + name + " ";
    if (!user.empty())     conn += "user=" + user + " ";
    if (!password.empty()) conn += "password=" + password + " ";
    return conn;
}

PilotConfig load_config(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        throw std::runtime_error("Cannot open config file: " + filepath);
    }
    nlohmann::json j;
    ifs >> j;
    return load_config_from_json(j);
}

PilotConfig load_config_from_json(const nlohmann::json& j) {
    PilotConfig cfg;

    // Device
    if (j.contains("device")) {
        auto& d = j["device"];
        if (d.contains("id"))          cfg.device.id          = d["id"].get<std::string>();
        if (d.contains("name"))        cfg.device.name        = d["name"].get<std::string>();
        if (d.contains("type"))        cfg.device.type        = d["type"].get<std::string>();
        if (d.contains("description")) cfg.device.description = d["description"].get<std::string>();
    }

    // Server
    if (j.contains("server")) {
        auto& s = j["server"];
        if (s.contains("port")) cfg.server.port = s["port"].get<int>();
        if (s.contains("host")) cfg.server.host = s["host"].get<std::string>();
    }

    // Database
    if (j.contains("database")) {
        auto& db = j["database"];
        if (db.contains("host"))              cfg.database.host              = db["host"].get<std::string>();
        if (db.contains("port"))              cfg.database.port              = db["port"].get<int>();
        if (db.contains("name"))              cfg.database.name              = db["name"].get<std::string>();
        if (db.contains("user"))              cfg.database.user              = db["user"].get<std::string>();
        if (db.contains("password"))          cfg.database.password          = db["password"].get<std::string>();
        if (db.contains("connection_string")) cfg.database.connection_string = db["connection_string"].get<std::string>();
    }

    // Sensors
    if (j.contains("sensors")) {
        auto& se = j["sensors"];
        if (se.contains("poll_interval_ms"))        cfg.sensors.poll_interval_ms        = se["poll_interval_ms"].get<int>();
        if (se.contains("buffer_size"))              cfg.sensors.buffer_size             = se["buffer_size"].get<int>();
        if (se.contains("condition_window_seconds")) cfg.sensors.condition_window_seconds = se["condition_window_seconds"].get<int>();

        // Temperature
        if (se.contains("temperature")) {
            auto& t = se["temperature"];
            if (t.contains("id"))           cfg.sensors.temp_id      = t["id"].get<std::string>();
            if (t.contains("enabled"))      cfg.sensors.temp_enabled = t["enabled"].get<bool>();
            if (t.contains("base_temp"))    cfg.sensors.temp_base    = t["base_temp"].get<double>();
            if (t.contains("noise_stddev")) cfg.sensors.temp_noise   = t["noise_stddev"].get<double>();
        }

        // IMU
        if (se.contains("imu")) {
            auto& i = se["imu"];
            if (i.contains("id"))               cfg.sensors.imu_id         = i["id"].get<std::string>();
            if (i.contains("enabled"))           cfg.sensors.imu_enabled    = i["enabled"].get<bool>();
            if (i.contains("accel_noise_stddev")) cfg.sensors.imu_accel_noise = i["accel_noise_stddev"].get<double>();
            if (i.contains("gyro_noise_stddev"))  cfg.sensors.imu_gyro_noise  = i["gyro_noise_stddev"].get<double>();
        }

        // GPS
        if (se.contains("gps")) {
            auto& g = se["gps"];
            if (g.contains("id"))      cfg.sensors.gps_id      = g["id"].get<std::string>();
            if (g.contains("enabled")) cfg.sensors.gps_enabled = g["enabled"].get<bool>();
        }

        // Depth
        if (se.contains("depth")) {
            auto& dp = se["depth"];
            if (dp.contains("id"))      cfg.sensors.depth_id      = dp["id"].get<std::string>();
            if (dp.contains("enabled")) cfg.sensors.depth_enabled = dp["enabled"].get<bool>();
        }

        // DVL
        if (se.contains("dvl")) {
            auto& dv = se["dvl"];
            if (dv.contains("id"))      cfg.sensors.dvl_id      = dv["id"].get<std::string>();
            if (dv.contains("enabled")) cfg.sensors.dvl_enabled = dv["enabled"].get<bool>();
        }

        // Battery SOC
        if (se.contains("battery_soc")) {
            auto& b = se["battery_soc"];
            if (b.contains("id"))             cfg.sensors.soc_id        = b["id"].get<std::string>();
            if (b.contains("enabled"))        cfg.sensors.soc_enabled   = b["enabled"].get<bool>();
            if (b.contains("initial_soc"))    cfg.sensors.soc_initial   = b["initial_soc"].get<double>();
            if (b.contains("discharge_rate")) cfg.sensors.soc_discharge = b["discharge_rate"].get<double>();
            if (b.contains("charge_rate"))    cfg.sensors.soc_charge    = b["charge_rate"].get<double>();
        }

        // Thrusters
        if (se.contains("thrusters")) {
            cfg.sensors.thrusters.clear();
            for (auto& t : se["thrusters"]) {
                SensorConfig::ThrusterConfig tc;
                if (t.contains("id"))       tc.id       = t["id"].get<std::string>();
                if (t.contains("enabled"))  tc.enabled  = t["enabled"].get<bool>();
                if (t.contains("base_rpm")) tc.base_rpm = t["base_rpm"].get<double>();
                cfg.sensors.thrusters.push_back(tc);
            }
        }
    }

    // Control
    if (j.contains("control")) {
        auto& c = j["control"];
        if (c.contains("default_mode"))          cfg.control.default_mode          = c["default_mode"].get<std::string>();
        if (c.contains("soc_low_threshold"))     cfg.control.soc_low_threshold     = c["soc_low_threshold"].get<double>();
        if (c.contains("main_loop_interval_ms")) cfg.control.main_loop_interval_ms = c["main_loop_interval_ms"].get<int>();
    }

    // Default thrusters if none specified
    if (cfg.sensors.thrusters.empty()) {
        cfg.sensors.thrusters.push_back({"thruster_0", true, 3000.0});
        cfg.sensors.thrusters.push_back({"thruster_1", true, 3000.0});
    }

    return cfg;
}

} // namespace pilot::config
