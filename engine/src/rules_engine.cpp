#include "bb/rules_engine.h"
#include "bb/helpers.h"
#include "bb/pathfinder.h"

namespace bb {

void getAvailableActions(const GameState& state, std::vector<Action>& out) {
    out.clear();

    if (state.phase != GamePhase::PLAY) return;

    TeamSide side = state.activeTeam;
    const TeamState& team = state.getTeamState(side);

    // END_TURN is always available
    out.push_back({ActionType::END_TURN, -1, -1, {-1, -1}});

    state.forEachOnPitch(side, [&](const Player& p) {
        if (!p.canAct()) return;

        // BallAndChain players can ONLY use the BALL_AND_CHAIN action
        if (p.hasSkill(SkillName::BallAndChain)) {
            out.push_back({ActionType::BALL_AND_CHAIN, p.id, -1, {-1, -1}});
            return; // Skip all other action types
        }

        // MOVE: each adjacent empty square (single-step)
        auto adj = p.position.getAdjacent();
        for (auto& pos : adj) {
            if (!pos.isOnPitch()) continue;
            if (state.getPlayerAtPosition(pos) != nullptr) continue;

            // Check movement remaining (including GFI)
            int maxGfi = p.hasSkill(SkillName::Sprint) ? 3 : 2;
            if (p.movementRemaining - 1 < -maxGfi) continue;

            out.push_back({ActionType::MOVE, p.id, -1, pos});
        }

        // BLOCK: each adjacent standing enemy
        TeamSide enemySide = opponent(side);
        for (auto& pos : adj) {
            if (!pos.isOnPitch()) continue;
            const Player* enemy = state.getPlayerAtPosition(pos);
            if (enemy && enemy->teamSide == enemySide && canAct(enemy->state)) {
                out.push_back({ActionType::BLOCK, p.id, enemy->id, enemy->position});
            }
        }

        // BLITZ: if not used this turn, each reachable enemy
        if (!team.blitzUsedThisTurn && !p.usedBlitz) {
            state.forEachOnPitch(enemySide, [&](const Player& enemy) {
                if (!canAct(enemy.state) && !isOnPitch(enemy.state)) return;
                if (enemy.state != PlayerState::STANDING) return;

                // Check if already adjacent
                if (p.position.distanceTo(enemy.position) == 1) {
                    // Already adjacent — blitz is just a block with blitz flag
                    out.push_back({ActionType::BLITZ, p.id, enemy.id, enemy.position});
                    return;
                }

                // Check if we can reach adjacent to enemy
                Position adjPos;
                if (canReachAdjacentTo(state, p, enemy.position, adjPos)) {
                    out.push_back({ActionType::BLITZ, p.id, enemy.id, enemy.position});
                }
            });
        }

        // PASS: if not used this turn, has ball, each standing teammate within range 13
        if (!team.passUsedThisTurn && state.ball.isHeld && state.ball.carrierId == p.id &&
            !p.hasSkill(SkillName::NoHands)) {
            state.forEachOnPitch(side, [&](const Player& teammate) {
                if (teammate.id == p.id) return;
                if (teammate.state != PlayerState::STANDING) return;
                int dist = p.position.distanceTo(teammate.position);
                if (dist > 13) return;
                out.push_back({ActionType::PASS, p.id, -1, teammate.position});
            });
        }

        // HAND_OFF: if not used this turn, has ball, each adjacent standing teammate
        if (!team.passUsedThisTurn && state.ball.isHeld && state.ball.carrierId == p.id &&
            !p.hasSkill(SkillName::NoHands)) {
            for (auto& pos : adj) {
                if (!pos.isOnPitch()) continue;
                const Player* teammate = state.getPlayerAtPosition(pos);
                if (teammate && teammate->teamSide == side &&
                    teammate->state == PlayerState::STANDING) {
                    out.push_back({ActionType::HAND_OFF, p.id, teammate->id, teammate->position});
                }
            }
        }

        // FOUL: if not used this turn, each adjacent prone/stunned enemy
        if (!team.foulUsedThisTurn) {
            for (auto& pos : adj) {
                if (!pos.isOnPitch()) continue;
                const Player* enemy = state.getPlayerAtPosition(pos);
                if (enemy && enemy->teamSide == enemySide &&
                    (enemy->state == PlayerState::PRONE ||
                     enemy->state == PlayerState::STUNNED)) {
                    out.push_back({ActionType::FOUL, p.id, enemy->id, enemy->position});
                }
            }
        }

        // THROW_TEAM_MATE: player has ThrowTeamMate + adjacent RightStuff teammate
        if (p.hasSkill(SkillName::ThrowTeamMate) && !team.passUsedThisTurn) {
            for (auto& pos : adj) {
                if (!pos.isOnPitch()) continue;
                const Player* teammate = state.getPlayerAtPosition(pos);
                if (teammate && teammate->teamSide == side &&
                    teammate->state == PlayerState::STANDING &&
                    teammate->hasSkill(SkillName::RightStuff)) {
                    // Target positions: any square within pass range
                    // For simplicity, generate targets every 3 squares in each direction
                    for (int tx = 0; tx < 26; tx += 3) {
                        for (int ty = 0; ty < 15; ty += 3) {
                            int dist = p.position.distanceTo({static_cast<int8_t>(tx),
                                                              static_cast<int8_t>(ty)});
                            if (dist > 0 && dist <= 13) {
                                out.push_back({ActionType::THROW_TEAM_MATE, p.id,
                                              teammate->id,
                                              {static_cast<int8_t>(tx), static_cast<int8_t>(ty)}});
                            }
                        }
                    }
                }
            }
        }

        // BOMB_THROW: Bombardier player, target positions within range 13
        if (p.hasSkill(SkillName::Bombardier) && !team.passUsedThisTurn) {
            state.forEachOnPitch(enemySide, [&](const Player& enemy) {
                if (enemy.state != PlayerState::STANDING) return;
                int dist = p.position.distanceTo(enemy.position);
                if (dist > 13) return;
                out.push_back({ActionType::BOMB_THROW, p.id, -1, enemy.position});
            });
        }

        // HYPNOTIC_GAZE: each adjacent standing enemy
        if (p.hasSkill(SkillName::HypnoticGaze)) {
            for (auto& pos : adj) {
                if (!pos.isOnPitch()) continue;
                const Player* enemy = state.getPlayerAtPosition(pos);
                if (enemy && enemy->teamSide == enemySide &&
                    enemy->state == PlayerState::STANDING) {
                    out.push_back({ActionType::HYPNOTIC_GAZE, p.id, enemy->id, enemy->position});
                }
            }
        }

        // MULTIPLE_BLOCK: player has MultipleBlock, 2+ adjacent enemies, no Frenzy
        if (p.hasSkill(SkillName::MultipleBlock) && !p.hasSkill(SkillName::Frenzy)) {
            // Collect adjacent standing enemies
            int adjEnemies[8];
            int nAdj = 0;
            for (auto& pos : adj) {
                if (!pos.isOnPitch()) continue;
                const Player* enemy = state.getPlayerAtPosition(pos);
                if (enemy && enemy->teamSide == enemySide &&
                    enemy->state == PlayerState::STANDING) {
                    if (nAdj < 8) adjEnemies[nAdj++] = enemy->id;
                }
            }
            // Generate all pairs
            for (int i = 0; i < nAdj; i++) {
                for (int j = i + 1; j < nAdj; j++) {
                    // Encode: targetId=first target, target.x=second target ID
                    out.push_back({ActionType::MULTIPLE_BLOCK, p.id, adjEnemies[i],
                                  {static_cast<int8_t>(adjEnemies[j]), 0}});
                }
            }
        }
    });

    // Also allow standing up prone players
    state.forEachOnPitch(side, [&](const Player& p) {
        if (p.state != PlayerState::PRONE) return;
        if (p.hasActed || p.lostTacklezones) return;

        // Can stand up if JumpUp or movementRemaining >= 3
        if (p.hasSkill(SkillName::JumpUp) || p.movementRemaining >= 3) {
            // After standing up, the player can move — generate a MOVE action
            // to their own position as a "stand up" action
            out.push_back({ActionType::MOVE, p.id, -1, p.position});
        }
    });
}

} // namespace bb
