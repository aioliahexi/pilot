#include "server/api_server.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace pilot::server {

// ---- helpers ---------------------------------------------------------------

static std::string iso8601(std::chrono::system_clock::time_point tp) {
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&tt), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

static void add_cors(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

// ---- ApiServer -------------------------------------------------------------

ApiServer::ApiServer(
    std::shared_ptr<pilot::control::VehicleController> controller,
    std::shared_ptr<pilot::engine::ConditionEngine>    condition_engine,
    int port)
    : port_(port),
      controller_(std::move(controller)),
      condition_engine_(std::move(condition_engine)),
      server_(std::make_unique<httplib::Server>())
{
    setup_routes();
}

ApiServer::~ApiServer() {
    stop();
}

void ApiServer::setup_routes() {
    // CORS pre-flight
    server_->Options(".*", [](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        res.status = 204;
    });

    // GET /api/status
    server_->Get("/api/status", [this](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        res.set_content(controller_->status_json(), "application/json");
    });

    // GET /api/profile
    server_->Get("/api/profile", [this](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        res.set_content(controller_->status_json(), "application/json");
    });

    // POST /api/profile  body: {"mode":"ECO"|"NORMAL"|"SPORT"|"CHARGE"}
    server_->Post("/api/profile", [this](const httplib::Request& req, httplib::Response& res) {
        add_cors(res);
        try {
            auto body = nlohmann::json::parse(req.body);
            std::string mode_str = body.at("mode").get<std::string>();
            auto mode = pilot::control::mode_from_string(mode_str);
            controller_->set_profile_by_mode(mode);
            nlohmann::json resp;
            resp["ok"]   = true;
            resp["mode"] = mode_str;
            res.set_content(resp.dump(), "application/json");
        } catch (const std::exception& ex) {
            res.status = 400;
            nlohmann::json err;
            err["ok"]    = false;
            err["error"] = ex.what();
            res.set_content(err.dump(), "application/json");
        }
    });

    // GET /api/conditions
    server_->Get("/api/conditions", [this](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        nlohmann::json j;
        for (const auto& [name, state] : condition_engine_->condition_states()) {
            j[name] = state;
        }
        res.set_content(j.dump(), "application/json");
    });

    // GET /events  – Server-Sent Events
    server_->Get("/events", [this](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        res.set_header("Cache-Control",               "no-cache");
        res.set_header("X-Accel-Buffering",            "no");
        res.set_header("Connection",                   "keep-alive");

        auto client = std::make_shared<SseClient>();
        {
            std::lock_guard<std::mutex> lock(sinks_mutex_);
            sse_clients_.push_back(client);
        }

        res.set_chunked_content_provider(
            "text/event-stream",
            [client](std::size_t /*offset*/, httplib::DataSink& sink) -> bool {
                std::unique_lock<std::mutex> lock(client->mtx);
                client->cv.wait_for(lock, std::chrono::seconds(1), [&client] {
                    return !client->messages.empty() || client->done.load();
                });
                if (client->done.load()) return false;
                while (!client->messages.empty()) {
                    const std::string& msg = client->messages.front();
                    if (!sink.write(msg.data(), msg.size())) return false;
                    client->messages.pop();
                }
                return true;
            },
            [client, this](bool /*success*/) {
                client->done.store(true);
                client->cv.notify_all();
                std::lock_guard<std::mutex> lock(sinks_mutex_);
                sse_clients_.erase(
                    std::remove_if(sse_clients_.begin(), sse_clients_.end(),
                                   [&client](const auto& c) { return c == client; }),
                    sse_clients_.end());
            });
    });

    // GET / – serve the frontend
    server_->Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_redirect("/frontend/index.html");
    });

    server_->set_mount_point("/frontend", "./frontend");
}

void ApiServer::start() {
    if (running_.load()) return;
    running_.store(true);
    server_thread_ = std::thread([this] {
        if (!server_->listen("0.0.0.0", port_)) {
            std::cerr << "[ApiServer] Failed to listen on port " << port_ << "\n";
        }
        running_.store(false);
    });
}

void ApiServer::stop() {
    if (!running_.load() && !server_thread_.joinable()) return;
    running_.store(false);
    server_->stop();
    if (server_thread_.joinable()) server_thread_.join();

    // Wake all SSE clients so they exit cleanly
    std::lock_guard<std::mutex> lock(sinks_mutex_);
    for (auto& c : sse_clients_) {
        c->done.store(true);
        c->cv.notify_all();
    }
    sse_clients_.clear();
}

void ApiServer::push_reading(const pilot::sensors::SensorReading& reading) {
    nlohmann::json j;
    j["sensor_id"] = reading.sensor_id;
    j["value"]     = reading.value;
    j["unit"]      = reading.unit;
    j["timestamp"] = iso8601(reading.timestamp);

    std::string msg = "event: sensor_data\ndata: " + j.dump() + "\n\n";

    std::lock_guard<std::mutex> lock(sinks_mutex_);
    for (auto& client : sse_clients_) {
        std::lock_guard<std::mutex> clt_lock(client->mtx);
        client->messages.push(msg);
        client->cv.notify_one();
    }
}

} // namespace pilot::server
