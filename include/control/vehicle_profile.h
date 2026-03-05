#pragma once
#include <string>
#include <unordered_map>

namespace pilot::control {

enum class VehicleMode {
    ECO,
    NORMAL,
    SPORT,
    CHARGE
};

std::string  mode_to_string(VehicleMode mode);
VehicleMode  mode_from_string(const std::string& s);

struct VehicleProfile {
    VehicleMode mode;
    std::string name;
    double      max_rpm;           // RPM
    double      target_soc;        // % battery target
    double      temp_threshold;    // °C max engine temp
    double      regen_brake_level; // 0.0 – 1.0
    double      power_limit;       // kW
    std::string description;

    static VehicleProfile eco();
    static VehicleProfile normal();
    static VehicleProfile sport();
    static VehicleProfile charge();
};

} // namespace pilot::control
