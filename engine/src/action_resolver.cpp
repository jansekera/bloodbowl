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

namespace {

// M2/N13 = P55 (29.08.2026): deklarovana akce, ktera propadne big-guy
// kontrolou, MUSI tymu odecist jeho limit -- Bone-head to rika doslova
// ("the team cannot declare another Blitz Action that turn", r. 7981-7983).
// Dosud se `blitzUsedThisTurn` nastavovalo az uvnitr `case BLITZ`, kam se
// pri zablokovane akci nikdy nedoslo, takze tym dostal DRUHY blitz.
// Plati na kazdou akci s tymovym limitem, ne jen na blitz -- pravidlo mluvi
// o "the declared Action", ne o blitzu.
void consumeDeclaredTeamAction(GameState& state, TeamSide side, ActionType type) {
    TeamState& team = state.getTeamState(side);
    switch (type) {
        case ActionType::BLITZ:           team.blitzUsedThisTurn = true;   break;
        case ActionType::PASS:
        case ActionType::THROW_TEAM_MATE: team.passUsedThisTurn = true;    break;
        case ActionType::HAND_OFF:        team.handOffUsedThisTurn = true; break;
        case ActionType::FOUL:            team.foulUsedThisTurn = true;    break;
        default: break;   // MOVE a BLOCK zadny tymovy limit nemaji
    }
}

}  // namespace

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
        // ⭐ VSTÁVÁNÍ SE NEBLOKUJE TAKE ROOTEM (oprava 21.08.). BB2016
        // l. 8583-8584 doslova: "...he may not block that turn (HE CAN STILL
        // ROLL TO STAND UP IF HE IS PRONE)." Bone Head ("can't do anything
        // for the turn") a Really Stupid blokují správně -- výjimku má JEN
        // Take Root. Bez toho vstane Treeman s p = 5/6 x 1/2 = 41,7 % místo
        // 50 %, a je to jediné tělo pod 3 MA v pěti TV1200 sestavách.
        const bool standUpAttempt =
            (action.type == ActionType::MOVE &&
             p.state == PlayerState::PRONE &&
             action.target == p.position);
        const bool onlyTakeRoot = p.hasSkill(SkillName::TakeRoot) &&
                                  !p.hasSkill(SkillName::BoneHead) &&
                                  !p.hasSkill(SkillName::ReallyStupid) &&
                                  !p.hasSkill(SkillName::WildAnimal) &&
                                  !p.hasSkill(SkillName::Bloodlust);
        // ⭐ A JEN JEDNOU ZA AKTIVACI (oprava 21.08.). BB2016 l. 8573:
        // "Immediately after declaring an ACTION". Vícepolový pohyb je u nás
        // N akcí MOVE, takže se házelo N-krát -- Ogre a Treeman rolovali
        // Bone Head / Take Root za každé pole. Spící vada, kterou probudilo
        // P45 (dokud se ležící nezvedal, nikam nešel).
        if (hasBigGuySkill && !p.bigGuyCheckedThisTurn &&
            !(standUpAttempt && onlyTakeRoot)) {
            p.bigGuyCheckedThisTurn = true;
            BigGuyResult bgResult = resolveBigGuyCheck(state, action.playerId,
                                                        action.type, dice, events);
            if (bgResult.turnover) {
                // TA10: Blood Lust -- upir nemel koho kousnout (l. 7942-7943),
                // nebo kousnuty Thrall drzel mic (l. 7941-7942).
                return ActionResult::turnovr();
            }
            if (bgResult.actionBlocked && !bgResult.proceed) {
                // M2: akce propadla -- pokud ji pravidlo bere i TYMU, odecist
                // ji tady, protoze do switche (kde se limit nastavuje) se uz
                // nedostaneme. `wastesTeamAction` nese to rozliseni.
                if (bgResult.wastesTeamAction) {
                    consumeDeclaredTeamAction(state, p.teamSide, action.type);
                }
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

            // Move toward target if not adjacent. Distance stays the primary
            // criterion (progress toward the target is guaranteed, same as the
            // old raw-distance picker), but enemy tackle zones now break ties
            // between equally-close squares — this loop was the one movement
            // path in the engine with zero TZ awareness (item 7), unlike every
            // macro routed through scoreMoveAction. Weights mirror
            // scoreMoveAction's 20/12 split; both are < 100 so a TZ-laden
            // square is still taken when it's the only one making progress
            // (a blitz through an unavoidable TZ wall must not fail outright).
            while (player.position.distanceTo(target.position) > 1) {
                // Reachability gate only: canReachAdjacentTo's adjPos (BFS by
                // pure movement cost, TZ-blind) is deliberately ignored — the
                // TZ-scored picker below owns both the route and the final
                // adjacent square (fewer enemies next to the blitzer = fewer
                // defender assists on the block, see getBlockDiceCount).
                Position adjPos;
                if (!canReachAdjacentTo(state, player, target.position, adjPos)) {
                    // Can't reach — shouldn't happen if actions are valid
                    return ActionResult::fail();
                }

                Position bestNext = pickApproachStep(state, player,
                                                     player.position,
                                                     target.position);
                if (bestNext.x < 0) return ActionResult::fail();

                Position beforeStep = player.position;
                ActionResult moveResult = resolveMoveStep(state, action.playerId,
                                                           bestNext, dice, events);
                if (moveResult.turnover) return moveResult;
                if (!moveResult.success) return moveResult;

                // Check if player is still standing (might have been knocked down)
                if (player.state != PlayerState::STANDING) return ActionResult::turnovr();

                // A step that reports success without actually moving the player
                // (e.g. caught by Tentacles: resolveMoveStep returns ok() but the
                // player stays at `from`, see move_handler.cpp's checkTentacles)
                // would otherwise retry the identical step forever — this loop has
                // no other progress guard. Treat no-progress as "can't reach",
                // consistent with the other bail-out paths above.
                if (player.position == beforeStep) return ActionResult::fail();
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

        case ActionType::LEAP: {
            // F12: dosud sem nevedla zadna cesta. l. 8283 -- jednou za kolo.
            Player& p = state.getPlayer(action.playerId);
            if (p.leapUsedThisTurn) return ActionResult::fail();
            // T5.32 (26.08.): priznak se UZ NENASTAVUJE tady. Z validace
            // v resolveLeap vedou tri cesty k fail() (vzdalenost, obsazene
            // pole, strop GFI) a kazda z nich brala hracovi skok na cele
            // kolo, ac se zadny skok nekonal. Propaluje se az v resolveLeap,
            // tesne pred agility hodem -- tedy ve chvili, kdy se skok
            // SKUTECNE pokousi.
            return resolveLeap(state, action.playerId, action.target, dice, events);
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
    // Activation close-out at the actor-switch boundary: a successful MOVE never
    // sets hasActed (only failure paths do), so without this a player who moved
    // could be independently reactivated later in the same team-turn (free
    // blitz / second action bug, see evidence/fable_hasacted_bug_20260715.md).
    // When a DIFFERENT player starts acting, the previous player's activation is
    // over: if they had moved, mark them as having acted. Continuous multi-step
    // moves and move->pass/foul/score sequences by the SAME player are untouched.
    if (requiresPlayer(action.type) && action.playerId > 0) {
        if (state.currentActivationId > 0 &&
            state.currentActivationId != action.playerId) {
            Player& prev = state.getPlayer(state.currentActivationId);
            if (prev.hasMoved) prev.hasActed = true;
        }
        state.currentActivationId = action.playerId;
    }

    ActionResult result = resolveAction(state, action, dice, events);

    // Auto end turn on turnover
    if (result.turnover) {
        state.turnoverPending = true;
        resolveEndTurn(state, events, /*wasTurnover=*/true);
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
