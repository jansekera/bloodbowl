#include "bb/ball_and_chain_handler.h"
#include "bb/helpers.h"
#include "bb/injury.h"
#include "bb/ball_handler.h"
#include "bb/block_handler.h"
#include <algorithm>

namespace bb {

namespace {

// Simplified 1-die auto-block for B&C: D6 mapping
// 1=AD, 2=BD, 3-4=Pushed, 5=DS, 6=DD
// Returns true if B&C player was knocked down
bool resolveAutoBlock(GameState& state, int bcPlayerId, int targetId,
                      DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& bcp = state.getPlayer(bcPlayerId);
    Player& target = state.getPlayer(targetId);

    BlockDiceFace face = dice.rollBlockDie();

    emitEvent(events, {GameEvent::Type::BLOCK, bcPlayerId, targetId, bcp.position,
                      target.position, static_cast<int>(face), true});

    switch (face) {
        case BlockDiceFace::ATTACKER_DOWN: {
            bcp.state = PlayerState::PRONE;
            emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, bcPlayerId, -1,
                              bcp.position, {}, 0, false});
            InjuryContext ctx;
            resolveArmourAndInjury(state, bcPlayerId, dice, ctx, events);
            handleBallOnPlayerDown(state, bcPlayerId, dice, events);
            return true;
        }

        case BlockDiceFace::BOTH_DOWN: {
            // Block/Dodge/Tackle apply
            bool bcFalls = !bcp.hasSkill(SkillName::Block);
            bool defFalls = !target.hasSkill(SkillName::Block);

            if (bcFalls) {
                bcp.state = PlayerState::PRONE;
                emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, bcPlayerId, -1,
                                  bcp.position, {}, 0, false});
                InjuryContext ctx;
                resolveArmourAndInjury(state, bcPlayerId, dice, ctx, events);
                handleBallOnPlayerDown(state, bcPlayerId, dice, events);
            }
            if (defFalls) {
                target.state = PlayerState::PRONE;
                emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, targetId, -1,
                                  target.position, {}, 0, false});
                InjuryContext ctx;
                resolveArmourAndInjury(state, targetId, dice, ctx, events);
                handleBallOnPlayerDown(state, targetId, dice, events);
            }
            return bcFalls;
        }

        case BlockDiceFace::PUSHED: {
            // Target pushed away but no knockdown
            return false;
        }

        case BlockDiceFace::DEFENDER_STUMBLES: {
            // Dodge saves
            if (target.hasSkill(SkillName::Dodge) && !bcp.hasSkill(SkillName::Tackle)) {
                // Just pushed
                return false;
            }
            // Knocked down
            target.state = PlayerState::PRONE;
            emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, targetId, -1,
                              target.position, {}, 0, false});
            InjuryContext ctx;
            resolveArmourAndInjury(state, targetId, dice, ctx, events);
            handleBallOnPlayerDown(state, targetId, dice, events);
            return false;
        }

        case BlockDiceFace::DEFENDER_DOWN: {
            target.state = PlayerState::PRONE;
            emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, targetId, -1,
                              target.position, {}, 0, false});
            InjuryContext ctx;
            resolveArmourAndInjury(state, targetId, dice, ctx, events);
            handleBallOnPlayerDown(state, targetId, dice, events);
            return false;
        }
    }
    return false;
}

} // anonymous namespace

ActionResult resolveBallAndChain(GameState& state, int playerId,
                                 DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& bcp = state.getPlayer(playerId);
    bcp.hasActed = true;

    int ma = bcp.stats.movement;

    for (int step = 0; step < ma; step++) {
        // D8 scatter direction
        int d8 = dice.rollD8();
        Position scatter = scatterDirection(d8);
        Position target{static_cast<int8_t>(bcp.position.x + scatter.x),
                        static_cast<int8_t>(bcp.position.y + scatter.y)};

        // Off-pitch: player KO, drop ball, stop. NOT turnover
        if (!target.isOnPitch()) {
            handleBallOnPlayerDown(state, playerId, dice, events);
            bcp.state = PlayerState::KO;
            bcp.position = {-1, -1};
            return ActionResult::ok(); // Never turnover
        }

        // Occupied: auto-block
        Player* occupant = state.getPlayerAtPosition(target);
        if (occupant && occupant->state == PlayerState::STANDING) {
            bool bcDown = resolveAutoBlock(state, playerId, occupant->id, dice, events);
            if (bcDown) {
                return ActionResult::ok(); // B&C down stops, never turnover
            }
            // Don't move into occupied square, continue to next step
            continue;
        }

        // Move to empty square
        Position oldPos = bcp.position;
        bcp.position = target;
        emitEvent(events, {GameEvent::Type::PLAYER_MOVE, playerId, -1, oldPos, target, 0, true});

        // Ball carrier moves with ball
        if (state.ball.isHeld && state.ball.carrierId == playerId) {
            state.ball.position = target;
        }

        // Ball on ground: NoHands means bounce
        if (!state.ball.isHeld && state.ball.position == target) {
            // B&C players have NoHands, so ball bounces
            resolveBounce(state, target, dice, 0, events);
        }
    }

    return ActionResult::ok(); // Never turnover
}

} // namespace bb
