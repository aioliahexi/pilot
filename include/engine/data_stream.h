#pragma once
#include "ring_buffer.h"
#include "../sensors/sensor_base.h"
#include <thread>
#include <atomic>
#include <functional>
#include <vector>
#include <memory>
#include <chrono>

namespace pilot::engine {

using SensorReading   = pilot::sensors::SensorReading;
using ReadingCallback = std::function<void(const SensorReading&)>;

class DataStream {
public:
    static constexpr std::size_t BUFFER_SIZE = 1024; // must be power of 2

    explicit DataStream(std::chrono::milliseconds poll_interval = std::chrono::milliseconds(100));
    ~DataStream();

    void add_sensor(std::shared_ptr<pilot::sensors::SensorBase> sensor);
    void add_callback(ReadingCallback cb);
    void start();
    void stop();
    bool is_running() const { return running_.load(); }

private:
    void poll_loop();
    void dispatch_loop();

    std::chrono::milliseconds poll_interval_;
    std::vector<std::shared_ptr<pilot::sensors::SensorBase>> sensors_;
    std::vector<ReadingCallback>                              callbacks_;
    RingBuffer<SensorReading, BUFFER_SIZE>                    ring_buffer_;
    std::atomic<bool>                                         running_{false};
    std::thread poll_thread_;
    std::thread dispatch_thread_;
};

} // namespace pilot::engine
