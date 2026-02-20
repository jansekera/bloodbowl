#pragma once

#include "bb/game_state.h"
#include "bb/rules_engine.h"
#include "bb/dice.h"
#include "bb/mcts.h"
#include "bb/value_function.h"
#include "bb/feature_extractor.h"
#include "bb/action_features.h"

namespace bb {

// Random action selection
Action randomPolicy(const GameState& state, DiceRollerBase& dice);

// Greedy: prefer scoring moves, then blocks, then other
Action greedyPolicy(const GameState& state, DiceRollerBase& dice);

// Learning: epsilon-greedy using value function for state evaluation
Action learningPolicy(const GameState& state, DiceRollerBase& dice,
                      const ValueFunction& vf, float epsilon);

// Policy decision logged by MCTS for training the policy network
struct PolicyDecision {
    float stateFeatures[NUM_FEATURES];
    TeamSide perspective;
    struct ActionVisit {
        float actionFeatures[NUM_ACTION_FEATURES];
        float visitFraction;
    };
    std::vector<ActionVisit> visits;  // top-K visited actions
};

// MCTS-powered selection with optional decision logging
class MCTSPolicy {
    MCTSSearch search_;
    std::vector<PolicyDecision> decisions_;
    int topK_ = 20;
    bool logDecisions_ = false;

public:
    MCTSPolicy(const ValueFunction* vf, MCTSConfig config, uint32_t seed = 0);
    Action operator()(const GameState& state);

    int lastIterations() const { return search_.lastIterations(); }
    double lastBestValue() const { return search_.lastBestValue(); }

    void setLogDecisions(bool log, int topK = 20);
    const std::vector<PolicyDecision>& decisions() const { return decisions_; }
    void clearDecisions() { decisions_.clear(); }
};

} // namespace bb
