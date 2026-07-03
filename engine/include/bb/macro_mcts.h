#pragma once

#include "bb/game_state.h"
#include "bb/macro_actions.h"
#include "bb/mcts.h"
#include "bb/value_function.h"
#include "bb/feature_extractor.h"
#include "bb/policy_network.h"
#include "bb/policies.h"
#include "bb/dice.h"
#include <vector>
#include <cstdint>

namespace bb {

struct MacroMCTSNode {
    Macro macro;
    MacroMCTSNode* parent = nullptr;
    std::vector<MacroMCTSNode> children;
    int visits = 0;
    double totalValue = 0.0;
    bool expanded = false;
    float prior = 1.0f;

    MacroMCTSNode* bestChildPUCT(double C) const;
    MacroMCTSNode* mostVisitedChild() const;
};

struct MacroChildVisitInfo {
    Macro macro;
    int visits;
};

// Outcome of an open-loop replay toward a target node: `reached` is the
// deepest node whose macro was actually attempted (may be an ancestor of
// the target if a turnover or terminal phase cut the replay short — fresh
// dice each replay means the same node can play out differently than when
// the tree was first built); `complete` is true only if the replay reached
// the target node itself without incident.
struct ReplayOutcome {
    MacroMCTSNode* reached;
    bool complete;
};

class MacroMCTSSearch {
    const ValueFunction* valueFn_;
    MCTSConfig config_;
    DiceRoller dice_;

    int lastIterations_ = 0;
    double lastBestValue_ = 0.0;
    std::vector<MacroChildVisitInfo> lastChildVisits_;

public:
    MacroMCTSSearch(const ValueFunction* vf, MCTSConfig config, uint32_t seed = 0);

    Macro search(const GameState& state);

    int lastIterations() const { return lastIterations_; }
    double lastBestValue() const { return lastBestValue_; }
    const std::vector<MacroChildVisitInfo>& lastChildVisits() const { return lastChildVisits_; }

private:
    MacroMCTSNode* select(MacroMCTSNode* root);
    void expand(MacroMCTSNode* node, const GameState& state);
    double simulate(const GameState& state, TeamSide perspective);
    void backpropagate(MacroMCTSNode* node, double value);
    ReplayOutcome replayToNode(GameState& state, MacroMCTSNode* node);
    // Bounded greedy one-ply forward look from a leaf state (see macro_mcts.cpp).
    double greedyLookaheadBonus(const GameState& leafState, TeamSide perspective);
};

// Stateful policy: searches over macros, expands best into action plan,
// returns actions one at a time
class MacroMCTSPolicy {
    MacroMCTSSearch search_;
    DiceRoller expansionDice_;
    std::vector<Action> currentPlan_;
    int planIndex_ = 0;

    // Decision logging (reuses PolicyDecision struct)
    std::vector<PolicyDecision> decisions_;
    bool logDecisions_ = false;
    int topK_ = 20;

public:
    MacroMCTSPolicy(const ValueFunction* vf, MCTSConfig config, uint32_t seed = 0);

    Action operator()(const GameState& state);

    void setLogDecisions(bool log, int topK = 20);
    const std::vector<PolicyDecision>& decisions() const { return decisions_; }
    void clearDecisions() { decisions_.clear(); }

    int lastIterations() const { return search_.lastIterations(); }
    double lastBestValue() const { return search_.lastBestValue(); }
};

} // namespace bb
