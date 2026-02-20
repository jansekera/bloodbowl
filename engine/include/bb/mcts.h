#pragma once

#include "bb/game_state.h"
#include "bb/rules_engine.h"
#include "bb/value_function.h"
#include "bb/feature_extractor.h"
#include "bb/policy_network.h"
#include "bb/dice.h"
#include <vector>
#include <cstdint>

namespace bb {

struct MCTSConfig {
    int timeBudgetMs = 1000;
    int maxIterations = 100000;
    double explorationC = 1.41;  // UCT constant (sqrt(2))
    int rolloutDepth = 0;        // 0 = pure value function eval
    bool verbose = false;
    const PolicyNetwork* policy = nullptr;  // If set, use PUCT instead of UCT
    int maxChildren = 0;   // Progressive widening: max children per node (0 = unlimited)
    float dirichletAlpha = 0.0f;   // Dirichlet noise alpha (0 = disabled, 0.3 for training)
    float dirichletWeight = 0.25f; // prior = (1-w)*policy + w*Dir(alpha)
};

struct MCTSNode {
    Action action;
    MCTSNode* parent = nullptr;
    std::vector<MCTSNode> children;
    int visits = 0;
    double totalValue = 0.0;
    bool expanded = false;
    float prior = 1.0f;  // Prior probability from policy network (default uniform)

    double ucb(double parentLogN, double C) const;
    double puct(double parentVisits, double C) const;
    MCTSNode* bestChild(double C) const;
    MCTSNode* bestChildPUCT(double C) const;
    MCTSNode* mostVisitedChild() const;
};

struct ChildVisitInfo {
    Action action;
    int visits;
};

class MCTSSearch {
    const ValueFunction* valueFn_;
    MCTSConfig config_;
    DiceRoller dice_;

    int lastIterations_ = 0;
    double lastBestValue_ = 0.0;
    std::vector<ChildVisitInfo> lastChildVisits_;

public:
    MCTSSearch(const ValueFunction* vf, MCTSConfig config, uint32_t seed = 0);

    Action search(const GameState& state);

    int lastIterations() const { return lastIterations_; }
    double lastBestValue() const { return lastBestValue_; }
    const std::vector<ChildVisitInfo>& lastChildVisits() const { return lastChildVisits_; }

private:
    MCTSNode* select(MCTSNode* root);
    void expand(MCTSNode* node, const GameState& state);
    double simulate(const GameState& state, TeamSide perspective);
    void backpropagate(MCTSNode* node, double value, TeamSide searchingSide,
                       const GameState& rootState);
    double rollout(GameState state, TeamSide perspective, int depth);

    // Replay actions from root to node on a cloned state
    bool replayToNode(GameState& state, MCTSNode* node);
};

} // namespace bb
