#pragma once
#include "../Types.h"
#include "../game/InstrumentPool.h"
#include <string>
#ifdef HAVE_LIBLO
#include <lo/lo.h>
#endif

class OscSender {
public:
    OscSender(const std::string& host, int port);
    ~OscSender();

    bool connect();
    // Also send /ctrl to Tidal on port 6010
    void connectTidal(const std::string& host = "127.0.0.1", int port = 6010);
    void silenceAllOrbits();
    void* tidalAddr() const { return tidal_; }
    // Send pool assignment state to Tidal
    void sendPool(const class InstrumentPool& pool, const HandList& hands, const MusicParams& p);
    // Called once per frame with full hand list + game state.
    // Per-hand: /hand id x y z_mm z_vel gesture_id
    // Then:     /hands_end bucket0 bucket1 ... (active bucket indices this frame)
    // Global:   /tempo /fx /level /mute on change
    void send(const HandList& hands, const MusicParams& p, const GameStateData& state, bool scInstruments = true);

    // Custom helper methods for visualizer/game triggering
    void sendTidalCtrl(const char* key, float val);
    void sendTidalCtrlStr(const char* key, const char* val);
    void sendDirtPlay(int midi, float amp, float decay, const std::string& instrument = "superpiano", int orbit = 0);
    void sendMuteBacking(bool mute);
    void sendReflexHush();
    void sendMute(bool muted);
    void setPreviewMute(bool muted);
    void setElapsedTime(float t);
    void setTargetNode(int idx);
    void setReflexActive(bool active);
    void setReflexCps(float cps);
    void setActiveSong(int id) { active_song_ = id; }
    int getActiveSong() const { return active_song_; }

private:
    int active_song_ = -1;
    bool reflex_active_ = false;
    float reflex_cps_ = 0.3f;
    std::string host_;
    int         port_;
#ifdef HAVE_LIBLO
    lo_address  addr_  = nullptr;  // scsynth 57120
    lo_address  tidal_ = nullptr;  // tidal 6010
#else
    void*       addr_  = nullptr;
    void*       tidal_ = nullptr;
#endif

    MusicParams prev_;
    bool first_ = true;
    bool preview_muted_ = false;
    float elapsed_time_ = 0.0f;
    int target_node_idx_ = -1;

    void sendInstrument(int id);
    void sendNote(int pitch, float velocity);
    void sendTempo(float bpm);
    void sendFx(float reverb, float delay);
    void sendSpatial(float x, float y);
    void sendLevel(int level);
};
