#include "bb/action_resolver.h"
#include "bb/move_handler.h"
#include "bb/block_handler.h"
#include "bb/foul_handler.h"
#include "bb/pass_handler.h"
#include "bb/big_guy_handler.h"
#include "bb/turn_handler.h"
#include "bb/pathfinder.h"
#include "bb/helpers.h"
#include "bb/ttm_handler.h"
#include "bb/bomb_handler.h"
#include "bb/gaze_handler.h"
#include "bb/ball_and_chain_handler.h"

namespace bb {

ActionResult resolveAction(GameState& state, const Action& action,
                           DiceRollerBase& dice, std::vector<GameEvent>* events) {
    // BigGuy pre-action checks for player actions
    if (requiresPlayer(action.type) && action.playerId > 0) {
        Player& p = state.getPlayer(action.playerId);
        bool hasBigGuySkill = p.hasSkill(SkillName::BoneHead) ||
                              p.hasSkill(SkillName::ReallyStupid) ||
                              p.hasSkill(SkillName::WildAnimal) ||
                              p.hasSkill(SkillName::TakeRoot) ||
                              p.hasSkill(SkillName::Bloodlust);
        if (hasBigGuySkill) {
            BigGuyResult bgResult = resolveBigGuyCheck(state, action.playerId,
                                                        action.type, dice, events);
            if (bgResult.actionBlocked && !bgResult.proceed) {
                return ActionResult::ok();  // Action wasted, not turnover
            }
        }
    }

    switch (action.type) {
        case ActionType::MOVE: {
            Player& player = state.getPlayer(action.playerId);

            // If prone, stand up first
            if (player.state == PlayerState::PRONE) {
                ActionResult standResult = resolveStandUp(state, action.playerId, dice, events);
                if (!standResult.success) return standResult;

                // If target is player's own position, this was just a stand-up
                if (action.target == player.position) {
                    return ActionResult::ok();
                }
            }

            return resolveMoveStep(state, action.playerId, action.target, dice, events);
        }

        case ActionType::BLOCK: {
            BlockParams params;
            params.attackerId = action.playerId;
            params.targetId = action.targetId;
            params.isBlitz = false;
            params.hornsBonus = false;
            return resolveBlock(state, params, dice, events);
        }

        case ActionType::BLITZ: {
            Player& player = state.getPlayer(action.playerId);
            Player& target = state.getPlayer(action.targetId);

            // Mark blitz used
            state.getTeamState(player.teamSide).blitzUsedThisTurn = true;
            player.usedBlitz = true;

            // If prone, stand up first
            if (player.state == PlayerState::PRONE) {
                ActionResult standResult = resolveStandUp(state, action.playerId, dice, events);
                if (!standResult.success) return standResult;
            }

            // Move toward target if not adjacent
            while (player.position.distanceTo(target.position) > 1) {
                // Find adjacent square closer to target using pathfinder
                Position adjPos;
                if (!canReachAdjacentTo(state, player, target.position, adjPos)) {
                    // Can't reach — shouldn't happen if actions are valid
                    return ActionResult::fail();
                }

                // Find next step toward adjPos using simple greedy approach
                Position bestNext{-1, -1};
                int bestDist = 999;
                auto adj = player.position.getAdjacent();
                for (auto& pos : adj) {
                    if (!pos.isOnPitch()) continue;
                    if (state.getPlayerAtPosition(pos) != nullptr) continue;
                    int d = pos.distanceTo(target.position);
                    if (d < bestDist) {
                        bestDist = d;
                        bestNext = pos;
                    }
                }

                if (bestNext.x < 0) return ActionResult::fail();

                ActionResult moveResult = resolveMoveStep(state, action.playerId,
                                                           bestNext, dice, events);
                if (moveResult.turnover) return moveResult;
                if (!moveResult.success) return moveResult;

                // Check if player is still standing (might have been knocked down)
                if (player.state != PlayerState::STANDING) return ActionResult::turnovr();
            }

            // Now adjacent — perform block
            if (player.position.distanceTo(target.position) != 1) {
                return ActionResult::fail();
            }

            BlockParams params;
            params.attackerId = action.playerId;
            params.targetId = action.targetId;
            params.isBlitz = true;
            params.hornsBonus = true; // Horns applies on blitz
            return resolveBlock(state, params, dice, events);
        }

        case ActionType::PASS: {
            return resolvePass(state, action.playerId, action.target, dice, events);
        }

        case ActionType::HAND_OFF: {
            return resolveHandOff(state, action.playerId, action.targetId, dice, events);
        }

        case ActionType::FOUL: {
            return resolveFoul(state, action.playerId, action.targetId, dice, events);
        }

        case ActionType::THROW_TEAM_MATE: {
            return resolveThrowTeamMate(state, action.playerId, action.targetId,
                                        action.target, dice, events);
        }

        case ActionType::BOMB_THROW: {
            return resolveBombThrow(state, action.playerId, action.target, dice, events);
        }

        case ActionType::HYPNOTIC_GAZE: {
            return resolveHypnoticGaze(state, action.playerId, action.targetId, dice, events);
        }

        case ActionType::BALL_AND_CHAIN: {
            return resolveBallAndChain(state, action.playerId, dice, events);
        }

        case ActionType::MULTIPLE_BLOCK: {
            // targetId encodes first target, target.x/y encodes second target ID
            // We use targetId for first target and target position's x as second target ID
            return resolveMultipleBlock(state, action.playerId, action.targetId,
                                        action.target.x, dice, events);
        }

        case ActionType::END_TURN: {
            resolveEndTurn(state, events);
            return ActionResult::ok();
        }

        default:
            return ActionResult::fail();
    }
}

ActionResult executeAction(GameState& state, const Action& action,
                           DiceRollerBase& dice, std::vector<GameEvent>* events) {
    ActionResult result = resolveAction(state, action, dice, events);

    // Auto end turn on turnover
    if (result.turnover) {
        state.turnoverPending = true;
        resolveEndTurn(state, events);
    }

    // Check touchdown
    if (checkTouchdown(state)) {
        TeamSide scoringSide = state.getPlayer(state.ball.carrierId).teamSide;
        state.getTeamState(scoringSide).score++;
        state.phase = GamePhase::TOUCHDOWN;
        emitEvent(events, {GameEvent::Type::TOUCHDOWN, state.ball.carrierId, -1,
                          state.ball.position, {}, 0, true});
    }

    // Check half over
    if (checkHalfOver(state)) {
        if (state.half >= 2) {
            state.phase = GamePhase::GAME_OVER;
        } else {
            state.phase = GamePhase::HALF_TIME;
        }
    }

    return result;
}

} // namespace bb
