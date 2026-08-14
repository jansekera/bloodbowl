#pragma once

#include "bb/game_state.h"
#include "bb/dice.h"
#include "bb/game_event.h"
#include "bb/action_result.h"
#include <vector>
#include <algorithm>

namespace bb {

inline void emitEvent(std::vector<GameEvent>* events, GameEvent evt) {
    if (events) events->push_back(evt);
}

struct BlockDiceInfo {
    int count = 1;
    bool attackerChooses = true;
};

// Tacklezone counting (excludeId: player to skip in TZ check)
int countTacklezones(const GameState& state, Position pos, TeamSide friendlySide,
                     int excludeId = -1);
int countDisturbingPresence(const GameState& state, Position pos, TeamSide friendlySide);

// Agility roll targets (clamped to 2-6)
int calculateDodgeTarget(const GameState& state, const Player& player,
                         Position dest, Position source);
int calculatePickupTarget(const GameState& state, const Player& player);
int calculateCatchTarget(const GameState& state, const Player& catcher, int modifier = 0);

// Block helpers
// tzExcludeId: CRP "except the player being blocked" — exclude from TZ check
int countAssists(const GameState& state, Position targetPos, TeamSide assistingSide,
                 int excludeId1 = -1, int excludeId2 = -1, int tzExcludeId = -1);
BlockDiceInfo getBlockDiceInfo(int attST, int defST);
int getPushbackSquares(Position attackerPos, Position defenderPos, Position out[3]);
Position scatterDirection(int d8);

// Reroll chain: skill → Pro → team reroll (with Loner gate)
// skillReroll = SKILL_COUNT means no skill reroll available
bool attemptRoll(GameState& state, int playerId, DiceRollerBase& dice,
                 int target, SkillName skillReroll,
                 bool skillNegatedByOpponent, bool canUseTeamReroll,
                 std::vector<GameEvent>* events);

} // namespace bb
