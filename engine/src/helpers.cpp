#include "bb/helpers.h"
#include <algorithm>

namespace bb {

int countTacklezones(const GameState& state, Position pos, TeamSide friendlySide,
                     int excludeId) {
    int count = 0;
    auto adj = pos.getAdjacent();
    for (auto& apos : adj) {
        if (!apos.isOnPitch()) continue;
        const Player* p = state.getPlayerAtPosition(apos);
        if (p && p->teamSide != friendlySide && p->id != excludeId &&
            exertsTacklezone(p->state) && !p->lostTacklezones) {
            count++;
        }
    }
    return count;
}

int countDisturbingPresence(const GameState& state, Position pos, TeamSide friendlySide) {
    int count = 0;
    TeamSide enemySide = opponent(friendlySide);
    state.forEachOnPitch(enemySide, [&](const Player& p) {
        if (p.hasSkill(SkillName::DisturbingPresence) &&
            p.position.distanceTo(pos) <= 3) {
            count++;
        }
    });
    return count;
}

int calculateDodgeTarget(const GameState& state, const Player& player,
                         Position dest, Position source) {
    int ag = player.stats.agility;
    // BreakTackle: use ST if higher
    if (player.hasSkill(SkillName::BreakTackle) && player.stats.strength > ag) {
        ag = player.stats.strength;
    }

    int target = 7 - ag;

    // TZ at destination
    target += countTacklezones(state, dest, player.teamSide);

    // Skills that make dodging easier
    // Dodge: -1 (negated if any opponent adjacent to source has Tackle)
    if (player.hasSkill(SkillName::Dodge)) {
        bool tacklePresent = false;
        auto srcAdj = source.getAdjacent();
        for (auto& apos : srcAdj) {
            if (!apos.isOnPitch()) continue;
            const Player* opp = state.getPlayerAtPosition(apos);
            if (opp && opp->teamSide != player.teamSide &&
                exertsTacklezone(opp->state) && !opp->lostTacklezones &&
                opp->hasSkill(SkillName::Tackle)) {
                tacklePresent = true;
                break;
            }
        }
        if (!tacklePresent) target -= 1;
    }

    if (player.hasSkill(SkillName::Stunty)) target -= 1;
    if (player.hasSkill(SkillName::Titchy)) target -= 1;
    if (player.hasSkill(SkillName::TwoHeads)) target -= 1;

    // Skills that make dodging harder (opponents at source)
    auto srcAdj = source.getAdjacent();
    for (auto& apos : srcAdj) {
        if (!apos.isOnPitch()) continue;
        const Player* opp = state.getPlayerAtPosition(apos);
        if (opp && opp->teamSide != player.teamSide &&
            exertsTacklezone(opp->state) && !opp->lostTacklezones) {
            if (opp->hasSkill(SkillName::PrehensileTail)) target += 1;
        }
    }

    // DivingTackle: +2 from one opponent at source
    for (auto& apos : srcAdj) {
        if (!apos.isOnPitch()) continue;
        const Player* opp = state.getPlayerAtPosition(apos);
        if (opp && opp->teamSide != player.teamSide &&
            exertsTacklezone(opp->state) && !opp->lostTacklezones &&
            opp->hasSkill(SkillName::DivingTackle)) {
            target += 2;
            break; // only one DivingTackle applies
        }
    }

    return std::clamp(target, 2, 6);
}

int calculatePickupTarget(const GameState& state, const Player& player) {
    int target = 6 - player.stats.agility;

    if (!player.hasSkill(SkillName::BigHand)) {
        target += countTacklezones(state, player.position, player.teamSide);
        if (state.weather == Weather::POURING_RAIN) {
            target += 1;
        }
    }

    if (player.hasSkill(SkillName::ExtraArms)) target -= 1;

    return std::clamp(target, 2, 6);
}

int calculateCatchTarget(const GameState& state, const Player& catcher, int modifier) {
    int target = 7 - catcher.stats.agility - modifier;

    if (!catcher.hasSkill(SkillName::NervesOfSteel)) {
        target += countTacklezones(state, catcher.position, catcher.teamSide);
    }

    target += countDisturbingPresence(state, catcher.position, catcher.teamSide);

    if (catcher.hasSkill(SkillName::ExtraArms)) target -= 1;
    if (catcher.hasSkill(SkillName::DivingCatch)) target -= 1;
    if (state.weather == Weather::POURING_RAIN) target += 1;

    return std::clamp(target, 2, 6);
}

int countAssists(const GameState& state, Position targetPos, TeamSide assistingSide,
                 int excludeId1, int excludeId2, int tzExcludeId) {
    int count = 0;
    auto adj = targetPos.getAdjacent();
    for (auto& apos : adj) {
        if (!apos.isOnPitch()) continue;
        const Player* p = state.getPlayerAtPosition(apos);
        if (!p || p->teamSide != assistingSide) continue;
        if (p->id == excludeId1 || p->id == excludeId2) continue;
        if (!canAct(p->state)) continue;
        if (p->lostTacklezones) continue;

        // Can assist if: has Guard, or not in any enemy TZ
        // CRP: "except the player being blocked" → exclude tzExcludeId from TZ check
        if (p->hasSkill(SkillName::Guard)) {
            count++;
        } else {
            int enemyTZ = countTacklezones(state, p->position, assistingSide, tzExcludeId);
            if (enemyTZ == 0) {
                count++;
            }
        }
    }
    return count;
}

BlockDiceInfo getBlockDiceInfo(int attST, int defST) {
    if (attST > 2 * defST) return {3, true};
    if (attST > defST) return {2, true};
    if (attST == defST) return {1, true};
    if (defST > 2 * attST) return {3, false};
    // defST > attST
    return {2, false};
}

int getPushbackSquares(Position attackerPos, Position defenderPos, Position out[3]) {
    int dx = defenderPos.x - attackerPos.x;
    int dy = defenderPos.y - attackerPos.y;
    // Normalize
    if (dx > 0) dx = 1; else if (dx < 0) dx = -1;
    if (dy > 0) dy = 1; else if (dy < 0) dy = -1;

    // 8 compass directions (clockwise from N)
    static const int8_t compass[8][2] = {
        {0,-1}, {1,-1}, {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}
    };

    // Find the direction index
    int idx = 0;
    for (int i = 0; i < 8; i++) {
        if (compass[i][0] == dx && compass[i][1] == dy) {
            idx = i;
            break;
        }
    }

    // Three pushback directions: straight, CW 45°, CCW 45°
    int dirs[3] = { idx, (idx + 1) % 8, (idx + 7) % 8 };

    int count = 0;
    for (int i = 0; i < 3; i++) {
        Position p{
            static_cast<int8_t>(defenderPos.x + compass[dirs[i]][0]),
            static_cast<int8_t>(defenderPos.y + compass[dirs[i]][1])
        };
        if (p.isOnPitch()) {
            out[count++] = p;
        }
    }
    return count;
}

Position scatterDirection(int d8) {
    // Clockwise from North: 1=N, 2=NE, 3=E, 4=SE, 5=S, 6=SW, 7=W, 8=NW
    static const int8_t offsets[8][2] = {
        {0,-1}, {1,-1}, {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}
    };
    int idx = std::clamp(d8, 1, 8) - 1;
    return {offsets[idx][0], offsets[idx][1]};
}

bool attemptRoll(GameState& state, int playerId, DiceRollerBase& dice,
                 int target, SkillName skillReroll,
                 bool skillNegatedByOpponent, bool canUseTeamReroll,
                 std::vector<GameEvent>* events) {
    Player& player = state.getPlayer(playerId);

    // Initial roll
    int roll = dice.rollD6();
    if (roll >= target) return true;

    // Skill reroll
    if (skillReroll != SkillName::SKILL_COUNT &&
        player.hasSkill(skillReroll) && !skillNegatedByOpponent) {
        roll = dice.rollD6();
        emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                          static_cast<int>(skillReroll), roll >= target});
        if (roll >= target) return true;
    }

    // Pro reroll
    if (player.hasSkill(SkillName::Pro) && !player.proUsedThisTurn) {
        player.proUsedThisTurn = true;
        int proRoll = dice.rollD6();
        if (proRoll >= 4) {
            roll = dice.rollD6();
            emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                              static_cast<int>(SkillName::Pro), roll >= target});
            if (roll >= target) return true;
        }
    }

    // Team reroll
    if (canUseTeamReroll) {
        TeamState& team = state.getTeamState(player.teamSide);
        if (team.canUseReroll()) {
            team.rerolls--;
            team.rerollUsedThisTurn = true;

            // Loner gate
            if (player.hasSkill(SkillName::Loner)) {
                int lonerRoll = dice.rollD6();
                if (lonerRoll < 4) {
                    return false; // reroll wasted
                }
            }

            roll = dice.rollD6();
            if (roll >= target) return true;
        }
    }

    return false;
}

} // namespace bb
