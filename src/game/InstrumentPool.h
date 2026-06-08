#pragma once
#include "../Types.h"
#include <array>
#include <string>
#include <vector>
#include <random>
#include <algorithm>

// Instrument descriptor — name for SuperDirt + colour for shader
struct InstrumentDef {
    std::string name;     // SuperDirt sample name
    float       hue;      // 0-1 hue for shader colour
    float       slow;     // slow factor (1=normal, 2=half speed etc)
    int         nMax;     // max n value for this bank
};

// Pool of available instruments — randomly assigned to hands
class InstrumentPool {
public:
    static constexpr int MAX_HANDS = 12;

    struct Assignment {
        bool        active    = false;
        int         handId    = -1;
        int         orbit     = -1;    // SuperDirt orbit (0-11 = d1-d12)
        InstrumentDef inst;
        float       fadeOut      = 0.f;  // 1=full, 0=silent
        float       releaseTimer = 0.f;  // >0 = releasing
    };

    InstrumentPool();

    // Called when a new hand appears — assigns random instrument + orbit
    Assignment* assign(int handId);

    // Called when hand disappears — starts release, returns instrument to pool
    void release(int handId);

    // Called each frame — updates fade timers
    void tick(float dt);

    const std::array<Assignment, MAX_HANDS>& assignments() const { return assignments_; }

    // Get colour for a hand (for shader)
    float getHue(int handId) const;

private:
    static const std::vector<InstrumentDef> POOL;
    std::vector<bool> orbitUsed_;
    std::vector<bool> instUsed_;
    std::array<Assignment, MAX_HANDS> assignments_;
    std::mt19937 rng_;

    int  pickOrbit();
    int  pickInstrument();
    void freeSlot(int slot);
};
