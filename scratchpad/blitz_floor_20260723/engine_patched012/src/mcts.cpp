#include "bb/mcts.h"
#include "bb/action_resolver.h"
#include "bb/action_features.h"
#include "bb/helpers.h"
#include <chrono>
#include <cmath>
#include <algorithm>
#include <limits>

namespace bb {

// --- MCTSNode ---

double MCTSNode::ucb(double parentLogN, double C) const {
    if (visits == 0) return std::numeric_limits<double>::max();
    double exploitation = totalValue / visits;
    double exploration = C * std::sqrt(parentLogN / visits);
    return exploitation + exploration;
}

double MCTSNode::puct(double parentVisits, double C) const {
    // AlphaZero PUCT: Q(s,a) + C * P(s,a) * sqrt(N_parent) / (1 + N(s,a))
    double q = (visits == 0) ? 0.0 : totalValue / visits;
    double u = C * prior * std::sqrt(parentVisits) / (1.0 + visits);
    return q + u;
}

MCTSNode* MCTSNode::bestChild(double C) const {
    if (children.empty()) return nullptr;
    double parentLogN = std::log(static_cast<double>(visits));

    MCTSNode* best = nullptr;
    double bestUCB = -std::numeric_limits<double>::max();
    for (auto& child : const_cast<std::vector<MCTSNode>&>(children)) {
        double u = child.ucb(parentLogN, C);
        if (u > bestUCB) {
            bestUCB = u;
            best = &child;
        }
    }
    return best;
}

MCTSNode* MCTSNode::bestChildPUCT(double C) const {
    if (children.empty()) return nullptr;
    double parentVisits = static_cast<double>(visits);

    // Compute FPU (First Play Urgency): average Q of visited children
    // Unvisited children use this instead of Q=0
    double visitedSum = 0.0;
    int visitedCount = 0;
    for (auto& child : children) {
        if (child.visits > 0) {
            visitedSum += child.totalValue / child.visits;
            visitedCount++;
        }
    }
    double fpu = (visitedCount > 0) ? visitedSum / visitedCount : 0.0;

    MCTSNode* best = nullptr;
    double bestScore = -std::numeric_limits<double>::max();
    for (auto& child : const_cast<std::vector<MCTSNode>&>(children)) {
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

MCTSNode* MCTSNode::mostVisitedChild() const {
    if (children.empty()) return nullptr;
    MCTSNode* best = nullptr;
    int bestVisits = -1;
    for (auto& child : const_cast<std::vector<MCTSNode>&>(children)) {
        if (child.visits > bestVisits) {
            bestVisits = child.visits;
            best = &child;
        }
    }
    return best;
}

// --- MCTSSearch ---

MCTSSearch::MCTSSearch(const ValueFunction* vf, MCTSConfig config, uint32_t seed)
    : valueFn_(vf), config_(config), dice_(seed) {}

Action MCTSSearch::search(const GameState& state) {
    // Get available actions
    std::vector<Action> actions;
    getAvailableActions(state, actions);

    if (actions.empty()) {
        return Action{ActionType::END_TURN, -1, -1, {-1, -1}};
    }
    if (actions.size() == 1) {
        lastIterations_ = 0;
        lastBestValue_ = 0.0;
        lastChildVisits_.clear();
        return actions[0];
    }

    // Create root
    MCTSNode root;
    root.visits = 1;  // Virtual visit so PUCT exploration term is non-zero
    root.expanded = false;

    // Expand root immediately
    expand(&root, state);

    TeamSide searchingSide = state.activeTeam;

    auto startTime = std::chrono::steady_clock::now();
    int iterations = 0;

    while (iterations < config_.maxIterations) {
        // Check time every 64 iterations
        if ((iterations & 63) == 0 && iterations > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - startTime).count();
            if (elapsed >= config_.timeBudgetMs) break;
        }

        // 1. Select
        MCTSNode* node = select(&root);

        // 2. Expand (if not terminal)
        GameState sim = state.clone();
        if (!replayToNode(sim, node)) {
            iterations++;
            continue;
        }

        if (!node->expanded && node->visits > 0) {
            expand(node, sim);
            if (!node->children.empty()) {
                // Pick first unvisited child
                node = &node->children[0];
                executeAction(sim, node->action, dice_, nullptr);
            }
        }

        // 3. Simulate (evaluate)
        double value = simulate(sim, searchingSide);

        // 4. Backpropagate
        backpropagate(node, value, searchingSide, state);

        iterations++;
    }


    lastIterations_ = iterations;

    // Save child visit info before tree is destroyed
    lastChildVisits_.clear();
    for (auto& child : root.children) {
        if (child.visits > 0) {
            lastChildVisits_.push_back({child.action, child.visits});
        }
    }

    // Return most-visited child's action
    MCTSNode* best = root.mostVisitedChild();
    if (best) {
        lastBestValue_ = best->visits > 0
            ? best->totalValue / best->visits : 0.0;
        return best->action;
    }

    return actions[0];
}

MCTSNode* MCTSSearch::select(MCTSNode* root) {
    MCTSNode* node = root;
    while (node->expanded && !node->children.empty()) {
        MCTSNode* child = config_.policy
            ? node->bestChildPUCT(config_.explorationC)
            : node->bestChild(config_.explorationC);
        if (!child) break;
        node = child;
    }
    return node;
}

void MCTSSearch::expand(MCTSNode* node, const GameState& state) {
    if (state.phase == GamePhase::GAME_OVER ||
        state.phase == GamePhase::TOUCHDOWN ||
        state.phase == GamePhase::HALF_TIME) {
        node->expanded = true;
        return;
    }

    std::vector<Action> actions;
    getAvailableActions(state, actions);

    int n = static_cast<int>(actions.size());

    // Compute priors from policy network first (needed for progressive widening)
    std::vector<float> priors(n, 1.0f / std::max(n, 1));  // default uniform
    if (config_.policy && n > 0) {
        float stateFeats[NUM_FEATURES];
        extractFeatures(state, state.activeTeam, stateFeats);

        std::vector<float> actionFeats(n * NUM_ACTION_FEATURES);
        for (int i = 0; i < n; ++i) {
            extractActionFeatures(state, actions[i], &actionFeats[i * NUM_ACTION_FEATURES]);
        }
        config_.policy->computePriors(stateFeats, actionFeats.data(), n, priors.data());
    }

    // Progressive widening: sort by prior desc, keep top maxChildren
    int keep = n;
    std::vector<int> indices(n);
    for (int i = 0; i < n; ++i) indices[i] = i;

    if (config_.maxChildren > 0 && n > config_.maxChildren && config_.policy) {
        // Sort indices by prior descending
        std::sort(indices.begin(), indices.end(), [&priors](int a, int b) {
            return priors[a] > priors[b];
        });
        keep = config_.maxChildren;

        // Renormalize priors for kept children
        float priorSum = 0.0f;
        for (int i = 0; i < keep; ++i) priorSum += priors[indices[i]];
        if (priorSum > 0.0f) {
            for (int i = 0; i < keep; ++i) priors[indices[i]] /= priorSum;
        }
    }

    node->children.reserve(keep);
    for (int i = 0; i < keep; ++i) {
        int idx = indices[i];
        MCTSNode child;
        child.action = actions[idx];
        child.parent = node;
        child.prior = priors[idx];
        node->children.push_back(std::move(child));
    }
    node->expanded = true;
}

double MCTSSearch::simulate(const GameState& state, TeamSide perspective) {
    if (config_.rolloutDepth > 0) {
        return rollout(state.clone(), perspective, config_.rolloutDepth);
    }

    // Pure value function evaluation
    if (valueFn_) {
        float features[NUM_FEATURES];
        extractFeatures(state, perspective, features);
        double raw = static_cast<double>(valueFn_->evaluate(features, NUM_FEATURES));
        // Normalize to [-1, 1] so Q values don't dominate PUCT exploration term
        return std::tanh(raw);
    }

    // No value function: simple heuristic based on score and ball possession
    const TeamState& my = state.getTeamState(perspective);
    const TeamState& opp = state.getTeamState(opponent(perspective));
    double value = 0.0;
    value += (my.score - opp.score) * 0.5;
    if (state.ball.isHeld && state.ball.carrierId > 0) {
        const Player& carrier = state.getPlayer(state.ball.carrierId);
        if (carrier.teamSide == perspective) value += 0.1;
        else value -= 0.1;
    }
    return std::clamp(value, -1.0, 1.0);
}

void MCTSSearch::backpropagate(MCTSNode* node, double value,
                                TeamSide searchingSide,
                                const GameState& rootState) {
    // Walk up to root, updating each node
    // Value is always from searchingSide's perspective
    while (node) {
        node->visits++;
        node->totalValue += value;
        node = node->parent;
    }
}

double MCTSSearch::rollout(GameState state, TeamSide perspective, int depth) {
    for (int i = 0; i < depth; ++i) {
        if (state.phase != GamePhase::PLAY) break;

        std::vector<Action> actions;
        getAvailableActions(state, actions);
        if (actions.empty()) break;

        // Pick random action
        int idx = (dice_.rollD6() - 1 + dice_.rollD6() - 1) % static_cast<int>(actions.size());
        if (idx < 0) idx = 0;
        executeAction(state, actions[idx], dice_, nullptr);
    }

    // Evaluate final state
    if (valueFn_) {
        float features[NUM_FEATURES];
        extractFeatures(state, perspective, features);
        double raw = static_cast<double>(valueFn_->evaluate(features, NUM_FEATURES));
        return std::tanh(raw);
    }

    // Fallback heuristic
    const TeamState& my = state.getTeamState(perspective);
    const TeamState& opp = state.getTeamState(opponent(perspective));
    return std::clamp((my.score - opp.score) * 0.5, -1.0, 1.0);
}

bool MCTSSearch::replayToNode(GameState& state, MCTSNode* node) {
    // Build path from root to node
    std::vector<MCTSNode*> path;
    MCTSNode* cur = node;
    while (cur->parent) {
        path.push_back(cur);
        cur = cur->parent;
    }

    // Replay in reverse (root-to-leaf order)
    for (int i = static_cast<int>(path.size()) - 1; i >= 0; --i) {
        // Skip if game is over
        if (state.phase == GamePhase::GAME_OVER ||
            state.phase == GamePhase::TOUCHDOWN ||
            state.phase == GamePhase::HALF_TIME) {
            return false;
        }
        executeAction(state, path[i]->action, dice_, nullptr);
    }

    return true;
}

} // namespace bb
