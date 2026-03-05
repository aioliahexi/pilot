#pragma once
#include <string>
#include <chrono>
#include <optional>

namespace pilot::sensors {

struct SensorReading {
    std::chrono::system_clock::time_point timestamp;
    double value;
    std::string unit;
    std::string sensor_id;
};

class SensorBase {
public:
    SensorBase(std::string id, std::string name)
        : id_(std::move(id)), name_(std::move(name)) {}
    virtual ~SensorBase() = default;

    virtual std::optional<SensorReading> read() = 0;

    const std::string& id()   const { return id_; }
    const std::string& name() const { return name_; }

protected:
    std::string id_;
    std::string name_;
};

} // namespace pilot::sensors
