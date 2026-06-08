#include "InstrumentPool.h"
#include <iostream>

// The liked instrument pool — name, hue (0-1), slow factor, max n
const std::vector<InstrumentDef> InstrumentPool::POOL = {
    // Melodic / tonal
    {"moog",          0.60, 8.0,  7},
    {"sitar",         0.35, 8.0,  8},
    {"pluck",         0.40, 2.0, 17},
    {"arpy",          0.70, 1.0, 11},
    {"latibro",       0.65, 1.0,  8},
    {"juno",          0.55, 4.0, 12},
    {"padlong",       0.75, 8.0,  1},
    {"supermandolin", 0.45, 1.0,  0},  // uses note not n
    {"sid",           0.80, 1.0, 12},
    {"newnotes",      0.72, 4.0, 15},
    {"stab",          0.50, 4.0, 23},
    {"em2",           0.30, 4.0,  6},
    // Rhythmic / percussive
    {"tabla2",        0.10, 8.0, 46},
    {"tabla",         0.12, 8.0, 26},
    {"gretsch",       0.08, 2.0, 24},
    {"industrial",    0.15, 2.0, 32},
    {"feel",          0.20, 1.0,  7},
    {"voodoo",        0.25, 1.0,  5},
    {"metal",         0.85, 1.0, 10},
    {"stomp",         0.05, 1.0, 10},
    {"tink",          0.90, 1.0,  5},
    {"peri",          0.22, 1.0, 15},
    {"msg",           0.18, 1.0,  9},
    // Textural / atmospheric
    {"pebbles",       0.50, 8.0,  1},
    {"tacscan",       0.40, 8.0, 22},
    {"wind",          0.55, 2.0, 10},
    {"yeah",          0.30, 2.0, 31},
    {"ul",            0.28, 1.0, 10},
    {"sf",            0.38, 1.0, 18},
    {"flick",         0.42, 2.0, 17},
};

InstrumentPool::InstrumentPool()
    : orbitUsed_(MAX_HANDS, false)
    , instUsed_(POOL.size(), false)
    , rng_(std::random_device{}()) {
    for (auto& a : assignments_) a = {};
}

int InstrumentPool::pickOrbit() {
    std::vector<int> free;
    for (int i = 0; i < MAX_HANDS; ++i)
        if (!orbitUsed_[i]) free.push_back(i);
    if (free.empty()) return -1;
    return free[std::uniform_int_distribution<int>(0, free.size()-1)(rng_)];
}

int InstrumentPool::pickInstrument() {
    std::vector<int> free;
    for (size_t i = 0; i < POOL.size(); ++i)
        if (!instUsed_[i]) free.push_back(i);
    if (free.empty()) return -1;
    return free[std::uniform_int_distribution<int>(0, free.size()-1)(rng_)];
}

void InstrumentPool::freeSlot(int slot) {
    auto& a = assignments_[slot];
    if (!a.active) return;
    // Mark orbit and instrument as free
    for (int i = 0; i < MAX_HANDS; ++i)
        if (orbitUsed_[i] && assignments_[i].orbit == a.orbit)
            orbitUsed_[a.orbit] = false;
    for (size_t i = 0; i < POOL.size(); ++i)
        if (POOL[i].name == a.inst.name) { instUsed_[i] = false; break; }
    a = {};
}

InstrumentPool::Assignment* InstrumentPool::assign(int handId) {
    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_HANDS; ++i)
        if (!assignments_[i].active) { slot = i; break; }
    if (slot < 0) return nullptr;

    int orbit = pickOrbit();
    int inst  = pickInstrument();
    if (orbit < 0 || inst < 0) return nullptr;

    orbitUsed_[orbit] = true;
    instUsed_[inst]   = true;

    auto& a       = assignments_[slot];
    a.active      = true;
    a.handId      = handId;
    a.orbit       = orbit;
    a.inst        = POOL[inst];
    a.fadeOut     = 1.f;
    a.releaseTimer = 0.f;

    std::cout << "[Pool] hand " << handId
              << " → orbit " << orbit+1
              << " → " << a.inst.name << "\n";
    return &a;
}

void InstrumentPool::release(int handId) {
    for (int i = 0; i < MAX_HANDS; ++i) {
        if (assignments_[i].active && assignments_[i].handId == handId) {
            assignments_[i].releaseTimer = 3.f; // 3s release
            std::cout << "[Pool] hand " << handId
                      << " releasing " << assignments_[i].inst.name << "\n";
            return;
        }
    }
}

void InstrumentPool::tick(float dt) {
    for (int i = 0; i < MAX_HANDS; ++i) {
        auto& a = assignments_[i];
        if (!a.active) continue;
        if (a.releaseTimer > 0.f) {
            a.releaseTimer -= dt;
            a.fadeOut = std::max(0.f, a.releaseTimer / 3.f);
            if (a.releaseTimer <= 0.f) freeSlot(i);
        }
    }
}

float InstrumentPool::getHue(int handId) const {
    for (const auto& a : assignments_)
        if (a.active && a.handId == handId)
            return a.inst.hue;
    return 0.f;
}
