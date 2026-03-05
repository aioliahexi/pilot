#include "engine/condition_engine.h"
#include <chrono>
#include <stdexcept>

namespace pilot::engine {

ConditionEngine::ConditionEngine(std::chrono::seconds window_duration)
    : window_duration_(window_duration)
{}

void ConditionEngine::add_condition(Condition cond) {
    std::lock_guard<std::mutex> lock(mutex_);
    condition_states_[cond.name] = false;
    conditions_.push_back(std::move(cond));
}

void ConditionEngine::add_callback(ConditionCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(std::move(cb));
}

void ConditionEngine::process(const pilot::sensors::SensorReading& reading) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Ensure a window exists for this sensor
    auto it = windows_.find(reading.sensor_id);
    if (it == windows_.end()) {
        windows_.emplace(reading.sensor_id, SlidingWindow<>(window_duration_));
        it = windows_.find(reading.sensor_id);
    }
    it->second.push(reading);

    // Evaluate all conditions targeting this sensor
    for (auto& cond : conditions_) {
        if (cond.sensor_id != reading.sensor_id) continue;

        bool triggered = evaluate(cond, reading);
        if (triggered != cond.last_state) {
            cond.last_state               = triggered;
            condition_states_[cond.name]  = triggered;
            for (auto& cb : callbacks_) {
                cb(cond.name, reading, triggered);
            }
        }
    }
}

bool ConditionEngine::evaluate(const Condition& cond, const pilot::sensors::SensorReading& reading) {
    switch (cond.type) {
        case ConditionType::THRESHOLD_ABOVE:
            return reading.value > cond.threshold;

        case ConditionType::THRESHOLD_BELOW:
            return reading.value < cond.threshold;

        case ConditionType::AVG_ABOVE: {
            auto it = windows_.find(reading.sensor_id);
            if (it == windows_.end()) return false;
            auto s = it->second.stats();
            return s && (s->avg_val > cond.threshold);
        }

        case ConditionType::AVG_BELOW: {
            auto it = windows_.find(reading.sensor_id);
            if (it == windows_.end()) return false;
            auto s = it->second.stats();
            return s && (s->avg_val < cond.threshold);
        }

        case ConditionType::RATE_OF_CHANGE_ABOVE:
        case ConditionType::RATE_OF_CHANGE_BELOW: {
            auto it = windows_.find(reading.sensor_id);
            if (it == windows_.end()) return false;
            const auto& data = it->second.data();
            if (data.size() < 2) return false;

            const auto& prev = data[data.size() - 2];
            double dt = std::chrono::duration<double>(reading.timestamp - prev.timestamp).count();
            if (dt <= 0.0) return false;
            double rate = (reading.value - prev.value) / dt;

            if (cond.type == ConditionType::RATE_OF_CHANGE_ABOVE)
                return rate > cond.threshold;
            else
                return rate < cond.threshold;
        }
    }
    return false;
}

std::optional<WindowStats> ConditionEngine::get_stats(const std::string& sensor_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = windows_.find(sensor_id);
    if (it == windows_.end()) return std::nullopt;
    return it->second.stats();
}

} // namespace pilot::engine
