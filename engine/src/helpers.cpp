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

Position pickApproachStep(const GameState& state, const Player& mover,
                          Position from, Position targetPos) {
    bool currentlyInTZ = countTacklezones(state, from, mover.teamSide) > 0;
    Position bestNext{-1, -1};
    int bestScore = 99999;
    for (auto& pos : from.getAdjacent()) {
        if (!pos.isOnPitch()) continue;
        const Player* occ = state.getPlayerAtPosition(pos);
        if (occ && occ->id != mover.id) continue;
        int destTZ = countTacklezones(state, pos, mover.teamSide);
        int score = pos.distanceTo(targetPos) * 100
                  + (currentlyInTZ ? 12 : 20) * destTZ;
        if (score < bestScore) {
            bestScore = score;
            bestNext = pos;
        }
    }
    return bestNext;
}

int calculateDodgeTarget(const GameState& state, const Player& player,
                         Position dest, Position source) {
    int ag = player.stats.agility;
    // Break Tackle, l. 7987-7990: "The player may use his Strength instead of
    // his Agility when making a Dodge roll ... This skill may only be used once
    // per turn." ⚠️ Limit "jednou za kolo" nehlidame.
    if (player.hasSkill(SkillName::BreakTackle) && player.stats.strength > ag) {
        ag = player.stats.strength;
    }

    // Agility table (7 - AG) MINUS the flat +1 every dodge gets (rules
    // parity, 2026-08-10). CRP "DODGING MODIFIERS: Making a dodge roll +1
    // / Per enemy tackle zone on the square that the player is dodging to
    // -1", and the worked example in the rules: AG3 needs a basic 4+, gets
    // +1 for making a dodge, -2 for two tackle zones on the destination,
    // and a roll of 5 succeeds. We were missing the +1, so every dodge in
    // the game was one step harder than it should be -- the most frequent
    // roll there is, and it hurt Dodge-less teams the most since they had
    // no re-roll to cushion it.
    int target = 6 - ag;

    // TZ at destination
    target += countTacklezones(state, dest, player.teamSide);

    // ⛔⛔ 24.08.2026: TADY BYL VYMYSLENY MODIFIKATOR. Dodge dostaval -1 na cil
    // (tj. +1 na hod), a k tomu jeste reroll pres attemptRoll v move_handleru.
    // BB2016 l. 8086-8092 dava Dodge POUZE REROLL: "is allowed to RE-ROLL the
    // D6 if he fails to dodge out of any of an opposing player's tackle zones.
    // However, the player may only re-roll one failed Dodge roll per turn."
    // A tabulka modifikatoru dodge (l. 597-600) zna jen "+1 za hod na dodge"
    // (ten uz je v `6 - ag`) a "-1 za kazdou tackle zonu na CILOVEM poli".
    // Zadny bonus za dovednost Dodge tam neni.
    // ⇒ Dodge tymy (skaven, wood-elf, human) mely kazdy dodge o stupen levnejsi,
    // nez maji mit. Tataz strana jako TA1 (retez rerollu), taky opraveno dnes.
    //
    // Tackle tu proto nema co rusit: l. 8567-8571 rusi Dodge jako REROLL,
    // a to uz dela move_handler.cpp (parametr skillNegatedByOpponent).

    // Stunty, l. 8530-8533: "may IGNORE ANY ENEMY TACKLE ZONES ON THE SQUARE HE
    // IS MOVING TO when he makes a Dodge roll". Neni to plosne -1, je to
    // odecteni CELE penalizace za cil -- ve trech tackle zonach je to rozdil
    // -1 proti -3.
    if (player.hasSkill(SkillName::Stunty)) {
        target -= countTacklezones(state, dest, player.teamSide);
    }
    // Titchy, l. 8638-8639: "may add 1 to any Dodge roll he attempts."
    if (player.hasSkill(SkillName::Titchy)) target -= 1;
    // Two Heads, l. 8644-8646: "Add 1 to all Dodge rolls the player makes."
    if (player.hasSkill(SkillName::TwoHeads)) target -= 1;

    // Skills that make dodging harder (opponents at source)
    auto srcAdj = source.getAdjacent();
    for (auto& apos : srcAdj) {
        if (!apos.isOnPitch()) continue;
        const Player* opp = state.getPlayerAtPosition(apos);
        if (opp && opp->teamSide != player.teamSide &&
            exertsTacklezone(opp->state) && !opp->lostTacklezones) {
            // Prehensile Tail, l. 8598-8601: "opposing players must subtract 1
            // from the D6 roll if they attempt to dodge out of any of the
            // player's tackle zones."
            if (opp->hasSkill(SkillName::PrehensileTail)) target += 1;
        }
    }

    // Diving Tackle, l. 8076-8080: "The opposing player must subtract 2 from
    // his Dodge roll ... only ONE of the opposing players may use Diving
    // Tackle." ⚠️ NEUPLNE: hráč s Diving Tackle se ma pritom POLOZIT NA ZEM
    // do pole, ktere utikajici opustil (l. 8074-8075) -- to nedelame (ukol B2).
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

int calculatePickupTargetAt(const GameState& state, const Player& player,
                            Position at) {
    int target = 6 - player.stats.agility;

    if (!player.hasSkill(SkillName::BigHand)) {
        target += countTacklezones(state, at, player.teamSide);
        if (state.weather == Weather::POURING_RAIN) {
            target += 1;
        }
    }

    if (player.hasSkill(SkillName::ExtraArms)) target -= 1;

    return std::clamp(target, 2, 6);
}

int calculatePickupTarget(const GameState& state, const Player& player) {
    return calculatePickupTargetAt(state, player, player.position);
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
                 int excludeId1, int excludeId2, int tzExcludeId,
                 bool guardApplies) {
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
        if (guardApplies && p->hasSkill(SkillName::Guard)) {
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

    // ⛔⛔ JEDEN HOD = NEJVÝŠ JEDEN REROLL (oprava TA1/P57, 24.08.2026).
    // BB2016 l. 925-927: "VERY IMPORTANT: No matter how many re-rolls you have,
    // or what type they are, you may never re-roll a single dice roll more than
    // once." Do 21.08. tady byla KASKÁDA skill -> Pro -> týmový reroll, takže
    // jeden hod dostal až TŘI opravné pokusy; nadržovalo to týmům s Dodge a
    // Sure Feet (skaven, wood-elf). Každá větev níž proto KONČÍ návratem.

    // (1) Skill reroll
    // ⛔ LIMIT JEDNOU ZA KOLO (oprava 21.08.). BB2016 l. 8089-8090 (Dodge):
    // "the player may only re-roll ONE failed Dodge roll per turn"; l. 8541
    // (Sure Feet): "may only use the Sure Feet skill once per turn".
    bool* skillOncePerTurn = nullptr;
    if (skillReroll == SkillName::Dodge)    skillOncePerTurn = &player.dodgeRerollUsedThisTurn;
    if (skillReroll == SkillName::SureFeet) skillOncePerTurn = &player.sureFeetRerollUsedThisTurn;
    if (skillReroll != SkillName::SKILL_COUNT &&
        player.hasSkill(skillReroll) && !skillNegatedByOpponent &&
        !(skillOncePerTurn && *skillOncePerTurn)) {
        if (skillOncePerTurn) *skillOncePerTurn = true;
        roll = dice.rollD6();
        emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                          static_cast<int>(skillReroll), roll >= target});
        return roll >= target;   // hod už byl jednou přehozen -- konec
    }

    // (2) Pro
    // BB2016 l. 8385-8389: před rerollem hod D6; na 4-6 se reroll provede, na
    // 1-3 "the original result stands and may not be re-rolled with a skill or
    // team re-roll; however you can re-roll the Pro roll with a Team re-roll."
    // ⇒ týmový reroll tu smí přehodit BRÁNU, ne původní hod.
    if (player.hasSkill(SkillName::Pro) && !player.proUsedThisTurn) {
        player.proUsedThisTurn = true;
        int proRoll = dice.rollD6();
        if (proRoll < 4 && canUseTeamReroll) {
            TeamState& team = state.getTeamState(player.teamSide);
            if (team.canUseReroll()) {
                team.rerolls--;
                team.rerollUsedThisTurn = true;
                bool lonerOk = true;
                if (player.hasSkill(SkillName::Loner)) {
                    lonerOk = dice.rollD6() >= 4;
                }
                if (lonerOk) proRoll = dice.rollD6();
            }
        }
        if (proRoll >= 4) {
            roll = dice.rollD6();
            emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                              static_cast<int>(SkillName::Pro), roll >= target});
            return roll >= target;
        }
        return false;   // původní výsledek platí a nesmí se přehodit
    }

    // (3) Týmový reroll -- jen když hod ještě nebyl přehozen
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
            return roll >= target;
        }
    }

    return false;
}

} // namespace bb
