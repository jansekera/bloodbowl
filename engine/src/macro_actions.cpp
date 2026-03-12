#include "bb/macro_actions.h"
#include "bb/action_resolver.h"
#include "bb/helpers.h"
#include <algorithm>
#include <cmath>

namespace bb {

// --- Helper functions ---

static int endzoneX(TeamSide side) {
    return (side == TeamSide::HOME) ? 25 : 0;
}

static int distToEndzone(Position pos, TeamSide side) {
    return std::abs(pos.x - endzoneX(side));
}

// Direction toward endzone: +1 or -1 in X
static int forwardDx(TeamSide side) {
    return (side == TeamSide::HOME) ? 1 : -1;
}

// Find the ball carrier for the active team, or nullptr
static const Player* findCarrier(const GameState& state) {
    if (!state.ball.isHeld || state.ball.carrierId <= 0) return nullptr;
    const Player& p = state.getPlayer(state.ball.carrierId);
    if (p.teamSide != state.activeTeam) return nullptr;
    if (!p.isOnPitch()) return nullptr;
    return &p;
}

// Score a MOVE action: lower is better.
// Prefers: close to target, no enemy TZ, no GFI.
static int scoreMoveAction(const GameState& state, const Action& a,
                           Position target, int playerId) {
    const Player& p = state.getPlayer(playerId);
    int dist = a.target.distanceTo(target);

    // Enemy tackle zones at destination
    int destTZ = countTacklezones(state, a.target, p.teamSide);

    // Currently in TZ? (leaving requires dodge regardless of destination)
    bool currentlyInTZ = countTacklezones(state, p.position, p.teamSide) > 0;

    // GFI penalty: movementRemaining <= 0 means GFI roll needed
    bool needsGfi = (p.movementRemaining <= 0);

    // Score: distance * 10 + TZ penalty + GFI penalty
    // Distance is primary (each square = 10 points)
    // TZ penalty must exceed distance savings (10 per square) to prefer going around
    int score = dist * 10;
    if (destTZ > 0 && !currentlyInTZ) {
        score += 20 * destTZ;  // Entering enemy TZ from safe = very bad
    } else if (destTZ > 0) {
        // Already dodging: prefer TZ-free dest even if 1 square farther
        // 12 > 10 means going around (1 sq farther, 0 TZ) beats through (1 sq closer, 1 TZ)
        score += 12 * destTZ;
    }
    if (needsGfi) {
        score += 8;  // GFI is risky
    }
    // Sideline penalty: avoid Y=0 and Y=14 (Frenzy crowd-surf risk)
    if (a.target.y <= 1 || a.target.y >= 13) {
        score += 6;  // mild sideline avoidance
    }
    return score;
}

// Find available MOVE action toward a target position.
// Prefers safe routes (avoids enemy tackle zones and GFI).
static bool findMoveToward(const std::vector<Action>& actions, int playerId,
                           Position target, Action& bestMove,
                           const GameState* state = nullptr) {
    int bestScore = 9999;
    bool found = false;
    for (auto& a : actions) {
        if (a.type != ActionType::MOVE || a.playerId != playerId) continue;

        int score;
        if (state) {
            score = scoreMoveAction(*state, a, target, playerId);
        } else {
            // Fallback: pure distance (no safety check)
            score = a.target.distanceTo(target) * 10;
        }

        if (score < bestScore) {
            bestScore = score;
            bestMove = a;
            found = true;
        }
    }
    return found;
}

// Check if a player is standing, on pitch, free to act
static bool isFreeToAct(const Player& p) {
    return p.canAct() && !p.hasMoved;
}

// Count block dice for attacker vs defender
static int getBlockDiceCount(const GameState& state, const Player& att, const Player& def,
                             bool isBlitz) {
    int attST = att.stats.strength;
    int defST = def.stats.strength;
    if (isBlitz && att.hasSkill(SkillName::Horns)) attST += 1;
    int attAssists = countAssists(state, def.position, att.teamSide,
                                  att.id, def.id, def.id);
    int defAssists = countAssists(state, att.position, def.teamSide,
                                  def.id, att.id, att.id);
    BlockDiceInfo info = getBlockDiceInfo(attST + attAssists, defST + defAssists);
    return info.attackerChooses ? info.count : -info.count;
}

// Find nearest free standing teammate to a position (excluding specific player)
static const Player* findNearestFreePlayer(const GameState& state, Position target,
                                            int excludeId = -1) {
    const Player* best = nullptr;
    int bestDist = 999;
    state.forEachOnPitch(state.activeTeam, [&](const Player& p) {
        if (p.id == excludeId) return;
        if (!isFreeToAct(p)) return;
        if (p.hasSkill(SkillName::BallAndChain)) return;
        int d = p.position.distanceTo(target);
        if (d < bestDist) {
            bestDist = d;
            best = &p;
        }
    });
    return best;
}

// --- Macro Generation ---

void getAvailableMacros(const GameState& state, std::vector<Macro>& out) {
    out.clear();

    if (state.phase != GamePhase::PLAY) return;

    TeamSide mySide = state.activeTeam;
    const TeamState& myTeam = state.getTeamState(mySide);

    // Always: END_TURN
    out.push_back({MacroType::END_TURN, -1, -1, {-1, -1}});

    const Player* carrier = findCarrier(state);
    bool iHaveBall = (carrier != nullptr);
    bool ballOnGround = !state.ball.isHeld && state.ball.isOnPitch();

    // SCORE: carrier can reach endzone with MA + 2 GFI
    if (iHaveBall && carrier->canAct()) {
        int dist = distToEndzone(carrier->position, mySide);
        int maxReach = carrier->movementRemaining + 2; // +2 GFI
        if (dist <= maxReach && dist > 0) {
            out.push_back({MacroType::SCORE, carrier->id, -1, {-1, -1}});
        }
    }

    // HAND_OFF_SCORE: carrier stuck/in heavy TZ, nearby teammate can score
    if (iHaveBall && carrier->canAct() && !myTeam.passUsedThisTurn) {
        int carrierDist = distToEndzone(carrier->position, mySide);
        int carrierMaxReach = carrier->movementRemaining + 2;
        int carrierTZ = countTacklezones(state, carrier->position, carrier->teamSide);
        bool carrierStuck = (carrierDist > carrierMaxReach) || (carrierTZ >= 2 && carrierDist > 0);

        if (carrierStuck) {
            state.forEachOnPitch(mySide, [&](const Player& teammate) {
                if (teammate.id == carrier->id) return;
                if (teammate.state != PlayerState::STANDING) return;
                if (teammate.hasActed) return;
                if (teammate.hasSkill(SkillName::NoHands)) return;

                int adjDist = carrier->position.distanceTo(teammate.position);
                if (adjDist > 2) return; // carrier must reach adjacency within 1 move

                int receiverDist = distToEndzone(teammate.position, mySide);
                int receiverMaxReach = teammate.movementRemaining + 2;
                if (receiverDist > 0 && receiverDist <= receiverMaxReach) {
                    out.push_back({MacroType::HAND_OFF_SCORE, carrier->id, teammate.id, {-1, -1}});
                }
            });
        }
    }

    // PASS_SCORE: carrier stuck, pass (longer range) to teammate who can score
    if (iHaveBall && carrier->canAct() && !myTeam.passUsedThisTurn) {
        int carrierDist = distToEndzone(carrier->position, mySide);
        int carrierMaxReach = carrier->movementRemaining + 2;
        int carrierTZ = countTacklezones(state, carrier->position, carrier->teamSide);
        bool carrierStuck = (carrierDist > carrierMaxReach) || (carrierTZ >= 2 && carrierDist > 0);

        if (carrierStuck) {
            int bestScore = -999;
            int bestTargetId = -1;
            state.forEachOnPitch(mySide, [&](const Player& teammate) {
                if (teammate.id == carrier->id) return;
                if (teammate.state != PlayerState::STANDING) return;
                if (teammate.hasActed) return;
                if (teammate.hasSkill(SkillName::NoHands)) return;

                int passDist = carrier->position.distanceTo(teammate.position);
                if (passDist < 2 || passDist > 10) return; // hand-off is separate; max pass ~10

                int receiverDist = distToEndzone(teammate.position, mySide);
                int receiverMaxReach = teammate.movementRemaining + 2;
                if (receiverDist <= 0 || receiverDist > receiverMaxReach) return;

                int score = teammate.stats.agility * 5 - passDist;
                if (teammate.hasSkill(SkillName::Catch)) score += 5;
                if (score > bestScore) {
                    bestScore = score;
                    bestTargetId = teammate.id;
                }
            });
            if (bestTargetId > 0) {
                out.push_back({MacroType::PASS_SCORE, carrier->id, bestTargetId, {-1, -1}});
            }
        }
    }

    // CHAIN_SCORE: carrier passes to relay, relay hand-offs to scorer near endzone
    if (iHaveBall && carrier->canAct() && !myTeam.passUsedThisTurn) {
        int carrierDist = distToEndzone(carrier->position, mySide);
        int carrierMaxReach = carrier->movementRemaining + 2;
        bool carrierStuck = (carrierDist > carrierMaxReach);

        if (carrierStuck) {
            int bestChainScore = -999;
            int bestRelayId = -1, bestScorerId = -1;

            state.forEachOnPitch(mySide, [&](const Player& relay) {
                if (relay.id == carrier->id) return;
                if (relay.state != PlayerState::STANDING || relay.hasActed) return;
                if (relay.hasSkill(SkillName::NoHands)) return;

                int passDist = carrier->position.distanceTo(relay.position);
                if (passDist < 1 || passDist > 10) return;

                state.forEachOnPitch(mySide, [&](const Player& scorer) {
                    if (scorer.id == carrier->id || scorer.id == relay.id) return;
                    if (scorer.state != PlayerState::STANDING || scorer.hasActed) return;
                    if (scorer.hasSkill(SkillName::NoHands)) return;

                    int adjDist = relay.position.distanceTo(scorer.position);
                    if (adjDist > 2) return; // relay must reach adjacency for hand-off

                    int scorerDist = distToEndzone(scorer.position, mySide);
                    int scorerMaxReach = scorer.movementRemaining + 2;
                    if (scorerDist <= 0 || scorerDist > scorerMaxReach) return;

                    int score = relay.stats.agility * 3 + scorer.stats.agility * 5
                              + scorer.stats.movement - passDist * 2;
                    if (relay.hasSkill(SkillName::Catch)) score += 5;
                    if (scorer.hasSkill(SkillName::Catch)) score += 3;
                    if (score > bestChainScore) {
                        bestChainScore = score;
                        bestRelayId = relay.id;
                        bestScorerId = scorer.id;
                    }
                });
            });

            if (bestRelayId > 0 && bestScorerId > 0) {
                Macro m{MacroType::CHAIN_SCORE, carrier->id, bestRelayId, {-1, -1}};
                m.thirdId = bestScorerId;
                out.push_back(m);
            }
        }
    }

    // ADVANCE: carrier can move forward but can't score
    if (iHaveBall && carrier->canAct() && carrier->movementRemaining > 0) {
        int dist = distToEndzone(carrier->position, mySide);
        int maxReach = carrier->movementRemaining + 2;
        if (dist > maxReach) {
            out.push_back({MacroType::ADVANCE, carrier->id, -1, {-1, -1}});
        }
    }

    // CAGE: have ball and at least one free teammate
    if (iHaveBall) {
        bool hasFreePlayer = false;
        state.forEachOnPitch(mySide, [&](const Player& p) {
            if (p.id != carrier->id && isFreeToAct(p) && !p.hasSkill(SkillName::BallAndChain)) {
                hasFreePlayer = true;
            }
        });
        if (hasFreePlayer) {
            out.push_back({MacroType::CAGE, carrier->id, -1, {-1, -1}});
        }
    }

    // BLITZ: not used this turn, at least one standing enemy
    // Defense-aware: prioritizes ball carrier and scoring threats
    if (!myTeam.blitzUsedThisTurn) {
        bool onDef = !iHaveBall && !ballOnGround; // opponent has ball
        int oppCarrierId = (state.ball.isHeld && state.ball.carrierId > 0)
                            ? state.ball.carrierId : -1;

        // Score each target (best blitzer for each)
        struct BlitzCandidate {
            int targetId;
            int bestScore;
        };
        std::vector<BlitzCandidate> candidates;

        state.forEachOnPitch(opponent(mySide), [&](const Player& def) {
            if (def.state != PlayerState::STANDING) return;

            int targetBestScore = -999;

            state.forEachOnPitch(mySide, [&](const Player& blitzer) {
                if (!isFreeToAct(blitzer)) return;
                if (blitzer.hasSkill(SkillName::BallAndChain)) return;

                int dice = getBlockDiceCount(state, blitzer, def, true);
                int score = dice * 2;

                // Sideline trap: target near sideline = fewer escape routes
                if (def.position.y <= 2 || def.position.y >= Position::PITCH_HEIGHT - 3) {
                    score += 3;
                } else if (def.position.y <= 4 || def.position.y >= Position::PITCH_HEIGHT - 5) {
                    score += 1;
                }

                if (onDef) {
                    // DEFENSE: ball carrier is top priority
                    if (def.id == oppCarrierId) {
                        score += 10;
                    }
                    // Opponent scoring threat (can score this turn)
                    int oppEzX = (def.teamSide == TeamSide::HOME) ? 25 : 0;
                    int distEz = std::abs(def.position.x - oppEzX);
                    if (def.stats.movement + 2 >= distEz) {
                        score += 4;
                    }
                    // Free opponent (no friendly TZ on them) — more dangerous
                    if (countTacklezones(state, def.position, def.teamSide) == 0) {
                        score += 2;
                    }
                } else {
                    // OFFENSE: near carrier bonus + ball carrier bonus
                    if (iHaveBall && def.position.distanceTo(carrier->position) <= 2) {
                        score += 2;
                    }
                    if (state.ball.isHeld && state.ball.carrierId == def.id) {
                        score += 5;
                    }
                }

                if (score > targetBestScore) {
                    targetBestScore = score;
                }
            });

            if (targetBestScore > -999) {
                candidates.push_back({def.id, targetBestScore});
            }
        });

        // Sort by score descending
        std::sort(candidates.begin(), candidates.end(),
                  [](const BlitzCandidate& a, const BlitzCandidate& b) {
                      return a.bestScore > b.bestScore;
                  });

        // On defense: top 2 targets for MCTS choice; on offense: top 1
        int maxBlitz = (onDef && candidates.size() > 1) ? 2 : 1;
        maxBlitz = std::min(maxBlitz, static_cast<int>(candidates.size()));
        for (int i = 0; i < maxBlitz; ++i) {
            out.push_back({MacroType::BLITZ, -1, candidates[i].targetId, {-1, -1}});
        }
    }

    // BLITZ_AND_SCORE: carrier can almost reach endzone, but opponent blocks path
    // Blitz the blocker out of the way, then move carrier to score
    if (iHaveBall && carrier->canAct() && !myTeam.blitzUsedThisTurn) {
        int dist = distToEndzone(carrier->position, mySide);
        int maxReach = carrier->movementRemaining + 2;
        int ezX = endzoneX(mySide);
        int dx = forwardDx(mySide);

        // Carrier can't directly score (SCORE not available) or would need to go through enemies
        if (dist > 0 && dist <= maxReach + 3) {
            // Find opponent on the path between carrier and endzone
            int bestBlocker = -1;
            int bestBlockerDist = 999;
            state.forEachOnPitch(opponent(mySide), [&](const Player& def) {
                if (def.state != PlayerState::STANDING) return;
                // Is the defender roughly between carrier and endzone?
                int defDist = distToEndzone(def.position, mySide);
                int carrierDistEz = distToEndzone(carrier->position, mySide);
                if (defDist >= carrierDistEz) return; // defender is behind carrier
                // Is defender close to carrier's path (within 2 Y)?
                int yDiff = std::abs(def.position.y - carrier->position.y);
                if (yDiff > 2) return;
                // Is defender in carrier's TZ or blocking the direct path?
                int xDist = std::abs(def.position.x - carrier->position.x);
                if (xDist <= 2 && xDist + yDiff <= 3) {
                    int totalDist = xDist + yDiff;
                    if (totalDist < bestBlockerDist) {
                        bestBlockerDist = totalDist;
                        bestBlocker = def.id;
                    }
                }
            });

            if (bestBlocker > 0) {
                out.push_back({MacroType::BLITZ_AND_SCORE, carrier->id, bestBlocker, {-1, -1}});
            }
        }
    }

    // BLOCK: favorable block (2+ dice, attacker chooses)
    state.forEachOnPitch(mySide, [&](const Player& att) {
        if (!att.canAct() || att.hasActed) return;
        if (att.hasSkill(SkillName::BallAndChain)) return;

        auto adj = att.position.getAdjacent();
        for (auto& pos : adj) {
            if (!pos.isOnPitch()) continue;
            const Player* def = state.getPlayerAtPosition(pos);
            if (!def || def->teamSide == mySide) continue;
            if (def->state != PlayerState::STANDING) continue;

            int dice = getBlockDiceCount(state, att, *def, false);
            if (dice >= 2) {
                out.push_back({MacroType::BLOCK, att.id, def->id, {-1, -1}});
            }
        }
    });

    // PICKUP: ball on ground, best player by AG/distance/skills
    if (ballOnGround) {
        const Player* bestPicker = nullptr;
        int bestPickerScore = -999;

        state.forEachOnPitch(mySide, [&](const Player& p) {
            if (!isFreeToAct(p)) return;
            if (p.hasSkill(SkillName::BallAndChain)) return;
            if (p.hasSkill(SkillName::NoHands)) return;

            int dist = p.position.distanceTo(state.ball.position);
            int maxReach = p.movementRemaining + 2;
            if (dist > maxReach) return;

            int score = p.stats.agility * 10 - dist * 3;
            if (p.hasSkill(SkillName::SureHands)) score += 15;
            if (p.hasSkill(SkillName::BigHand)) score += 5;

            if (score > bestPickerScore) {
                bestPickerScore = score;
                bestPicker = &p;
            }
        });

        if (!bestPicker) {
            bestPicker = findNearestFreePlayer(state, state.ball.position);
        }
        if (bestPicker) {
            out.push_back({MacroType::PICKUP, bestPicker->id, -1, state.ball.position});
        }
    }

    // PASS: have ball, pass not used, teammate in range
    if (iHaveBall && !myTeam.passUsedThisTurn && carrier->canAct()) {
        state.forEachOnPitch(mySide, [&](const Player& target) {
            if (target.id == carrier->id) return;
            if (target.state != PlayerState::STANDING) return;
            int dist = carrier->position.distanceTo(target.position);
            // Within pass range and target is ahead
            int targetDist = distToEndzone(target.position, mySide);
            int carrierDist = distToEndzone(carrier->position, mySide);
            if (dist <= 10 && dist >= 1 && targetDist < carrierDist) {
                out.push_back({MacroType::PASS_ACTION, carrier->id, target.id, {-1, -1}});
            }
        });
    }

    // FOUL: foul not used, prone/stunned enemy adjacent
    if (!myTeam.foulUsedThisTurn) {
        state.forEachOnPitch(mySide, [&](const Player& fouler) {
            if (!fouler.canAct() || fouler.hasActed) return;
            if (fouler.hasSkill(SkillName::BallAndChain)) return;

            auto adj = fouler.position.getAdjacent();
            for (auto& pos : adj) {
                if (!pos.isOnPitch()) continue;
                const Player* target = state.getPlayerAtPosition(pos);
                if (!target || target->teamSide == mySide) continue;
                if (target->state != PlayerState::PRONE &&
                    target->state != PlayerState::STUNNED) continue;
                out.push_back({MacroType::FOUL, fouler.id, target->id, {-1, -1}});
                return; // one foul macro per fouler is enough
            }
        });
    }

    // REPOSITION: free (no adjacent enemies) standing player
    // Smart targeting: carrier protection, safety player, defensive screen
    int myEndzone = endzoneX(opponent(mySide));  // our own endzone to defend
    bool onDefense = !iHaveBall && !ballOnGround;
    bool receiverPlaced = false;
    bool hunterPlaced = false;
    bool cageTagPlaced = false;
    bool safetyPlaced = false;
    bool markerPlaced = false;
    int turnsLeft = std::max(0, 9 - myTeam.turnNumber);
    int endzoneGuardCount = 0;
    int screenSlot = 0;

    // Pre-compute defensive info
    const Player* oppCarrierPtr = nullptr;
    int oppScoringThreatCount = 0;
    if (onDefense) {
        if (state.ball.isHeld && state.ball.carrierId > 0) {
            oppCarrierPtr = &state.getPlayer(state.ball.carrierId);
            if (!oppCarrierPtr->isOnPitch()) oppCarrierPtr = nullptr;
        }
        state.forEachOnPitch(opponent(mySide), [&](const Player& op) {
            if (op.state != PlayerState::STANDING) return;
            int oppEzX = (op.teamSide == TeamSide::HOME) ? 25 : 0;
            int dist = std::abs(op.position.x - oppEzX);
            if (op.stats.movement + 2 >= dist &&
                countTacklezones(state, op.position, op.teamSide) == 0) {
                oppScoringThreatCount++;
            }
        });
    }

    state.forEachOnPitch(mySide, [&](const Player& p) {
        if (!isFreeToAct(p)) return;
        if (p.hasSkill(SkillName::BallAndChain)) return;
        if (iHaveBall && p.id == carrier->id) return; // carrier has SCORE/ADVANCE

        // Check if player is free (no adjacent enemies)
        bool hasAdjacentEnemy = false;
        auto adj = p.position.getAdjacent();
        for (auto& pos : adj) {
            if (!pos.isOnPitch()) continue;
            const Player* other = state.getPlayerAtPosition(pos);
            if (other && other->teamSide != mySide && other->state == PlayerState::STANDING) {
                hasAdjacentEnemy = true;
                break;
            }
        }
        if (hasAdjacentEnemy) return;

        Position target;

        if (ballOnGround) {
            // Loose ball: surround it
            target = state.ball.position;
        } else if (iHaveBall) {
            // Offense: support carrier (cage corners, screen ahead, receiver setup)
            int dx = forwardDx(mySide);
            int carrierDist = p.position.distanceTo(carrier->position);
            int ezX = endzoneX(mySide);

            // Hunter/shield split: fast players (MA≥7) pressure opponent scoring threats
            // while slow players stay as shields near carrier
            if (!hunterPlaced && p.stats.movement >= 7 && carrierDist > 4) {
                Position huntTarget = carrier->position;
                int bestThreat = -999;
                state.forEachOnPitch(opponent(mySide), [&](const Player& opp) {
                    if (opp.state != PlayerState::STANDING) return;
                    int threat = opp.stats.movement * 2 + opp.stats.agility;
                    if (countTacklezones(state, opp.position, opp.teamSide) == 0) threat += 5;
                    if (threat > bestThreat) {
                        bestThreat = threat;
                        huntTarget = opp.position;
                    }
                });
                target = huntTarget;
                hunterPlaced = true;
            }
            // Receiver setup: when ≤2 turns left, send fast player near endzone
            // as a pass/hand-off target for next turn's scoring chain
            else if (!receiverPlaced && turnsLeft <= 2 && p.stats.movement >= 6 &&
                carrierDist > 3) {
                int recvY = carrier->position.y + ((p.position.y > carrier->position.y) ? 2 : -2);
                recvY = std::clamp(recvY, 2, 12);
                int recvX = ezX - dx * 3; // 3 squares from endzone (reachable next turn)
                recvX = std::clamp(recvX, 1, 24);
                target = {static_cast<int8_t>(recvX), static_cast<int8_t>(recvY)};
                receiverPlaced = true;
            } else if (carrierDist <= 3) {
                // Already near carrier — move to cage/screen position ahead of carrier
                target = {static_cast<int8_t>(carrier->position.x + dx * 2),
                          static_cast<int8_t>(carrier->position.y)};
            } else {
                // Far from carrier — move toward carrier
                target = carrier->position;
            }
        } else if (onDefense) {
            // Defense: safety + marker on carrier + endzone guard + screen
            Position oppBallPos = state.ball.isOnPitch() ? state.ball.position
                : Position{static_cast<int8_t>(endzoneX(mySide)), 7};

            // Strategy 0: Cage corner tag — break opponent cage (one player at a time)
            bool usedCageTag = false;
            if (!cageTagPlaced && oppCarrierPtr != nullptr) {
                int cageCount = 0;
                state.forEachOnPitch(opponent(mySide), [&](const Player& opp) {
                    if (opp.id == oppCarrierPtr->id) return;
                    if (opp.state != PlayerState::STANDING) return;
                    if (oppCarrierPtr->position.distanceTo(opp.position) == 1) cageCount++;
                });
                if (cageCount >= 2) {
                    Position bestCorner = oppCarrierPtr->position;
                    int minFriendlyTZ = 999;
                    auto adj = oppCarrierPtr->position.getAdjacent();
                    for (auto& apos : adj) {
                        if (!apos.isOnPitch()) continue;
                        if (state.getPlayerAtPosition(apos)) continue;
                        int friendlyTZ = countTacklezones(state, apos, opponent(mySide));
                        if (friendlyTZ < minFriendlyTZ) {
                            minFriendlyTZ = friendlyTZ;
                            bestCorner = apos;
                        }
                    }
                    if (bestCorner.x != oppCarrierPtr->position.x ||
                        bestCorner.y != oppCarrierPtr->position.y) {
                        target = bestCorner;
                        cageTagPlaced = true;
                        usedCageTag = true;
                    }
                }
            }
            // Strategies 1-4: only when cage tag not used this iteration
            if (!usedCageTag) {
            // Strategy 1: Safety player (fast, near our endzone)
            if (!safetyPlaced && p.stats.movement >= 6) {
                target = {static_cast<int8_t>(myEndzone),
                          static_cast<int8_t>(7)};
                safetyPlaced = true;
            }
            // Strategy 2: Pressure marker — move toward opponent carrier
            else if (!markerPlaced && oppCarrierPtr != nullptr) {
                target = oppCarrierPtr->position;
                markerPlaced = true;
            }
            // Strategy 3: Endzone guard — prevent one-turn TD
            else if (oppScoringThreatCount > 0 && endzoneGuardCount < 2) {
                int guardX = myEndzone + forwardDx(mySide) * 4;
                int guardY = (endzoneGuardCount == 0) ? 5 : 9;
                target = {static_cast<int8_t>(std::clamp(guardX, 1, 24)),
                          static_cast<int8_t>(guardY)};
                endzoneGuardCount++;
            }
            // Strategy 4: Defensive screen — evenly spread between ball and endzone
            else {
                int screenX = (oppBallPos.x + myEndzone) / 2;
                static const int screenYs[] = {3, 5, 7, 9, 11};
                int screenY = screenYs[screenSlot % 5];
                screenSlot++;
                target = {static_cast<int8_t>(std::clamp(screenX, 1, 24)),
                          static_cast<int8_t>(screenY)};
            }
            } // end !usedCageTag
        } else {
            // Move forward toward center
            int dx = forwardDx(mySide);
            target = {static_cast<int8_t>(p.position.x + dx * 3),
                      static_cast<int8_t>(7)}; // center Y
        }

        out.push_back({MacroType::REPOSITION, p.id, -1, target});
    });
}

// --- Macro Expansion ---

// Execute a single action, add to result, return true if turnover
static bool executeAndRecord(GameState& state, const Action& action,
                             DiceRollerBase& dice, MacroExpansionResult& result) {
    result.actions.push_back(action);
    ActionResult ar = executeAction(state, action, dice, nullptr);
    if (ar.turnover) {
        result.turnover = true;
        return true;
    }
    return false;
}

// Find and execute MOVE actions for playerId toward target, up to maxSteps.
// Uses state-aware scoring to avoid enemy tackle zones (prefers safe routes).
static bool movePlayerToward(GameState& state, int playerId, Position target,
                              DiceRollerBase& dice, MacroExpansionResult& result,
                              int maxSteps = 12) {
    Position lastPos{-1, -1};  // Detect loops
    for (int step = 0; step < maxSteps; ++step) {
        const Player& p = state.getPlayer(playerId);
        if (!p.isOnPitch() || p.lostTacklezones) return false;
        if (p.position == target) return true; // arrived

        // Get available actions
        std::vector<Action> actions;
        getAvailableActions(state, actions);

        // Find best move toward target (with TZ avoidance)
        Action bestMove;
        if (!findMoveToward(actions, playerId, target, bestMove, &state)) return false;

        // Allow sideways moves to dodge around opponents, but don't go too far
        int currentDist = p.position.distanceTo(target);
        int moveDist = bestMove.target.distanceTo(target);
        if (moveDist > currentDist + 1) return false; // max 1 square detour
        if (moveDist >= currentDist && bestMove.target == lastPos) return false; // loop

        lastPos = p.position;
        if (executeAndRecord(state, bestMove, dice, result)) return false;
    }
    return false;
}

static MacroExpansionResult expandScore(GameState& state, const Macro& macro,
                                         DiceRollerBase& dice) {
    MacroExpansionResult result;
    const Player& carrier = state.getPlayer(macro.playerId);
    int targetX = endzoneX(carrier.teamSide);
    int dx = forwardDx(carrier.teamSide);

    // Evaluate TZ exposure for different Y-target routes, pick safest
    int bestY = carrier.position.y;
    int bestTZ = 999;

    for (int yOff = -2; yOff <= 2; ++yOff) {
        int testY = carrier.position.y + yOff;
        if (testY < 1 || testY > 13) continue; // avoid sidelines

        // Count enemy TZ along approximate path
        int tzSum = 0;
        int cx = carrier.position.x;
        int cy = carrier.position.y;
        while (cx != targetX || cy != testY) {
            if (cy < testY) cy++;
            else if (cy > testY) cy--;
            if (cx != targetX) cx += dx;
            Position p{static_cast<int8_t>(cx), static_cast<int8_t>(cy)};
            if (p.isOnPitch()) {
                tzSum += countTacklezones(state, p, carrier.teamSide);
            }
        }
        if (tzSum < bestTZ) {
            bestTZ = tzSum;
            bestY = testY;
        }
    }

    Position target{static_cast<int8_t>(targetX), static_cast<int8_t>(bestY)};
    movePlayerToward(state, macro.playerId, target, dice, result, 14);
    return result;
}

static MacroExpansionResult expandAdvance(GameState& state, const Macro& macro,
                                           DiceRollerBase& dice) {
    MacroExpansionResult result;
    const Player& carrier = state.getPlayer(macro.playerId);
    int dx = forwardDx(carrier.teamSide);
    const auto& myTeam = state.getTeamState(carrier.teamSide);

    // Stall-aware advancement: move just enough to reach endzone on the last turn
    int dist = distToEndzone(carrier.position, carrier.teamSide);
    int turnsRemaining = std::max(1, 9 - myTeam.turnNumber); // turns left including this one
    int maPerTurn = carrier.stats.movement;  // approximate MA per future turn

    // Target: advance dist/turnsRemaining per turn (arrive on last turn)
    // Add small buffer for GFI (2 per turn)
    int idealStepsThisTurn = std::max(1, (dist + turnsRemaining - 1) / turnsRemaining);

    // Don't exceed remaining movement, don't use more than half MA (save for cage/dodge)
    int mvRemaining = static_cast<int>(carrier.movementRemaining);
    int maxSafe = std::max(1, mvRemaining / 2);
    int steps = std::min(idealStepsThisTurn, maxSafe);
    // But if last 2 turns, use all remaining movement
    if (turnsRemaining <= 2) {
        steps = std::min(idealStepsThisTurn, mvRemaining);
    }

    int targetX = carrier.position.x + dx * steps;
    targetX = std::clamp(targetX, 1, 24); // stay on pitch
    // Bias Y toward center (7)
    int targetY = carrier.position.y;
    if (targetY < 5) targetY++;
    else if (targetY > 9) targetY--;

    Position target{static_cast<int8_t>(targetX), static_cast<int8_t>(targetY)};
    movePlayerToward(state, macro.playerId, target, dice, result, steps + 2);
    return result;
}

static MacroExpansionResult expandCage(GameState& state, const Macro& macro,
                                        DiceRollerBase& dice) {
    MacroExpansionResult result;
    const Player& carrier = state.getPlayer(macro.playerId);
    Position cp = carrier.position;

    // 4 diagonal cage positions
    Position cagePositions[4] = {
        {static_cast<int8_t>(cp.x + 1), static_cast<int8_t>(cp.y + 1)},
        {static_cast<int8_t>(cp.x + 1), static_cast<int8_t>(cp.y - 1)},
        {static_cast<int8_t>(cp.x - 1), static_cast<int8_t>(cp.y + 1)},
        {static_cast<int8_t>(cp.x - 1), static_cast<int8_t>(cp.y - 1)},
    };

    for (auto& cagePos : cagePositions) {
        if (!cagePos.isOnPitch()) continue;

        // Already occupied?
        const Player* occupant = state.getPlayerAtPosition(cagePos);
        if (occupant) {
            // If it's our standing player, that's fine
            if (occupant->teamSide == state.activeTeam &&
                occupant->state == PlayerState::STANDING) continue;
            // Otherwise skip this position
            continue;
        }

        // Find nearest free player (not carrier)
        const Player* mover = findNearestFreePlayer(state, cagePos, carrier.id);
        if (!mover) continue;

        // Move them there (max 4 steps)
        movePlayerToward(state, mover->id, cagePos, dice, result, 4);
        if (result.turnover) return result;
    }
    return result;
}

static MacroExpansionResult expandBlitz(GameState& state, const Macro& macro,
                                         DiceRollerBase& dice) {
    MacroExpansionResult result;

    const Player& target = state.getPlayer(macro.targetId);

    // Find best BLITZ action for this target (prefer more dice, closer blitzer)
    std::vector<Action> actions;
    getAvailableActions(state, actions);

    Action bestBlitzAction{};
    bool found = false;
    int bestScore = -999;

    for (auto& a : actions) {
        if (a.type != ActionType::BLITZ || a.targetId != macro.targetId) continue;
        const Player& blitzer = state.getPlayer(a.playerId);
        int diceCount = getBlockDiceCount(state, blitzer, target, true);
        int dist = blitzer.position.distanceTo(target.position);
        int score = diceCount * 10 - dist; // more dice + closer = better
        if (score > bestScore) {
            bestScore = score;
            bestBlitzAction = a;
            found = true;
        }
    }

    if (!found) return result;

    executeAndRecord(state, bestBlitzAction, dice, result);
    return result;
}

static MacroExpansionResult expandBlitzAndScore(GameState& state, const Macro& macro,
                                                  DiceRollerBase& dice) {
    MacroExpansionResult result;

    // Step 1: Find a blitzer and blitz the blocker out of the way
    const Player& blocker = state.getPlayer(macro.targetId);
    int carrierId = macro.playerId;

    std::vector<Action> actions;
    getAvailableActions(state, actions);

    // Find the best blitzer for this target (prefer non-carrier with good dice)
    Action bestBlitz{};
    bool foundBlitz = false;
    int bestDice = -99;

    for (auto& a : actions) {
        if (a.type != ActionType::BLITZ) continue;
        if (a.targetId != macro.targetId) continue;
        const Player& blitzer = state.getPlayer(a.playerId);
        int dice_count = getBlockDiceCount(state, blitzer, blocker, true);
        // Prefer non-carrier blitzer; if same dice count, use non-carrier
        bool isCarrier = (a.playerId == carrierId);
        int score = dice_count * 10 + (isCarrier ? 0 : 5);
        if (score > bestDice) {
            bestDice = score;
            bestBlitz = a;
            foundBlitz = true;
        }
    }

    if (!foundBlitz) return result; // can't blitz, abort

    // Execute the blitz
    if (executeAndRecord(state, bestBlitz, dice, result)) return result;

    // After blitz, continue with any follow-up moves/blocks from the blitz action
    // The blitz action may generate further MOVE/BLOCK actions
    for (int step = 0; step < 12; ++step) {
        actions.clear();
        getAvailableActions(state, actions);

        // If we can block the target, do it
        bool blocked = false;
        for (auto& a : actions) {
            if (a.type == ActionType::BLOCK && a.playerId == bestBlitz.playerId &&
                a.targetId == macro.targetId) {
                if (executeAndRecord(state, a, dice, result)) return result;
                blocked = true;
                break;
            }
        }
        if (blocked) break;

        // Move blitzer toward target
        Action moveAction;
        if (!findMoveToward(actions, bestBlitz.playerId, blocker.position, moveAction, &state))
            break;
        if (executeAndRecord(state, moveAction, dice, result)) return result;
    }

    // Step 2: Now move the carrier to score
    const Player& carrier = state.getPlayer(carrierId);
    if (!carrier.isOnPitch() || carrier.lostTacklezones) return result;
    if (!carrier.canAct()) return result;

    int targetX = endzoneX(carrier.teamSide);
    Position target{static_cast<int8_t>(targetX), carrier.position.y};
    movePlayerToward(state, carrierId, target, dice, result, 14);
    return result;
}

static MacroExpansionResult expandBlock(GameState& state, const Macro& macro,
                                         DiceRollerBase& dice) {
    MacroExpansionResult result;

    std::vector<Action> actions;
    getAvailableActions(state, actions);

    for (auto& a : actions) {
        if (a.type == ActionType::BLOCK && a.playerId == macro.playerId &&
            a.targetId == macro.targetId) {
            executeAndRecord(state, a, dice, result);
            return result;
        }
    }
    return result;
}

static MacroExpansionResult expandPickup(GameState& state, const Macro& macro,
                                          DiceRollerBase& dice) {
    MacroExpansionResult result;
    movePlayerToward(state, macro.playerId, macro.targetPos, dice, result, 8);
    if (result.turnover) return result;

    // After pickup: if we now have the ball, advance toward endzone
    const Player& p = state.getPlayer(macro.playerId);
    if (state.ball.isHeld && state.ball.carrierId == macro.playerId &&
        p.isOnPitch() && p.movementRemaining > 0 && !p.lostTacklezones) {
        int targetX = endzoneX(p.teamSide);
        int targetY = p.position.y;
        if (targetY < 5) targetY++;
        else if (targetY > 9) targetY--;
        Position target{static_cast<int8_t>(targetX), static_cast<int8_t>(targetY)};
        movePlayerToward(state, macro.playerId, target, dice, result,
                          p.movementRemaining);
    }
    return result;
}

static MacroExpansionResult expandPass(GameState& state, const Macro& macro,
                                        DiceRollerBase& dice) {
    MacroExpansionResult result;

    std::vector<Action> actions;
    getAvailableActions(state, actions);

    // Try HAND_OFF first (safer), then PASS
    for (ActionType passType : {ActionType::HAND_OFF, ActionType::PASS}) {
        for (auto& a : actions) {
            if (a.type == passType && a.playerId == macro.playerId &&
                a.targetId == macro.targetId) {
                executeAndRecord(state, a, dice, result);
                return result;
            }
        }
    }
    return result;
}

static MacroExpansionResult expandFoul(GameState& state, const Macro& macro,
                                        DiceRollerBase& dice) {
    MacroExpansionResult result;

    std::vector<Action> actions;
    getAvailableActions(state, actions);

    for (auto& a : actions) {
        if (a.type == ActionType::FOUL && a.playerId == macro.playerId &&
            a.targetId == macro.targetId) {
            executeAndRecord(state, a, dice, result);
            return result;
        }
    }
    return result;
}

static MacroExpansionResult expandReposition(GameState& state, const Macro& macro,
                                              DiceRollerBase& dice) {
    MacroExpansionResult result;
    movePlayerToward(state, macro.playerId, macro.targetPos, dice, result, 4);
    return result;
}

static MacroExpansionResult expandEndTurn(GameState& state, const Macro& /*macro*/,
                                           DiceRollerBase& dice) {
    MacroExpansionResult result;
    Action endTurn{ActionType::END_TURN, -1, -1, {-1, -1}};
    executeAndRecord(state, endTurn, dice, result);
    return result;
}

static MacroExpansionResult expandHandOffScore(GameState& state, const Macro& macro,
                                                DiceRollerBase& dice) {
    MacroExpansionResult result;
    int carrierId = macro.playerId;
    int receiverId = macro.targetId;
    if (carrierId <= 0 || receiverId <= 0) return result;

    const Player& receiver = state.getPlayer(receiverId);
    if (!receiver.isOnPitch()) return result;

    // Step 1: Move carrier adjacent to receiver (if not already adjacent)
    {
        const Player& carrier = state.getPlayer(carrierId);
        if (!carrier.isOnPitch() || !carrier.canAct()) return result;
        int dist = carrier.position.distanceTo(receiver.position);
        if (dist > 1) {
            movePlayerToward(state, carrierId, receiver.position, dice, result,
                             carrier.movementRemaining);
            if (result.turnover) return result;
        }
    }

    // Step 2: Execute HAND_OFF
    {
        std::vector<Action> actions;
        getAvailableActions(state, actions);
        bool executed = false;
        for (auto& a : actions) {
            if (a.type == ActionType::HAND_OFF && a.playerId == carrierId &&
                a.targetId == receiverId) {
                if (executeAndRecord(state, a, dice, result)) return result;
                executed = true;
                break;
            }
        }
        if (!executed) return result;
    }

    // Step 3: Move receiver to score
    if (!state.ball.isHeld || state.ball.carrierId != receiverId) return result;
    const Player& newCarrier = state.getPlayer(receiverId);
    if (!newCarrier.isOnPitch() || !newCarrier.canAct()) return result;

    int targetX = endzoneX(newCarrier.teamSide);
    Position target{static_cast<int8_t>(targetX), newCarrier.position.y};
    movePlayerToward(state, receiverId, target, dice, result, 14);
    return result;
}

static MacroExpansionResult expandPassScore(GameState& state, const Macro& macro,
                                             DiceRollerBase& dice) {
    MacroExpansionResult result;
    int carrierId = macro.playerId;
    int receiverId = macro.targetId;
    if (carrierId <= 0 || receiverId <= 0) return result;

    // Step 1: Pass to receiver
    std::vector<Action> actions;
    getAvailableActions(state, actions);

    bool passed = false;
    for (auto& a : actions) {
        if (a.type == ActionType::PASS && a.playerId == carrierId &&
            a.targetId == receiverId) {
            if (executeAndRecord(state, a, dice, result)) return result;
            passed = true;
            break;
        }
    }
    if (!passed) return result;

    // Step 2: Move receiver to endzone (if catch succeeded)
    if (!state.ball.isHeld || state.ball.carrierId != receiverId) return result;
    const Player& rcv = state.getPlayer(receiverId);
    if (!rcv.isOnPitch() || rcv.lostTacklezones) return result;

    int targetX = endzoneX(rcv.teamSide);
    Position target{static_cast<int8_t>(targetX), rcv.position.y};
    movePlayerToward(state, receiverId, target, dice, result, 14);
    return result;
}

static MacroExpansionResult expandChainScore(GameState& state, const Macro& macro,
                                              DiceRollerBase& dice) {
    MacroExpansionResult result;
    int carrierId = macro.playerId;
    int relayId   = macro.targetId;
    int scorerId  = macro.thirdId;
    if (carrierId <= 0 || relayId <= 0 || scorerId <= 0) return result;

    // Step 1: Pass to relay
    std::vector<Action> actions;
    getAvailableActions(state, actions);
    bool passed = false;
    for (auto& a : actions) {
        if (a.type == ActionType::PASS && a.playerId == carrierId &&
            a.targetId == relayId) {
            if (executeAndRecord(state, a, dice, result)) return result;
            passed = true;
            break;
        }
    }
    if (!passed) return result;
    if (!state.ball.isHeld || state.ball.carrierId != relayId) return result;

    // Step 2: Relay moves adjacent to scorer and hand-offs
    const Player& relay = state.getPlayer(relayId);
    if (!relay.isOnPitch() || !relay.canAct()) return result;

    const Player& scorer = state.getPlayer(scorerId);
    if (!scorer.isOnPitch()) return result;

    // Move relay adjacent to scorer
    if (relay.position.distanceTo(scorer.position) > 1) {
        movePlayerToward(state, relayId, scorer.position, dice, result, relay.movementRemaining);
        if (result.turnover) return result;
    }

    // Hand-off to scorer
    actions.clear();
    getAvailableActions(state, actions);
    for (auto& a : actions) {
        if (a.type == ActionType::HAND_OFF && a.playerId == relayId &&
            a.targetId == scorerId) {
            if (executeAndRecord(state, a, dice, result)) return result;
            break;
        }
    }
    if (!state.ball.isHeld || state.ball.carrierId != scorerId) return result;

    // Step 3: Scorer moves to endzone
    const Player& sc = state.getPlayer(scorerId);
    if (!sc.isOnPitch() || !sc.canAct()) return result;
    int targetX = endzoneX(sc.teamSide);
    Position target{static_cast<int8_t>(targetX), sc.position.y};
    movePlayerToward(state, scorerId, target, dice, result, 14);
    return result;
}

MacroExpansionResult greedyExpandMacro(GameState& state, const Macro& macro,
                                       DiceRollerBase& dice) {
    switch (macro.type) {
        case MacroType::SCORE:       return expandScore(state, macro, dice);
        case MacroType::ADVANCE:     return expandAdvance(state, macro, dice);
        case MacroType::CAGE:        return expandCage(state, macro, dice);
        case MacroType::BLITZ:       return expandBlitz(state, macro, dice);
        case MacroType::BLOCK:       return expandBlock(state, macro, dice);
        case MacroType::PICKUP:      return expandPickup(state, macro, dice);
        case MacroType::PASS_ACTION: return expandPass(state, macro, dice);
        case MacroType::FOUL:        return expandFoul(state, macro, dice);
        case MacroType::REPOSITION:  return expandReposition(state, macro, dice);
        case MacroType::END_TURN:    return expandEndTurn(state, macro, dice);
        case MacroType::BLITZ_AND_SCORE: return expandBlitzAndScore(state, macro, dice);
        case MacroType::HAND_OFF_SCORE:  return expandHandOffScore(state, macro, dice);
        case MacroType::PASS_SCORE:      return expandPassScore(state, macro, dice);
        case MacroType::CHAIN_SCORE:     return expandChainScore(state, macro, dice);
        default:                     return {};
    }
}

// --- Macro Feature Extraction ---

void extractMacroFeatures(const GameState& state, const Macro& macro, float* out) {
    for (int i = 0; i < NUM_ACTION_FEATURES; ++i) out[i] = 0.0f;

    int typeIdx = static_cast<int>(macro.type);

    // [0-9] one-hot macro type (BLITZ_AND_SCORE shares BLITZ slot, HAND_OFF_SCORE shares SCORE slot)
    if (macro.type == MacroType::BLITZ_AND_SCORE) {
        out[static_cast<int>(MacroType::BLITZ)] = 1.0f;
    } else if (macro.type == MacroType::HAND_OFF_SCORE ||
               macro.type == MacroType::PASS_SCORE ||
               macro.type == MacroType::CHAIN_SCORE) {
        out[static_cast<int>(MacroType::SCORE)] = 1.0f;
    } else if (typeIdx >= 0 && typeIdx < 10) {
        out[typeIdx] = 1.0f;
    }

    TeamSide mySide = state.activeTeam;

    // [10] scoring_potential
    if (macro.type == MacroType::SCORE || macro.type == MacroType::BLITZ_AND_SCORE ||
        macro.type == MacroType::HAND_OFF_SCORE ||
        macro.type == MacroType::PASS_SCORE ||
        macro.type == MacroType::CHAIN_SCORE) {
        out[10] = 1.0f;
    } else if (macro.type == MacroType::ADVANCE && macro.playerId > 0) {
        const Player& p = state.getPlayer(macro.playerId);
        if (p.isOnPitch()) {
            int dist = distToEndzone(p.position, mySide);
            int ma = p.movementRemaining + 2;
            out[10] = std::min(1.0f, static_cast<float>(ma) / std::max(dist, 1));
        }
    }

    // [11] block_dice_quality
    if ((macro.type == MacroType::BLOCK || macro.type == MacroType::BLITZ ||
         macro.type == MacroType::BLITZ_AND_SCORE) &&
        macro.targetId > 0 && macro.playerId > 0) {
        const Player& att = state.getPlayer(macro.playerId);
        const Player& def = state.getPlayer(macro.targetId);
        if (att.isOnPitch() && def.isOnPitch()) {
            bool isBlitz = (macro.type == MacroType::BLITZ);
            int dice = getBlockDiceCount(state, att, def, isBlitz);
            out[11] = dice / 3.0f;
        }
    } else if (macro.type == MacroType::BLITZ && macro.targetId > 0 && macro.playerId <= 0) {
        // Blitz with unspecified blitzer — estimate based on target
        out[11] = 0.3f; // default moderate quality
    }

    // [12] player_strength / 7
    if (macro.playerId > 0) {
        const Player& p = state.getPlayer(macro.playerId);
        out[12] = p.stats.strength / 7.0f;
    }

    // [13] risk_level (probability of failure estimate)
    switch (macro.type) {
        case MacroType::END_TURN:
            out[13] = 0.0f; // no risk
            break;
        case MacroType::BLOCK:
            out[13] = 0.15f; // low risk for favorable block
            break;
        case MacroType::BLITZ:
            out[13] = 0.25f; // moderate (movement + block)
            break;
        case MacroType::BLITZ_AND_SCORE:
            out[13] = 0.35f; // higher risk (blitz + movement to score)
            break;
        case MacroType::SCORE:
            // Risk depends on distance (GFIs)
            if (macro.playerId > 0) {
                const Player& p = state.getPlayer(macro.playerId);
                if (p.isOnPitch()) {
                    int dist = distToEndzone(p.position, mySide);
                    int gfis = std::max(0, dist - p.movementRemaining);
                    out[13] = gfis * 0.17f; // ~1/6 per GFI
                }
            }
            break;
        case MacroType::PICKUP:
            out[13] = 0.33f; // pickup roll
            break;
        case MacroType::PASS_ACTION:
            out[13] = 0.4f; // catch + interception risk
            break;
        case MacroType::PASS_SCORE:
            out[13] = 0.40f; // pass accuracy + catch + interception risk
            break;
        case MacroType::CHAIN_SCORE:
            out[13] = 0.55f; // pass + catch + hand-off + catch — highest risk
            break;
        case MacroType::FOUL:
            out[13] = 0.08f; // ejection risk
            break;
        default:
            out[13] = 0.1f;
            break;
    }

    // [14] positional_gain
    if (macro.type == MacroType::SCORE || macro.type == MacroType::BLITZ_AND_SCORE ||
        macro.type == MacroType::HAND_OFF_SCORE ||
        macro.type == MacroType::PASS_SCORE ||
        macro.type == MacroType::CHAIN_SCORE) {
        out[14] = 1.0f;
    } else if (macro.type == MacroType::ADVANCE && macro.playerId > 0) {
        const Player& p = state.getPlayer(macro.playerId);
        if (p.isOnPitch()) {
            int steps = std::max(1, p.movementRemaining / 2);
            out[14] = std::min(1.0f, steps / 6.0f);
        }
    } else if (macro.type == MacroType::CAGE) {
        out[14] = 0.5f; // good positional improvement
    } else if (macro.type == MacroType::REPOSITION) {
        out[14] = 0.3f;
    }
}

} // namespace bb
