#include "bb/pass_handler.h"
#include "bb/ball_handler.h"
#include "bb/helpers.h"
#include <algorithm>
#include <cmath>

namespace bb {

namespace {

// Bresenham line from src to dst, returning all positions along the path
// (excluding src and dst themselves)
int getPassPath(Position src, Position dst, Position* out, int maxOut) {
    int count = 0;
    int x0 = src.x, y0 = src.y;
    int x1 = dst.x, y1 = dst.y;

    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    // Skip the start position
    while (true) {
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }

        // Reached destination
        if (x0 == x1 && y0 == y1) break;

        if (count < maxOut) {
            out[count++] = {static_cast<int8_t>(x0), static_cast<int8_t>(y0)};
        }
    }
    return count;
}

// Check for interception along pass path
// Returns interceptor player ID or -1
int checkInterception(GameState& state, int passerId, Position target,
                      DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& passer = state.getPlayer(passerId);
    TeamSide enemySide = opponent(passer.teamSide);

    // Get pass path
    Position path[30];
    int pathLen = getPassPath(passer.position, target, path, 30);

    // Find first eligible interceptor along path
    for (int i = 0; i < pathLen; i++) {
        if (!path[i].isOnPitch()) continue;

        // Check for standing enemy at this position
        const Player* interceptor = state.getPlayerAtPosition(path[i]);
        if (!interceptor || interceptor->teamSide != enemySide) continue;
        if (!canAct(interceptor->state) || interceptor->lostTacklezones) continue;
        if (interceptor->hasSkill(SkillName::NoHands)) continue;

        // Interception target: 7 - AG + 2 (base modifier)
        int intTarget = 7 - interceptor->stats.agility + 2;
        if (interceptor->hasSkill(SkillName::VeryLongLegs)) intTarget -= 1;
        if (interceptor->hasSkill(SkillName::ExtraArms)) intTarget -= 1;

        if (!interceptor->hasSkill(SkillName::NervesOfSteel)) {
            intTarget += countTacklezones(state, interceptor->position, interceptor->teamSide);
        }

        intTarget = std::clamp(intTarget, 2, 6);

        // Interception attempt
        int roll = dice.rollD6();
        bool success = (roll >= intTarget);

        // SafeThrow: if intercepted, passer can force a reroll
        if (success && passer.hasSkill(SkillName::SafeThrow)) {
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
        // Only first eligible interceptor gets a chance (simplified)
        break;
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
            resolveThrowIn(state, target, dice, events);
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
    PassRange range = passRangeFromDistance(dist);

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

    // Weather
    if (state.weather == Weather::POURING_RAIN || state.weather == Weather::BLIZZARD ||
        state.weather == Weather::VERY_SUNNY) {
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
            resolveThrowIn(state, target, dice, events);
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

    // Mark pass used
    state.getTeamState(giver.teamSide).passUsedThisTurn = true;
    giver.hasActed = true;

    // Must be adjacent
    if (giver.position.distanceTo(receiver.position) != 1) {
        return ActionResult::fail();
    }

    // Ball must be held by giver
    if (!state.ball.isHeld || state.ball.carrierId != giverId) {
        return ActionResult::turnovr();
    }

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
