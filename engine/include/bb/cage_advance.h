#pragma once

#include "bb/game_state.h"
#include "bb/macro_actions.h"
#include "bb/macro_mcts.h"
#include "bb/mcts.h"
#include "bb/dice.h"
#include <vector>
#include <cstdint>

namespace bb {

// F1 (2026-08-03): CAGE_ADVANCE -- move the cage AS A UNIT, 1-2 squares
// forward per turn. Root cause it closes: expandCage only ever BUILDS the
// 4-diagonal cage around the carrier's current square and never moves it;
// the only way forward was a solo carrier ADVANCE that abandons the cage
// (evidence/fable_dwarf_playstyle_gap_20260803.md section 2.1, situations
// g0018 "perfect 4/4 cage frozen for 3 turns" and g0003/g0023 "carrier
// leaves the cage on solo runs"). Doctrine: team1_brief_per_player.md,
// "Cage advance (pomaly tym)": hold cage -> clear blockers -> shift cage
// 1-2 squares -> repeat, 8 turns to a TD.
//
// Implemented as a config-gated whole-turn staged plan (MCTSConfig::
// cageAdvance, DEFAULT OFF -- pattern of item13's stagedPickupPlanner):
// the plan supplies existing REPOSITION macros directly to MacroMCTSPolicy,
// BYPASSING search()'s prior system entirely. This is deliberate prior-floor
// hygiene: no new macro family means no new floored prior mass and no
// re-run of item7's "family mass" rebalancing lesson (macro_mcts.cpp,
// expand()). Deviation handling reuses stagedMacroStillValid + the existing
// search() fallback, exactly like item13.
//
// Binding design constraints (user, 2026-08-03):
//  1. TEMPO IS COMPUTED, NOT A CONSTANT.
//     requiredPace = carrier distance to the TD zone / usable turns, where
//     usable turns = min(turns left in half, 8) - RESERVE_TURNS (mandatory
//     >= 1 turn of slack). The turns-left arithmetic is the SAME
//     turnsLeft = 9 - turnNumber schedule simulate()'s idealDist =
//     turnsLeft*MA pacing already encodes (macro_mcts.cpp, scoringBonus) --
//     no second pacing mechanism, this is that schedule read per-turn.
//     achievablePace = the largest per-turn step the ROLES can sustain
//     (carrier movement; corner reformation -- a corner travels up to
//     step+2 squares when rotating, verified by the actual assignment below)
//     MINUS opponent resistance (standing opponents in the advance corridor;
//     a screen in the path costs an extra arc/block, so 1-2 opponents
//     subtract one square of pace, 3+ subtract two).
//     If requiredPace > achievablePace the planner returns
//     TEMPO_INSUFFICIENT: NO blind push -- the caller falls back to
//     search() (which can blitz, pass, or solo-run as today).
//  2. CORNER SELECTION: Guard/StandFirm preference SUBORDINATE to tempo and
//     activation reliability. Sort key per slot: arrives without GFI first,
//     then shortest distance, then Guard, then StandFirm. Hard exclusions
//     are GENERIC OVER SKILLS, never race/position names
//     (eligibleCornerPlayer): activation nega-traits (BoneHead,
//     ReallyStupid, WildAnimal, TakeRoot), SecretWeapon (ejected after the
//     drive -- Deathroller-type; its NoHands is irrelevant for corner duty
//     but SecretWeapon alone disqualifies), BallAndChain (cannot position).
//     In TV1200 this excludes the human Ogre (BoneHead) and the wood-elf
//     Treeman (TakeRoot) despite the Treeman's Guard+StandFirm.
//     GFI feasibility: every corner must arrive WITHOUT GFI; at most ONE
//     corner may be assigned at distance movement+1. Because REPOSITION is
//     the engine's only dice-free macro and deliberately never GFIs
//     (expandReposition caps at movementRemaining), that one "1-GFI corner"
//     is executed as a dice-free walk that stops ONE square short and
//     closes next turn -- the allowance is an assignment-VALUE rule, not a
//     real GFI gamble. Anything needing more counts as an open corner
//     (lower assignment value; too many open corners = no plan).
//  3. ROLE-AWARE SHARED BUDGET: build() takes reservedPlayerIds -- players
//     needed elsewhere (future: blitzer, foul assists) are never drafted to
//     corners. Collision with the item13 staged PICKUP plan is structural:
//     the two planners fire on mutually exclusive goals (PICKUP_BALL needs
//     a loose ball, ADVANCE_BALL needs a held one) and MacroMCTSPolicy
//     builds at most one staged plan per team-turn. The parameter is the
//     forward-compatible API for finer-grained reservations; today the
//     policy passes an empty list.
//
// Safety: every emitted macro is MC-probed on the evolving projection with
// the same SAFE_PTO=0.02 / PROBE_K=48 thresholds item10's Q-guard and
// item13's safe stage validated -- a cage whose corners are already marked
// (moving them would mean dodges) yields DICEY and the turn falls back to
// search(), which can blitz the markers off first.

enum class CageAdvanceVerdict : uint8_t {
    NOT_APPLICABLE = 0,   // trigger not met (goal, corners, carrier state)
    TEMPO_INSUFFICIENT,   // computed pace cannot meet the schedule -> fallback
    DICEY,                // a required move failed the dice-free probe -> fallback
    PLAN_READY,
};

struct CageAdvancePlan {
    CageAdvanceVerdict verdict = CageAdvanceVerdict::NOT_APPLICABLE;
    bool valid = false;          // verdict == PLAN_READY

