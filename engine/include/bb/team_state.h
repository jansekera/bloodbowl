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
    // 2026-08-17 (P4/P26): hand-off has its OWN per-turn allowance. CRP,
    // HANDING-OFF: "The Hand-Off Action is added to the list of Actions like
    // Move, Block, Blitz and Pass. A coach may only declare one Hand-Off Action
    // per turn." Until now both shared passUsedThisTurn, so a pass blocked the
    // hand-off and a hand-off blocked the pass -- and CHAIN_SCORE, which is a
    // pass followed by a hand-off, was offered 270 times over 3000 games and
    // could never once complete.
    bool handOffUsedThisTurn = false;
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
        handOffUsedThisTurn = false;
        foulUsedThisTurn = false;
    }
};

} // namespace bb
