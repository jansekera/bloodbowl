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
    double explorationC = 1.41;  // UCT constant
    int rolloutDepth = 0;        // 0 = pure value function eval
    bool verbose = false;
    const PolicyNetwork* policy = nullptr;  // If set, use PUCT instead of UCT
    int maxChildren = 0;   // Progressive widening: max children per node (0 = unlimited)
    float dirichletAlpha = 0.0f;   // Dirichlet noise alpha (0 = disabled, 0.3 for training)
    float dirichletWeight = 0.25f; // prior = (1-w)*policy + w*Dir(alpha)
    float policyBlend = 0.0f;     // Blend policy with heuristics: 0.0 = heuristics only, 1.0 = policy only
    float vfBlend = 0.0f;         // Blend VF with heuristic eval: 0.0 = heuristic only, 1.0 = VF only
    int nRollouts = 1;            // Rollouts averaged per leaf eval (open-loop): >1 cuts macro Q-variance ~sqrt(K)
    bool leafLookahead = false;   // Macro-MCTS only: bounded greedy 1-ply forward look at leaf eval (2026-07-02 experiment)
    bool riskDeferral = false;    // Macro-MCTS only: Q-guarded risk-sequencing defer at search() return (queue item 10, 2026-07-28)
    bool stagedPickupPlanner = false; // Macro-MCTS only: item13 MVP staged safe-then-PICKUP whole-turn planner (2026-07-31), see bb/turn_planner.h
    bool cageAdvance = false;         // Macro-MCTS only: F1 cage-advance whole-turn plan (2026-08-03), see bb/cage_advance.h
    bool cageGrind = false;           // DIAG/EXPERIMENT only (2026-08-06 tempo doctrine A/B): cage advance option (a) "grind" -- on TEMPO_INSUFFICIENT push at max dice-free achievable step instead of falling back to search(). DEFAULT OFF = no behavior change; only diag harnesses set it.
    // Offer a block whose strength Dauntless would equalise (2026-08-14).
    // getBlockDiceCount weighs Horns but never Dauntless, so a ST3 Slayer beside
    // a ST4 Black Orc prices as uphill, the count goes negative and the offer is
    // discarded -- for a block that resolves at equal strength on a 2+. Orcs are
    // the only side we face fielding four ST4 bodies, and they are also the side
    // we score 86 touchdowns against in 750 games versus 451 against Skaven.
    // 2026-08-17: ON in production. The A/B of 14.-15.08. ran 3000 pairs on
    // dwarf-orc and passed: +4.08 pp paired delta, and +3.59 pp (~3.4 sigma)
    // once read against the two matchups where the arm provably never fired --
    // which is the only honest way to read it, because those nulls moved too.
    // The Q1 sweep gives the mechanism rather than just the outcome: across 36
    // geometries the Black Orc is chosen 76.8% -> 83.3% of the time.
    // See evidence/weekend_result_20260817.md.
    bool dauntlessInOffer = true;
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
