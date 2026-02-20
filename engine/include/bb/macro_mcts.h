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
    bool replayToNode(GameState& state, MacroMCTSNode* node);
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
