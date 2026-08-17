#include "bb/pass_handler.h"
#include "bb/ball_handler.h"
#include "bb/helpers.h"
#include <algorithm>
#include <cmath>

namespace bb {

namespace {

// Squared distance from a square centre to the segment thrower->target,
// scaled by 4 so it stays in integers (half-square precision is enough).
static int distSqToSegment4(Position p, Position a, Position b) {
    int apx = 2 * (p.x - a.x), apy = 2 * (p.y - a.y);
    int abx = 2 * (b.x - a.x), aby = 2 * (b.y - a.y);
    int denom = abx * abx + aby * aby;
    if (denom == 0) return apx * apx + apy * apy;
    int t = apx * abx + apy * aby;          // projection numerator
    if (t <= 0) return apx * apx + apy * apy;
    if (t >= denom) {
        int bpx = apx - abx, bpy = apy - aby;
        return bpx * bpx + bpy * bpy;
    }
    // Perpendicular component, kept exact: |ap|^2 - t^2/denom, times denom.
    long long num = static_cast<long long>(apx * apx + apy * apy) * denom
                    - static_cast<long long>(t) * t;
    return static_cast<int>(num / denom);
}

// Check for interception. The RANGE RULER HAS WIDTH (rules parity,
// 2026-08-10): eligibility is "have the plastic Range Ruler pass over at
// least part of the square the intercepting player is standing in", i.e. a
// CORRIDOR of about one square either side of the thrower->target line, not
// the mathematical line itself. The BB2025 wording is blunter and agrees:
// "if the Range Ruler overlaps any squares containing a Standing opposition
// player". Walking a Bresenham line demanded distance ~0 and so allowed only
// a fraction of the players the rules let try -- which quietly favoured the
// throwing side. CRP also requires the interceptor to have a tackle zone and
// to be closer to each end than the ends are to each other, and lets only
// ONE player attempt; where several qualify we take the likeliest, since a
// free choice with no downside is always used (user's rule, 2026-08-10).
int checkInterception(GameState& state, int passerId, Position target,
                      DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& passer = state.getPlayer(passerId);
    TeamSide enemySide = opponent(passer.teamSide);

    // Pick the single best eligible interceptor inside the ruler corridor.
    const Player* interceptor = nullptr;
    int bestTarget = 99;
    int throwLen = passer.position.distanceTo(target);
    state.forEachOnPitch(enemySide, [&](const Player& p) {
        if (!canAct(p.state) || p.lostTacklezones) return;   // must have a TZ
        if (p.hasSkill(SkillName::NoHands)) return;
        if (p.position == passer.position || p.position == target) return;
        // Closer to each end than the ends are to each other (CRP).
        if (p.position.distanceTo(passer.position) >= throwLen) return;
        if (p.position.distanceTo(target) >= throwLen) return;
        // Ruler overlaps the square: centre within ~1 square of the line.
        if (distSqToSegment4(p.position, passer.position, target) > 4) return;

        int t = 7 - p.stats.agility + 2;
        if (p.hasSkill(SkillName::VeryLongLegs)) t -= 1;
        if (p.hasSkill(SkillName::ExtraArms)) t -= 1;
        if (!p.hasSkill(SkillName::NervesOfSteel)) {
            t += countTacklezones(state, p.position, p.teamSide);
        }
        if (state.weather == Weather::POURING_RAIN) t += 1;
        t = std::clamp(t, 2, 6);
        if (t < bestTarget || (t == bestTarget && interceptor &&
                               p.hasSkill(SkillName::Catch) &&
                               !interceptor->hasSkill(SkillName::Catch))) {
            bestTarget = t;
            interceptor = &p;
        }
    });

    if (interceptor) {
        // Interception target: 7 - AG + 2 (base modifier)
        int intTarget = 7 - interceptor->stats.agility + 2;
        if (interceptor->hasSkill(SkillName::VeryLongLegs)) intTarget -= 1;
        if (interceptor->hasSkill(SkillName::ExtraArms)) intTarget -= 1;

        if (!interceptor->hasSkill(SkillName::NervesOfSteel)) {
            intTarget += countTacklezones(state, interceptor->position, interceptor->teamSide);
        }

        // Pouring Rain applies to interceptions too (rules parity,
        // 2026-08-10). CRP: "A -1 modifier applies to all catch, intercept,
        // or pick-up rolls." We had it on catch and pick-up but not here.
        if (state.weather == Weather::POURING_RAIN) intTarget += 1;

        intTarget = std::clamp(intTarget, 2, 6);

        // Interception attempt
        int roll = dice.rollD6();
        bool success = (roll >= intTarget);

        // Catch re-rolls a FAILED interception (rules parity, 2026-08-10).
        // CRP Catch: "allowed to re-roll the D6 if he fails a catch roll.
        // It also allows the player to re-roll the D6 if he drops a hand-off
        // or fails to make an interception." A skill re-roll is free and
        // never competes with the scarce team re-roll, so it is always taken
        // (user's rule, 2026-08-10: optional skills default ON when they
        // cost nothing).
        if (!success && interceptor->hasSkill(SkillName::Catch)) {
            emitEvent(events, {GameEvent::Type::SKILL_USED, interceptor->id, -1, {}, {},
                              static_cast<int>(SkillName::Catch), true});
            roll = dice.rollD6();
            success = (roll >= intTarget);
        }

        // SafeThrow: if intercepted, passer can force a reroll -- but Very
        // Long Legs blocks it (rules parity, 2026-08-10). CRP Very Long
        // Legs: "the Safe Throw skill may not be used to affect any
        // Interception rolls made by this player."
        if (success && passer.hasSkill(SkillName::SafeThrow) &&
            !interceptor->hasSkill(SkillName::VeryLongLegs)) {
            int reroll = dice.rollD6();
            if (reroll < intTarget) {
                success = false;
                emitEvent(events, {GameEvent::Type::SKILL_USED, passerId, -1, {}, {},
                                  static_cast<int>(SkillName::SafeThrow), true});
            }
        }

        if (success) {
            // Interceptor catches the ball
            state.ball = BallState::carried(interceptor->position, interceptor->id);
            emitEvent(events, {GameEvent::Type::CATCH, interceptor->id, passerId,
                              interceptor->position, {}, intTarget, true});
            return interceptor->id;
        }
    }
    return -1;
}

} // anonymous namespace

ActionResult resolvePass(GameState& state, int passerId, Position target,
                         DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& passer = state.getPlayer(passerId);

    // Mark pass used
    state.getTeamState(passer.teamSide).passUsedThisTurn = true;
    passer.hasActed = true;

    // Ball must be held by passer
    if (!state.ball.isHeld || state.ball.carrierId != passerId) {
        return ActionResult::turnovr();
    }

    bool isHailMary = passer.hasSkill(SkillName::HailMaryPass);
    int dist = passer.position.distanceTo(target);

    // Regular pass range check (Hail Mary has no range limit)
    if (!isHailMary && dist > 13) {
        return ActionResult::turnovr();
    }

    // Release the ball from passer
    state.ball = BallState::onGround(passer.position);

    if (isHailMary) {
        // Hail Mary: no interception, D6: 1=fumble, 2+=inaccurate
        int hmpRoll = dice.rollD6();
        emitEvent(events, {GameEvent::Type::PASS, passerId, -1, passer.position, target,
                          hmpRoll, hmpRoll >= 2});

        if (hmpRoll == 1) {
            // Fumble: ball bounces from thrower
            resolveBounce(state, passer.position, dice, 0, events);
            return ActionResult::turnovr();
        }

        // Inaccurate: 3 single scatters from target
        Position landPos = target;
        for (int i = 0; i < 3; i++) {
            int d8 = dice.rollD8();
            Position scatter = scatterDirection(d8);
            landPos.x += scatter.x;
            landPos.y += scatter.y;
        }

        if (!landPos.isOnPitch()) {
            resolveThrowIn(state, target, landPos, dice, events);
            return ActionResult::turnovr();
        }

        state.ball = BallState::onGround(landPos);

        // Catch attempt at landing (no modifier)
        Player* catcher = state.getPlayerAtPosition(landPos);
        if (catcher && canAct(catcher->state) && catcher->teamSide == passer.teamSide) {
            if (resolveCatch(state, catcher->id, dice, 0, events)) {
                return ActionResult::ok();
            }
        } else if (catcher && canAct(catcher->state)) {
            // Enemy catch = interception
            if (resolveCatch(state, catcher->id, dice, 0, events)) {
                return ActionResult::turnovr();
            }
        }
        // Ball not caught by own team
        if (!state.ball.isHeld) {
            resolveBounce(state, landPos, dice, 0, events);
        }
        return state.ball.isHeld && state.getPlayer(state.ball.carrierId).teamSide == passer.teamSide
                   ? ActionResult::ok()
                   : ActionResult::turnovr();
    }

    // Normal pass

    // Check interception
    int interceptorId = checkInterception(state, passerId, target, dice, events);
    if (interceptorId >= 0) {
        return ActionResult::turnovr();
    }

    // Calculate pass accuracy target
    // Range comes from the ruler GRID, not a distance (rules parity,
    // 2026-08-10). Out-of-range targets are filtered out when actions are
    // generated; falling back to Long Bomb here is a belt-and-braces guard
    // so a hand-built Action can never silently become a free long throw.
    PassRange range = PassRange::LONG_BOMB;
    if (!passRangeFromOffset(target.x - passer.position.x,
                             target.y - passer.position.y, range)) {
        return ActionResult::fail();
    }

    // Blizzard restricts the RANGE, it does not tax the roll (rules parity,
    // 2026-08-10). CRP: "the snow means that only quick or short passes can
    // be attempted." Checked on the ruler band before Strong Arm, which
    // improves the modifier rather than the distance actually thrown.
    if (state.weather == Weather::BLIZZARD &&
        (range == PassRange::LONG_PASS || range == PassRange::LONG_BOMB)) {
        return ActionResult::fail();
    }

    // StrongArm reduces range by one band
    if (passer.hasSkill(SkillName::StrongArm) && range != PassRange::QUICK_PASS) {
        range = static_cast<PassRange>(static_cast<int>(range) - 1);
    }

    int passTarget = 7 - passer.stats.agility;
    passTarget -= passModifier(range);  // range modifier (QP=+1, SP=0, LP=-1, LB=-2)

    if (passer.hasSkill(SkillName::Accurate)) passTarget -= 1;

    if (!passer.hasSkill(SkillName::NervesOfSteel)) {
        passTarget += countTacklezones(state, passer.position, passer.teamSide);
    }

    passTarget += countDisturbingPresence(state, passer.position, passer.teamSide);

    // Weather: ONLY Very Sunny penalises a throw (rules parity,
    // 2026-08-10). CRP: "Very Sunny: the blinding sunshine causes a -1
    // modifier on all passing rolls"; Pouring Rain's -1 is on "catch,
    // intercept, or pick-up" and never on the throw, and Blizzard restricts
    // the RANGE ("only quick or short passes can be attempted") rather than
    // taxing the roll. We used to charge all three.
    if (state.weather == Weather::VERY_SUNNY) {
        passTarget += 1;
    }

    passTarget = std::clamp(passTarget, 2, 6);

    // Roll with Pass skill reroll chain
    int roll = dice.rollD6();

    emitEvent(events, {GameEvent::Type::PASS, passerId, -1, passer.position, target,
                      roll, roll >= passTarget});

    // Natural 1 = always fumble
    if (roll == 1) {
        // Attempt reroll (Pass skill, Pro, Team)
        bool rerolled = false;
        // Pass skill reroll
        if (passer.hasSkill(SkillName::Pass)) {
            roll = dice.rollD6();
            emitEvent(events, {GameEvent::Type::SKILL_USED, passerId, -1, {}, {},
                              static_cast<int>(SkillName::Pass), roll >= passTarget && roll != 1});
            if (roll != 1 && roll >= passTarget) {
                rerolled = true;
                // accurate pass handled below
            } else if (roll == 1) {
                // Still fumble after reroll
                resolveBounce(state, passer.position, dice, 0, events);
                return ActionResult::turnovr();
            } else {
                rerolled = true;
                // inaccurate — fall through
            }
        }

        if (!rerolled) {
            // Try Pro
            if (passer.hasSkill(SkillName::Pro) && !passer.proUsedThisTurn) {
                passer.proUsedThisTurn = true;
                int proRoll = dice.rollD6();
                if (proRoll >= 4) {
                    roll = dice.rollD6();
                    if (roll != 1 && roll >= passTarget) {
                        rerolled = true;
                    } else if (roll == 1) {
                        resolveBounce(state, passer.position, dice, 0, events);
                        return ActionResult::turnovr();
                    } else {
                        rerolled = true;
                    }
                }
            }
        }

        if (!rerolled) {
            // Try team reroll
            TeamState& team = state.getTeamState(passer.teamSide);
            if (team.canUseReroll()) {
                team.rerolls--;
                team.rerollUsedThisTurn = true;
                if (passer.hasSkill(SkillName::Loner)) {
                    int lonerRoll = dice.rollD6();
                    if (lonerRoll < 4) {
                        resolveBounce(state, passer.position, dice, 0, events);
                        return ActionResult::turnovr();
                    }
                }
                roll = dice.rollD6();
                if (roll != 1 && roll >= passTarget) {
                    rerolled = true;
                } else if (roll == 1) {
                    resolveBounce(state, passer.position, dice, 0, events);
                    return ActionResult::turnovr();
                } else {
                    rerolled = true;
                }
            }
        }

        if (!rerolled) {
            // Fumble: ball bounces from passer
            resolveBounce(state, passer.position, dice, 0, events);
            return ActionResult::turnovr();
        }
    }

    bool accurate = (roll >= passTarget);

    if (accurate) {
        // Ball lands at target
        state.ball = BallState::onGround(target);
        Player* catcher = state.getPlayerAtPosition(target);
        if (catcher && canAct(catcher->state) && !catcher->hasSkill(SkillName::NoHands)) {
            // Catch with +1 modifier for accurate pass
            if (resolveCatch(state, catcher->id, dice, 1, events)) {
                return catcher->teamSide == passer.teamSide
                           ? ActionResult::ok()
                           : ActionResult::turnovr();
            }
        }
    } else {
        // Inaccurate: scatter D8 + D6 from target
        int scatterDir = dice.rollD8();
        int scatterDist = dice.rollD6();
        Position scatter = scatterDirection(scatterDir);
        Position landPos{
            static_cast<int8_t>(target.x + scatter.x * scatterDist),
            static_cast<int8_t>(target.y + scatter.y * scatterDist)
        };

        if (!landPos.isOnPitch()) {
            resolveThrowIn(state, target, landPos, dice, events);
            return ActionResult::turnovr();
        }

        state.ball = BallState::onGround(landPos);

        // Catch attempt at landing (no modifier)
        Player* catcher = state.getPlayerAtPosition(landPos);
        if (catcher && canAct(catcher->state) && !catcher->hasSkill(SkillName::NoHands)) {
            if (resolveCatch(state, catcher->id, dice, 0, events)) {
                return catcher->teamSide == passer.teamSide
                           ? ActionResult::ok()
                           : ActionResult::turnovr();
            }
        }
    }

    // Ball not caught — bounce
    if (!state.ball.isHeld) {
        resolveBounce(state, state.ball.position, dice, 0, events);
    }

    // If own team caught it somehow (via bounce), not a turnover
    if (state.ball.isHeld && state.getPlayer(state.ball.carrierId).teamSide == passer.teamSide) {
        return ActionResult::ok();
    }

    return ActionResult::turnovr();
}

ActionResult resolveHandOff(GameState& state, int giverId, int receiverId,
                            DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& giver = state.getPlayer(giverId);
    Player& receiver = state.getPlayer(receiverId);

    // Mark the HAND-OFF allowance used, not the pass one (P4/P26, 2026-08-17).
    // CRP gives the hand-off its own once-per-turn declaration.
    state.getTeamState(giver.teamSide).handOffUsedThisTurn = true;
    giver.hasActed = true;

    // Must be adjacent
    if (giver.position.distanceTo(receiver.position) != 1) {
        return ActionResult::fail();
    }

    // Ball must be held by giver
    if (!state.ball.isHeld || state.ball.carrierId != giverId) {
        return ActionResult::turnovr();
    }

    // A hand-off left no trace of its own in the log: this function calls
    // resolveCatch and what came out was a bare CATCH, which reads exactly like
    // catching a bounce or a kick-off. So the corpus could not answer "did we
    // hand off at all" -- the check written for the pricing fix counted a
    // HAND_OFF string the engine never emitted and duly reported zero across
    // 3000 games. Declare the action here; the CATCH that follows carries the
    // roll, the same shape as PASS.
    emitEvent(events, {GameEvent::Type::HAND_OFF, giverId, receiverId,
                       giver.position, receiver.position, 0, true});

    // Transfer ball to ground at receiver position
    state.ball = BallState::onGround(receiver.position);

    // Catch with +1 modifier
    if (resolveCatch(state, receiverId, dice, 1, events)) {
        return ActionResult::ok();
    }

    // Failed catch — ball bounces from receiver's position
    if (!state.ball.isHeld) {
        resolveBounce(state, receiver.position, dice, 0, events);
    }

    // If own team caught on bounce, not a turnover
    if (state.ball.isHeld && state.getPlayer(state.ball.carrierId).teamSide == giver.teamSide) {
        return ActionResult::ok();
    }

    return ActionResult::turnovr();
}

} // namespace bb
