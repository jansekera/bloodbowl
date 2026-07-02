#include "bb/macro_mcts.h"
#include "bb/action_resolver.h"
#include "bb/helpers.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

namespace bb {

static int distToEndzone(Position pos, TeamSide side) {
    int ezX = (side == TeamSide::HOME) ? 25 : 0;
    return std::abs(pos.x - ezX);
}

// Bounded greedy leaf look-ahead (2026-07-02).
//
// The leaf eval otherwise has ZERO lookahead (pure static heuristic on the
// current snapshot). This ranks a leaf's own available macros by a fixed,
// hand-coded priority (score-family > advance-the-ball > everything else),
// applies ONLY the single top-ranked one to a CLONE via the existing
// greedyExpandMacro() (real atomic-action resolution, real dice), and turns
// the resulting one-ply-deeper state into a small bonus/penalty term. This
// is mechanistically NOT the reverted carrierTDHorizon lever (2026-06-26):
// that transformed an EXISTING value with a floor/max clamp applied AFTER
// the vf blend, which flattened the landscape. This computes a genuinely
// NEW, independent signal from an extra forward-simulated ply and is folded
// into scoringBonus (added post-vf-blend, same bucket as the existing
// offensive pull terms below) -- nothing here clamps or transforms a prior
// value, it only adds evidence the static pacing formula cannot see (e.g.
// whether continuing this drive one more macro actually loses the ball or
// gains no real ground, vs. the static idealDist=turnsLeft*ma formula which
// scores pure book-keeping distance regardless of whether the path is open).
static int greedyMacroRank(MacroType t) {
    switch (t) {
        case MacroType::SCORE:            return 100;
        case MacroType::BLITZ_AND_SCORE:  return 95;
        case MacroType::HAND_OFF_SCORE:   return 90;
        case MacroType::PASS_SCORE:       return 88;
        case MacroType::CHAIN_SCORE:      return 86;
        case MacroType::ADVANCE:          return 50;
        case MacroType::CAGE:             return 45;
        case MacroType::PICKUP:           return 40;
        case MacroType::BLITZ:            return 20;
        case MacroType::BLOCK:            return 15;
        case MacroType::REPOSITION:       return 10;
        case MacroType::FOUL:             return 5;
        case MacroType::END_TURN:         return 0;
        default:                          return 1;
    }
}

// --- MacroMCTSNode ---

MacroMCTSNode* MacroMCTSNode::bestChildPUCT(double C) const {
    if (children.empty()) return nullptr;
    double parentVisits = static_cast<double>(visits);

    // FPU: average Q of visited children
    double visitedSum = 0.0;
    int visitedCount = 0;
    for (auto& child : children) {
        if (child.visits > 0) {
            visitedSum += child.totalValue / child.visits;
            visitedCount++;
        }
    }
    double fpu = (visitedCount > 0) ? visitedSum / visitedCount : 0.0;

    MacroMCTSNode* best = nullptr;
    double bestScore = -std::numeric_limits<double>::max();
    for (auto& child : const_cast<std::vector<MacroMCTSNode>&>(children)) {
        double q = (child.visits == 0) ? fpu : child.totalValue / child.visits;
        double u = C * child.prior * std::sqrt(parentVisits) / (1.0 + child.visits);
        double score = q + u;
        if (score > bestScore) {
            bestScore = score;
            best = &child;
        }
    }
    return best;
}

MacroMCTSNode* MacroMCTSNode::mostVisitedChild() const {
    if (children.empty()) return nullptr;
    MacroMCTSNode* best = nullptr;
    int bestVisits = -1;
    for (auto& child : const_cast<std::vector<MacroMCTSNode>&>(children)) {
        if (child.visits > bestVisits) {
            bestVisits = child.visits;
            best = &child;
        }
    }
    return best;
}

// --- MacroMCTSSearch ---

MacroMCTSSearch::MacroMCTSSearch(const ValueFunction* vf, MCTSConfig config, uint32_t seed)
    : valueFn_(vf), config_(config), dice_(seed) {}

Macro MacroMCTSSearch::search(const GameState& state) {
    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    if (macros.empty()) {
        return {MacroType::END_TURN, -1, -1, {-1, -1}};
    }
    if (macros.size() == 1) {
        lastIterations_ = 0;
        lastBestValue_ = 0.0;
        lastChildVisits_.clear();
        return macros[0];
    }

    // Create root
    MacroMCTSNode root;
    root.visits = 1;  // Virtual visit for PUCT
    root.expanded = false;

    // Expand root
    expand(&root, state);

    // Dirichlet noise on root priors (AlphaZero-style exploration)
    if (config_.dirichletAlpha > 0.0f && !root.children.empty()) {
        int n = static_cast<int>(root.children.size());
        std::mt19937 rng(dice_.rollD6() * 1000 + dice_.rollD6());
        std::gamma_distribution<float> gamma(config_.dirichletAlpha, 1.0f);
        std::vector<float> noise(n);
        float noiseSum = 0.0f;
        for (int i = 0; i < n; ++i) {
            noise[i] = gamma(rng);
            noiseSum += noise[i];
        }
        if (noiseSum > 0.0f) {
            float w = config_.dirichletWeight;
            for (int i = 0; i < n; ++i) {
                noise[i] /= noiseSum;
                root.children[i].prior = (1.0f - w) * root.children[i].prior + w * noise[i];
            }
        }
    }

    TeamSide searchingSide = state.activeTeam;

    int iterations = 0;
    while (iterations < config_.maxIterations) {
        // 1. Select
        MacroMCTSNode* node = select(&root);

        // 2. Replay state to this node (open-loop: fresh dice each replay)
        GameState sim = state.clone();
        if (!replayToNode(sim, node)) {
            iterations++;
            continue;
        }

        // 3. Expand if unexpanded
        if (!node->expanded && node->visits > 0) {
            expand(node, sim);
            if (!node->children.empty()) {
                node = &node->children[0];
                // Execute this child's macro to get leaf state
                greedyExpandMacro(sim, node->macro, dice_);
            }
        }

        // 4. Evaluate leaf, averaging nRollouts open-loop samples to cut
        //    Q-variance (~sqrt(K)). The first rollout reuses the already-replayed
        //    `sim`; extra rollouts re-replay with fresh dice from this node.
        double value = simulate(sim, searchingSide);
        int nRollouts = std::max(1, config_.nRollouts);
        for (int r = 1; r < nRollouts; ++r) {
            GameState extra = state.clone();
            if (!replayToNode(extra, node)) continue;
            value += simulate(extra, searchingSide);
        }
        value /= static_cast<double>(nRollouts);

        // 5. Backpropagate
        backpropagate(node, value);

        iterations++;
    }

    lastIterations_ = iterations;

    // Save child visit info
    lastChildVisits_.clear();
    for (auto& child : root.children) {
        if (child.visits > 0) {
            lastChildVisits_.push_back({child.macro, child.visits});
        }
    }

    MacroMCTSNode* best = root.mostVisitedChild();
    if (best) {
        lastBestValue_ = best->visits > 0
            ? best->totalValue / best->visits : 0.0;
        return best->macro;
    }

    return macros[0];
}

MacroMCTSNode* MacroMCTSSearch::select(MacroMCTSNode* root) {
    MacroMCTSNode* node = root;
    while (node->expanded && !node->children.empty()) {
        MacroMCTSNode* child = node->bestChildPUCT(config_.explorationC);
        if (!child) break;
        node = child;
    }
    return node;
}

void MacroMCTSSearch::expand(MacroMCTSNode* node, const GameState& state) {
    if (state.phase == GamePhase::GAME_OVER ||
        state.phase == GamePhase::TOUCHDOWN ||
        state.phase == GamePhase::HALF_TIME) {
        node->expanded = true;
        return;
    }

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    int n = static_cast<int>(macros.size());

    // Compute priors: blend policy network with heuristic priors
    std::vector<float> priors(n, 1.0f / std::max(n, 1));

    // A) Compute policy network priors (if available)
    std::vector<float> policyPriors;
    if (config_.policy && n > 0 && config_.policyBlend > 0.0f) {
        policyPriors.resize(n);
        float stateFeats[NUM_FEATURES];
        extractFeatures(state, state.activeTeam, stateFeats);

        std::vector<float> macroFeats(n * NUM_ACTION_FEATURES);
        for (int i = 0; i < n; ++i) {
            extractMacroFeatures(state, macros[i], &macroFeats[i * NUM_ACTION_FEATURES]);
        }

        float maxLogit = -1e30f;
        for (int i = 0; i < n; ++i) {
            policyPriors[i] = config_.policy->evaluateAction(
                stateFeats, &macroFeats[i * NUM_ACTION_FEATURES]);
            if (policyPriors[i] > maxLogit) maxLogit = policyPriors[i];
        }
        float sumExp = 0.0f;
        for (int i = 0; i < n; ++i) {
            policyPriors[i] = std::exp(policyPriors[i] - maxLogit); // temp=1.0
            sumExp += policyPriors[i];
        }
        if (sumExp > 0.0f) {
            for (int i = 0; i < n; ++i) policyPriors[i] /= sumExp;
        }
    }

    if (config_.policy && n > 0) {
        // B) Compute heuristic priors (when policy is set, blend with heuristics)
        const auto& myTeam = state.getTeamState(state.activeTeam);
        const auto& oppTeam = state.getTeamState(opponent(state.activeTeam));
        int turnsRemaining = std::max(0, 9 - myTeam.turnNumber);
        int scoreDiff = myTeam.score - oppTeam.score;
        bool trailing2plus = (scoreDiff <= -2);
        bool leading = (scoreDiff >= 1);
        bool isFirstTurn = (myTeam.turnNumber == 1);
        bool needsRenorm = false;

        // Detect defensive situation (opponent has ball)
        bool activeHasBall = (state.ball.isHeld && state.ball.carrierId > 0 &&
                              state.getPlayer(state.ball.carrierId).teamSide == state.activeTeam);
        bool onDef = !activeHasBall && state.ball.isHeld;

        for (int i = 0; i < n; ++i) {
            float minPrior = 0.0f;
            float maxPrior = 1.0f;
            switch (macros[i].type) {
                case MacroType::SCORE:
                case MacroType::BLITZ_AND_SCORE:
                case MacroType::HAND_OFF_SCORE:
                case MacroType::PASS_SCORE:
                case MacroType::CHAIN_SCORE: {
                    if (turnsRemaining <= 1) {
                        // One-turn TD: last turn, force scoring attempt
                        if (macros[i].playerId > 0) {
                            const Player& p = state.getPlayer(macros[i].playerId);
                            int dist = distToEndzone(p.position, state.activeTeam);
                            bool safeWalkIn = (dist <= static_cast<int>(p.movementRemaining));
                            minPrior = safeWalkIn ? 0.90f : 0.70f;
                        } else {
                            minPrior = 0.70f;
                        }
                    } else if (trailing2plus) {
                        minPrior = 0.50f;
                    } else if (isFirstTurn && !trailing2plus) {
                        maxPrior = 0.05f;
                    } else if (leading && turnsRemaining > 2) {
                        maxPrior = 0.02f;
                    } else if (turnsRemaining <= 2) {
                        minPrior = 0.35f;
                    } else if (turnsRemaining <= 4) {
                        minPrior = 0.20f;
                    } else {
                        minPrior = 0.08f;
                    }
                    break;
                }
                case MacroType::ADVANCE:
                    if (trailing2plus) minPrior = 0.15f;
                    break;
                case MacroType::BLITZ:
                    if (onDef) minPrior = 0.20f;
                    break;
                case MacroType::BLOCK:
                    minPrior = 0.12f;
                    break;
                case MacroType::CAGE:
                    minPrior = 0.08f;
                    break;
                case MacroType::PICKUP:
                    minPrior = 0.20f;
                    if (scoreDiff < 0) minPrior = 0.30f;
                    if (turnsRemaining <= 3) minPrior = 0.35f;
                    break;
                case MacroType::REPOSITION:
                    if (onDef) minPrior = 0.05f;
                    break;
                case MacroType::END_TURN:
                    if (priors[i] > 0.10f && n > 2) {
                        priors[i] = 0.10f;
                        needsRenorm = true;
                    }
                    break;
                default:
                    break;
            }
            if (minPrior > 0.0f && priors[i] < minPrior) {
                priors[i] = minPrior;
                needsRenorm = true;
            }
            if (maxPrior < 1.0f && priors[i] > maxPrior) {
                priors[i] = maxPrior;
                needsRenorm = true;
            }
        }
        if (needsRenorm) {
            float sum = 0.0f;
            for (int i = 0; i < n; ++i) sum += priors[i];
            if (sum > 0.0f) {
                for (int i = 0; i < n; ++i) priors[i] /= sum;
            }
        }

        // C) Blend heuristic priors with policy priors
        //    prior = (1 - blend) * heuristic + blend * policy
        if (!policyPriors.empty() && config_.policyBlend > 0.0f) {
            float blend = config_.policyBlend;
            for (int i = 0; i < n; ++i) {
                priors[i] = (1.0f - blend) * priors[i] + blend * policyPriors[i];
            }
            // Renormalize after blending
            float sum = 0.0f;
            for (int i = 0; i < n; ++i) sum += priors[i];
            if (sum > 0.0f) {
                for (int i = 0; i < n; ++i) priors[i] /= sum;
            }
        }
    }

    node->children.reserve(n);
    for (int i = 0; i < n; ++i) {
        MacroMCTSNode child;
        child.macro = macros[i];
        child.parent = node;
        child.prior = priors[i];
        node->children.push_back(std::move(child));
    }
    node->expanded = true;
}

double MacroMCTSSearch::greedyLookaheadBonus(const GameState& leafState, TeamSide perspective) {
    // Only meaningful while it is still OUR turn: getAvailableMacros() and
    // greedyExpandMacro() both operate on state.activeTeam, so if the turn
    // has already passed to the opponent this would silently simulate THEIR
    // next macro instead of ours. Bail out rather than mislabel that signal.
    if (leafState.phase != GamePhase::PLAY) return 0.0;
    if (leafState.activeTeam != perspective) return 0.0;
    if (!leafState.ball.isHeld || leafState.ball.carrierId <= 0) return 0.0;

    const Player& carrier = leafState.getPlayer(leafState.ball.carrierId);
    if (carrier.teamSide != perspective) return 0.0;

    int distBefore = distToEndzone(carrier.position, perspective);
    if (distBefore <= 0) return 0.0;  // already in the endzone somehow

    std::vector<Macro> macros;
    getAvailableMacros(leafState, macros);
    if (macros.empty()) return 0.0;

    int bestIdx = -1, bestRank = -1;
    for (size_t i = 0; i < macros.size(); ++i) {
        int r = greedyMacroRank(macros[i].type);
        if (r > bestRank) {
            bestRank = r;
            bestIdx = static_cast<int>(i);
        }
    }
    if (bestIdx < 0 || macros[static_cast<size_t>(bestIdx)].type == MacroType::END_TURN) {
        return 0.0;  // nothing constructive left to try from here
    }

    // Apply exactly ONE more macro on a clone -- bounded cost, never mutates
    // the real leaf/search state. Real dice (via dice_) resolve the atomic
    // actions, same as every other macro application in this search.
    GameState projected = leafState.clone();
    auto result = greedyExpandMacro(projected, macros[static_cast<size_t>(bestIdx)], dice_);

    if (result.turnover) {
        // The greedy continuation actually loses the ball one ply out --
        // real information the static idealDist=turnsLeft*ma pacing formula
        // has no way to see. Small, bounded penalty.
        return -0.10;
    }
    if (projected.phase == GamePhase::TOUCHDOWN) {
        return 0.30;  // the forced greedy continuation actually scores
    }

    if (!projected.ball.isHeld || projected.ball.carrierId <= 0) return 0.0;
    const Player& carrierAfter = projected.getPlayer(projected.ball.carrierId);
    if (carrierAfter.teamSide != perspective) return 0.0;

    int distAfter = distToEndzone(carrierAfter.position, perspective);
    double progress = (distBefore - distAfter) / 25.0;  // normalized pitch-length progress
    return std::clamp(progress, -0.10, 0.20) * 0.5;      // bounded, modest weight
}

double MacroMCTSSearch::simulate(const GameState& state, TeamSide perspective) {
    // Heuristic baseline — always computed (provides signal even with zero VF)
    const TeamState& my = state.getTeamState(perspective);
    const TeamState& opp = state.getTeamState(opponent(perspective));
    double heuristic = 0.0;
    // fix #1 (2026-06-24): offensive forward/scoring pull is accumulated
    // separately and added AFTER the vf blend, so a calibrated value head —
    // which is flat/negative on the rare scoring-frontier states — cannot
    // dilute the only signal telling MCTS to carry the ball into the endzone
    // (root cause of the 0-0 draw collapse).
    double scoringBonus = 0.0;

    // Score advantage (dominant signal)
    heuristic += (my.score - opp.score) * 0.5;

    int turnsLeft = std::max(0, 9 - my.turnNumber);

    // Ball possession + endzone proximity + scoring urgency
    if (state.ball.isHeld && state.ball.carrierId > 0) {
        const Player& carrier = state.getPlayer(state.ball.carrierId);
        int ezX = (carrier.teamSide == TeamSide::HOME) ? 25 : 0;
        int dist = std::abs(carrier.position.x - ezX);
        int ma = carrier.stats.movement;
        double proximity = 1.0 - dist / 25.0; // 0..1, 1=at endzone

        if (carrier.teamSide == perspective) {
            heuristic += 0.1;  // have ball (possession value — may be blended)
            // fix #1: all offensive endzone/scoring pull below -> scoringBonus
            scoringBonus += 0.25 * proximity;  // closer to endzone = better

            // search-side #2 (2026-06-25): advance the whole CAGE, not just the carrier.
            // search-side #1 (stronger lone-carrier pull) was a no-op in smoke: the
            // search correctly refuses to push an UNESCORTED carrier forward (exposure ->
            // turnover risk pulls it back). The missing signal is the protective cage
            // moving up. Reward EARLY-TURN forward progress of standing teammates near
            // the carrier so "cage advanced, carrier screened" outranks "cage sat back";
            // the carrier then follows safely. Inside scoringBonus (post-vfBlend) -> no dilution.
            if (turnsLeft >= 3) {  // early/mid turns — late turns are already urgency-driven
                double cageProxSum = 0.0;
                int cageN = 0;
                state.forEachOnPitch(perspective, [&](const Player& p) {
                    if (p.state != PlayerState::STANDING) return;
                    if (p.id == carrier.id) return;
                    if (p.position.distanceTo(carrier.position) > 4) return;  // in/near the cage
                    int pd = std::abs(p.position.x - ezX);
                    cageProxSum += 1.0 - pd / 25.0;   // this escort's forward progress
                    cageN++;
                });
                if (cageN > 0) scoringBonus += 0.20 * (cageProxSum / cageN);
            }

            // Can score without GFI (safe walk-in)
            if (dist <= static_cast<int>(carrier.movementRemaining)) {
                scoringBonus += 0.4;  // strong bonus — safe TD
            }
            // Can score with GFI (risky but possible)
            else if (dist <= carrier.movementRemaining + 2) {
                scoringBonus += 0.2;
            }

            // Stall pacing: reward being on-track to score on last turn
            // Ideal: dist == turnsLeft * MA (arrive at endzone on final turn)
            if (turnsLeft > 0 && dist > 0) {
                int idealDist = turnsLeft * ma;
                double pacing = 1.0 - std::abs(dist - idealDist) / (double)std::max(idealDist, 1);
                if (pacing > 0) scoringBonus += 0.1 * pacing;
            }

            // Urgency: last 2 turns and near endzone — must score!
            if (turnsLeft <= 2 && dist <= ma + 2) {
                scoringBonus += 0.3;
            }

            // One-turn TD: last turn, carrier can score NOW — massive bonus
            if (turnsLeft <= 1) {
                if (dist <= static_cast<int>(carrier.movementRemaining)) {
                    scoringBonus += 0.8;  // safe walk-in on last turn
                } else if (dist <= carrier.movementRemaining + 2) {
                    scoringBonus += 0.5;  // GFI needed but scoreable
                }
            }

            // Hand-off scoring potential: carrier can't reach EZ but adjacent teammate can
            if (dist > static_cast<int>(carrier.movementRemaining) + 2) {
                auto adj = carrier.position.getAdjacent();
                for (auto& apos : adj) {
                    if (!apos.isOnPitch()) continue;
                    const Player* tm = state.getPlayerAtPosition(apos);
                    if (!tm || tm->teamSide != perspective) continue;
                    if (tm->state != PlayerState::STANDING) continue;
                    int tmDist = distToEndzone(tm->position, perspective);
                    if (tmDist > 0 && tmDist <= static_cast<int>(tm->movementRemaining) + 2) {
                        scoringBonus += 0.15;
                        break;
                    }
                }
            }

            // Bounded greedy 1-ply forward look (2026-07-02 experiment, off by
            // default via config_.leafLookahead): see greedyLookaheadBonus().
            if (config_.leafLookahead) {
                scoringBonus += greedyLookaheadBonus(state, perspective);
            }

        } else {
            heuristic -= 0.1;
            heuristic -= 0.25 * proximity;
            // Opponent can score — bad
            if (dist <= static_cast<int>(carrier.movementRemaining)) {
                heuristic -= 0.4;
            }
        }
    } else if (!state.ball.isHeld && state.ball.isOnPitch()) {
        heuristic -= 0.1;  // loose ball is bad

        // Bonus for having a player near the loose ball (quick pickup potential)
        int nearestDist = 999;
        state.forEachOnPitch(perspective, [&](const Player& p) {
            if (p.state != PlayerState::STANDING) return;
            int d = p.position.distanceTo(state.ball.position);
            if (d < nearestDist) nearestDist = d;
        });
        if (nearestDist <= 2) heuristic += 0.08;
        else if (nearestDist <= 4) heuristic += 0.04;
    }

    // Defense: bonus for marking opponent carrier with tackle zones
    if (state.ball.isHeld && state.ball.carrierId > 0) {
        const Player& ballHolder = state.getPlayer(state.ball.carrierId);
        if (ballHolder.teamSide != perspective && ballHolder.isOnPitch() &&
            ballHolder.state == PlayerState::STANDING) {
            int carrierTZ = countTacklezones(state, ballHolder.position, ballHolder.teamSide);
            if (carrierTZ > 0) {
                heuristic += 0.08 * std::min(carrierTZ, 3); // max +0.24
            }
        }
    }

    // Dodge-back vs bash: penalty when our players are adjacent to strong (ST≥4) opponents
    {
        int bashExposure = 0;
        state.forEachOnPitch(perspective, [&](const Player& p) {
            if (p.state != PlayerState::STANDING) return;
            auto adj = p.position.getAdjacent();
            for (auto& apos : adj) {
                if (!apos.isOnPitch()) continue;
                const Player* opp = state.getPlayerAtPosition(apos);
                if (opp && opp->teamSide != perspective &&
                    opp->state == PlayerState::STANDING &&
                    opp->stats.strength >= 4) {
                    bashExposure++;
                    break; // count once per our player
                }
            }
        });
        heuristic -= 0.05 * bashExposure;
    }

    // Sideline trap: bonus when opponent carrier is near sideline (limited escape routes)
    if (state.ball.isHeld && state.ball.carrierId > 0) {
        const Player& bh = state.getPlayer(state.ball.carrierId);
        if (bh.teamSide != perspective && bh.isOnPitch() &&
            bh.state == PlayerState::STANDING) {
            int y = bh.position.y;
            if (y <= 2 || y >= 12) heuristic += 0.10;
            else if (y <= 4 || y >= 10) heuristic += 0.05;
        }
    }

    // Contain vs agility: bonus when we TZ agile (AG≥4) opponent carrier from multiple sides
    if (state.ball.isHeld && state.ball.carrierId > 0) {
        const Player& bh = state.getPlayer(state.ball.carrierId);
        if (bh.teamSide != perspective && bh.isOnPitch() &&
            bh.state == PlayerState::STANDING && bh.stats.agility >= 4) {
            int tzCount = countTacklezones(state, bh.position, bh.teamSide);
            if (tzCount >= 2) heuristic += 0.06 * std::min(tzCount - 1, 2); // max +0.12
        }
    }

    // Player count advantage (H8.8-H8.9): more players = better
    int myPlayers = 0, oppPlayers = 0;
    state.forEachOnPitch(perspective, [&](const Player& p) {
        if (p.state == PlayerState::STANDING) myPlayers++;
    });
    state.forEachOnPitch(opponent(perspective), [&](const Player& p) {
        if (p.state == PlayerState::STANDING) oppPlayers++;
    });
    int playerDiff = myPlayers - oppPlayers;
    heuristic += playerDiff * 0.03;  // each player advantage = small bonus

    heuristic = std::clamp(heuristic, -1.0, 1.0);

    // Blend with value function if available. fix #1: the offensive scoring
    // pull (scoringBonus) is added AFTER the blend so vf_blend never dilutes
    // it — the VF is flat/negative on scoring-frontier states and would
    // otherwise steer the search to the safe 0-0 line.
    double leaf;
    if (valueFn_ && config_.vfBlend > 0.0f) {
        float features[NUM_FEATURES];
        extractFeatures(state, perspective, features);
        double vfRaw = static_cast<double>(valueFn_->evaluate(features, NUM_FEATURES));
        double vfValue = std::clamp(vfRaw, -1.0, 1.0);

        double blend = static_cast<double>(config_.vfBlend);
        leaf = (1.0 - blend) * heuristic + blend * vfValue;
    } else {
        leaf = heuristic;
    }

    return std::clamp(leaf + scoringBonus, -1.0, 1.0);
}

void MacroMCTSSearch::backpropagate(MacroMCTSNode* node, double value) {
    while (node) {
        node->visits++;
        node->totalValue += value;
        node = node->parent;
    }
}

bool MacroMCTSSearch::replayToNode(GameState& state, MacroMCTSNode* node) {
    // Build path from root to node
    std::vector<MacroMCTSNode*> path;
    MacroMCTSNode* cur = node;
    while (cur->parent) {
        path.push_back(cur);
        cur = cur->parent;
    }

    // Replay in root-to-leaf order (open-loop: fresh dice each time)
    for (int i = static_cast<int>(path.size()) - 1; i >= 0; --i) {
        if (state.phase == GamePhase::GAME_OVER ||
            state.phase == GamePhase::TOUCHDOWN ||
            state.phase == GamePhase::HALF_TIME) {
            return false;
        }
        auto result = greedyExpandMacro(state, path[i]->macro, dice_);
        if (result.turnover) {
            return false;
        }
    }

    return true;
}

// --- MacroMCTSPolicy ---

MacroMCTSPolicy::MacroMCTSPolicy(const ValueFunction* vf, MCTSConfig config, uint32_t seed)
    : search_(vf, config, seed), expansionDice_(seed + 12345) {}

void MacroMCTSPolicy::setLogDecisions(bool log, int topK) {
    logDecisions_ = log;
    topK_ = topK;
}

Action MacroMCTSPolicy::operator()(const GameState& state) {
    // Check if current plan still valid
    if (planIndex_ < static_cast<int>(currentPlan_.size())) {
        const Action& planned = currentPlan_[planIndex_];

        // Validate: is this action still available?
        std::vector<Action> available;
        getAvailableActions(state, available);
        for (auto& a : available) {
            if (a.type == planned.type && a.playerId == planned.playerId &&
                a.targetId == planned.targetId && a.target == planned.target) {
                planIndex_++;
                return planned;
            }
        }
        // Plan invalidated — search again
        currentPlan_.clear();
        planIndex_ = 0;
    }

    // Search for best macro
    Macro bestMacro = search_.search(state);

    // Log decision if enabled
    if (logDecisions_) {
        const auto& childVisits = search_.lastChildVisits();
        if (!childVisits.empty()) {
            PolicyDecision decision;
            extractFeatures(state, state.activeTeam, decision.stateFeatures);
            decision.perspective = state.activeTeam;

            int totalVisits = 0;
            for (auto& cv : childVisits) totalVisits += cv.visits;

            if (totalVisits > 0) {
                std::vector<MacroChildVisitInfo> sorted = childVisits;
                std::sort(sorted.begin(), sorted.end(),
                          [](const MacroChildVisitInfo& a, const MacroChildVisitInfo& b) {
                              return a.visits > b.visits;
                          });

                int k = std::min(topK_, static_cast<int>(sorted.size()));
                for (int i = 0; i < k; ++i) {
                    PolicyDecision::ActionVisit av;
                    extractMacroFeatures(state, sorted[i].macro, av.actionFeatures);
                    av.visitFraction = static_cast<float>(sorted[i].visits) / totalVisits;
                    decision.visits.push_back(av);
                }
                decisions_.push_back(std::move(decision));
            }
        }
    }

    // Expand the chosen macro into a plan
    GameState planState = state.clone();
    auto expansion = greedyExpandMacro(planState, bestMacro, expansionDice_);

    currentPlan_ = std::move(expansion.actions);
    planIndex_ = 0;

    if (currentPlan_.empty()) {
        // No actions from expansion — fall back to END_TURN
        return Action{ActionType::END_TURN, -1, -1, {-1, -1}};
    }

    // Validate first action
    std::vector<Action> available;
    getAvailableActions(state, available);
    const Action& first = currentPlan_[0];
    for (auto& a : available) {
        if (a.type == first.type && a.playerId == first.playerId &&
            a.targetId == first.targetId && a.target == first.target) {
            planIndex_ = 1;
            return first;
        }
    }

    // First action invalid — just pick something safe
    currentPlan_.clear();
    planIndex_ = 0;

    // Fallback: greedy policy
    return greedyPolicy(state, expansionDice_);
}

} // namespace bb
