#include "bb/move_handler.h"
#include "bb/helpers.h"
#include "bb/injury.h"
#include "bb/ball_handler.h"
#include <algorithm>

namespace bb {

namespace {

// Check Tentacles: adjacent enemies with Tentacles contest the move
// Returns true if player is caught (movement ends)
bool checkTentacles(GameState& state, int playerId, Position from,
                    DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& mover = state.getPlayer(playerId);
    auto adj = from.getAdjacent();

    for (auto& pos : adj) {
        if (!pos.isOnPitch()) continue;
        const Player* opp = state.getPlayerAtPosition(pos);
        if (!opp || opp->teamSide == mover.teamSide) continue;
        if (!canAct(opp->state) || opp->lostTacklezones) continue;
        if (!opp->hasSkill(SkillName::Tentacles)) continue;

        // Contest: D6+moverST vs D6+tentaclesST, strictly greater to escape
        int moverRoll = dice.rollD6();
        int tentRoll = dice.rollD6();
        bool escaped = (moverRoll + mover.stats.strength) > (tentRoll + opp->stats.strength);

        emitEvent(events, {GameEvent::Type::SKILL_USED, opp->id, playerId, {}, {},
                          static_cast<int>(SkillName::Tentacles), !escaped});

        if (!escaped) {
            // Caught: movement ends, player stays at from
            mover.hasMoved = true;
            return true;
        }
        // Only one Tentacles check per dodge step
        break;
    }
    return false;
}

// Check Shadowing: after successful dodge, adjacent enemy with Shadowing may follow
void checkShadowing(GameState& state, int playerId, Position from,
                    DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& mover = state.getPlayer(playerId);
    auto adj = from.getAdjacent();

    for (auto& pos : adj) {
        if (!pos.isOnPitch()) continue;
        Player* opp = state.getPlayerAtPosition(pos);
        if (!opp || opp->teamSide == mover.teamSide) continue;
        if (!canAct(opp->state) || opp->lostTacklezones) continue;
        if (!opp->hasSkill(SkillName::Shadowing)) continue;

        // Roll: D6 + shadowMA - moverMA. If >= 6, follower moves to vacated square
        int roll = dice.rollD6();
        int total = roll + opp->stats.movement - mover.stats.movement;
        bool follows = (total >= 6);

        emitEvent(events, {GameEvent::Type::SKILL_USED, opp->id, playerId, opp->position, from,
                          static_cast<int>(SkillName::Shadowing), follows});

        if (follows) {
            // Check that vacated square is empty (should be, since we just left)
            if (!state.getPlayerAtPosition(from)) {
                opp->position = from;
            }
        }
        // Only one Shadowing attempt per dodge step
        break;
    }
}

} // anonymous namespace

ActionResult resolveMoveStep(GameState& state, int playerId, Position to,
                             DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& player = state.getPlayer(playerId);
    Position from = player.position;

    // Validate: adjacent and on-pitch
    if (from.distanceTo(to) != 1 || !to.isOnPitch()) {
        return ActionResult::fail();
    }

    // Validate: destination not occupied
    if (state.getPlayerAtPosition(to) != nullptr) {
        return ActionResult::fail();
    }

    // Check if leaving a tackle zone → dodge required
    bool needsDodge = countTacklezones(state, from, player.teamSide) > 0;

    // Check Tentacles before dodge (if leaving TZ)
    if (needsDodge) {
        if (checkTentacles(state, playerId, from, dice, events)) {
            return ActionResult::ok();  // Caught, movement ends, not turnover
        }
    }

    // Decrement movement
    player.movementRemaining--;
    player.hasMoved = true;

    // Check GFI
    bool needsGfi = false;
    if (player.movementRemaining < 0) {
        // Allow up to -2 GFI squares (or -3 with Sprint)
        int maxGfi = player.hasSkill(SkillName::Sprint) ? -3 : -2;
        if (player.movementRemaining < maxGfi) {
            player.movementRemaining++; // undo
            return ActionResult::fail();
        }
        needsGfi = true;
    }

    // Perform dodge roll if needed
    if (needsDodge) {
        int target = calculateDodgeTarget(state, player, to, from);

        // Check if Tackle negates Dodge reroll
        bool tackleNegates = false;
        auto srcAdj = from.getAdjacent();
        for (auto& apos : srcAdj) {
            if (!apos.isOnPitch()) continue;
            const Player* opp = state.getPlayerAtPosition(apos);
            if (opp && opp->teamSide != player.teamSide &&
                exertsTacklezone(opp->state) && !opp->lostTacklezones &&
                opp->hasSkill(SkillName::Tackle)) {
                tackleNegates = true;
                break;
            }
        }

        bool dodgeOk = attemptRoll(state, playerId, dice, target,
                                    SkillName::Dodge, tackleNegates, true, events);

        emitEvent(events, {GameEvent::Type::DODGE, playerId, -1, from, to,
                          target, dodgeOk});

        if (!dodgeOk) {
            // Failed dodge: player falls at destination
            player.position = to;
            player.state = PlayerState::PRONE;
            player.hasActed = true;

            InjuryContext ctx;
            resolveArmourAndInjury(state, playerId, dice, ctx, events);
            handleBallOnPlayerDown(state, playerId, dice, events);

            return ActionResult::turnovr();
        }
    }

    // Perform GFI roll if needed
    if (needsGfi) {
        int gfiTarget = (state.weather == Weather::BLIZZARD) ? 3 : 2;

        bool gfiOk = attemptRoll(state, playerId, dice, gfiTarget,
                                  SkillName::SureFeet, false, true, events);

        emitEvent(events, {GameEvent::Type::GFI, playerId, -1, from, to,
                          gfiTarget, gfiOk});

        if (!gfiOk) {
            // Failed GFI: player falls at destination
            player.position = to;
            player.state = PlayerState::PRONE;
            player.hasActed = true;

            InjuryContext ctx;
            resolveArmourAndInjury(state, playerId, dice, ctx, events);
            handleBallOnPlayerDown(state, playerId, dice, events);

            return ActionResult::turnovr();
        }
    }

    // Move player
    player.position = to;

    // Update ball position if carrier
    if (state.ball.isHeld && state.ball.carrierId == playerId) {
        state.ball.position = to;
    }

    emitEvent(events, {GameEvent::Type::PLAYER_MOVE, playerId, -1, from, to, 0, true});

    // Shadowing: after successful dodge, enemy may follow
    if (needsDodge) {
        checkShadowing(state, playerId, from, dice, events);
    }

    // Pickup ball if on ground at destination
    if (!state.ball.isHeld && state.ball.position == to) {
        bool pickupOk = resolvePickup(state, playerId, dice, events);
        if (!pickupOk) {
            // Failed pickup — turnover
            player.hasActed = true;
            return ActionResult::turnovr();
        }
    }

    return ActionResult::ok();
}

ActionResult resolveLeap(GameState& state, int playerId, Position to,
                         DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& player = state.getPlayer(playerId);
    Position from = player.position;

    // Validate: within distance 2, on pitch, unoccupied
    int dist = from.distanceTo(to);
    if (dist < 1 || dist > 2 || !to.isOnPitch()) {
        return ActionResult::fail();
    }
    if (state.getPlayerAtPosition(to) != nullptr) {
        return ActionResult::fail();
    }

    // Leap costs 2 MA
    player.movementRemaining -= 2;
    player.hasMoved = true;

    bool needsGfi = false;
    if (player.movementRemaining < 0) {
        int maxGfi = player.hasSkill(SkillName::Sprint) ? -3 : -2;
        if (player.movementRemaining < maxGfi) {
            player.movementRemaining += 2; // undo
            return ActionResult::fail();
        }
        needsGfi = true;
    }

    // Leap agility check: target = max(2, min(6, 7-AG+TZ_at_dest))
    int target = 7 - player.stats.agility;
    target += countTacklezones(state, to, player.teamSide);
    if (player.hasSkill(SkillName::VeryLongLegs)) target -= 1;
    target = std::clamp(target, 2, 6);

    bool leapOk = attemptRoll(state, playerId, dice, target,
                               SkillName::SKILL_COUNT, false, true, events);

    emitEvent(events, {GameEvent::Type::DODGE, playerId, -1, from, to,
                      target, leapOk});

    if (!leapOk) {
        // Failed leap: player prone at destination, armor+injury, turnover
        player.position = to;
        player.state = PlayerState::PRONE;
        player.hasActed = true;

        InjuryContext ctx;
        resolveArmourAndInjury(state, playerId, dice, ctx, events);
        handleBallOnPlayerDown(state, playerId, dice, events);

        return ActionResult::turnovr();
    }

    // GFI if needed
    if (needsGfi) {
        int gfiTarget = (state.weather == Weather::BLIZZARD) ? 3 : 2;
        bool gfiOk = attemptRoll(state, playerId, dice, gfiTarget,
                                  SkillName::SureFeet, false, true, events);
        if (!gfiOk) {
            player.position = to;
            player.state = PlayerState::PRONE;
            player.hasActed = true;

            InjuryContext ctx;
            resolveArmourAndInjury(state, playerId, dice, ctx, events);
            handleBallOnPlayerDown(state, playerId, dice, events);

            return ActionResult::turnovr();
        }
    }

    // Move player
    player.position = to;

    if (state.ball.isHeld && state.ball.carrierId == playerId) {
        state.ball.position = to;
    }

    emitEvent(events, {GameEvent::Type::PLAYER_MOVE, playerId, -1, from, to, 0, true});

    // Pickup ball if on ground at destination
    if (!state.ball.isHeld && state.ball.position == to) {
        bool pickupOk = resolvePickup(state, playerId, dice, events);
        if (!pickupOk) {
            player.hasActed = true;
            return ActionResult::turnovr();
        }
    }

    return ActionResult::ok();
}

ActionResult resolveStandUp(GameState& state, int playerId, DiceRollerBase& dice,
                            std::vector<GameEvent>* events) {
    Player& player = state.getPlayer(playerId);

    if (player.state != PlayerState::PRONE) {
        return ActionResult::fail();
    }

    if (player.hasSkill(SkillName::JumpUp)) {
        // Free stand up
        player.state = PlayerState::STANDING;
        return ActionResult::ok();
    }

    // Costs 3 MA
    if (player.movementRemaining < 3) {
        return ActionResult::fail();
    }

    player.movementRemaining -= 3;
    player.state = PlayerState::STANDING;
    return ActionResult::ok();
}

} // namespace bb
