#include "WsServer.h"
#include <algorithm>
#include <iostream>
#if __has_include(<nlohmann/json.hpp>)
#  include <nlohmann/json.hpp>
#else
#  include "../../third_party/json.hpp"
#endif
#include <httplib.h>

WsServer::WsServer(int port) : port_(port) {}

// Fixed normaliser — maps observed camera range to 0-1
struct AxisRange {
    float lo, hi;
    AxisRange(float l, float h) : lo(l), hi(h) {}
    float map(float v) const {
        if (hi <= lo) return 0.5f;
        return std::max(0.f, std::min(1.f, (v - lo) / (hi - lo)));
    }
};

static std::string buildJson(const GameStateData& state, const HandList& hands, bool mirrorX,
                              float xMin, float xMax, float yMin, float yMax, float zMin, float zMax,
                              bool showHints) {
    using json = nlohmann::json;
    AxisRange rx(xMin, xMax), ry(yMin, yMax), rz(zMin, zMax);

    json j;
    j["level"]      = state.level;
    j["progress"]   = state.progress;
    j["hint"]       = gestureName(state.hint);
    j["tempo"]      = state.music.tempo;
    j["num_hands"]  = (int)hands.size();
    j["fx"]         = { {"reverb", state.music.reverb} };
    j["show_hints"] = showHints;

    float cx = 0, cy = 0;
    json jarr = json::array();
    for (const auto& h : hands) {
        float rawx = rx.map(h.x);
        float tx = mirrorX ? (1.f - rawx) : rawx;
        float ty = ry.map(h.y);
        float tz = rz.map(h.z);
        cx += tx; cy += ty;
        jarr.push_back({ {"x",tx},{"y",ty},{"z",tz},{"g_id",(int)h.gesture},{"bucket",h.bucket} });
    }
    if (!hands.empty()) { cx /= hands.size(); cy /= hands.size(); }


    j["cx"]    = cx;
    j["cy"]    = cy;
    j["hands"] = jarr;
    return j.dump();
}

void WsServer::broadcast(const GameStateData& state, const HandList& hands, bool mirrorX,
                          float xMin, float xMax, float yMin, float yMax, float zMin, float zMax,
                          bool showHints) {
    std::lock_guard<std::mutex> lk(mutex_);
    latest_json_ = buildJson(state, hands, mirrorX, xMin, xMax, yMin, yMax, zMin, zMax, showHints);
}

void WsServer::start() {
    running_ = true;
    thread_ = std::thread([this]() {
        httplib::Server svr;
        svr.new_task_queue = [] { return new httplib::ThreadPool(4); };

        // True persistent SSE — pushes to each connected client at 30Hz
        svr.Get("/state", [this](const httplib::Request&, httplib::Response& res) {
            res.set_header("Content-Type",  "text/event-stream");
            res.set_header("Cache-Control", "no-cache");
            res.set_header("Connection",    "keep-alive");
            res.set_header("Access-Control-Allow-Origin", "*");

            res.set_chunked_content_provider("text/event-stream",
                [this](size_t /*offset*/, httplib::DataSink& sink) {
                    // Push at ~30Hz until client disconnects
                    while (running_) {
                        std::string data;
                        {
                            std::lock_guard<std::mutex> lk(mutex_);
                            data = latest_json_.empty() ? "{}" : latest_json_;
                        }
                        std::string msg = "data: " + data + "\n\n";
                        if (!sink.write(msg.c_str(), msg.size())) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds(33));
                    }
                    return false; // done
                });
        });

        // Plain JSON endpoint for debugging
        svr.Get("/state.json", [this](const httplib::Request&, httplib::Response& res) {
            std::lock_guard<std::mutex> lk(mutex_);
            res.set_content(latest_json_.empty() ? "{}" : latest_json_, "application/json");
            res.set_header("Access-Control-Allow-Origin", "*");
        });

        std::cout << "[WsServer] SSE streaming on port " << port_ << "\n";
        svr.listen("0.0.0.0", port_);
    });
}

void WsServer::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.detach();
}
