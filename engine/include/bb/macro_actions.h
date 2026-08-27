#pragma once

#include "bb/game_state.h"
#include "bb/rules_engine.h"
#include "bb/dice.h"
#include "bb/action_features.h"
#include <vector>
#include <cstdint>

namespace bb {

enum class MacroType : uint8_t {
    SCORE = 0,
    ADVANCE,
    CAGE,
    BLITZ,
    BLOCK,
    PICKUP,
    PASS_ACTION,
    FOUL,
    REPOSITION,
    END_TURN,
    BLITZ_AND_SCORE,
    HAND_OFF_SCORE,
    PASS_SCORE,
    CHAIN_SCORE,
    MACRO_COUNT  // = 14
};

struct Macro {
    MacroType type = MacroType::END_TURN;
    int playerId = -1;      // primary player
    int targetId = -1;      // target (blitz/block/foul/pass/relay)
    Position targetPos{-1, -1}; // target position (reposition)
    int thirdId = -1;       // third player (CHAIN_SCORE scorer)
    // REPOSITION only: extra GFI squares the walk may roll for (0-2).
    // Default 0 keeps REPOSITION dice-free; the cage-advance planner sets
    // it for the ball carrier in tempo emergencies (user doctrine 2026-08-04:
    // "the carrier MUST arrive even at GFI dice cost near the end").
    int gfiAllowance = 0;
};

struct MacroExpansionResult {
    std::vector<Action> actions;
    bool turnover = false;
};

// Generate all available macros for the current game state
// dauntlessInOffer: price a block at the strength Dauntless would equalise to.
// ⚠️ This parameter default is NOT production. Production is
// MCTSConfig::dauntlessInOffer, which has been TRUE since 17.08.2026, and every
// engine call site passes config_.dauntlessInOffer explicitly. The false here
// exists only so the unit tests that call this directly keep asserting the raw
// offer; a new direct caller that omits the argument gets the non-production
// filter, which is the same "prices a different action than the one played"
// trap the arm was written to fix. Pass it explicitly.
void getAvailableMacros(const GameState& state, std::vector<Macro>& out,
                        bool dauntlessInOffer = false);

// ⚠️ READ THE UNIT BEFORE QUOTING EITHER OF THESE (P25, 2026-08-17).
//
// getAvailableMacros runs at the MCTS root AND at every expansion, so these
// count EVALUATIONS INSIDE THE SEARCH -- roughly a hundred per decision. They
// are NOT a count of anything that happened on the pitch, and the difference is
// not a rounding error: takeDauntlessRollEvalsInSearch reported 349 per game
// while the corpus logged 1.88 Dauntless rolls actually played. A factor of 186.
//
// They answer exactly one question, and answer it well: DID THE ARM RUN AT ALL?
// Zero means the two arms executed the same code, which makes that matchup a
// true null. Any other reading needs the played count instead, which comes from
// the event log: SKILL_USED with roll == SkillName::Dauntless for the rolls,
// HAND_OFF events for the hand-offs.
//
// The old names (takeDauntlessOfferCount, takeHandOffOfferCount) said "count"
// and were duly read as "how often we did it". Hence the rename.

// Times the block offer priced a block at the strength Dauntless would equalise
// to, per SEARCH EVALUATION, since the last call -- and resets. Only ever
// non-zero where dauntlessInOffer is set, so it needs no per-side bookkeeping.
long takeDauntlessOfferEvalsInSearch();

// Times a HAND_OFF (a PASS_ACTION macro whose target is adjacent) was put on the
// menu, per SEARCH EVALUATION, since the last call -- and resets. P21: the
// corpus logs zero hand-offs PLAYED across 3000 games while this reports 10.4
// per game offered, which is what separates "the gate never offers" from "the
// search never picks". Played hand-offs: count HAND_OFF events.
long takeHandOffOfferEvalsInSearch();

// --- P35 arm (2026-08-19): price a BLITZ block from the square the blitzer
// LANDS on, not the one he starts from.
//
// getBlockDiceCount counts the DEFENDER's assists around the attacker's square.
// A blitz moves first and blocks second (action_resolver.cpp:86-118), so
// block_handler.cpp:491 counts them on arrival. The candidate ranking counted
// them at home -- a blitzer in the open shows zero and can pick up several by
// stepping next to the target. Measured on the corpus (2026-08-19, 27 928
// reconstructed blitzes): the dice bracket changes in 16.2 %, and in 9.7 % it
// flips from "we choose" to "the opponent chooses", most often +1 -> -2.
//
// Per side, default OFF. Turning it on for one side only is what makes a paired
// A/B legible.
void setBlitzLandingArm(TeamSide side, bool on);
// LEAP do makrove chuze (26.08.2026). Gate se ptá na stranu MOVERA, ne na
// activeTeam -- Leap mají v korpusu jen soupeřovi wardanceři, takže per-side
// zapnutí je jediný způsob, jak měřit "wood-elf s rukama vs. bez rukou".
void setLeapWalkArm(TeamSide side, bool on);
bool leapWalkArm(TeamSide side);
bool blitzLandingArm(TeamSide side);

// Times the arm actually changed WHICH blitzer gets sent, per SEARCH EVALUATION
// (read the unit warning above -- this is not a count of blitzes played). Zero
// over a matchup means both arms took the same decision, i.e. a true null arm.
long takeBlitzLandingRepicksInSearch();
// Tiká, když findMoveToward VRÁTÍ LEAP jako vítěze -- ne když ho jen připustí
// mezi kandidáty. OFF rameno LEAP vrátit nemůže, takže každý tik je z definice
// změněná volba. Je to počet VYHODNOCENÍ V SEARCHI, ne skoků na hřišti.
long takeLeapWalkPicksInSearch();

// --- M1/N10 arm (2026-08-25): a Blitz is a MOVE with a block inside it ---
//
// BB2016 l. 347-350: "He may make one block during the move. The block may be
// made AT ANY POINT during the move." The engine ended the activation on every
// block, so hit-and-run was impossible and a carrier could never open his own
// lane. The user reported it on 22.07; M9 measured the ceiling on 24.08 --
// 4.09 blitzes a game end stuck in contact with MA left and somewhere to go,
// AV7 pieces 1.5x more often than AV9.
//
// ⚠️ THE ARM COVERS ALL THREE HALVES AT ONCE, deliberately. They are one rule:
//   (1) the activation stays open after the block   (block_handler)
//   (2) the blitzer is OFFERED a retreat            (getAvailableMacros)
//   (3) the follow-up becomes a choice, l. 608-611  (block_handler)
// Splitting them into separate switches would measure a mixture: (1) without
// (2) is permission with nowhere to go, and (1)+(2) without (3) is mostly eaten
// by the push dragging him in before he can withdraw.
//
// Per side, default OFF.
void setBlitzContinuationArm(TeamSide side, bool on);
bool blitzContinuationArm(TeamSide side);

// Times the arm actually changed a decision: an activation left open, a retreat
// offered, or a follow-up declined. Zero over a matchup means both arms played
// the same game -- the null test. Per SEARCH EVALUATION, like the P35 counter.
long takeBlitzContinuationEventsInSearch();
// Internal: bump the counter from block_handler, which owns two of the three
// halves. Not part of the harness API.
void noteBlitzContinuationEvent();

// --- B2 arm (2026-08-25): price a block against a defender who can WRESTLE ---
//
// blockDieBadFraction knew only the ATTACKER's Block and returned 1/6 with it.
// BB2016 l. 8670-8676: Wrestle places BOTH players prone "even if one or both
// have the Block skill", so against a Wrestle defender BOTH_DOWN is bad for us
// too and the truth is 2/6. On one die that is 16.7% against 33.3%; on two dice
// -- which is what Guard buys -- 2.8% against 11.1%, a FOURFOLD underestimate.
//
// M7 measured the ceiling on the 6 000 corpus games containing a skaven:
// 5.45 blocks a game land on a Wrestle defender, 15.1% come up Both Down, and
// every single one of those put our own body on the ground as well.
//
// Per side, default OFF.
void setWrestlePricingArm(TeamSide side, bool on);
bool wrestlePricingArm(TeamSide side);
// Times the arm actually changed the price -- defender has Wrestle AND the
// attacker has Block, the only case where the two answers differ.
long takeWrestlePricingEventsInSearch();

// --- P38 arm (2026-08-19): derive the carrier's destination square from the
// cage it would produce (user's rule, spec 15.0c).
//
// expandAdvance picks a y-offset by counting tackle zones on the route; the
// four squares that will BE the cage never enter the choice. Corpus measurement
// (19 964 turns): a reachable square giving a full clean cage -- four corners,
// all clean, no other neighbour of the carrier, four free bodies able to reach
// those corners -- exists in 95.6 % of turns, and in 25.7 % of those the carrier
// already stands on one. The rule is met in 2.7 %. Body budget blocks 3.7 %,
// the opponent 0.7 %; the rest is the choice of square.
//
// ⚠️ The arm only ranks squares within ONE square of the best available forward
// progress: K9a (schedule floor) is our strongest predictor at 20.7 sigma, so
// this changes WHICH square the carrier ends on, never how far it goes.
void setCageAwareAdvanceArm(TeamSide side, bool on);
bool cageAwareAdvanceArm(TeamSide side);

// --- P40 placebo arm (2026-08-20): the SAME square search as P38, minus the
// cage criterion.
//
// P38 bundles three changes at once and its +0.0827 cannot be attributed:
// (A) lateral freedom -- the baseline computes the target arithmetically and
// every candidate lies on one line forward; (B) the cage criterion; (C) it
// bypasses the fallback loop that otherwise pulls `steps` down to 0, i.e. the
// carrier does not move at all (that is literally P39).
//
// The placebo is identical to P38 in EVERYTHING -- same step budget, same
// `prog >= maxProgress - 1` band, same tackle-zone filter, same bypass of the
// fallback -- and differs ONLY by dropping cageScoreForSquare. So:
//   placebo ~ P38  => the finding is "the carrier could not step aside", not
//                     the cage, and P32/P9/P35 become one brief about WHERE;
//   placebo < P38  => the cage criterion earns part of the gain.
//
// Corpus evidence that motivated it (fable_p38_decomposition_20260820.md):
// over the 5 213 turns where the carrier neither moved nor acted, the placebo
// would find a square in 97.9 % of them and P38 in only 58.9 % -- the cage
// criterion BLOCKS the arm in two out of five idle turns.
//
// ⚠️ Both arms must never be on at once; setting one clears the other, so a
// caller cannot accidentally measure their sum.
void setPlaceboAdvanceArm(TeamSide side, bool on);
bool placeboAdvanceArm(TeamSide side);

// Times the arm actually moved the carrier's target square, per SEARCH
// EVALUATION. Zero over a matchup means both arms took the same decision.
long takeCageAwareAdvancePicksInSearch();

// Expand a macro into a sequence of low-level actions via greedy heuristics.
// Modifies state in-place as actions are executed.
MacroExpansionResult greedyExpandMacro(GameState& state, const Macro& macro,
                                       DiceRollerBase& dice);

// Stall-aware step budget for a ball carrier's own movement (shared by the
// ADVANCE expansion and the PICKUP advance continuation). Exported so the
// staged pickup planner can project the carrier's post-pickup position for
// its cage-fill stage with the SAME arithmetic the executor will use.
int carrierStallAwareSteps(const GameState& state, const Player& carrier,
                           const TeamState& myTeam);

// Extract NUM_ACTION_FEATURES features for a macro (shared count with action_features.h for policy reuse)
void extractMacroFeatures(const GameState& state, const Macro& macro, float* out);

} // namespace bb
