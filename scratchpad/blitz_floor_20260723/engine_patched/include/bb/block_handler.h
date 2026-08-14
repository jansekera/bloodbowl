#pragma once

#include "bb/game_state.h"
#include "bb/dice.h"
#include "bb/game_event.h"
#include "bb/action_result.h"
#include <vector>

namespace bb {

struct BlockParams {
    int attackerId = -1;
    int targetId = -1;
    bool isBlitz = false;
    bool hornsBonus = false;  // +1 ST on blitz with Horns
};

ActionResult resolveBlock(GameState& state, const BlockParams& params,
                          DiceRollerBase& dice, std::vector<GameEvent>* events,
                          bool frenzySecondBlock = false,
                          bool noFollowUp = false);

ActionResult resolveMultipleBlock(GameState& state, int attackerId,
                                  int target1Id, int target2Id,
                                  DiceRollerBase& dice, std::vector<GameEvent>* events);

BlockDiceFace autoChooseBlockDie(const BlockDiceFace* faces, int count,
                                 bool attackerChooses,
                                 const Player& att, const Player& def);

} // namespace bb