    int step = 0;                // squares the cage advances this turn (1..2)
    double requiredPace = 0.0;   // dist / usable turns (reserve deducted)
    int rawAchievableStep = 0;   // role-limited step before resistance
    double achievablePace = 0.0; // rawAchievableStep - resistance penalty
    int resistance = 0;          // standing opponents in the advance corridor

    int builtCorners = 0;        // diagonal corners standing at plan time
    int filledCorners = 0;       // slots filled after the advance (movers+stays)
    int openCorners = 0;
    int gfiCorners = 0;          // slots on the 1-GFI allowance (0 or 1)
    int carrierGfi = 0;          // real GFI rolls the carrier leg takes (0-2)

    double planValue = 0.0;      // leaf eval of the projected end state (diag)

    // Ordered: front-slot movers, back-slot movers, carrier LAST (the screen
    // forms before the carrier commits). All macros are REPOSITION.
    std::vector<Macro> macros;
};

class CageAdvancePlanner {
public:
    // Step ceiling is COMPUTED from real role MA (user constraint 2026-08-03,
    // re-affirmed 2026-08-04: "a dwarf with 8 turns MUST be able to arrive" --
    // the old hard MAX_STEP=2 made the schedule unsatisfiable from own half
    // and the plan never fired). The carrier may add up to CARRIER_GFI_MAX
    // real GFI rolls in a tempo emergency; corner sustainability emerges from
    // tryAssign's per-slot reach checks.
    static constexpr int CARRIER_GFI_MAX = 2;     // BB rules: 2 GFI/turn
    static constexpr int RESERVE_TURNS = 1;       // mandatory schedule slack
    static constexpr int MAX_HALF_TURNS = 8;      // team-turns per half cap
    static constexpr int TRIGGER_MIN_CORNERS = 2; // diagonal corners built
    static constexpr int CORRIDOR_DEPTH = 4;      // step+2 horizon ahead
    static constexpr int CORRIDOR_HALF_WIDTH = 2;
    // Same probe regime item10/item13 validated (macro_mcts.cpp /
    // turn_planner.h): a macro is safe only if its MC-probed expansion is
    // dice-free in practice and not a no-op.
    static constexpr double SAFE_PTO = 0.02;
    // Relaxed probe ceilings for the carrier's GFI leg ONLY (tempo emergency,
    // user doctrine 2026-08-04: the carrier must arrive even at dice cost).
    // 1 GFI fails 1/6 ~= 0.167, 2 GFI ~= 0.31; the PROBE_K=48 MC estimate
    // has sd ~= 0.054, so each ceiling sits ~1.5 sd above its true rate --
    // tight enough to still reject plans with EXTRA danger on the walk
    // (tackle-zone dodges on top of the GFI), loose enough not to veto the
    // sanctioned GFI risk itself on sampling noise.
    static constexpr double SAFE_PTO_GFI1 = 0.25;
    static constexpr double SAFE_PTO_GFI2 = 0.40;
    static constexpr int PROBE_K = 48;

    CageAdvancePlanner(const ValueFunction* vf, MCTSConfig config,
                       uint32_t seed = 0);

    // Build a cage-advance plan. valid=false unless the goal is
    // ADVANCE_BALL, >= TRIGGER_MIN_CORNERS diagonal corners stand around the
    // carrier, and tempo + probes clear (see verdict for why not).
    CageAdvancePlan build(const GameState& state,
                          const std::vector<int>& reservedPlayerIds = {});

    // Generic skill-based corner eligibility (constraint 2). Public for
    // tests and future planners.
    static bool eligibleCornerPlayer(const Player& p);

private:
    MCTSConfig config_;
    MacroMCTSSearch evaler_;  // leaf eval only (evaluateLeaf)
    DiceRoller dice_;

    struct SlotAssignment {
        Position slot{-1, -1};
        int playerId = -1;   // -1 = open
        bool stayPut = false;
        bool needsGfi = false;  // 1-GFI allowance slot (walks, stops 1 short)
    };
    struct AssignmentResult {
        bool feasible = false;
        int filled = 0, open = 0, gfi = 0;
        Position newCarrierPos{-1, -1};
        std::vector<SlotAssignment> slots;  // front pair first, then back pair
    };
    AssignmentResult tryAssign(const GameState& state, const Player& carrier,
                               int step,
                               const std::vector<int>& reservedPlayerIds) const;

    struct ProbeStats {
        double pto = 0.0;
        double meanActions = 0.0;
    };
    ProbeStats probeMacro(const GameState& state, const Macro& m);
};

} // namespace bb
