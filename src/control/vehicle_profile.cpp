#include "control/vehicle_profile.h"
#include <stdexcept>

namespace pilot::control {

std::string mode_to_string(VehicleMode mode) {
    switch (mode) {
        case VehicleMode::ECO:    return "ECO";
        case VehicleMode::NORMAL: return "NORMAL";
        case VehicleMode::SPORT:  return "SPORT";
        case VehicleMode::CHARGE: return "CHARGE";
    }
    return "UNKNOWN";
}

VehicleMode mode_from_string(const std::string& s) {
    if (s == "ECO")    return VehicleMode::ECO;
    if (s == "NORMAL") return VehicleMode::NORMAL;
    if (s == "SPORT")  return VehicleMode::SPORT;
    if (s == "CHARGE") return VehicleMode::CHARGE;
    throw std::invalid_argument("Unknown vehicle mode: " + s);
}

VehicleProfile VehicleProfile::eco() {
    return VehicleProfile{
        VehicleMode::ECO,
        "ECO",
        3000.0,
        80.0,
        80.0,
        0.8,
        50.0,
        "Energy-saving mode with reduced power output and high regenerative braking."
    };
}

VehicleProfile VehicleProfile::normal() {
    return VehicleProfile{
        VehicleMode::NORMAL,
        "NORMAL",
        5000.0,
        60.0,
        90.0,
        0.5,
        100.0,
        "Balanced performance for everyday driving."
    };
}

VehicleProfile VehicleProfile::sport() {
    return VehicleProfile{
        VehicleMode::SPORT,
        "SPORT",
        8000.0,
        40.0,
        100.0,
        0.2,
        200.0,
        "Maximum performance with reduced regenerative braking."
    };
}

VehicleProfile VehicleProfile::charge() {
    return VehicleProfile{
        VehicleMode::CHARGE,
        "CHARGE",
        1000.0,
        100.0,
        75.0,
        1.0,
        20.0,
        "Battery charging mode with minimal power consumption."
    };
}

} // namespace pilot::control
