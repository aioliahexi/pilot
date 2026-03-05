#pragma once
#include "sensor_base.h"
#include <atomic>

namespace pilot::sensors {

class BatterySocSensor : public SensorBase {
public:
    explicit BatterySocSensor(std::string id, double initial_soc = 80.0);
    std::optional<SensorReading> read() override;
    void set_charging(bool charging);

private:
    std::atomic<double> soc_;
    std::atomic<bool>   charging_;
    double discharge_rate_; // % per read
    double charge_rate_;
};

} // namespace pilot::sensors
