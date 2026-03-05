#pragma once
#include "../sensors/sensor_base.h"
#include <deque>
#include <chrono>
#include <optional>
#include <algorithm>
#include <numeric>

namespace pilot::engine {

struct WindowStats {
    double      min_val;
    double      max_val;
    double      avg_val;
    std::size_t count;
};

template<typename T = pilot::sensors::SensorReading>
class SlidingWindow {
public:
    explicit SlidingWindow(std::chrono::seconds window_duration)
        : window_duration_(window_duration) {}

    void push(const T& reading) {
        evict_old();
        data_.push_back(reading);
    }

    void evict_old() {
        auto now    = std::chrono::system_clock::now();
        auto cutoff = now - window_duration_;
        while (!data_.empty() && data_.front().timestamp < cutoff) {
            data_.pop_front();
        }
    }

    std::optional<WindowStats> stats() const {
        if (data_.empty()) return std::nullopt;
        double min_val = data_.front().value;
        double max_val = data_.front().value;
        double sum     = 0.0;
        for (const auto& r : data_) {
            min_val = std::min(min_val, r.value);
            max_val = std::max(max_val, r.value);
            sum    += r.value;
        }
        return WindowStats{min_val, max_val, sum / static_cast<double>(data_.size()), data_.size()};
    }

    const std::deque<T>& data()     const { return data_; }
    std::size_t          size()     const { return data_.size(); }
    bool                 empty()    const { return data_.empty(); }
    std::chrono::seconds window_duration() const { return window_duration_; }

private:
    std::chrono::seconds window_duration_;
    std::deque<T>        data_;
};

} // namespace pilot::engine
