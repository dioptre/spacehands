#pragma once
#include "../Types.h"

class InstrumentMapper {
public:
    MusicParams map(const HandList& hands, const GameStateData& state) const;

private:
    // Find the leftmost / rightmost hand, or null if absent
    const Hand* leftHand(const HandList& hands) const;
    const Hand* rightHand(const HandList& hands) const;
};
