#include "InstrumentMapper.h"
#include <algorithm>
#include <cmath>

const Hand* InstrumentMapper::leftHand(const HandList& hands) const {
    if (hands.empty()) return nullptr;
    return &*std::min_element(hands.begin(), hands.end(),
                              [](const Hand& a, const Hand& b){ return a.x < b.x; });
}

const Hand* InstrumentMapper::rightHand(const HandList& hands) const {
    if (hands.empty()) return nullptr;
    return &*std::max_element(hands.begin(), hands.end(),
                              [](const Hand& a, const Hand& b){ return a.x < b.x; });
}

MusicParams InstrumentMapper::map(const HandList& hands, const GameStateData& state) const {
    MusicParams p;

    // Instrument bucket is per-hand (Z axis), handled in OscSender/SC.
    // InstrumentMapper sets global params that apply across all hands.
    p.instrument = std::min(state.level, 4);

    if (hands.empty()) {
        p.muted = true;
        return p;
    }

    // --- Y axis → pitch (all hands contribute; use average for global pitch) ---
    float pitch_sum = 0.f;
    for (const auto& h : hands)
        pitch_sum += 36.f + (1.f - h.y) * 48.f; // top=high, bottom=low, MIDI 36-84
    p.pitch = pitch_sum / hands.size();

    // --- X axis → tempo (use average X across all hands) ---
    float x_avg = 0.f;
    for (const auto& h : hands)
        x_avg += h.x;
    x_avg /= hands.size();
    // X=0 (far left) → 60bpm, X=0.5 (centre) → 120bpm, X=1 (far right) → 240bpm
    p.tempo = 60.f * std::pow(2.f, x_avg * 2.f); // 60–240 bpm exponential

    // --- Z axis → handled per-hand as instrument bucket in OscSender ---
    // Global reverb from average Z (closer hands = more reverb)
    float z_avg = 0.f;
    for (const auto& h : hands)
        z_avg += h.z;
    p.reverb = (z_avg / hands.size()) * 0.7f;

    // Pan from spread of hands across X
    const Hand* lh = leftHand(hands);
    const Hand* rh = rightHand(hands);
    p.pan = (lh && rh) ? ((lh->x + rh->x) * 0.5f * 2.f - 1.f) : 0.f;

    // Velocity from number of hands
    p.velocity = std::min(0.4f + state.active_hands * 0.15f, 1.f);

    // Gestures
    for (const auto& h : hands) {
        if (h.gesture == Gesture::FIST)  { p.muted      = true; break; }
        if (h.gesture == Gesture::SPREAD) { p.chord_mode = true; }
    }

    p.delay = state.progress * 0.5f;

    return p;
}
