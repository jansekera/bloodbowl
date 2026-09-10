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
#include <memory>
#include <cstdint>

namespace bb {

// P10a (2026-08-18): times the carrier-block prior floor applied, per SEARCH
// EVALUATION, since the last call -- and resets. Same unit warning as
// takeDauntlessOfferEvalsInSearch() in bb/macro_actions.h: this counts
// evaluations inside the search, not events on the pitch. It exists to answer
// one question -- DID THE ARM RUN AT ALL -- which is what the per-pair null
// control ("MOVED WITHOUT THE ARM ACTING") needs.
long takeCarrierBlockPriorEvalsInSearch();

// ⭐ W-GFI krok (0) (04.09.2026): kolik F1 cage-advance planu vyslo DICEY
//   (zamitnuto jako prilis rizikove) a kolik z toho souviselo s 1-GFI
//   povolenim na rohu. `cage_advance.cpp` zustava jen ZDROJ DAT -- toto
//   cte uz existujici `CageAdvancePlan::diagMacroCornerGfi`, nic v cage
//   logice se nemeni.
//   [0] pocet planu s verdiktem DICEY
//   [1] z toho: soucet rohu vyzadujicich 1-GFI napric temi plany
//   [2] z toho: kolikrat byl PRAVE SELHAVSI krok rohem s 1-GFI
void takeCageDiceyGfiStats(long* out3);

// ⭐⭐⭐ KLEC/K6 (10.09.2026): ZAHRANE makro, ktere se rozbalilo do NICEHO.
//   `macro_mcts.cpp` na tom miste vracel `END_TURN`, tedy propadl CELY ZBYTEK
//   kola, ne jen aktivaci nosice. Bez jmenovatele se to cislo necte, proto se
//   tika i pocet vsech zahranych maker.
//   [0] vsechna zahrana makra (kazdy pick, ktery sel do rozbaleni)
//   [1] z toho: prvni rozbaleni dalo PRAZDNO
//   [2] z toho: zachraneno -- jine nabidnute makro uz akce dalo (K6 krok 2)
//   [3] z toho: presto END_TURN (opravdu nebylo co delat)
//   [4 + t] rozpad [1] podle MacroType t
//   ⇒ INVARIANT: [1] >= [2] + [3] (rozdil = zachranil staged-plan guard vyse),
//     a soucet [4..] == [1].
constexpr int kMacroNoopSlots = 4 + static_cast<int>(MacroType::MACRO_COUNT);
void takeMacroNoopStats(long* out);   // kMacroNoopSlots cisel

class StagedTurnPlanner;  // bb/turn_planner.h (item 13)
class CageAdvancePlanner; // bb/cage_advance.h (F1, 2026-08-03)

struct MacroMCTSNode {
    Macro macro;
    MacroMCTSNode* parent = nullptr;
    std::vector<MacroMCTSNode> children;
    int visits = 0;
    double totalValue = 0.0;
    bool expanded = false;
    float prior = 1.0f;
    // Team whose macro choice this node's CHILDREN represent (= activeTeam
    // of the state this node was expand()-ed from). totalValue/Q is always
    // stored in the search's fixed searchingSide perspective (simulate()
    // never sign-flips) — bestChildPUCT uses this to know whether to
    // maximize or minimize Q when picking among children, since a node
    // whose actingTeam differs from searchingSide represents the
    // opponent's decision (adversarial, not cooperative).
    TeamSide actingTeam = TeamSide::HOME;

    MacroMCTSNode* bestChildPUCT(double C, bool maximize) const;
    MacroMCTSNode* mostVisitedChild() const;
};

struct MacroChildVisitInfo {
    Macro macro;
    int visits;
    float prior = 0.0f;  // post-renorm root prior (diagnostics/tests)
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
    // KLEC/K6: MacroMCTSPolicy potrebuje NABIDKU se stejnym filtrem, jaky mel
    // root hledani, kdyz zvolene makro nic nerozbalilo. Jinak by zachrana
    // nabizela jine makro, nez hledani videlo.
    bool dauntlessInOffer() const { return config_.dauntlessInOffer; }

