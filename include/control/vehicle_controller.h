#pragma once
#include "vehicle_profile.h"
#include "../engine/condition_engine.h"
#include "../sensors/sensor_base.h"
#include <memory>
#include <functional>
#include <mutex>
#include <string>

namespace pilot::db { class PgClient; }

namespace pilot::control {

class VehicleController {
public:
    explicit VehicleController(std::shared_ptr<pilot::db::PgClient> db_client = nullptr);

    void set_profile(VehicleProfile profile);
    const VehicleProfile& current_profile() const { return current_profile_; }

    void on_condition_triggered(const std::string& condition_name,
                                const pilot::sensors::SensorReading& reading,
                                bool triggered);

    void register_with_engine(pilot::engine::ConditionEngine& engine);

    VehicleMode current_mode() const { return current_profile_.mode; }
    void        set_profile_by_mode(VehicleMode mode);

    std::string status_json() const;

private:
    void apply_profile_conditions(pilot::engine::ConditionEngine& engine);
    void log_event(const std::string& event_type, const std::string& data);

    mutable std::mutex                     mutex_;
    VehicleProfile                         current_profile_;
    std::shared_ptr<pilot::db::PgClient>   db_client_;
    pilot::engine::ConditionEngine*        engine_{nullptr};
};

} // namespace pilot::control
