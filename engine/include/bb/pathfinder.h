#pragma once

#include "bb/game_state.h"
#include "bb/position.h"

namespace bb {

struct MoveTarget {
    Position pos{};
    bool requiresDodge = false;
    bool isGfi = false;
};

// Can the player reach any square adjacent to target?
// If yes, returns true and sets outAdjacent to the best adjacent square.
// reserveMove: movement points to hold back from the budget (a blitz must
// keep 1 for the block itself -- CRP: the block costs 1 MP/GFI).
// Prvni krok po NEJKRATSI ceste na pole sousedici s `target`, s rezervou
// jednoho pole na blok. Tacklezony rozhoduji mezi stejne dlouhymi cestami.
// false = nikam nevede (pak se blitz nema o co pokouset).
long takeBlitzPathPicksInSearch();
int optimalPathStepsToAdjacent(const GameState& state, const Player& player,
                               Position target);
bool nextStepTowardAdjacent(const GameState& state, const Player& player,
                            Position target, Position& outStep);

bool canReachAdjacentTo(const GameState& state, const Player& player,
                        Position target, Position& outAdjacent,
                        int reserveMove = 0);

// Get all valid single-step move targets for a player.
// Returns the number of targets written to out (up to maxOut).
int getValidMoveTargets(const GameState& state, const Player& player,
                        MoveTarget* out, int maxOut);

} // namespace bb
