#pragma once
#include <array>
#include <string>
#include <vector>

enum class Gesture { UNKNOWN, OPEN, FIST, POINT, PINCH, SPREAD, THUMBS_UP, THUMBS_DOWN };
// Gesture integer IDs sent over OSC (must match SC instruments.scd):
//   0=UNKNOWN 1=OPEN 2=FIST 3=POINT 4=PINCH 5=SPREAD 6=THUMBS_UP 7=THUMBS_DOWN

inline const char* gestureName(Gesture g) {
    switch (g) {
    case Gesture::OPEN:   return "OPEN";
    case Gesture::FIST:   return "FIST";
    case Gesture::POINT:  return "POINT";
    case Gesture::PINCH:  return "PINCH";
    case Gesture::SPREAD:    return "SPREAD";
    case Gesture::THUMBS_UP:   return "THUMBS_UP";
    case Gesture::THUMBS_DOWN: return "THUMBS_DOWN";
    default:                   return "UNKNOWN";
    }
}

struct Hand {
    int   id        = -1;
    float x         = 0.f;    // 0–1, bbox centre x
    float y         = 0.f;    // 0–1, bbox centre y
    float bw        = 0.f;    // 0–1, bbox width
    float bh        = 0.f;    // 0–1, bbox height
    float z         = 0.f;    // 0–1, normalized depth (0 = close)
    float z_mm      = 0.f;    // raw depth in mm
    float z_vel     = 0.f;    // mm/frame
    float conf      = 0.f;
    Gesture gesture = Gesture::UNKNOWN;
    int   bucket    = 0;
};

using HandList = std::vector<Hand>;

struct MusicParams {
    int   instrument = 0;
    float pitch      = 60.f; // MIDI note
    float velocity   = 0.8f;
    float tempo      = 120.f;
    float reverb     = 0.2f;
    float delay      = 0.f;
    float pan        = 0.f; // -1 to 1
    bool  muted      = false;
    bool  chord_mode = false;
};

enum class GamePhase { IDLE, ACTIVE, LEVELING, MAX_LEVEL };

struct GameStateData {
    GamePhase phase    = GamePhase::IDLE;
    int       level    = 0;
    float     progress = 0.f;
    int       active_hands = 0;
    Gesture   hint     = Gesture::UNKNOWN;
    MusicParams music;
};

struct TargetSpawn {
    float x;
    float y;
    float z;
    int type;
    int hand;
    bool playCustom = false;
    int midi = 60;
    float gain = 1.0f;
    float sustain = 1.0f;
    std::string instrument = "arpy";
};
