#pragma once
#include "../Types.h"
#include <vector>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

// Broadcasts JSON state messages to all connected WebSocket clients.
class WsServer {
public:
    explicit WsServer(int port);
    void start();
    void stop();
    void broadcast(const GameStateData& state, const HandList& hands = {}, bool mirrorX = false,
                   float xMin=0.f, float xMax=1.f, float yMin=0.f, float yMax=1.f,
                   float zMin=0.f, float zMax=1.f, bool showHints = true,
                   bool showPreviewHistory = false,
                   const std::vector<int>& sequence = {});

private:
    int         port_;
    std::thread thread_;
    std::atomic<bool> running_{false};

    // Latest JSON payload, protected by mutex
    std::mutex  mutex_;
    std::string latest_json_;
};
