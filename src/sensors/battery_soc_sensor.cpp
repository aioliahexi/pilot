#include "sensors/battery_soc_sensor.h"
#include <algorithm>
#include <chrono>

namespace pilot::sensors {

BatterySocSensor::BatterySocSensor(std::string id, double initial_soc)
    : SensorBase(std::move(id), "BatterySocSensor"),
      soc_(initial_soc),
      charging_(false),
      discharge_rate_(0.05), // 0.05% per read
      charge_rate_(0.1)      // 0.10% per read
{}

std::optional<SensorReading> BatterySocSensor::read() {
    double current = soc_.load(std::memory_order_relaxed);
    if (charging_.load(std::memory_order_relaxed)) {
        current = std::min(100.0, current + charge_rate_);
    } else {
        current = std::max(0.0, current - discharge_rate_);
    }
    soc_.store(current, std::memory_order_relaxed);

    SensorReading r;
    r.timestamp = std::chrono::system_clock::now();
    r.value     = current;
    r.unit      = "%";
    r.sensor_id = id_;
    return r;
}

void BatterySocSensor::set_charging(bool charging) {
    charging_.store(charging, std::memory_order_relaxed);
}

} // namespace pilot::sensors
