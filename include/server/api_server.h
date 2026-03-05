#pragma once
#include "../sensors/sensor_base.h"
#include "../engine/condition_engine.h"
#include "../control/vehicle_controller.h"
#include <memory>
#include <string>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

// Forward declare httplib types to keep compile times reasonable
namespace httplib { class Server; }

namespace pilot::server {

class ApiServer {
public:
    explicit ApiServer(
        std::shared_ptr<pilot::control::VehicleController> controller,
        std::shared_ptr<pilot::engine::ConditionEngine>    condition_engine,
        int port = 8080
    );
    ~ApiServer();

    void start();  // non-blocking, spawns a background thread
    void stop();
    bool is_running() const { return running_.load(); }

    // Push a sensor reading to all connected SSE clients
    void push_reading(const pilot::sensors::SensorReading& reading);

    int port() const { return port_; }

private:
    void setup_routes();

    struct SseClient {
        std::queue<std::string> messages;
        std::mutex              mtx;
        std::condition_variable cv;
        std::atomic<bool>       done{false};
    };

    int  port_;
    std::shared_ptr<pilot::control::VehicleController> controller_;
    std::shared_ptr<pilot::engine::ConditionEngine>    condition_engine_;

    std::unique_ptr<httplib::Server> server_;
    std::thread                      server_thread_;
    std::atomic<bool>                running_{false};

    std::vector<std::shared_ptr<SseClient>> sse_clients_;
    std::mutex                              sinks_mutex_;
};

} // namespace pilot::server
