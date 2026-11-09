#include "bb/helpers.h"
#include "bb/coach_choice.h"
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

bool tackleNegatesDodgeReroll(const GameState& state, const Player& mover,
                              Position from) {
    for (const Position& apos : from.getAdjacent()) {
        if (!apos.isOnPitch()) continue;
        const Player* opp = state.getPlayerAtPosition(apos);
        if (opp && opp->teamSide != mover.teamSide &&
            exertsTacklezone(opp->state) && !opp->lostTacklezones &&
            opp->hasSkill(SkillName::Tackle)) {
            return true;
        }
    }
    return false;
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

int movementAfterStandUp(const Player& player) {
    const int mv = static_cast<int>(player.movementRemaining);
    if (player.state != PlayerState::PRONE) return mv;
    if (player.hasSkill(SkillName::JumpUp)) return mv;   // r. 8196-8198
    return mv >= 3 ? mv - 3 : 0;                          // r. 690-695
}

int maxGfiSquares(const Player& player) {
    if (player.rooted) return 0;                                  // r. 8577-8578
    return player.hasSkill(SkillName::Sprint) ? 3 : 2;            // r. 8487-8490
}

int calculateLeapTarget(const Player& player) {
    int target = 7 - player.stats.agility;
    if (player.hasSkill(SkillName::VeryLongLegs)) target -= 1;
    return std::clamp(target, 2, 6);
}

int calculateDodgeTarget(const GameState& state, const Player& player,
                         Position dest, Position source) {
    int ag = player.stats.agility;
    // Break Tackle, l. 7987-7990: "The player may use his Strength instead of
    // his Agility when making a Dodge roll ... This skill may only be used once
    // per turn." Limit hlida `breakTackleUsedThisTurn` (P77, 30.09.2026).
    if (player.hasSkill(SkillName::BreakTackle) && !player.breakTackleUsedThisTurn &&
        player.stats.strength > ag) {
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

int calculateCatchTarget(const GameState& state, const Player& catcher, int modifier, bool accuratePass) {
    int target = 7 - catcher.stats.agility - modifier;

    if (!catcher.hasSkill(SkillName::NervesOfSteel)) {
        target += countTacklezones(state, catcher.position, catcher.teamSide);
    }

    target += countDisturbingPresence(state, catcher.position, catcher.teamSide);

    if (catcher.hasSkill(SkillName::ExtraArms)) target -= 1;
    // OPRAVENO 07.10.2026 (P165) — tady bylo +1 z Diving Catch na KAŽDÉ chytání. Pravidla
    // ř. 8063-8065: "may add 1 to any catch roll from an accurate pass targeted to his square"
    // ⇒ jen přesná přihrávka; ne odskok, předání, výkop ani vhazování.
    if (accuratePass && catcher.hasSkill(SkillName::DivingCatch)) target -= 1;
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

Position looseBallSquare(const GameState& state) {
    if (!state.ball.isHeld && state.ball.isOnPitch()) return state.ball.position;
    if (state.kickoffBallInAir && state.kickoffLanding.isOnPitch()) {
        const bool homeReceives = state.kickingTeam == TeamSide::AWAY;
        if (homeReceives == (state.kickoffLanding.x <= 12)) return state.kickoffLanding;
    }
    return {-1, -1};
}

Position scatterDirection(int d8) {
    // Clockwise from North: 1=N, 2=NE, 3=E, 4=SE, 5=S, 6=SW, 7=W, 8=NW
    static const int8_t offsets[8][2] = {
        {0,-1}, {1,-1}, {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}
    };
    int idx = std::clamp(d8, 1, 8) - 1;
    return {offsets[idx][0], offsets[idx][1]};
}

bool attemptRollLogged(GameState& state, int playerId, DiceRollerBase& dice, int target,
                       SkillName skillReroll, bool skillNegatedByOpponent, bool canUseTeamReroll,
                       std::vector<GameEvent>* events, GameEvent::Type type, Position from, Position to) {
    int rolled = 0;
    const bool ok = attemptRoll(state, playerId, dice, target, skillReroll, skillNegatedByOpponent,
                                canUseTeamReroll, events, &rolled);
    emitEvent(events, {type, playerId, -1, from, to, target, ok, rolled});
    return ok;
}

void spendTeamRerollByEngine(TeamState& team, int playerId, std::vector<GameEvent>* events) {
    team.rerolls--;
    team.rerollUsedThisTurn = true;
    if (coachModeOn()) emitEvent(events, {GameEvent::Type::REROLL, playerId, -1, {}, {}, 4, true});
}

int throwRollMods(const GameState& state, const Player& thrower, PassRange band) {
    if (thrower.hasSkill(SkillName::StrongArm) && band != PassRange::QUICK_PASS)
        band = static_cast<PassRange>(static_cast<int>(band) - 1);
    int mods = passModifier(band);   // rychlá +1, krátká 0, dlouhá −1, dlouhá bomba −2
    if (thrower.hasSkill(SkillName::Accurate)) mods += 1;
    if (!thrower.hasSkill(SkillName::NervesOfSteel)) mods -= countTacklezones(state, thrower.position, thrower.teamSide);
    mods -= countDisturbingPresence(state, thrower.position, thrower.teamSide);
    if (state.weather == Weather::VERY_SUNNY) mods -= 1;   // jen Very Sunny; déšť patří k chytání, vánice k dosahu
    return mods;
}

PassLegality passTargetLegal(const GameState& state, const Player& thrower, Position target) {
    PassLegality r{false, false, PassRange::LONG_BOMB};
    if (passRangeFromOffset(target.x - thrower.position.x, target.y - thrower.position.y, r.band)) {
        r.legal = passBandAllowedInWeather(r.band, state.weather);
        return r;
    }
    r.hailMary = thrower.hasSkill(SkillName::HailMaryPass) && state.weather != Weather::BLIZZARD;
    r.legal = r.hailMary;
    return r;
}

bool ttmTargetLegal(const Player& thrower, Position target, PassRange* band) {
    PassRange r;
    if (!passRangeFromOffset(target.x - thrower.position.x, target.y - thrower.position.y, r)) return false;
    if (band) *band = r;
    return r == PassRange::QUICK_PASS || r == PassRange::SHORT_PASS;
}

bool canBeThrownBy(const Player& thrower, const Player& mate) {
    return mate.teamSide == thrower.teamSide && mate.id != thrower.id && mate.state == PlayerState::STANDING &&
           mate.hasSkill(SkillName::RightStuff) && mate.position.distanceTo(thrower.position) == 1;
}

bool sideHasPlayerWithin(const GameState& state, TeamSide side, Position c, int r, bool standingOnly) {
    bool found = false;
    state.forEachOnPitch(side, [&](const Player& p) {
        if ((!standingOnly || p.state == PlayerState::STANDING) && p.position.distanceTo(c) <= r) found = true;
    });
    return found;
}

bool canCatchBomb(const Player& p) {
    return p.state == PlayerState::STANDING && !p.lostTacklezones && !p.hasSkill(SkillName::NoHands);
}

namespace {
// Obsazení hřiště pro výběr cíle bomby: 26×15 polí × 9 dotazů by jinak každý procházel všechny hráče
// (getPlayerAtPosition). Při více hráčích na poli platí první, jako getPlayerAtPosition.
struct Occupancy {
    const Player* at[26][15] = {};
    explicit Occupancy(const GameState& state) {
        for (const auto& p : state.players)
            if (p.isOnPitch() && p.position.isOnPitch() && !at[p.position.x][p.position.y]) at[p.position.x][p.position.y] = &p;
    }
    const Player* operator()(Position q) const { return at[q.x][q.y]; }
};

template <typename Lookup>
double bombValueWith(const GameState& state, const Lookup& playerAt, Position c, TeamSide mySide, bool& hitsUs) {
    double v = 0.0;
    hitsUs = false;
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy) {
            const Position q{static_cast<int8_t>(c.x + dx), static_cast<int8_t>(c.y + dy)};
            if (!q.isOnPitch()) continue;
            const Player* p = playerAt(q);
            if (!p) continue;
            const bool centre = dx == 0 && dy == 0;
            const double hit = centre ? 1.0 : 0.5;
            if (p->teamSide == mySide) { hitsUs = true; v -= 2.0 * hit; continue; }
            double w = p->state == PlayerState::STANDING ? 1.0 : 0.3;
            if (centre && canCatchBomb(*p)) {
                // přesná bomba: chytá s +1 (Diving Catch platí); Catch přehodí neúspěch
                double fail = (calculateCatchTarget(state, *p, 1, true) - 1) / 6.0;
                if (p->hasSkill(SkillName::Catch)) fail *= fail;
                w *= fail;
            }
            v += hit * w;
        }
    return v;
}

bool hailMaryLandingClear(const GameState& state, Position target, TeamSide throwerSide) {
    return !sideHasPlayerWithin(state, opponent(throwerSide), target, kHailMaryClearRadius, true);
}
}  // namespace

double bombSquareValue(const GameState& state, Position c, TeamSide mySide, bool& hitsUs) {
    return bombValueWith(state, [&](Position q) { return state.getPlayerAtPosition(q); }, c, mySide, hitsUs);
}

bool shouldInterceptBomb(const GameState& state, TeamSide interceptorSide, Position target) {
    return sideHasPlayerWithin(state, interceptorSide, target, 1, false);
}

BombChoice chooseBombTarget(const GameState& state, const Player& thrower, bool mustThrow) {
    const Occupancy occ(state);
    const TeamSide side = thrower.teamSide;
    if (mustThrow) {
        // hodit se musí: nejvyšší cena ze všech legálních polí, při shodě nejdál od našich (vzdálenost jen při shodě)
        BombChoice best;
        best.value = -1e9;
        int bestFar = -1;
        for (int x = 0; x < 26; ++x)
            for (int y = 0; y < 15; ++y) {
                const Position c{static_cast<int8_t>(x), static_cast<int8_t>(y)};
                if (c == thrower.position) continue;
                const PassLegality leg = passTargetLegal(state, thrower, c);
                if (!leg.legal) continue;
                bool hitsUs = false;
                const double v = bombValueWith(state, occ, c, side, hitsUs);
                if (v < best.value) continue;
                int far = 99;
                state.forEachOnPitch(side, [&](const Player& p) { far = std::min(far, p.position.distanceTo(c)); });
                if (v > best.value || far > bestFar) { best = {c, v, leg.hailMary}; bestFar = far; }
            }
        return best;
    }
    // AI: cena ≥ 1 jde jen na poli, které má v dosahu výbuchu soupeře (prázdné pole 0, naši jen ubírají)
    bool nearEnemy[26][15] = {};
    state.forEachOnPitch(opponent(side), [&](const Player& e) {
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy) {
                const Position q{static_cast<int8_t>(e.position.x + dx), static_cast<int8_t>(e.position.y + dy)};
                if (q.isOnPitch()) nearEnemy[q.x][q.y] = true;
            }
    });
    BombChoice best[2];            // [0] pravítko, [1] Hail Mary
    best[0].value = best[1].value = 0.999;
    for (int x = 0; x < 26; ++x)
        for (int y = 0; y < 15; ++y) {
            if (!nearEnemy[x][y]) continue;
            const Position c{static_cast<int8_t>(x), static_cast<int8_t>(y)};
            if (c == thrower.position) continue;
            const PassLegality leg = passTargetLegal(state, thrower, c);
            if (!leg.legal) continue;
            bool hitsUs = false;
            const double v = bombValueWith(state, occ, c, side, hitsUs);
            BombChoice& b = best[leg.hailMary ? 1 : 0];
            if (hitsUs || v <= b.value) continue;
            if (leg.hailMary && sideHasPlayerWithin(state, side, c, 4, false)) continue;
            b = {c, v, leg.hailMary};
        }
    if (best[0].pos.x >= 0) return best[0];
    return best[1].pos.x >= 0 ? best[1] : BombChoice{};
}

bool hailMaryPassWorthOffering(const GameState& state, const Player& thrower, const Player& target) {
    const bool ahead = thrower.teamSide == TeamSide::HOME ? target.position.x > thrower.position.x
                                                          : target.position.x < thrower.position.x;
    return ahead && hailMaryLandingClear(state, target.position, thrower.teamSide);
}

PassChance passCompletionChance(double pThrow, double qCatch, bool passSkill, bool catchSkill,
                                bool teamReroll, bool throwerLoner, bool catcherLoner) {
    const double lt = throwerLoner ? 0.5 : 1.0;   // Loner 4+ (ř. 8287-8290)
    const double lc = catcherLoner ? 0.5 : 1.0;
    const double qSkill = catchSkill ? qCatch + (1.0 - qCatch) * qCatch : qCatch;
    // chycení, když je týmový přehoz ještě po ruce (Catch má přednost — nestojí přehoz)
    const double qTeam = catchSkill ? qSkill : qCatch + (1.0 - qCatch) * lc * qCatch;
    if (pThrow < 0) {   // předání: žádný hod (pass_handler resolveHandOff)
        return {teamReroll ? qTeam : qSkill, qSkill};
    }
    const double pSkill = passSkill ? pThrow + (1.0 - pThrow) * pThrow : pThrow;
    const double skillOnly = pSkill * qSkill;
    if (!teamReroll) return {skillOnly, skillOnly};
    if (passSkill) return {pSkill * qTeam, skillOnly};   // hod přehodí Pass, týmový přehoz zbude na chycení
    // bez Pass: přesný napoprvé ⇒ přehoz zbude na chycení; jinak padne na hod (i když Loner neprojde)
    const double withTeam = pThrow * qTeam + (1.0 - pThrow) * lt * pThrow * qSkill;
    return {withTeam, skillOnly};
}

// Loner po spotřebovaném týmovém přehozu (pravidla ř. 8287-8290): 4+, jinak „the original result
// stands ... but the team re-roll is lost“. P244 (3.11.2026): dřív se brána hodila na čtyřech
// místech bez události — v živé partii (ogr lidí, GFI) nešlo poznat, zda propadl Loner, nebo padl
// znovu přehozený hod. Neúspěch = REROLL druh 3 (die1 = hod Loner), jako u kouče.
bool lonerAllowsTeamReroll(const Player& player, DiceRollerBase& dice, std::vector<GameEvent>* events) {
    if (!player.hasSkill(SkillName::Loner)) return true;
    const int lonerRoll = dice.rollD6();
    if (lonerRoll >= 4) return true;
    if (coachModeOn()) emitEvent(events, {GameEvent::Type::REROLL, player.id, -1, {}, {}, 3, false, lonerRoll, 0});
    return false;
}

bool coachAskReroll(GameState& state, Player& player, DiceRollerBase& dice,
                    CoachQuestion question, int otherId, int shown, int target,
                    bool teamAllowed, std::vector<GameEvent>* events, bool& viaPro) {
    viaPro = false;
    TeamState& team = state.getTeamState(player.teamSide);
    const bool proAvail = player.hasSkill(SkillName::Pro) && !player.proUsedThisTurn;
    const bool teamAvail = teamAllowed && team.canUseReroll();
    if (!proAvail && !teamAvail) return false;
    const int opts = (1 << CoachAnswer::NONE) | (teamAvail ? 1 << CoachAnswer::TEAM : 0) |
                     (proAvail ? 1 << CoachAnswer::PRO : 0);
    const int answer = askCoach(question, player.id, otherId, shown, target, opts);
    if (answer == CoachAnswer::NONE) return false;
    // Týmový přehoz: spotřebuje se, Loner napřed 4+ (l. 8287-8290: na 1-3
    // "the original result stands ... but the team re-roll is lost").
    auto spendTeamReroll = [&]() {
        team.rerolls--;
        team.rerollUsedThisTurn = true;
        return lonerAllowsTeamReroll(player, dice, events);
    };
    if (answer == CoachAnswer::TEAM) return spendTeamReroll();
    // Pro, l. 8385-8389: 4+ přehoz; na 1-3 původní výsledek platí, hod na Pro
    // ale smí přehodit týmový reroll -- druhá otázka.
    viaPro = true;
    player.proUsedThisTurn = true;
    int proRoll = dice.rollD6();
    emitEvent(events, {GameEvent::Type::REROLL, player.id, -1, {}, {}, 1, proRoll >= 4, proRoll, 0});
    if (proRoll < 4 && teamAvail &&
        askCoach(CoachQuestion::PRO_GATE_REROLL, player.id, otherId, proRoll, 4,
                 (1 << CoachAnswer::NONE) | (1 << CoachAnswer::TEAM)) == CoachAnswer::TEAM) {
        if (spendTeamReroll()) {
            const int gate = proRoll;
            proRoll = dice.rollD6();
            emitEvent(events, {GameEvent::Type::REROLL, player.id, -1, {}, {}, 2, proRoll >= 4, gate, proRoll});
        }
    }
    return proRoll >= 4;
}

// Kouč rozhodl, že se přehazuje: nový hod + událost (Pro: SKILL_USED, jinak REROLL původní/nový). Jediné místo
// rozhodování kouče o přehozu hodu (přihrávka, bomba, ostatní hody přes attemptRoll). Vrací, zda se přehazovalo.
// `fumbleMods` = modifikátor fumblu hodu (0 u hodů bez fumblu); `teamAllowed` = smí kouč vzít týmový přehoz.
static bool coachRerollRoll(GameState& state, Player& player, DiceRollerBase& dice, CoachQuestion question,
                            std::vector<GameEvent>* events, int& roll, int target, int fumbleMods,
                            bool teamAllowed) {
    const int original = roll;
    bool viaPro = false;
    if (!coachAskReroll(state, player, dice, question, -1, roll, target, teamAllowed, events, viaPro)) return false;
    roll = dice.rollD6();
    const bool ok = roll >= target && !isThrowFumble(roll, fumbleMods);
    if (viaPro) {
        emitEvent(events, {GameEvent::Type::SKILL_USED, player.id, -1, {}, {},
                          static_cast<int>(SkillName::Pro), ok, roll});
    } else {
        emitEvent(events, {GameEvent::Type::REROLL, player.id, -1, {}, {}, 0, ok, original, roll});
    }
    return true;
}

// Přehoz bez kouče: Pro -> týmový přehoz (jeden hod = nejvýš jeden přehoz, ř. 925-927). Pro, ř. 8385-8389: hod D6,
// na 4-6 se přehazuje, na 1-3 původní výsledek platí a týmový přehoz smí přehodit jen HOD PRO (P70). `roll` se
// přepíše novým hodem, jen když přehoz proběhl. Společné pro hody míče/bomby i attemptRoll.
static void engineRerollProTeam(GameState& state, Player& thrower, DiceRollerBase& dice,
                                std::vector<GameEvent>* events, int& roll, int target, int fumbleMods,
                                bool teamAllowed) {
    auto ok = [&](int r) { return r >= target && !isThrowFumble(r, fumbleMods); };
    TeamState& team = state.getTeamState(thrower.teamSide);
    if (thrower.hasSkill(SkillName::Pro) && !thrower.proUsedThisTurn) {
        thrower.proUsedThisTurn = true;
        int proRoll = dice.rollD6();
        if (proRoll < 4 && teamAllowed && team.canUseReroll()) {
            spendTeamRerollByEngine(team, thrower.id, events);
            if (lonerAllowsTeamReroll(thrower, dice, events)) proRoll = dice.rollD6();
        }
        if (proRoll >= 4) {
            roll = dice.rollD6();
            emitEvent(events, {GameEvent::Type::SKILL_USED, thrower.id, -1, {}, {},
                              static_cast<int>(SkillName::Pro), ok(roll), roll});   // review P247: kostka do záznamu
        }
    } else if (teamAllowed && team.canUseReroll()) {
        spendTeamRerollByEngine(team, thrower.id, events);
        if (lonerAllowsTeamReroll(thrower, dice, events)) {
            const int original = roll;
            roll = dice.rollD6();
            // P244: v živé partii i u strany bez kouče původní a nový hod
            if (coachModeOn())
                emitEvent(events, {GameEvent::Type::REROLL, thrower.id, -1, {}, {}, 0, ok(roll), original, roll});
        }
    }
}

// Jeden žebříček přehozu hodu "hází se míč" (přihrávka i bomba): Pass (zdarma) -> kouč (živá partie) | Pro -> týmový
// přehoz. Volající sám rozhodne, ZDA přehazovat. `teamAllowedForEngine` = smí týmový přehoz engine (bomba s Lonerem
// ne); kouč rozhoduje sám a smí vždy.
// ZMĚNĚNO 6.11.2026 (P247, uživatel: „přehoď i nepřesnou vždy“) — u přihrávky dřív Pro a týmový přehoz jen na
// FUMBLE; nepřesná přihrávka je turnover taky (ř. 881-889), ř. 929-931: týmový přehoz smí přehodit kterýkoli hod
// kromě zbroje/zranění. P251 (6.11.2026): bomba tentýž žebříček (ř. 7957-7958).
// P258 (7.11.2026): tři kopie (přihrávka, bomba, attemptRoll) sloučeny; událost Pass má die1 = nový hod,
// die2 = původní (dřív bez kostek, jako u attemptRoll P145).
void rerollThrow(GameState& state, Player& thrower, DiceRollerBase& dice, CoachQuestion coachQuestion,
                 std::vector<GameEvent>* events, int& roll, int target, int fumbleMods,
                 bool teamAllowedForEngine) {
    if (thrower.hasSkill(SkillName::Pass)) {
        // Ř. 8336-8337: Pass přehazuje nepřesný hod i fumble, zdarma.
        const int original = roll;
        roll = dice.rollD6();
        emitEvent(events, {GameEvent::Type::SKILL_USED, thrower.id, -1, {}, {},
                          static_cast<int>(SkillName::Pass), roll >= target && !isThrowFumble(roll, fumbleMods),
                          roll, original});
        return;
    }
    // OPRAVENO 9.11.2026 (review P264, M1) — kouč dostal týmový přehoz vždy, i při hodu chycené bomby týmem, který
    //   není na tahu; ř. 929-932: týmové přehozy jen ve vlastním tahu (teamRerollAllowed, pro kouče i engine).
    const bool teamOnTurn = teamRerollAllowed(state, thrower);
    // P130: v živé partii rozhoduje kouč (u fumble i nepřesné; schváleno 03.10. a 10.10.).
    if (coachDecides(thrower.teamSide)) {
        coachRerollRoll(state, thrower, dice, coachQuestion, events, roll, target, fumbleMods, teamOnTurn);
        return;
    }
    engineRerollProTeam(state, thrower, dice, events, roll, target, fumbleMods, teamAllowedForEngine && teamOnTurn);
}

static bool attemptRollImpl(GameState& state, int playerId, DiceRollerBase& dice,
                            int target, SkillName skillReroll,
                            bool skillNegatedByOpponent, bool canUseTeamReroll,
                            std::vector<GameEvent>* events, int& roll);

bool attemptRoll(GameState& state, int playerId, DiceRollerBase& dice,
                 int target, SkillName skillReroll,
                 bool skillNegatedByOpponent, bool canUseTeamReroll,
                 std::vector<GameEvent>* events, int* rolled) {
    int roll = 0;
    const bool ok = attemptRollImpl(state, playerId, dice, target, skillReroll,
                                    skillNegatedByOpponent, canUseTeamReroll, events, roll);
    if (rolled) *rolled = roll;
    return ok;
}

// `roll` = kostka, která platí (viz `rolled` v hlavičce).
bool teamRerollAllowed(const GameState& state, const Player& player) {
    return player.teamSide == state.activeTeam && !state.kickoffInProgress;
}

static bool attemptRollImpl(GameState& state, int playerId, DiceRollerBase& dice,
                            int target, SkillName skillReroll,
                            bool skillNegatedByOpponent, bool canUseTeamReroll,
                            std::vector<GameEvent>* events, int& roll) {
    Player& player = state.getPlayer(playerId);
    // Týmový přehoz jen hráči týmu na tahu a nikdy během výkopu -- platí pro
    // všechny hody, nejen pro chytání, takže tu, ne u volajících.
    canUseTeamReroll = canUseTeamReroll && teamRerollAllowed(state, player);

    // Initial roll
    roll = dice.rollD6();
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
        const int original = roll;
        roll = dice.rollD6();
        // P145: die1 = nový hod, die2 = původní (přehozený) hod.
        emitEvent(events, {GameEvent::Type::SKILL_USED, playerId, -1, {}, {},
                          static_cast<int>(skillReroll), roll >= target, roll, original});
        return roll >= target;   // hod už byl jednou přehozen -- konec
    }

    // P130: v živé partii o přehozu rozhoduje kouč (nechat / tým / Pro); jinak engine: Pro -> týmový přehoz.
    // Hod už selhal ⇒ když přehoz neproběhne (Pro/Loner neprojde), `roll` zůstává původní a nesplňuje cíl.
    if (coachDecides(player.teamSide)) {
        coachRerollRoll(state, player, dice, CoachQuestion::REROLL, events, roll, target, /*fumbleMods=*/0,
                        canUseTeamReroll);
    } else {
        engineRerollProTeam(state, player, dice, events, roll, target, /*fumbleMods=*/0, canUseTeamReroll);
    }
    return roll >= target;
}

} // namespace bb
