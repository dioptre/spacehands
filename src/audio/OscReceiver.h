#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <vector>
#include "../Types.h"

class OscReceiver {
public:
    explicit OscReceiver(int port);
    ~OscReceiver();

    bool start();
    void stop();

    // Get and clear accumulated target spawns (thread-safe)
    std::vector<TargetSpawn> popSpawns();

#ifdef HAVE_LIBLO
    void addSpawn(float x, float y, float z, int type, int hand);
    void addSpawnCustom(float x, float y, float z, int type, int hand, const std::string& instrument, int midi, float gain, float sustain);
#endif

private:
    int port_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::mutex mutex_;
    std::vector<TargetSpawn> spawns_;

#ifdef HAVE_LIBLO
    void* server_ = nullptr;
#else
    void* server_ = nullptr;
#endif
};
