#include "sensors/rpm_sensor.h"
#include <chrono>

namespace pilot::sensors {

RpmSensor::RpmSensor(std::string id, double base_rpm)
    : SensorBase(std::move(id), "RpmSensor"),
      target_rpm_(base_rpm),
      current_rpm_(base_rpm),
      rng_(std::random_device{}()),
      noise_(0.0, 20.0) // mean=0, stddev=20 RPM
{}

std::optional<SensorReading> RpmSensor::read() {
    double target = target_rpm_.load(std::memory_order_relaxed);
    // Low-pass filter: current slowly tracks target
    current_rpm_ = current_rpm_ * 0.9 + target * 0.1;

    SensorReading r;
    r.timestamp = std::chrono::system_clock::now();
    r.value     = current_rpm_ + noise_(rng_);
    r.unit      = "RPM";
    r.sensor_id = id_;
    return r;
}

void RpmSensor::set_target_rpm(double rpm) {
    target_rpm_.store(rpm, std::memory_order_relaxed);
}

} // namespace pilot::sensors
