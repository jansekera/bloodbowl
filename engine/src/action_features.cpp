#include "bb/action_features.h"
#include "bb/helpers.h"
#include <algorithm>
#include <cmath>

namespace bb {

void extractActionFeatures(const GameState& state, const Action& action, float* out) {
    // Zero all features
    for (int i = 0; i < NUM_ACTION_FEATURES; ++i) out[i] = 0.0f;

    // [0] is_end_turn
    out[0] = (action.type == ActionType::END_TURN) ? 1.0f : 0.0f;

    // [1] is_move
    out[1] = (action.type == ActionType::MOVE) ? 1.0f : 0.0f;

    // [2] is_block
    out[2] = (action.type == ActionType::BLOCK) ? 1.0f : 0.0f;

    // [3] is_blitz
    out[3] = (action.type == ActionType::BLITZ) ? 1.0f : 0.0f;

    // [4] is_pass_or_handoff
    out[4] = (action.type == ActionType::PASS || action.type == ActionType::HAND_OFF) ? 1.0f : 0.0f;

    // [5] is_other (foul, TTM, bomb, gaze, B&C, multiblock)
    if (action.type == ActionType::FOUL || action.type == ActionType::THROW_TEAM_MATE ||
        action.type == ActionType::BOMB_THROW || action.type == ActionType::HYPNOTIC_GAZE ||
        action.type == ActionType::BALL_AND_CHAIN || action.type == ActionType::MULTIPLE_BLOCK) {
        out[5] = 1.0f;
    }

    // For actions with a player, extract player-based features
    if (action.playerId > 0 && action.playerId <= 22) {
        const Player& player = state.getPlayer(action.playerId);

        // [6] player_strength / 7
        out[6] = player.stats.strength / 7.0f;

        // [7] player_agility / 7
        out[7] = player.stats.agility / 7.0f;

        // [8] is_ball_carrier
        out[8] = (state.ball.isHeld && state.ball.carrierId == action.playerId) ? 1.0f : 0.0f;

        // [9] is_scoring_move (carrier moves to endzone)
        if (out[8] > 0.5f && action.type == ActionType::MOVE) {
            int endzoneX = (player.teamSide == TeamSide::HOME) ? 25 : 0;
            if (action.target.x == endzoneX) {
                out[9] = 1.0f;
            }
        }

        // [10] distance_to_endzone / 26
        if (player.isOnPitch()) {
            int endzoneX = (player.teamSide == TeamSide::HOME) ? 25 : 0;
            int dist = std::abs(player.position.x - endzoneX);
            // For move actions, use target position instead
            if (action.type == ActionType::MOVE && action.target.isOnPitch()) {
                dist = std::abs(action.target.x - endzoneX);
            }
            out[10] = dist / 26.0f;
        }

        // [11] block_dice / 3 (positive = attacker chooses, negative = defender)
        if ((action.type == ActionType::BLOCK || action.type == ActionType::BLITZ) &&
            action.targetId > 0 && action.targetId <= 22) {
            const Player& defender = state.getPlayer(action.targetId);
            if (defender.isOnPitch() && player.isOnPitch()) {
                int attST = player.stats.strength;
                int defST = defender.stats.strength;

                // Horns bonus for blitz
                if (action.type == ActionType::BLITZ && player.hasSkill(SkillName::Horns)) {
                    attST += 1;
                }

                // Count assists
                int attAssists = countAssists(state, defender.position, player.teamSide,
                                              player.id, defender.id, defender.id);
                int defAssists = countAssists(state, player.position, defender.teamSide,
                                              defender.id, player.id, player.id);

                int effectiveAttST = attST + attAssists;
                int effectiveDefST = defST + defAssists;

                BlockDiceInfo info = getBlockDiceInfo(effectiveAttST, effectiveDefST);
                float dice = static_cast<float>(info.count);
                if (!info.attackerChooses) dice = -dice;
                out[11] = dice / 3.0f;
            }
        }

        // [12] moves_ball_forward (toward endzone)
        if (action.type == ActionType::MOVE && player.isOnPitch() && action.target.isOnPitch()) {
            int endzoneX = (player.teamSide == TeamSide::HOME) ? 25 : 0;
            int currentDist = std::abs(player.position.x - endzoneX);
            int targetDist = std::abs(action.target.x - endzoneX);
            if (targetDist < currentDist) {
                out[12] = 1.0f;
            }
        }

        // [13] gfi_required / 3
        if (action.type == ActionType::MOVE && player.isOnPitch() && action.target.isOnPitch()) {
            int moveDist = player.position.distanceTo(action.target);
            int remaining = player.movementRemaining;
            int gfi = std::max(0, moveDist - remaining);
            out[13] = std::min(gfi, 3) / 3.0f;
        }

        // [14] target_is_prone (for fouls)
        if (action.type == ActionType::FOUL && action.targetId > 0 && action.targetId <= 22) {
            const Player& target = state.getPlayer(action.targetId);
            if (target.state == PlayerState::PRONE || target.state == PlayerState::STUNNED) {
                out[14] = 1.0f;
            }
        }

        // [15-22] MOVE IDENTITY — make distinct actions distinguishable (break collisions).
        // All coords are perspective-normalized along the acting team's attack axis so
        // they generalize across HOME/AWAY: progress = 0 own endzone, 1 opponent endzone.
        auto progress = [&](int x) {
            int p = (player.teamSide == TeamSide::HOME) ? x : (25 - x);
            return std::clamp(p / 25.0f, 0.0f, 1.0f);
        };

        // Effective target square: move target if on pitch, else defender position (block/blitz/foul).
        Position eff{-1, -1};
        if (action.target.isOnPitch()) {
            eff = action.target;
        } else if (action.targetId > 0 && action.targetId <= 22) {
            const Player& tgtP = state.getPlayer(action.targetId);
            if (tgtP.isOnPitch()) eff = tgtP.position;
        }

        if (player.isOnPitch()) {
            // [15] source progress toward opponent endzone
            out[15] = progress(player.position.x);
            // [16] source lateral position
            out[16] = player.position.y / 14.0f;
        }

        if (eff.isOnPitch()) {
            // [17] target progress toward opponent endzone
            out[17] = progress(eff.x);
            // [18] target lateral position
            out[18] = eff.y / 14.0f;
            if (player.isOnPitch()) {
                // [19] forward delta (signed: + toward opponent endzone)
                out[19] = progress(eff.x) - progress(player.position.x);
                // [20] lateral delta
                out[20] = (eff.y - player.position.y) / 14.0f;
                // [21] move distance (Chebyshev) normalized
                out[21] = std::min(player.position.distanceTo(eff), 14) / 14.0f;
            }
        }

        // [22] acting player identity within team (distinguishes same-typed moves by different players)
        out[22] = ((action.playerId - 1) % 11) / 10.0f;
    }
}

} // namespace bb
