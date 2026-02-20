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
    if (!myTeam.blitzUsedThisTurn) {
        // Find best blitz target (scoring: dice * 2 + sideline * 3 + near carrier * 2)
        int bestScore = -999;
        int bestTargetId = -1;

        state.forEachOnPitch(mySide, [&](const Player& blitzer) {
            if (!isFreeToAct(blitzer)) return;
            if (blitzer.hasSkill(SkillName::BallAndChain)) return;

            state.forEachOnPitch(opponent(mySide), [&](const Player& def) {
                if (def.state != PlayerState::STANDING) return;

                int dice = getBlockDiceCount(state, blitzer, def, true);
                int score = dice * 2;

                // Sideline bonus: target on edge
                if (def.position.y == 0 || def.position.y == Position::PITCH_HEIGHT - 1) {
                    score += 3;
                }
                // Near carrier bonus
                if (iHaveBall && def.position.distanceTo(carrier->position) <= 2) {
                    score += 2;
                }
                // Ball carrier target bonus
                if (state.ball.isHeld && state.ball.carrierId == def.id) {
                    score += 5;
                }

                if (score > bestScore) {
                    bestScore = score;
                    bestTargetId = def.id;
                }
            });
        });

        if (bestTargetId > 0) {
            out.push_back({MacroType::BLITZ, -1, bestTargetId, {-1, -1}});
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

    // PICKUP: ball on ground, move nearest player to ball
    if (ballOnGround) {
        const Player* nearest = findNearestFreePlayer(state, state.ball.position);
        if (nearest) {
            out.push_back({MacroType::PICKUP, nearest->id, -1, state.ball.position});
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
    bool safetyPlaced = false;

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
            // Offense: support carrier (cage corners, screen ahead)
            int dx = forwardDx(mySide);
            int carrierDist = p.position.distanceTo(carrier->position);

            if (carrierDist <= 3) {
                // Already near carrier — move to cage/screen position ahead of carrier
                target = {static_cast<int8_t>(carrier->position.x + dx * 2),
                          static_cast<int8_t>(carrier->position.y)};
            } else {
                // Far from carrier — move toward carrier
                target = carrier->position;
            }
        } else if (onDefense) {
            // Defense: safety player (H6.9) + screen between ball and endzone (H6.1, H6.2)
            if (!safetyPlaced && p.stats.movement >= 6) {
                // Safety player: fast player near our endzone
                target = {static_cast<int8_t>(myEndzone),
                          static_cast<int8_t>(7)};
                safetyPlaced = true;
            } else {
                // Defensive screen: position between ball and our endzone
                Position ballPos = state.ball.isOnPitch() ? state.ball.position
                    : Position{static_cast<int8_t>(endzoneX(mySide)), 7};
                int screenX = (ballPos.x + myEndzone) / 2;
                // Spread Y across the pitch
                int screenY = 3 + (p.id % 9);  // distribute 3-11
                target = {static_cast<int8_t>(screenX),
                          static_cast<int8_t>(std::clamp(screenY, 1, 13))};
            }
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
    Position target{static_cast<int8_t>(targetX), carrier.position.y};
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

    // Find the best blitzer that can reach the target
    const Player& target = state.getPlayer(macro.targetId);

    // Look for BLITZ action for this target in available actions
    std::vector<Action> actions;
    getAvailableActions(state, actions);

    for (auto& a : actions) {
        if (a.type == ActionType::BLITZ && a.targetId == macro.targetId) {
            executeAndRecord(state, a, dice, result);
            return result;
        }
    }

    // If no direct blitz, try to find any blitz on this target
    // (different blitzer might be available)
    for (auto& a : actions) {
        if (a.type == ActionType::BLITZ && a.targetId == macro.targetId) {
            executeAndRecord(state, a, dice, result);
            return result;
        }
    }

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
        default:                     return {};
    }
}

// --- Macro Feature Extraction ---

void extractMacroFeatures(const GameState& state, const Macro& macro, float* out) {
    for (int i = 0; i < NUM_ACTION_FEATURES; ++i) out[i] = 0.0f;

    int typeIdx = static_cast<int>(macro.type);

    // [0-9] one-hot macro type (BLITZ_AND_SCORE shares BLITZ slot)
    if (macro.type == MacroType::BLITZ_AND_SCORE) {
        out[static_cast<int>(MacroType::BLITZ)] = 1.0f;  // shares BLITZ slot
    } else if (typeIdx >= 0 && typeIdx < 10) {
        out[typeIdx] = 1.0f;
    }

    TeamSide mySide = state.activeTeam;

    // [10] scoring_potential
    if (macro.type == MacroType::SCORE || macro.type == MacroType::BLITZ_AND_SCORE) {
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
        case MacroType::FOUL:
            out[13] = 0.08f; // ejection risk
            break;
        default:
            out[13] = 0.1f;
            break;
    }

    // [14] positional_gain
    if (macro.type == MacroType::SCORE || macro.type == MacroType::BLITZ_AND_SCORE) {
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
