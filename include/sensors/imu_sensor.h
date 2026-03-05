#pragma once
#include "sensor_base.h"
#include <random>
#include <array>

namespace pilot::sensors {

struct ImuReading {
    std::chrono::system_clock::time_point timestamp;
    double ax, ay, az; // acceleration m/s^2
    double gx, gy, gz; // gyroscope deg/s
    std::string sensor_id;
};

class ImuSensor : public SensorBase {
public:
    explicit ImuSensor(std::string id);
    std::optional<SensorReading> read() override; // returns magnitude of accel as value
    std::optional<ImuReading>    read_imu();

private:
    std::mt19937 rng_;
    std::normal_distribution<double> accel_noise_;
    std::normal_distribution<double> gyro_noise_;
};

} // namespace pilot::sensors