    // Test-only: expand a fresh root for `state` and return each child's
    // (macro, prior) after floor/cap + renorm. Pure wrapper over the private
    // expand(); exists so the prior floor/cap regime is pinnable by gtest
    // (2026-07-10 audit: 8/11 shipped prior-floor fixes had zero C++
    // regression tests).
    std::vector<std::pair<Macro, float>> expandRootPriorsForTest(const GameState& state);

    // Public wrapper over the private static leaf heuristic (simulate()).
    // The item13 staged turn planner evaluates its projected branch states
    // with the SAME leaf eval the search uses -- confirmed MVP design: no
    // new value function. Non-const because the (config-gated) leafLookahead
    // path inside simulate() rolls real dice.
    double evaluateLeaf(const GameState& state, TeamSide perspective) {
        return simulate(state, perspective);
    }

private:
    MacroMCTSNode* select(MacroMCTSNode* root, TeamSide searchingSide);
    void expand(MacroMCTSNode* node, const GameState& state);
    double simulate(const GameState& state, TeamSide perspective);
    void backpropagate(MacroMCTSNode* node, double value);
    ReplayOutcome replayToNode(GameState& state, MacroMCTSNode* node);
    // Bounded greedy one-ply forward look from a leaf state (see macro_mcts.cpp).
    double greedyLookaheadBonus(const GameState& leafState, TeamSide perspective);
    // Q-guarded risk-sequencing defer (queue item 10, config_.riskDeferral):
    // see macro_mcts.cpp for the full rationale and validation reference.
    Macro applyRiskDeferral(const MacroMCTSNode& root, const GameState& state,
                            const Macro& pick, TeamSide perspective);
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

    // Item 13 staged safe-then-PICKUP planner (config_.stagedPickupPlanner,
    // default off). When a turn's goal is PICKUP_BALL, the planner supplies
    // the whole turn's macro sequence up front (safe backups first, PICKUP
    // branch last); any deviation falls back to per-macro search() for the
    // rest of the turn -- the existing re-planning path, unchanged.
    std::unique_ptr<StagedTurnPlanner> stagedPlanner_;
    // F1 cage advance (config_.cageAdvance, default off): when a turn's goal
    // is ADVANCE_BALL and a cage stands around the carrier, this planner may
    // supply the whole cage-shift macro sequence (corners first, carrier
    // last) through the same staged-plan machinery. Mutually exclusive with
    // the pickup plan by goal (PICKUP_BALL vs ADVANCE_BALL).
    std::unique_ptr<CageAdvancePlanner> cagePlanner_;
    std::vector<Macro> stagedMacros_;
    size_t stagedIndex_ = 0;
    // Index of the first cage-fill macro (item13 step 2) in stagedMacros_;
    // macros from here on are validated with requireHeldBall (a failed
    // pickup drops the stage). SIZE_MAX = plan has no cage-fill stage.
    size_t stagedCageFillFrom_ = SIZE_MAX;
    bool stagedPlanBuilt_ = false;  // at most one plan build per team-turn
    int stagedPlansAdopted_ = 0;    // diagnostics: valid plans taken (game total)
    int stagedPlanTurn_ = -1;
    int stagedPlanHalf_ = -1;
    TeamSide stagedPlanTeam_ = TeamSide::HOME;

    // Next staged macro if an active plan covers this state (validates the
    // macro against the current state; on deviation clears the plan and
    // reports none so the caller re-enters search()).
    bool nextStagedMacro(const GameState& state, Macro& out);

public:
    MacroMCTSPolicy(const ValueFunction* vf, MCTSConfig config, uint32_t seed = 0);
    ~MacroMCTSPolicy();  // out-of-line: unique_ptr over fwd-declared planner

    Action operator()(const GameState& state);

    void setLogDecisions(bool log, int topK = 20);
    const std::vector<PolicyDecision>& decisions() const { return decisions_; }
    void clearDecisions() { decisions_.clear(); }

    int lastIterations() const { return search_.lastIterations(); }
    double lastBestValue() const { return search_.lastBestValue(); }
    // Diagnostics: how many staged plans (item13 pickup or F1 cage advance)
    // this policy adopted over its lifetime -- harnesses report "did the
    // gated feature even fire" alongside outcome deltas.
    int stagedPlansAdopted() const { return stagedPlansAdopted_; }
};

} // namespace bb
