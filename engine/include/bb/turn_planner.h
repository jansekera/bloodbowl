#pragma once

#include "bb/game_state.h"
#include "bb/macro_actions.h"
#include "bb/macro_mcts.h"
#include "bb/mcts.h"
#include "bb/dice.h"
#include <vector>
#include <cstdint>

namespace bb {

// Queue item 13 MVP (2026-07-31): simplified whole-turn planner, staged
// safe-then-PICKUP. Confirmed scope (user, 2026-07-28, see
// project_bloodbowl_item13_wholeturn_pickup_planner_20260728): ONE turn, no
// multi-turn lookahead. Every dice-free (safe) macro is sequenced FIRST --
// concretely the "bring backup near the loose ball" behavior item 11's
// REPOSITION targeting already generates -- then exactly ONE stochastic
// branch point: the PICKUP attempt, with two modeled outcomes:
//   success -> the plan continues under the ADVANCE goal (expandPickup
//              already advances the fresh carrier with the stall-aware
//              throttle -- no separate post-branch machinery in the MVP);
//   failure -> the plan stops; the projected state is evaluated with a
//              fixed penalty (same spirit as greedyLookaheadBonus's -0.10
//              observed-turnover term), no bounce/recovery dice tree.
// Plan value = P(success)*V(success) + P(fail)*V(fail), both V(...) via the
// EXISTING static simulate() leaf heuristic (MacroMCTSSearch::evaluateLeaf)
// on projected states -- no new value function, no MCTS for the safe stage.
// BLITZ / LOS-block / FOUL stages and any multi-turn chaining are explicit
// LATER extensions, deliberately absent here.

// Turn goal, staged by drive phase -- mirrors what simulate()'s
// turnsLeft/idealDist pacing encodes implicitly (macro_mcts.cpp, offensive
// scoringBonus block), made explicit.
enum class TurnGoal : uint8_t {
    NONE = 0,     // opponent holds the ball / not in PLAY -- no offensive plan
    PICKUP_BALL,  // ball loose on the pitch -> recover it
    ADVANCE_BALL, // we hold it, endzone out of reach this turn
    SCORE_BALL,   // we hold it, in scoring range (or last-turns urgency)
};

TurnGoal classifyTurnGoal(const GameState& state);

struct StagedPlan {
    TurnGoal goal = TurnGoal::NONE;
    // MVP builds plans only for PICKUP_BALL; other goals return valid=false
    // and the caller keeps using per-macro search() as today.
    bool valid = false;

    std::vector<Macro> safeMacros;  // deterministic stage, executed in order
    Macro pickupMacro{};            // the single stochastic branch point (last)

    // 2-branch value model (all values in the planning side's perspective)
    double pSuccess = 0.0;      // P(pickup macro ends with us holding the ball)
    double valueSuccess = 0.0;  // mean leaf eval over success samples
    double valueFail = 0.0;     // mean leaf eval over fail samples + FAIL_PENALTY
    double planValue = 0.0;     // pSuccess*valueSuccess + (1-pSuccess)*valueFail

    // Diagnostics: safe macros whose REPOSITION target is adjacent to the
    // loose ball -- the item 11 "backup before the pickup roll" closure metric.
    int backupCount = 0;
};

// Semantic re-validation of a planned macro against the CURRENT state --
// deliberately not "regenerate getAvailableMacros and compare": REPOSITION
// targets are recomputed each generation pass (nearest free ball-adjacent
// square shifts as teammates arrive), so exact-match validation would flag a
// healthy plan as deviated after its own first step. Only the macro types a
// staged plan can contain (REPOSITION, PICKUP) get real checks; anything
// else is conservatively invalid.
bool stagedMacroStillValid(const GameState& state, const Macro& m);

class StagedTurnPlanner {
public:
    // Safe-stage gate: same thresholds item 10's Q-guard validated
    // (RISK_DEFER_SAFE_PTO / probe K, macro_mcts.cpp) -- a macro is "safe"
    // only if a Monte-Carlo probe of its expansion is dice-free in practice.
    static constexpr double SAFE_PTO = 0.02;
    static constexpr int PROBE_K = 48;
    // Branch sampling for pSuccess / V(success) / V(fail).
    static constexpr int BRANCH_K = 64;
    // Safe-stage cap (user design constraint, 2026-07-31): send ONE, at most
    // TWO players as backups to the loose ball -- committing more strips the
    // rest of the pitch (and an uncapped stage degenerated into a whole-team
    // column converging on the ball, observed on mined state g0000).
    // Candidate ordering below picks WHICH two: arrivals-this-turn first,
    // then nearest player to the ball.
    static constexpr int MAX_SAFE_BACKUPS = 2;
    // Fixed fail-branch penalty, same spirit and magnitude as
    // greedyLookaheadBonus's observed-turnover term (macro_mcts.cpp).
    static constexpr double FAIL_PENALTY = -0.10;

    StagedTurnPlanner(const ValueFunction* vf, MCTSConfig config, uint32_t seed = 0);

    // Build a staged plan for `state`. Returns valid=false unless the goal
    // is PICKUP_BALL and a PICKUP macro exists.
    StagedPlan build(const GameState& state);

private:
    MCTSConfig config_;
    MacroMCTSSearch evaler_;  // leaf eval only (evaluateLeaf)
    DiceRoller dice_;

    struct BranchStats {
        double pSuccess = 0.0;
        double valueSuccess = 0.0;
        double valueFail = 0.0;
    };
    // Probe pTO/no-op-ness of a macro on `state` (item 10's probe pattern).
    struct ProbeStats {
        double pto = 0.0;
        double meanActions = 0.0;
    };
    ProbeStats probeMacro(const GameState& state, const Macro& m);
    BranchStats sampleBranch(const GameState& projected, const Macro& pickup,
                             TeamSide perspective);
};

} // namespace bb
