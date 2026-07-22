#include "OscReceiver.h"
#include <iostream>

#ifdef HAVE_LIBLO
#include <lo/lo.h>

static void handle_error(int num, const char *msg, const char *path) {
    std::cerr << "[OscReceiver] liblo error " << num << " in " << (path ? path : "null") << ": " << (msg ? msg : "unknown") << "\n";
}

static int spawn_handler(const char *path, const char *types, lo_arg **argv,
                         int argc, lo_message data, void *user_data) {
    auto* self = static_cast<OscReceiver*>(user_data);
    if (!self) return 0;

    // Parse arguments: /reflex/spawn x(f) y(f) z(f) type(i) hand(i)
    float x = argv[0]->f;
    float y = argv[1]->f;
    float z = argv[2]->f;
    int type = argv[3]->i;
    int hand = argv[4]->i;

    self->addSpawn(x, y, z, type, hand);
    return 0;
}
#endif

OscReceiver::OscReceiver(int port) : port_(port) {}

OscReceiver::~OscReceiver() {
    stop();
}

bool OscReceiver::start() {
    running_ = true;
    thread_ = std::thread([this]() {
#ifdef HAVE_LIBLO
        std::string port_str = std::to_string(port_);
        server_ = lo_server_new_with_proto(port_str.c_str(), LO_UDP, handle_error);
        if (!server_) {
            std::cerr << "[OscReceiver] failed to start on UDP port " << port_ << "\n";
            return;
        }

        lo_server_add_method(static_cast<lo_server>(server_), "/reflex/spawn", "ffiii", spawn_handler, this);
        std::cout << "[OscReceiver] listening on UDP port " << port_ << " for /reflex/spawn\n";

        while (running_) {
            lo_server_recv_noblock(static_cast<lo_server>(server_), 5);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        lo_server_free(static_cast<lo_server>(server_));
        server_ = nullptr;
#else
        std::cout << "[OscReceiver] Stub mode: liblo not linked, cannot receive OSC\n";
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
#endif
    });
    return true;
}

void OscReceiver::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

std::vector<TargetSpawn> OscReceiver::popSpawns() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<TargetSpawn> result = std::move(spawns_);
    spawns_.clear();
    return result;
}

#ifdef HAVE_LIBLO
void OscReceiver::addSpawn(float x, float y, float z, int type, int hand) {
    std::lock_guard<std::mutex> lock(mutex_);
    spawns_.push_back({x, y, z, type, hand});
}
#endif
