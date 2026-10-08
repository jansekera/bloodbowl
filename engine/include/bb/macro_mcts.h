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
#include <functional>
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

// P149 bod 2 (06.10.2026): hodnota jedné nevyužité aktivace v listovém odhadu hledání
// (cena turnoveru). Výchozí = změřených 0,024; 0 = vypnuto — jen pro měření před / po.
void setActivationValue(double v);
// P171 (a): váha členu „nechráněný nosič“ v listovém odhadu (0 = vypnuto, 1 = změřená cena).
void setCageLeafWeight(double w);
double cageLeafWeight();
double activationValue();

class CageController;     // bb/one_cage.h (P126, jedna klec)

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
    double q = 0.0;      // průměrná hodnota dítěte z pohledu hledající strany (diagnostika P149)
};

// P149 (06.10.2026): PROČ padla akce — které makro ji vyvolalo a kdo o něm rozhodl.
// Jen diagnostika (vazba ai_plan_turn ji přikládá ke kroku plánu), hru neovlivňuje.
struct MacroDecisionInfo {
    enum class Source : uint8_t { CAGE, SEARCH, RESCUE, GREEDY_FALLBACK };
    Macro macro;                 // makro, které se rozbalilo do plánu (po případné záchraně K6)
    Source source = Source::SEARCH;
    std::vector<MacroChildVisitInfo> children;   // děti kořene hledání; prázdné u makra klece
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
    std::function<bool(const Macro&)> rootVeto_;

public:
    MacroMCTSSearch(const ValueFunction* vf, MCTSConfig config, uint32_t seed = 0);

    // Review 08.10.2026 (P181 nález 7): makra, pro která `veto` vrátí true, se z kořene vyřadí
    // dřív, než hledání začne. Dřív se zakázané makro (např. skórování ve zdržovacím tahu)
    // odfiltrovalo až z VÝSLEDKU: celý strom šel za ním a hrálo se „další nejnavštěvovanější“
    // dítě s hrstkou návštěv — a to při každém rozhodnutí do konce tahu. END_TURN zůstává vždy.
    void setRootVeto(std::function<bool(const Macro&)> veto) { rootVeto_ = std::move(veto); }

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

    // P126 jedna klec (bb/one_cage.h): když tah patří kleci, další makro
    // navrhne ona; jinak a po každé odchylce rozhoduje search().
    std::unique_ptr<CageController> cage_;

    MacroDecisionInfo lastDecision_;
    int decisionCount_ = 0;

public:
    MacroMCTSPolicy(const ValueFunction* vf, MCTSConfig config, uint32_t seed = 0);
    ~MacroMCTSPolicy();  // out-of-line: unique_ptr nad dopředu deklarovanou klecí

    Action operator()(const GameState& state);

    void setLogDecisions(bool log, int topK = 20);
    const std::vector<PolicyDecision>& decisions() const { return decisions_; }
    void clearDecisions() { decisions_.clear(); }

    int lastIterations() const { return search_.lastIterations(); }
    double lastBestValue() const { return search_.lastBestValue(); }
    // P149: poslední rozhodnutí o makru a kolik jich už bylo (roste jen při novém rozhodnutí,
    // ne když se přehrává další akce téhož plánu).
    // Ohodnocení stavu tímtéž listovým odhadem, jaký používá hledání (diagnostika P149 bod 2).
    double evaluateLeaf(const GameState& state, TeamSide perspective) {
        return search_.evaluateLeaf(state, perspective);
    }
    const MacroDecisionInfo& lastDecision() const { return lastDecision_; }
    int decisionCount() const { return decisionCount_; }
    // Kolik plánů klece (po fázích tahu) hráč za život převzal.
    int cagePlansAdopted() const;
};

} // namespace bb
