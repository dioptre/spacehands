#pragma once
#include "../Types.h"
#include <chrono>

class GameState {
public:
    // Call once per camera frame (~30Hz). Returns updated state.
    const GameStateData& tick(const HandList& hands, float dt_sec);
    const GameStateData& data() const { return state_; }

private:
    GameStateData state_;

    static constexpr int   MAX_LEVEL       = 5;
    static constexpr float BASE_RATE       = 0.0003f; // progress/frame (~5min solo to level up)
    static constexpr float STALL_THRESHOLD = 5.f;     // seconds before hint fires

    float stall_timer_  = 0.f;
    float prev_progress_ = 0.f;
    // track which gestures have been seen this level
    uint8_t gesture_variety_ = 0;

    float levelThreshold(int level) const;
    float handMultiplier(int n) const;
    float gestureBonus() const;
    Gesture pickHint() const;
};
