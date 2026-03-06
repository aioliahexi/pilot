#include <gtest/gtest.h>
#include "config/config_loader.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cstdio>

using namespace pilot::config;
using json = nlohmann::json;

// ---- DatabaseConfig::build_connection_string ----

TEST(ConfigLoaderTest, BuildConnectionStringFromFields) {
    DatabaseConfig db;
    db.host     = "myhost";
    db.port     = 5432;
    db.name     = "mydb";
    db.user     = "myuser";
    db.password = "mypass";
    db.connection_string = "";

    std::string conn = db.build_connection_string();
    EXPECT_NE(conn.find("host=myhost"), std::string::npos);
    EXPECT_NE(conn.find("dbname=mydb"), std::string::npos);
    EXPECT_NE(conn.find("user=myuser"), std::string::npos);
    EXPECT_NE(conn.find("password=mypass"), std::string::npos);
    EXPECT_NE(conn.find("port=5432"), std::string::npos);
}

TEST(ConfigLoaderTest, BuildConnectionStringOverride) {
    DatabaseConfig db;
    db.connection_string = "host=override dbname=test";
    EXPECT_EQ(db.build_connection_string(), "host=override dbname=test");
}

// ---- load_config_from_json ----

TEST(ConfigLoaderTest, DefaultsWhenJsonEmpty) {
    json j = json::object();
    PilotConfig cfg = load_config_from_json(j);

    EXPECT_EQ(cfg.server.port, 8080);
    EXPECT_EQ(cfg.device.id, "uuv-001");
    EXPECT_EQ(cfg.database.host, "localhost");
    EXPECT_EQ(cfg.sensors.poll_interval_ms, 200);
    EXPECT_EQ(cfg.control.default_mode, "NORMAL");
    // Default thrusters
    ASSERT_EQ(cfg.sensors.thrusters.size(), 2u);
    EXPECT_EQ(cfg.sensors.thrusters[0].id, "thruster_0");
}

TEST(ConfigLoaderTest, OverridesFromJson) {
    json j = {
        {"server",   {{"port", 9090}}},
        {"device",   {{"id", "uuv-002"}, {"name", "UUV Beta"}}},
        {"database", {{"host", "dbhost"}, {"port", 5433}, {"name", "testdb"}}},
        {"sensors",  {
            {"poll_interval_ms", 500},
            {"condition_window_seconds", 30},
            {"temperature", {{"id", "temp_1"}, {"base_temp", 42.0}}},
            {"thrusters", json::array({
                {{"id", "t0"}, {"base_rpm", 1500.0}},
                {{"id", "t1"}, {"base_rpm", 2000.0}},
                {{"id", "t2"}, {"base_rpm", 2500.0}}
            })}
        }},
        {"control",  {{"default_mode", "ECO"}, {"soc_low_threshold", 15.0}}}
    };

    PilotConfig cfg = load_config_from_json(j);

    EXPECT_EQ(cfg.server.port, 9090);
    EXPECT_EQ(cfg.device.id, "uuv-002");
    EXPECT_EQ(cfg.device.name, "UUV Beta");
    EXPECT_EQ(cfg.database.host, "dbhost");
    EXPECT_EQ(cfg.database.port, 5433);
    EXPECT_EQ(cfg.database.name, "testdb");
    EXPECT_EQ(cfg.sensors.poll_interval_ms, 500);
    EXPECT_EQ(cfg.sensors.condition_window_seconds, 30);
    EXPECT_EQ(cfg.sensors.temp_id, "temp_1");
    EXPECT_DOUBLE_EQ(cfg.sensors.temp_base, 42.0);
    ASSERT_EQ(cfg.sensors.thrusters.size(), 3u);
    EXPECT_EQ(cfg.sensors.thrusters[0].id, "t0");
    EXPECT_EQ(cfg.sensors.thrusters[2].id, "t2");
    EXPECT_DOUBLE_EQ(cfg.sensors.thrusters[1].base_rpm, 2000.0);
    EXPECT_EQ(cfg.control.default_mode, "ECO");
    EXPECT_DOUBLE_EQ(cfg.control.soc_low_threshold, 15.0);
}

TEST(ConfigLoaderTest, LoadFromFile) {
    // Write a temporary JSON config file
    auto tmpdir = std::filesystem::temp_directory_path();
    std::string tmpfile = (tmpdir / "test_pilot_config.json").string();
    {
        json j = {
            {"server", {{"port", 7777}}},
            {"device", {{"id", "test-device"}}}
        };
        std::ofstream ofs(tmpfile);
        ofs << j.dump(2);
    }

    PilotConfig cfg = load_config(tmpfile);
    EXPECT_EQ(cfg.server.port, 7777);
    EXPECT_EQ(cfg.device.id, "test-device");

    std::remove(tmpfile.c_str());
}

TEST(ConfigLoaderTest, LoadFromFileMissingThrows) {
    auto tmpdir = std::filesystem::temp_directory_path();
    std::string missing = (tmpdir / "nonexistent_pilot_config.json").string();
    EXPECT_THROW(load_config(missing), std::runtime_error);
}

TEST(ConfigLoaderTest, PartialSensorConfig) {
    json j = {
        {"sensors", {
            {"imu", {{"id", "imu_99"}, {"accel_noise_stddev", 0.02}}},
            {"battery_soc", {{"initial_soc", 50.0}}}
        }}
    };

    PilotConfig cfg = load_config_from_json(j);
    EXPECT_EQ(cfg.sensors.imu_id, "imu_99");
    EXPECT_DOUBLE_EQ(cfg.sensors.imu_accel_noise, 0.02);
    EXPECT_DOUBLE_EQ(cfg.sensors.soc_initial, 50.0);
    // Unset fields keep defaults
    EXPECT_EQ(cfg.sensors.temp_id, "temp_0");
    EXPECT_DOUBLE_EQ(cfg.sensors.temp_base, 85.0);
}
