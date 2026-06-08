#pragma once
#include "../Types.h"
#include <string>
#ifdef HAVE_LIBLO
#include <lo/lo.h>
#endif

class OscSender {
public:
    OscSender(const std::string& host, int port);
    ~OscSender();

    bool connect();
    // Called once per frame with full hand list + game state.
    // Per-hand: /hand id x y z_mm z_vel gesture_id
    // Then:     /hands_end bucket0 bucket1 ... (active bucket indices this frame)
    // Global:   /tempo /fx /level /mute on change
    void send(const HandList& hands, const MusicParams& p, const GameStateData& state);

private:
    std::string host_;
    int         port_;
#ifdef HAVE_LIBLO
    lo_address  addr_ = nullptr;
#else
    void*       addr_ = nullptr;
#endif

    MusicParams prev_;
    bool first_ = true;

    void sendInstrument(int id);
    void sendNote(int pitch, float velocity);
    void sendTempo(float bpm);
    void sendFx(float reverb, float delay);
    void sendSpatial(float x, float y);
    void sendLevel(int level);
    void sendMute(bool muted);
};
