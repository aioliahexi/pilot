#pragma once
#include "../sensors/sensor_base.h"
#include "sliding_window.h"
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <optional>

namespace pilot::engine {

enum class ConditionType {
    THRESHOLD_ABOVE,
    THRESHOLD_BELOW,
    RATE_OF_CHANGE_ABOVE,
    RATE_OF_CHANGE_BELOW,
    AVG_ABOVE,
    AVG_BELOW
};

struct Condition {
    std::string   name;
    std::string   sensor_id;
    ConditionType type;
    double        threshold;
    bool          last_state{false};
};

using ConditionCallback = std::function<void(const std::string& condition_name,
                                             const pilot::sensors::SensorReading& reading,
                                             bool triggered)>;

class ConditionEngine {
public:
    explicit ConditionEngine(std::chrono::seconds window_duration = std::chrono::seconds(10));

    void add_condition(Condition cond);
    void add_callback(ConditionCallback cb);
    void process(const pilot::sensors::SensorReading& reading);

    std::optional<WindowStats> get_stats(const std::string& sensor_id) const;
    const std::unordered_map<std::string, bool>& condition_states() const { return condition_states_; }

private:
    bool evaluate(const Condition& cond, const pilot::sensors::SensorReading& reading);

    std::chrono::seconds                             window_duration_;
    std::unordered_map<std::string, SlidingWindow<>> windows_;
    std::vector<Condition>                           conditions_;
    std::vector<ConditionCallback>                   callbacks_;
    std::unordered_map<std::string, bool>            condition_states_;
    mutable std::mutex                               mutex_;
};

} // namespace pilot::engine
