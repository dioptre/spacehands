#include "OscSender.h"
#include <cmath>
#include <iostream>
#include <set>
#include <unordered_map>
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

void OscSender::connectTidal(const std::string& host, int port) {
#ifdef HAVE_LIBLO
    tidal_ = lo_address_new(host.c_str(), std::to_string(port).c_str());
    if (tidal_)
        std::cout << "[OscSender] Tidal /ctrl → " << host << ":" << port << "\n";
#endif
}

// Send /ctrl sf key value to Tidal
static void sendCtrl(lo_address tidal, const char* key, float val) {
#ifdef HAVE_LIBLO
    if (!tidal) return;
    lo_message m = lo_message_new();
    lo_message_add_string(m, key);
    lo_message_add_float(m, val);
    lo_send_message(tidal, "/ctrl", m);
    lo_message_free(m);
#endif
}

static void sendCtrlStr(lo_address tidal, const char* key, const char* val) {
#ifdef HAVE_LIBLO
    if (!tidal) return;
    lo_message m = lo_message_new();
    lo_message_add_string(m, key);
    lo_message_add_string(m, val);
    lo_send_message(tidal, "/ctrl", m);
    lo_message_free(m);
#endif
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

    // Send hand data as /ctrl to Tidal at ~10Hz (not every frame)
    static int ctrl_count = 0;
    if (tidal_ && (++ctrl_count % 3 == 0)) {
        // Per-hand: hand0_x, hand0_y, hand0_z, hand1_x etc.
        for (size_t i = 0; i < std::min(hands.size(), (size_t)4); ++i) {
            const auto& h = hands[i];
            std::string px = "hand" + std::to_string(i) + "_x";
            std::string py = "hand" + std::to_string(i) + "_y";
            std::string pz = "hand" + std::to_string(i) + "_z";
            std::string pg = "hand" + std::to_string(i) + "_g";
            sendCtrl(tidal_, px.c_str(), h.x);
            sendCtrl(tidal_, py.c_str(), h.y);
            sendCtrl(tidal_, pz.c_str(), h.z);
            sendCtrl(tidal_, pg.c_str(), (float)(int)h.gesture);
        }
        // Global controls
        sendCtrl(tidal_, "num_hands",        (float)hands.size());
        sendCtrl(tidal_, "progress",         state.progress);
        sendCtrl(tidal_, "instrument_level", (float)state.level);
        sendCtrl(tidal_, "tempo",            p.tempo);
    }

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

void OscSender::sendPool(const InstrumentPool& pool, const HandList& hands, const MusicParams& p) {
    if (!tidal_) return;

    static int pool_count = 0;
    if (++pool_count % 2 != 0) return; // ~15Hz

    // Build a map from handId → hand data for quick lookup
    std::unordered_map<int, const Hand*> handMap;
    for (const auto& h : hands) handMap[h.id] = &h;

    for (const auto& a : pool.assignments()) {
        if (!a.active) continue;

        int orbit = a.orbit; // 0-11
        std::string prefix = "o" + std::to_string(orbit) + "_";

        // Instrument name — Tidal uses cS to read string controls
        sendCtrlStr(tidal_, (prefix + "inst").c_str(), a.inst.name.c_str());
        sendCtrlStr(tidal_, (prefix + "slow").c_str(), std::to_string((int)a.inst.slow).c_str());

        // Hand position if available
        auto it = handMap.find(a.handId);
        if (it != handMap.end()) {
            const Hand* h = it->second;
            sendCtrl(tidal_, (prefix + "x").c_str(),      h->x);
            sendCtrl(tidal_, (prefix + "y").c_str(),      h->y);
            sendCtrl(tidal_, (prefix + "z").c_str(),      h->z);
            sendCtrl(tidal_, (prefix + "g").c_str(),      (float)(int)h->gesture);
            sendCtrl(tidal_, (prefix + "gain").c_str(),   a.fadeOut * (1.f - h->z * 0.3f));
            sendCtrl(tidal_, (prefix + "active").c_str(), 1.f);
            sendCtrl(tidal_, (prefix + "hue").c_str(),    a.inst.hue);
        } else {
            sendCtrl(tidal_, (prefix + "gain").c_str(),   a.fadeOut);
            sendCtrl(tidal_, (prefix + "active").c_str(), a.releaseTimer > 0 ? 1.f : 0.f);
        }
    }
}
