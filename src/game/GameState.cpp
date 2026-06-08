#include "GameState.h"
#include <algorithm>
#include <cmath>

float GameState::levelThreshold(int level) const {
    // Each level requires an equal chunk of progress [0,1]
    return (float)(level + 1) / (MAX_LEVEL + 1);
}

float GameState::handMultiplier(int n) const {
    // 1→1x, 2→1.8x, 3→2.5x, 4→3x
    static const float m[] = {0.f, 1.f, 1.8f, 2.5f, 3.f};
    n = std::clamp(n, 0, 4);
    return m[n];
}

float GameState::gestureBonus() const {
    // Each unique gesture type seen adds 10% bonus
    int seen = __builtin_popcount(gesture_variety_);
    return 1.f + seen * 0.1f;
}

Gesture GameState::pickHint() const {
    // Suggest the gesture not yet seen that gives most progress
    static const Gesture order[] = { Gesture::SPREAD, Gesture::OPEN, Gesture::POINT, Gesture::PINCH, Gesture::FIST };
    for (auto g : order) {
        uint8_t bit = 1u << (int)g;
        if (!(gesture_variety_ & bit)) return g;
    }
    return Gesture::UNKNOWN;
}

const GameStateData& GameState::tick(const HandList& hands, float dt_sec) {
    int n = (int)hands.size();
    state_.active_hands = n;

    // Track gesture variety (bitmask)
    for (const auto& h : hands)
        gesture_variety_ |= (1u << (int)h.gesture);

    if (n == 0) {
        // No hands — decay hint timer, switch to IDLE
        stall_timer_ += dt_sec;
        if (state_.phase == GamePhase::ACTIVE)
            state_.phase = GamePhase::IDLE;
    } else {
        state_.phase = (state_.level >= MAX_LEVEL) ? GamePhase::MAX_LEVEL : GamePhase::ACTIVE;

        if (state_.phase == GamePhase::ACTIVE) {
            float delta = BASE_RATE * handMultiplier(n) * gestureBonus() * dt_sec * 30.f;
            state_.progress = std::min(state_.progress + delta, 1.f);

            // Level up
            if (state_.progress >= levelThreshold(state_.level)) {
                state_.level    = std::min(state_.level + 1, MAX_LEVEL);
                gesture_variety_ = 0; // reset variety tracking per level
                state_.phase    = GamePhase::LEVELING;
                stall_timer_    = 0.f;
            }
        }

        // Hint: fire if progress hasn't moved in STALL_THRESHOLD seconds
        if (std::fabs(state_.progress - prev_progress_) < 0.001f)
            stall_timer_ += dt_sec;
        else
            stall_timer_ = 0.f;

        state_.hint = (stall_timer_ >= STALL_THRESHOLD) ? pickHint() : Gesture::UNKNOWN;
    }

    prev_progress_ = state_.progress;
    return state_;
}
