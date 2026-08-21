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

// Next step of a blitz-style approach walk toward targetPos: among free
// on-pitch squares adjacent to `from`, distance stays primary and enemy
// tackle zones break ties (scoreMoveAction's 20/12 weights). The mover's
// own current square counts as free (it vacates it). Returns {-1,-1} when
// no candidate exists. Shared by the BLITZ executor (action_resolver) and
// the blitzer-selection risk estimate (macro_actions) so the estimate
// prices exactly the walk that will happen (items 7+14).
Position pickApproachStep(const GameState& state, const Player& mover,
                          Position from, Position targetPos);

// Agility roll targets (clamped to 2-6)
int calculateDodgeTarget(const GameState& state, const Player& player,
                         Position dest, Position source);
int calculatePickupTarget(const GameState& state, const Player& player);
// Same roll, priced as if `player` stood on `at` -- macro generation needs
// the target BEFORE the picker walks to the ball (tackle zones are counted
// on the ball's square, not on the picker's current one).
int calculatePickupTargetAt(const GameState& state, const Player& player,
                            Position at);
int calculateCatchTarget(const GameState& state, const Player& catcher, int modifier = 0);

// Block helpers
// tzExcludeId: CRP "except the player being blocked" — exclude from TZ check
int countAssists(const GameState& state, Position targetPos, TeamSide assistingSide,
                 int excludeId1 = -1, int excludeId2 = -1, int tzExcludeId = -1,
                 // BB2016 l. 8160: Guard "may not be used to assist a FOUL",
                 // and l. 1849-1851 lists no exception for it: "No player from
                 // either side may assist a foul if they are in the tackle zone
                 // of an opposing player." resolveFoul passes false. Default
                 // true keeps every block caller unchanged.
                 bool guardApplies = true);
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
