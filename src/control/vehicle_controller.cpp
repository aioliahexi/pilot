#include "control/vehicle_controller.h"
#include "db/pg_client.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace pilot::control {

VehicleController::VehicleController(std::shared_ptr<pilot::db::PgClient> db_client)
    : current_profile_(VehicleProfile::normal()),
      db_client_(std::move(db_client))
{}

void VehicleController::set_profile(VehicleProfile profile) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_profile_ = std::move(profile);
    if (engine_) {
        apply_profile_conditions(*engine_);
    }
    log_event("profile_changed", mode_to_string(current_profile_.mode));
}

void VehicleController::set_profile_by_mode(VehicleMode mode) {
    switch (mode) {
        case VehicleMode::ECO:    set_profile(VehicleProfile::eco());    break;
        case VehicleMode::NORMAL: set_profile(VehicleProfile::normal()); break;
        case VehicleMode::SPORT:  set_profile(VehicleProfile::sport());  break;
        case VehicleMode::CHARGE: set_profile(VehicleProfile::charge()); break;
    }
}

void VehicleController::on_condition_triggered(const std::string& condition_name,
                                                const pilot::sensors::SensorReading& reading,
                                                bool triggered) {
    if (!triggered) return;

    std::string event_data = condition_name + ": " + std::to_string(reading.value) + reading.unit;
    log_event("condition_triggered", event_data);

    // Automatic safety responses
    if (condition_name == "temp_critical" && triggered) {
        std::cerr << "[VehicleController] Critical temperature! Switching to ECO mode.\n";
        set_profile_by_mode(VehicleMode::ECO);
    } else if (condition_name == "soc_low" && triggered) {
        std::cerr << "[VehicleController] Low SOC! Switching to CHARGE mode.\n";
        set_profile_by_mode(VehicleMode::CHARGE);
    }
}

void VehicleController::register_with_engine(pilot::engine::ConditionEngine& engine) {
    std::lock_guard<std::mutex> lock(mutex_);
    engine_ = &engine;
    apply_profile_conditions(engine);

    engine.add_callback([this](const std::string& name,
                               const pilot::sensors::SensorReading& reading,
                               bool triggered) {
        on_condition_triggered(name, reading, triggered);
    });
}

void VehicleController::apply_profile_conditions(pilot::engine::ConditionEngine& engine) {
    // Temperature critical condition
    engine.add_condition(pilot::engine::Condition{
        "temp_critical",
        "temp_0",
        pilot::engine::ConditionType::THRESHOLD_ABOVE,
        current_profile_.temp_threshold
    });

    // RPM too high condition
    engine.add_condition(pilot::engine::Condition{
        "rpm_high",
        "rpm_0",
        pilot::engine::ConditionType::THRESHOLD_ABOVE,
        current_profile_.max_rpm
    });

    // Battery SOC low condition
    engine.add_condition(pilot::engine::Condition{
        "soc_low",
        "soc_0",
        pilot::engine::ConditionType::THRESHOLD_BELOW,
        10.0 // always warn at 10%
    });

    // Battery SOC at target (using avg)
    engine.add_condition(pilot::engine::Condition{
        "soc_at_target",
        "soc_0",
        pilot::engine::ConditionType::AVG_ABOVE,
        current_profile_.target_soc
    });
}

std::string VehicleController::status_json() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j;
    j["mode"]             = mode_to_string(current_profile_.mode);
    j["name"]             = current_profile_.name;
    j["max_rpm"]          = current_profile_.max_rpm;
    j["target_soc"]       = current_profile_.target_soc;
    j["temp_threshold"]   = current_profile_.temp_threshold;
    j["regen_brake_level"]= current_profile_.regen_brake_level;
    j["power_limit"]      = current_profile_.power_limit;
    j["description"]      = current_profile_.description;
    return j.dump();
}

void VehicleController::log_event(const std::string& event_type, const std::string& data) {
    // Always log to stderr for visibility
    std::cerr << "[VehicleController] " << event_type << ": " << data << "\n";

    if (!db_client_ || !db_client_->is_connected()) return;

    pilot::db::SensorEvent ev;
    ev.event_type  = event_type;
    ev.metadata    = data;
    ev.sensor_id   = "controller";
    ev.sensor_name = "VehicleController";
    ev.value       = 0.0;
    ev.unit        = "";

    // ISO 8601 timestamp
    auto now  = std::chrono::system_clock::now();
    auto tt   = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&tt), "%Y-%m-%dT%H:%M:%SZ");
    ev.timestamp_iso = oss.str();

    db_client_->insert_sensor_event(ev);
}

} // namespace pilot::control
