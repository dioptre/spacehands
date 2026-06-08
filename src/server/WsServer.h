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
    void broadcast(const GameStateData& state, const HandList& hands = {});

private:
    int         port_;
    std::thread thread_;
    std::atomic<bool> running_{false};

    // Latest JSON payload, protected by mutex
    std::mutex  mutex_;
    std::string latest_json_;
};
