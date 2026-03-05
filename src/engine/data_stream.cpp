#include "engine/data_stream.h"
#include <chrono>
#include <thread>

namespace pilot::engine {

DataStream::DataStream(std::chrono::milliseconds poll_interval)
    : poll_interval_(poll_interval)
{}

DataStream::~DataStream() {
    stop();
}

void DataStream::add_sensor(std::shared_ptr<pilot::sensors::SensorBase> sensor) {
    sensors_.push_back(std::move(sensor));
}

void DataStream::add_callback(ReadingCallback cb) {
    callbacks_.push_back(std::move(cb));
}

void DataStream::start() {
    if (running_.load()) return;
    running_.store(true);
    poll_thread_     = std::thread(&DataStream::poll_loop, this);
    dispatch_thread_ = std::thread(&DataStream::dispatch_loop, this);
}

void DataStream::stop() {
    if (!running_.load()) return;
    running_.store(false);
    if (poll_thread_.joinable())     poll_thread_.join();
    if (dispatch_thread_.joinable()) dispatch_thread_.join();
}

void DataStream::poll_loop() {
    while (running_.load()) {
        for (auto& sensor : sensors_) {
            auto reading = sensor->read();
            if (reading) {
                // Drop on full buffer rather than block
                ring_buffer_.push(*reading);
            }
        }
        std::this_thread::sleep_for(poll_interval_);
    }
}

void DataStream::dispatch_loop() {
    while (running_.load()) {
        auto item = ring_buffer_.pop();
        if (item) {
            for (auto& cb : callbacks_) {
                cb(*item);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    // Drain any remaining items after stop
    while (auto item = ring_buffer_.pop()) {
        for (auto& cb : callbacks_) {
            cb(*item);
        }
    }
}

} // namespace pilot::engine
