#include "sensors/imu_sensor.h"
#include <cmath>
#include <chrono>

namespace pilot::sensors {

ImuSensor::ImuSensor(std::string id)
    : SensorBase(std::move(id), "ImuSensor"),
      rng_(std::random_device{}()),
      accel_noise_(0.0, 0.05), // stddev 0.05 m/s^2
      gyro_noise_(0.0, 0.1)    // stddev 0.1 deg/s
{}

std::optional<ImuReading> ImuSensor::read_imu() {
    ImuReading r;
    r.timestamp = std::chrono::system_clock::now();
    r.sensor_id = id_;

    // Simulate near-gravity acceleration in z-axis (vehicle at rest or mild motion)
    r.ax = accel_noise_(rng_);
    r.ay = accel_noise_(rng_);
    r.az = 9.81 + accel_noise_(rng_);

    r.gx = gyro_noise_(rng_);
    r.gy = gyro_noise_(rng_);
    r.gz = gyro_noise_(rng_);

    return r;
}

std::optional<SensorReading> ImuSensor::read() {
    auto imu = read_imu();
    if (!imu) return std::nullopt;

    double magnitude = std::sqrt(imu->ax * imu->ax +
                                 imu->ay * imu->ay +
                                 imu->az * imu->az);
    SensorReading r;
    r.timestamp = imu->timestamp;
    r.value     = magnitude;
    r.unit      = "m/s^2";
    r.sensor_id = id_;
    return r;
}

} // namespace pilot::sensors
