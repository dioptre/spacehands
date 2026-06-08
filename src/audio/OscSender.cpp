#include "OscSender.h"
#include <cmath>
#include <iostream>
#include <set>
#ifndef HAVE_LIBLO
typedef void* lo_address;
typedef void* lo_message;
static void lo_send_message(lo_address, const char*, lo_message) {}
static lo_message lo_message_new() { return nullptr; }
static void lo_message_add_int32(lo_message, int32_t) {}
static void lo_message_add_float(lo_message, float) {}
static void lo_message_free(lo_message) {}
static lo_address lo_address_new(const char*, const char*) { return nullptr; }
static void lo_address_free(lo_address) {}
#endif

OscSender::OscSender(const std::string& host, int port)
    : host_(host), port_(port) {}

OscSender::~OscSender() {
#ifdef HAVE_LIBLO
    if (addr_) lo_address_free(addr_);
#endif
}

bool OscSender::connect() {
    addr_ = lo_address_new(host_.c_str(), std::to_string(port_).c_str());
    if (!addr_) {
        std::cerr << "[OscSender] failed to create lo_address\n";
        return false;
    }
    std::cout << "[OscSender] targeting " << host_ << ":" << port_ << "\n";
    return true;
}

// Build and send a message using the lo_message API (avoids variadic float promotion)
static void sendMsg(lo_address addr, const char* path,
                    std::initializer_list<std::pair<char, float>> args_f,
                    std::initializer_list<int32_t> args_i = {}) {
#ifdef HAVE_LIBLO
    lo_message m = lo_message_new();
    for (int32_t v : args_i)  lo_message_add_int32(m, v);
    for (auto [t, v] : args_f) (void)t, lo_message_add_float(m, v);
    lo_send_message(addr, path, m);
    lo_message_free(m);
#endif
}

static void sendMsgI(lo_address addr, const char* path, int32_t v) {
#ifdef HAVE_LIBLO
    lo_message m = lo_message_new();
    lo_message_add_int32(m, v);
    lo_send_message(addr, path, m);
    lo_message_free(m);
#endif
}

void OscSender::sendInstrument(int id) { sendMsgI(addr_, "/instrument", id); }
void OscSender::sendNote(int pitch, float velocity) {
#ifdef HAVE_LIBLO
    lo_message m = lo_message_new();
    lo_message_add_int32(m, pitch);
    lo_message_add_int32(m, (int)(velocity * 127));
    lo_send_message(addr_, "/note", m);
    lo_message_free(m);
#endif
}
void OscSender::sendTempo(float bpm) {
#ifdef HAVE_LIBLO
    lo_message m = lo_message_new();
    lo_message_add_float(m, bpm);
    lo_send_message(addr_, "/tempo", m);
    lo_message_free(m);
#endif
}
void OscSender::sendFx(float reverb, float delay) {
#ifdef HAVE_LIBLO
    lo_message m = lo_message_new();
    lo_message_add_float(m, reverb);
    lo_message_add_float(m, delay);
    lo_message_add_float(m, 0.f);
    lo_send_message(addr_, "/fx", m);
    lo_message_free(m);
#endif
}
void OscSender::sendSpatial(float x, float y) {
#ifdef HAVE_LIBLO
    lo_message m = lo_message_new();
    lo_message_add_float(m, x);
    lo_message_add_float(m, y);
    lo_send_message(addr_, "/spatial", m);
    lo_message_free(m);
#endif
}
void OscSender::sendLevel(int level) { sendMsgI(addr_, "/level", level); }
void OscSender::sendMute(bool muted) { sendMsgI(addr_, "/mute", muted ? 1 : 0); }

void OscSender::send(const HandList& hands, const MusicParams& p, const GameStateData& state) {
    if (!addr_) return;

    // Per-hand: /hand id x y z_mm z_vel gesture_id
    std::set<int> active_buckets;
    for (const auto& h : hands) {
#ifdef HAVE_LIBLO
        lo_message m = lo_message_new();
        lo_message_add_int32(m, h.id);
        lo_message_add_float(m, h.x);
        lo_message_add_float(m, h.y);
        lo_message_add_float(m, h.z_mm);
        lo_message_add_float(m, h.z_vel);
        lo_message_add_float(m, (float)(int)h.gesture);
        lo_send_message(addr_, "/hand", m);
        lo_message_free(m);
#endif
        active_buckets.insert(h.bucket);
    }

    // /hands_end + list of active bucket indices
#ifdef HAVE_LIBLO
    lo_message me = lo_message_new();
    for (int b : active_buckets) lo_message_add_int32(me, b);
    lo_send_message(addr_, "/hands_end", me);
    lo_message_free(me);
#endif

    // Global params — throttled
    // Only send level on increase (not on reset to 0 when hands disappear)
    static int prev_level = -1;
    if (state.level > prev_level) {
        sendLevel(state.level);
        sendInstrument(state.level);
    }
    prev_level = state.level;
    if (first_ || std::fabs(p.tempo  - prev_.tempo)  > 1.f)  sendTempo(p.tempo);
    if (first_ || std::fabs(p.reverb - prev_.reverb) > 0.02f ||
                  std::fabs(p.delay  - prev_.delay)  > 0.02f) sendFx(p.reverb, p.delay);
    if (first_ || std::fabs(p.pan    - prev_.pan)    > 0.05f) sendSpatial(p.pan, 0.f);
    if (first_ || p.muted != prev_.muted)                      sendMute(p.muted);
    if (!p.muted) {
        int pitch = (int)std::round(p.pitch);
        if (first_ || pitch != (int)std::round(prev_.pitch) ||
            std::fabs(p.velocity - prev_.velocity) > 0.05f)
            sendNote(pitch, p.velocity);
    }
    prev_  = p;
    first_ = false;
}
