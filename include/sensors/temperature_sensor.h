#pragma once
#include "sensor_base.h"
#include <random>

namespace pilot::sensors {

class TemperatureSensor : public SensorBase {
public:
    explicit TemperatureSensor(std::string id, double base_temp = 25.0);
    std::optional<SensorReading> read() override;

private:
    double base_temp_;
    std::mt19937 rng_;
    std::normal_distribution<double> noise_;
};

} // namespace pilot::sensors
