#pragma once
#include "sensor_base.h"
#include <random>
#include <atomic>

namespace pilot::sensors {

class RpmSensor : public SensorBase {
public:
    explicit RpmSensor(std::string id, double base_rpm = 2000.0);
    std::optional<SensorReading> read() override;
    void set_target_rpm(double rpm);

private:
    std::atomic<double> target_rpm_;
    double current_rpm_;
    std::mt19937 rng_;
    std::normal_distribution<double> noise_;
};

} // namespace pilot::sensors
