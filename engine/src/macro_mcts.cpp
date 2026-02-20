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

        // 2. Replay state to this node
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

        // 4. Evaluate leaf
        double value = simulate(sim, searchingSide);

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

    // Compute priors from policy network
    std::vector<float> priors(n, 1.0f / std::max(n, 1));
    if (config_.policy && n > 0) {
        float stateFeats[NUM_FEATURES];
        extractFeatures(state, state.activeTeam, stateFeats);

        std::vector<float> macroFeats(n * NUM_ACTION_FEATURES);
        for (int i = 0; i < n; ++i) {
            extractMacroFeatures(state, macros[i], &macroFeats[i * NUM_ACTION_FEATURES]);
        }

        // A) Compute raw logits and apply softmax with temperature=1.0
        //    (override saved temperature which may be too low)
        float maxLogit = -1e30f;
        for (int i = 0; i < n; ++i) {
            priors[i] = config_.policy->evaluateAction(
                stateFeats, &macroFeats[i * NUM_ACTION_FEATURES]);
            if (priors[i] > maxLogit) maxLogit = priors[i];
        }
        float sumExp = 0.0f;
        for (int i = 0; i < n; ++i) {
            priors[i] = std::exp(priors[i] - maxLogit); // temp=1.0
            sumExp += priors[i];
        }
        if (sumExp > 0.0f) {
            for (int i = 0; i < n; ++i) priors[i] /= sumExp;
        }

        // B) Time-aware SCORE/ADVANCE priors (stall strategy)
        //    Last 1-2 turns: strongly prefer SCORE (40%)
        //    Early turns: prefer ADVANCE over SCORE to approach gradually
        const auto& myTeam = state.getTeamState(state.activeTeam);
        const auto& oppTeam = state.getTeamState(opponent(state.activeTeam));
        int turnsRemaining = std::max(0, 9 - myTeam.turnNumber);
        int scoreDiff = myTeam.score - oppTeam.score;
        bool trailing2plus = (scoreDiff <= -2);
        bool needsRenorm = false;

        for (int i = 0; i < n; ++i) {
            float minPrior = 0.0f;
            switch (macros[i].type) {
                case MacroType::SCORE:
                case MacroType::BLITZ_AND_SCORE: {
                    if (turnsRemaining <= 1) {
                        // Last turn: score is nearly mandatory
                        if (macros[i].playerId > 0) {
                            const Player& p = state.getPlayer(macros[i].playerId);
                            int dist = distToEndzone(p.position, state.activeTeam);
                            bool safeWalkIn = (dist <= static_cast<int>(p.movementRemaining));
                            minPrior = safeWalkIn ? 0.60f : 0.40f;
                        } else {
                            minPrior = 0.40f;
                        }
                    } else if (trailing2plus) {
                        // Losing badly: score ASAP, no stalling (H2.10)
                        minPrior = 0.50f;
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
                    // When trailing 2+, advance aggressively (H2.10)
                    if (trailing2plus) minPrior = 0.15f;
                    break;
                case MacroType::BLOCK:
                    minPrior = 0.12f;  // Safe 2D blocks = always worth doing
                    break;
                case MacroType::CAGE:
                    // Protect carrier (H3.1) — cage is important when we have ball
                    minPrior = 0.08f;
                    break;
                case MacroType::END_TURN:
                    // Penalize END_TURN: cap at 10% (don't waste a turn)
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
        }
        if (needsRenorm) {
            float sum = 0.0f;
            for (int i = 0; i < n; ++i) sum += priors[i];
            for (int i = 0; i < n; ++i) priors[i] /= sum;
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

double MacroMCTSSearch::simulate(const GameState& state, TeamSide perspective) {
    // Heuristic baseline — always computed (provides signal even with zero VF)
    const TeamState& my = state.getTeamState(perspective);
    const TeamState& opp = state.getTeamState(opponent(perspective));
    double heuristic = 0.0;

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
            heuristic += 0.1;  // have ball
            heuristic += 0.25 * proximity;  // closer to endzone = better

            // Can score without GFI (safe walk-in)
            if (dist <= static_cast<int>(carrier.movementRemaining)) {
                heuristic += 0.4;  // strong bonus — safe TD
            }
            // Can score with GFI (risky but possible)
            else if (dist <= carrier.movementRemaining + 2) {
                heuristic += 0.2;
            }

            // Stall pacing: reward being on-track to score on last turn
            // Ideal: dist == turnsLeft * MA (arrive at endzone on final turn)
            if (turnsLeft > 0 && dist > 0) {
                int idealDist = turnsLeft * ma;
                double pacing = 1.0 - std::abs(dist - idealDist) / (double)std::max(idealDist, 1);
                if (pacing > 0) heuristic += 0.1 * pacing;
            }

            // Urgency: last 2 turns and near endzone — must score!
            if (turnsLeft <= 2 && dist <= ma + 2) {
                heuristic += 0.3;
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

    return heuristic;
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
