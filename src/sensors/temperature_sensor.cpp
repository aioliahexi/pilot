#include "sensors/temperature_sensor.h"
#include <chrono>

namespace pilot::sensors {

TemperatureSensor::TemperatureSensor(std::string id, double base_temp)
    : SensorBase(std::move(id), "TemperatureSensor"),
      base_temp_(base_temp),
      rng_(std::random_device{}()),
      noise_(0.0, 0.5) // mean=0, stddev=0.5 °C
{}

std::optional<SensorReading> TemperatureSensor::read() {
    SensorReading r;
    r.timestamp = std::chrono::system_clock::now();
    r.value     = base_temp_ + noise_(rng_);
    r.unit      = u8"°C";
    r.sensor_id = id_;
    return r;
}

} // namespace pilot::sensors
