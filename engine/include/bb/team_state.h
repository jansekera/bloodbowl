#pragma once

#include "bb/enums.h"
#include <cstdint>

namespace bb {

struct TeamState {
    TeamSide side = TeamSide::HOME;
    int score = 0;
    int rerolls = 0;
    bool rerollUsedThisTurn = false;
    int turnNumber = 0;
    bool blitzUsedThisTurn = false;
    bool passUsedThisTurn = false;
    bool foulUsedThisTurn = false;
    bool hasApothecary = false;
    bool apothecaryUsed = false;

    bool canUseReroll() const {
        return rerolls > 0 && !rerollUsedThisTurn;
    }

    void resetForNewTurn() {
        rerollUsedThisTurn = false;
        blitzUsedThisTurn = false;
        passUsedThisTurn = false;
        foulUsedThisTurn = false;
    }
};

} // namespace bb
