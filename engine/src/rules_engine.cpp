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
            // Take Root, l. 8577-8578: zakorenený "may not Go For It" -- MA 0
            // by mu jinak porad nechalo dva kroky pres GFI.
            int maxGfi = p.rooted ? 0 : (p.hasSkill(SkillName::Sprint) ? 3 : 2);
            if (p.movementRemaining - 1 < -maxGfi) continue;

            out.push_back({ActionType::MOVE, p.id, -1, pos});
        }

        // LEAP -- BB2016 l. 8270-8283: "allowed to jump to any EMPTY square
        // WITHIN 2 SQUARES even if it requires jumping over a player from
        // either team. Making a leap costs the player TWO squares of movement.
        // ... A player may only use the Leap skill ONCE PER TURN."
        // ⛔ Tahle nabidka tu do 24.08.2026 nebyla, takze `resolveLeap` -- hotovy
        // a se tremi zelenymi testy -- nemel volajiciho a nikdo nikdy neskocil.
        // Je to tataz trida jako P45 vstavani: resolver bez nabidky.
        if (p.hasSkill(SkillName::Leap) && !p.leapUsedThisTurn && !p.rooted) {
            int maxGfi = p.hasSkill(SkillName::Sprint) ? 3 : 2;
            if (p.movementRemaining - 2 >= -maxGfi) {
                for (int dx = -2; dx <= 2; ++dx) {
                    for (int dy = -2; dy <= 2; ++dy) {
                        if (dx == 0 && dy == 0) continue;
                        Position dest{static_cast<int8_t>(p.position.x + dx),
                                      static_cast<int8_t>(p.position.y + dy)};
                        if (!dest.isOnPitch()) continue;
                        if (p.position.distanceTo(dest) > 2) continue;
                        if (state.getPlayerAtPosition(dest) != nullptr) continue;
                        out.push_back({ActionType::LEAP, p.id, -1, dest});
                    }
                }
            }
        }

        // BLOCK: each adjacent standing enemy.
        // Not for a player who already moved this activation: BB2016 l. 675,
        // "you may not move when you take a Block Action" -- move+block is a
        // BLITZ, and that is one per turn (gated below by blitzUsedThisTurn).
        // canAct() cannot carry this: it is also used for targeting checks.
        TeamSide enemySide = opponent(side);
        for (auto& pos : adj) {
            // ⚠️ VÝJIMKA PRO BLITZ (oprava 21.08.): pohyb + blok JE blitz.
            // `expandBlitzAndScore` provede BLITZ akci, pak si dojde k cíli
            // vlastními kroky MOVE a teprve pak hledá BLOCK -- holé
            // `if (p.hasMoved) break` mu ten blok sebralo a blitz se
            // spotřeboval bez rány. `usedBlitz` se nastavuje na začátku
            // BLITZ akce, takže rozlišuje "deklaroval blitz" od "jen se hnul".
            if (p.hasMoved && !p.usedBlitz) break;
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

                // Check if we can reach adjacent to enemy with 1 MP still in
                // the budget -- the blitz block itself costs a movement point
                // (CRP), so an approach burning the full MA+GFI range would
                // arrive unable to throw the block at all.
                Position adjPos;
                if (canReachAdjacentTo(state, p, enemy.position, adjPos, 1)) {
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
                // Reach comes from the ruler grid, not a Chebyshev radius
                // (rules parity, 2026-08-10): "within 13" offered 45 targets
                // the ruler cannot actually reach.
                PassRange range;
                if (!passRangeFromOffset(teammate.position.x - p.position.x,
                                         teammate.position.y - p.position.y,
                                         range)) {
                    return;
                }
                // Blizzard: "only quick or short passes can be attempted".
                if (state.weather == Weather::BLIZZARD &&
                    (range == PassRange::LONG_PASS || range == PassRange::LONG_BOMB)) {
                    return;
                }
                out.push_back({ActionType::PASS, p.id, teammate.id, teammate.position});
            });
        }

        // HAND_OFF: if not used this turn, has ball, each adjacent standing teammate
        // 2026-08-17: gated on its OWN allowance (P4/P26). It used to share
        // passUsedThisTurn, which made a pass and a hand-off mutually exclusive
        // in the same turn -- the rules make them separate declarations.
        if (!team.handOffUsedThisTurn && state.ball.isHeld && state.ball.carrierId == p.id &&
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

        // BOMB_THROW (rules parity, 2026-08-10). CRP Bombardier: the throw
        // "does not use the team's Pass Action for the turn", so it is NOT
        // gated on passUsedThisTurn; but "the player may not move or stand
        // up before throwing it (he needs time to light the fuse!)", so a
        // player who has already moved -- or who is Prone/Stunned -- cannot.
        // Reach comes from the ruler grid, not a Chebyshev radius.
        if (p.hasSkill(SkillName::Bombardier) && !p.hasMoved &&
            p.state == PlayerState::STANDING) {
            state.forEachOnPitch(enemySide, [&](const Player& enemy) {
                if (enemy.state != PlayerState::STANDING) return;
                PassRange range;
                if (!passRangeFromOffset(enemy.position.x - p.position.x,
                                         enemy.position.y - p.position.y,
                                         range)) {
                    return;
                }
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

        // MULTIPLE_BLOCK: player has MultipleBlock and 2+ adjacent enemies.
        // ⭐ FRENZY UZ TU NENI PODMINKA (oprava 29.08.2026, nalez L6).
        // r. 8302-8305: "...so Multiple Block can be used INSTEAD OF Frenzy,
        // but both skills cannot be used TOGETHER." Pravidlo zakazuje
        // KOMBINACI, ne drzeni obojiho -- `&& !p.hasSkill(Frenzy)` tedy hraci
        // s obema dovednostmi bralo VOLBU, kterou pravidlo predpoklada.
        // Vylouceni se resi na druhe strane: `resolveMultipleBlock` pousti oba
        // bloky s `frenzyDisabled`, takze Frenzy uvnitr nezasahne.
        if (p.hasSkill(SkillName::MultipleBlock)) {
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
        // Stejná výjimka jako v hlavní smyčce (:21) a v makro vrstvě:
        // BallAndChain smí JEN svou akci. Bez toho si dvě vrstvy odporovaly.
        if (p.hasSkill(SkillName::BallAndChain)) return;

        // Anyone prone may ATTEMPT to stand: 3 MA if he has it, otherwise a
        // 4+ roll (BB2016 l. 691-693). The old `movementRemaining >= 3` gate
        // meant a sub-3-MA player was never even offered the action, so a
        // Treeman (MA 2) stayed down for the rest of every drive.
        {
            // After standing up, the player can move — generate a MOVE action
            // to their own position as a "stand up" action
            out.push_back({ActionType::MOVE, p.id, -1, p.position});
        }
    });
}

} // namespace bb
