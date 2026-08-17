#include <gtest/gtest.h>
#include "bb/macro_actions.h"
#include "bb/game_state.h"
#include "bb/game_simulator.h"
#include "bb/roster.h"
#include "bb/dice.h"
#include "bb/action_features.h"
#include "bb/helpers.h"
#include <algorithm>
#include <set>

using namespace bb;

namespace {

// Minimal state with one home player and one away player
GameState makeMinimalState() {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 1;
    state.homeTeam.rerolls = 3;
    state.awayTeam.rerolls = 3;
    state.weather = Weather::NICE;

    Player& p1 = state.getPlayer(1);
    p1.id = 1;
    p1.teamSide = TeamSide::HOME;
    p1.state = PlayerState::STANDING;
    p1.position = {10, 7};
    p1.stats = {6, 3, 3, 8};
    p1.movementRemaining = 6;
    p1.hasMoved = false;
    p1.hasActed = false;

    Player& p2 = state.getPlayer(12);
    p2.id = 12;
    p2.teamSide = TeamSide::AWAY;
    p2.state = PlayerState::STANDING;
    p2.position = {20, 7};
    p2.stats = {6, 3, 3, 8};
    p2.movementRemaining = 6;

    state.ball = BallState::onGround({13, 7});

    return state;
}

// State with carrier near endzone
GameState makeScoringState() {
    GameState state = makeMinimalState();
    Player& p1 = state.getPlayer(1);
    p1.position = {23, 7};
    p1.movementRemaining = 6;
    state.ball = BallState::carried({23, 7}, 1);
    return state;
}

// State with carrier deep in own half
GameState makeAdvanceState() {
    GameState state = makeMinimalState();
    Player& p1 = state.getPlayer(1);
    p1.position = {5, 7};
    p1.movementRemaining = 6;
    state.ball = BallState::carried({5, 7}, 1);
    return state;
}

bool hasMacroType(const std::vector<Macro>& macros, MacroType type) {
    for (auto& m : macros) {
        if (m.type == type) return true;
    }
    return false;
}

int countMacroType(const std::vector<Macro>& macros, MacroType type) {
    int count = 0;
    for (auto& m : macros) {
        if (m.type == type) count++;
    }
    return count;
}

} // anonymous namespace

// =============================================================
// Macro Generation Tests
// =============================================================

TEST(MacroActions, AlwaysHasEndTurn) {
    GameState state = makeMinimalState();
    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::END_TURN));
}

TEST(MacroActions, EmptyInNonPlayPhase) {
    GameState state = makeMinimalState();
    state.phase = GamePhase::GAME_OVER;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(macros.empty());
}

TEST(MacroActions, ScoreAvailableWhenCarrierNearEndzone) {
    GameState state = makeScoringState();
    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::SCORE));
}

TEST(MacroActions, ScoreNotAvailableWhenCarrierFar) {
    GameState state = makeAdvanceState();
    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_FALSE(hasMacroType(macros, MacroType::SCORE));
}

TEST(MacroActions, AdvanceAvailableWhenCarrierCantScore) {
    GameState state = makeAdvanceState();
    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::ADVANCE));
}

TEST(MacroActions, AdvanceNotAvailableWhenCanScore) {
    GameState state = makeScoringState();
    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // ADVANCE shouldn't appear when SCORE is possible (within MA+2)
    EXPECT_FALSE(hasMacroType(macros, MacroType::ADVANCE));
}

TEST(MacroActions, PickupAvailableWhenBallOnGround) {
    GameState state = makeMinimalState();
    // Ball on ground, player nearby
    state.ball = BallState::onGround({11, 7});

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::PICKUP));
}

TEST(MacroActions, PickupNotAvailableWhenBallHeld) {
    GameState state = makeScoringState(); // carrier has ball
    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_FALSE(hasMacroType(macros, MacroType::PICKUP));
}

// Regression for master-list item 7 (top-2 PICKUP pickers): with two
// comparable pickers in reach, generation must emit BOTH, best-first.
// The best-first ORDER is a contract the prior-floor split in
// macro_mcts.cpp relies on -- the order assertions below pin it.
// NEGATIVE CONTROL: pre-patch generation emits a single bestPicker, so
// countMacroType == 1 and this test fails.
TEST(MacroActions, PickupEmitsTopTwoPickersBestFirst) {
    GameState state = makeMinimalState();  // ball on ground at {13,7}
    // Player 1: dist 3, AG3, clear ball -> 67% - 15 = 52 (secondary).
    state.getPlayer(1).position = {10, 7};
    // Player 2: dist 2, AG3 -> 67% - 10 = 57 (primary).
    Player& p2 = state.getPlayer(2);
    p2.id = 2;
    p2.teamSide = TeamSide::HOME;
    p2.state = PlayerState::STANDING;
    p2.position = {15, 7};
    p2.stats = {6, 3, 3, 8};
    p2.movementRemaining = 6;
    p2.hasMoved = false;
    p2.hasActed = false;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    ASSERT_EQ(countMacroType(macros, MacroType::PICKUP), 2);
    std::vector<int> pickerIds;
    for (auto& m : macros) {
        if (m.type == MacroType::PICKUP) pickerIds.push_back(m.playerId);
    }
    EXPECT_EQ(pickerIds[0], 2);  // higher score first (best-first contract)
    EXPECT_EQ(pickerIds[1], 1);
}

// Guard (passes pre- and post-patch): a categorically worse second picker
// (score gap > 25) must NOT be emitted -- no floored prior mass for a
// picker that is strictly dominated on the pickup itself.
TEST(MacroActions, PickupSecondPickerGatedByScoreGap) {
    GameState state = makeMinimalState();
    // Player 1: dist 1, AG4, clear ball -> 83% - 5 = 78.
    state.getPlayer(1).position = {12, 7};
    state.getPlayer(1).stats = {6, 3, 4, 8};
    // Player 2: dist 8 (reach 6+2 -- still eligible), AG2 -> 50% - 40 = 10.
    // Gap 68 > 25 -> gated out.
    Player& p2 = state.getPlayer(2);
    p2.id = 2;
    p2.teamSide = TeamSide::HOME;
    p2.state = PlayerState::STANDING;
    p2.position = {5, 7};
    p2.stats = {6, 3, 2, 8};
    p2.movementRemaining = 6;
    p2.hasMoved = false;
    p2.hasActed = false;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    ASSERT_EQ(countMacroType(macros, MacroType::PICKUP), 1);
    for (auto& m : macros) {
        if (m.type == MacroType::PICKUP) EXPECT_EQ(m.playerId, 1);
    }
}

// Guard (passes pre- and post-patch): a single eligible picker keeps
// today's behavior bit-exactly -- one PICKUP macro, no phantom second.
TEST(MacroActions, PickupSinglePickerUnchanged) {
    GameState state = makeMinimalState();
    state.getPlayer(1).position = {10, 7};   // dist 3, in reach
    // (only home player near the ball; player 12 is AWAY at {20,7})

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    ASSERT_EQ(countMacroType(macros, MacroType::PICKUP), 1);
}

TEST(MacroActions, BlockAvailableWithFavorableDice) {
    GameState state = makeMinimalState();
    // Place ST4 player adjacent to ST3 enemy
    state.getPlayer(1).stats.strength = 4;
    state.getPlayer(1).position = {10, 7};
    state.getPlayer(12).stats.strength = 3;
    state.getPlayer(12).position = {11, 7};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::BLOCK));
}

TEST(MacroActions, BlockNotAvailableWith1Die) {
    GameState state = makeMinimalState();
    // Equal strength adjacent — 1 die, not 2+
    state.getPlayer(1).stats.strength = 3;
    state.getPlayer(1).position = {10, 7};
    state.getPlayer(12).stats.strength = 3;
    state.getPlayer(12).position = {11, 7};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_FALSE(hasMacroType(macros, MacroType::BLOCK));
}

TEST(MacroActions, BlitzAvailable) {
    GameState state = makeMinimalState();
    state.homeTeam.blitzUsedThisTurn = false;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::BLITZ));
}

TEST(MacroActions, BlitzNotAvailableWhenUsed) {
    GameState state = makeMinimalState();
    state.homeTeam.blitzUsedThisTurn = true;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_FALSE(hasMacroType(macros, MacroType::BLITZ));
}

TEST(MacroActions, FoulAvailableWithProneEnemy) {
    GameState state = makeMinimalState();
    state.getPlayer(12).state = PlayerState::PRONE;
    state.getPlayer(12).position = {11, 7}; // adjacent
    state.homeTeam.foulUsedThisTurn = false;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::FOUL));
}

TEST(MacroActions, FoulNotAvailableWhenUsed) {
    GameState state = makeMinimalState();
    state.getPlayer(12).state = PlayerState::PRONE;
    state.getPlayer(12).position = {11, 7};
    state.homeTeam.foulUsedThisTurn = true;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_FALSE(hasMacroType(macros, MacroType::FOUL));
}

TEST(MacroActions, CageAvailableWithCarrierAndFreePlayer) {
    GameState state = makeMinimalState();
    state.getPlayer(1).position = {10, 7};
    state.ball = BallState::carried({10, 7}, 1);

    // Add a second home player that is free
    Player& p2 = state.getPlayer(2);
    p2.id = 2;
    p2.teamSide = TeamSide::HOME;
    p2.state = PlayerState::STANDING;
    p2.position = {8, 5};
    p2.stats = {6, 3, 3, 8};
    p2.movementRemaining = 6;
    p2.hasMoved = false;
    p2.hasActed = false;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::CAGE));
}

TEST(MacroActions, RepositionForFreePlayer) {
    GameState state = makeMinimalState();
    // Player 1 has no adjacent enemies → free to reposition
    state.getPlayer(12).position = {20, 7}; // far away

    // Ball held by someone else to avoid PICKUP macro
    state.ball = BallState::carried({15, 7}, 12);

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    EXPECT_TRUE(hasMacroType(macros, MacroType::REPOSITION));
}

TEST(MacroActions, RepositionNotForEngagedPlayer) {
    GameState state = makeMinimalState();
    // Place enemy adjacent to player 1
    state.getPlayer(12).position = {11, 7};
    state.ball = BallState::carried({15, 7}, 12);

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // Player 1 is engaged — no REPOSITION for them
    bool hasRepoForP1 = false;
    for (auto& m : macros) {
        if (m.type == MacroType::REPOSITION && m.playerId == 1) {
            hasRepoForP1 = true;
        }
    }
    EXPECT_FALSE(hasRepoForP1);
}

// Regression for Strategy 0.5 (intercept lane, research_fable_20260709
// section 3b): on defense the first goal-side defender must be sent into
// the carrier's ACTUAL Y lane, between the carrier and the defended
// endzone -- not to a fixed-Y spot.
// NEGATIVE CONTROL: pre-patch the defensive chain can only emit fixed Ys
// (safety y=7, guards y=5/9, screen y in {3,5,7,9,11}) or the carrier's
// own square as marker ({8,2} here); the intercept point {4,2} is emitted
// by nothing, and this defender gets the safety spot {0,7} instead -- the
// EXPECT_TRUE below fails without the patch.
TEST(MacroActions, DefensiveRepositionTargetsCarrierLane) {
    GameState state = makeMinimalState();
    // AWAY carrier sprinting the flank at y=2, attacking toward x=0.
    state.getPlayer(12).position = {8, 2};
    state.ball = BallState::carried({8, 2}, 12);
    // Free HOME defender (MA6), goal-side of the carrier, off the lane.
    state.getPlayer(1).position = {5, 7};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // Intercept point = {(8 + 0) / 2, clamp(2, 1, 13)} = {4, 2}.
    bool hasLaneIntercept = false;
    for (auto& m : macros) {
        if (m.type == MacroType::REPOSITION && m.playerId == 1 &&
            m.targetPos.x == 4 && m.targetPos.y == 2) {
            hasLaneIntercept = true;
        }
    }
    EXPECT_TRUE(hasLaneIntercept);
}

// Guard (passes pre- and post-patch): a defender already beaten by the
// carrier (not goal-side) must NOT chase the intercept lane; it falls
// through to Strategy 1 (safety) exactly as before the patch.
TEST(MacroActions, DefensiveRepositionInterceptRequiresGoalSide) {
    GameState state = makeMinimalState();
    // Same flank carrier, but the defender starts 4 squares BEHIND the
    // play (carrier attacks toward x=0, defender at x=12): not goal-side.
    state.getPlayer(12).position = {8, 2};
    state.ball = BallState::carried({8, 2}, 12);
    state.getPlayer(1).position = {12, 7};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    bool hasSafety = false;
    for (auto& m : macros) {
        if (m.type != MacroType::REPOSITION || m.playerId != 1) continue;
        EXPECT_NE(m.targetPos.y, 2);
        if (m.targetPos.x == 0 && m.targetPos.y == 7) hasSafety = true;
    }
    EXPECT_TRUE(hasSafety);
}

// Rewritten 2026-08-11. A pass used to be offered to anyone with the ball,
// at any agility, toward any team-mate ahead. For a dwarf side that is a
// standing invitation to lose the drive: 21 turnovers on the 08-11 corpus
// came from our own AG2 throws and catches. Two rolls at AG2 complete about
// a quarter of the time. The rule now is the user's: dwarves pass "only in
// an emergency", and emergency is defined, not felt.
TEST(MacroActions, ShortPassBetweenGoodHandsIsOffered) {
    GameState state = makeMinimalState();
    state.getPlayer(1).position = {5, 7};
    state.getPlayer(1).stats = {6, 3, 4, 8};      // AG4 thrower
    state.ball = BallState::carried({5, 7}, 1);
    state.homeTeam.passUsedThisTurn = false;

    Player& p2 = state.getPlayer(2);
    p2.id = 2;
    p2.teamSide = TeamSide::HOME;
    p2.state = PlayerState::STANDING;
    p2.position = {8, 7};                          // short, no tackle zones
    p2.stats = {6, 3, 4, 8};                       // AG4 catcher
    p2.movementRemaining = 6;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);
    EXPECT_TRUE(hasMacroType(macros, MacroType::PASS_ACTION));
}

TEST(MacroActions, LowAgilityPassIsNotOfferedAsARoutineOption) {
    GameState state = makeMinimalState();
    state.getPlayer(1).position = {5, 7};
    state.getPlayer(1).stats = {4, 3, 2, 9};      // AG2 Longbeard
    state.ball = BallState::carried({5, 7}, 1);
    state.homeTeam.passUsedThisTurn = false;
    state.homeTeam.turnNumber = 1;                 // plenty of time left

    Player& p2 = state.getPlayer(2);
    p2.id = 2;
    p2.teamSide = TeamSide::HOME;
    p2.state = PlayerState::STANDING;
    p2.position = {12, 7};
    p2.stats = {4, 3, 2, 9};                       // AG2 catcher
    p2.movementRemaining = 4;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);
    EXPECT_FALSE(hasMacroType(macros, MacroType::PASS_ACTION))
        << "two AG2 rolls complete about a quarter of the time";
}

TEST(MacroActions, TheSameBadPassIsOfferedWhenTheHalfEndsWithoutIt) {
    GameState state = makeMinimalState();
    state.getPlayer(1).position = {14, 7};         // 11 from the endzone, MA4
    state.getPlayer(1).stats = {4, 3, 2, 9};
    state.ball = BallState::carried({14, 7}, 1);
    state.homeTeam.passUsedThisTurn = false;
    state.homeTeam.turnNumber = 8;                 // last turn of the half

    Player& p2 = state.getPlayer(2);
    p2.id = 2;
    p2.teamSide = TeamSide::HOME;
    p2.state = PlayerState::STANDING;
    p2.position = {18, 7};                         // 7 out, MA6 -> he reaches it
    p2.stats = {6, 3, 2, 9};
    p2.movementRemaining = 6;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);
    EXPECT_TRUE(hasMacroType(macros, MacroType::PASS_ACTION))
        << "the carrier cannot reach the endzone and the half ends anyway";
}

TEST(MacroActions, AnAdjacentTeamMateIsOfferedBecauseTheHandOffIsWhatHappens) {
    // Same two agilities the routine-pass test rejects at range, but adjacent.
    // expandPass runs HAND_OFF for a neighbour, and resolveHandOff has no throw
    // roll -- so the offer is worth the catch alone (AG3, +1, in the clear:
    // 3+ = 67%), not catch times a throw that never gets made (33%).
    GameState state = makeMinimalState();
    state.getPlayer(1).position = {5, 7};
    state.getPlayer(1).stats = {4, 3, 2, 9};       // AG2 Longbeard holds it
    state.ball = BallState::carried({5, 7}, 1);
    state.homeTeam.passUsedThisTurn = false;
    state.homeTeam.turnNumber = 1;                 // no emergency to lean on

    Player& p2 = state.getPlayer(2);
    p2.id = 2;
    p2.teamSide = TeamSide::HOME;
    p2.state = PlayerState::STANDING;
    p2.position = {6, 7};                          // adjacent, one square ahead
    p2.stats = {6, 3, 3, 8};                       // AG3 Blitzer
    p2.movementRemaining = 6;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    bool toTheNeighbour = false;
    for (auto& m : macros) {
        if (m.type == MacroType::PASS_ACTION && m.targetId == 2) toTheNeighbour = true;
    }
    EXPECT_TRUE(toTheNeighbour)
        << "a hand-off to an adjacent AG3 team-mate is a 3+ catch, not a throw";
}

TEST(MacroActions, AGoodCarrierIsNotOfferedAHandOffToWorseHandsAhead) {
    // The regression the swap gate exists for. Pricing hand-offs correctly but
    // leaving the old "is he ahead" gate in place moved the Longbeard share of
    // carrying turns from 1-4% to 6-10% over 3000 games, because a Runner could
    // legally give the ball to any Longbeard standing one square further up.
    GameState state = makeMinimalState();
    state.getPlayer(1).position = {5, 7};
    state.getPlayer(1).stats = {6, 3, 3, 8};      // Runner, AG3
    state.getPlayer(1).skills.add(SkillName::SureHands);
    state.ball = BallState::carried({5, 7}, 1);
    state.homeTeam.passUsedThisTurn = false;
    state.homeTeam.turnNumber = 1;                 // no emergency

    Player& p2 = state.getPlayer(2);
    p2.id = 2;
    p2.teamSide = TeamSide::HOME;
    p2.state = PlayerState::STANDING;
    p2.position = {6, 7};                          // adjacent AND one ahead
    p2.stats = {4, 3, 2, 9};                       // Longbeard, AG2
    p2.movementRemaining = 4;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    bool offered = false;
    for (auto& m : macros) {
        if (m.type == MacroType::PASS_ACTION && m.targetId == 2) offered = true;
    }
    EXPECT_FALSE(offered)
        << "being one square ahead does not make a Longbeard a better pair of hands";
}

TEST(MacroActions, TheSamePairIsStillRefusedOnceItIsAThrow) {
    // Guards the other half: only the adjacent case was repriced. Step the very
    // same two players apart and it is a real throw again, and a throw off AG2
    // stays the turnover-in-waiting the range check was written to refuse.
    GameState state = makeMinimalState();
    state.getPlayer(1).position = {5, 7};
    state.getPlayer(1).stats = {4, 3, 2, 9};
    state.ball = BallState::carried({5, 7}, 1);
    state.homeTeam.passUsedThisTurn = false;
    state.homeTeam.turnNumber = 1;

    Player& p2 = state.getPlayer(2);
    p2.id = 2;
    p2.teamSide = TeamSide::HOME;
    p2.state = PlayerState::STANDING;
    p2.position = {12, 7};                         // seven squares out
    p2.stats = {6, 3, 3, 8};
    p2.movementRemaining = 6;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    bool toTheDistantOne = false;
    for (auto& m : macros) {
        if (m.type == MacroType::PASS_ACTION && m.targetId == 2) toTheDistantOne = true;
    }
    EXPECT_FALSE(toTheDistantOne)
        << "out of hand-off range the throw roll is real and AG2 still fails it";
}

TEST(MacroActions, ASlayerIsOfferedTheBlockDauntlessWouldEqualise) {
    // ST3 with Dauntless beside a ST4 Black Orc. Priced raw this is uphill, the
    // dice count comes out negative and the offer never reaches the search --
    // for a block that resolves at equal strength on a 2+, i.e. 83% of the time.
    GameState state = makeMinimalState();
    Player& slayer = state.getPlayer(1);
    slayer.position = {10, 7};
    slayer.stats = {5, 3, 2, 8};                   // Troll Slayer, ST3
    slayer.skills.add(SkillName::Block);
    slayer.skills.add(SkillName::Dauntless);

    Player& orc = state.getPlayer(12);
    orc.position = {11, 7};
    orc.stats = {4, 4, 2, 9};                      // Black Orc, ST4
    orc.state = PlayerState::STANDING;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros, /*dauntlessInOffer=*/true);

    bool offered = false;
    for (auto& m : macros) {
        if (m.type == MacroType::BLOCK && m.playerId == 1 && m.targetId == 12) offered = true;
    }
    EXPECT_TRUE(offered) << "Dauntless equalises ST3 onto ST4 before assists";
}

TEST(MacroActions, WithoutDauntlessTheSameUphillBlockStaysOut) {
    // Guard on the other side: the offer must still refuse a genuinely uphill
    // block. Same two players, minus the skill.
    GameState state = makeMinimalState();
    Player& blocker = state.getPlayer(1);
    blocker.position = {10, 7};
    blocker.stats = {5, 3, 2, 8};
    blocker.skills.add(SkillName::Block);

    Player& orc = state.getPlayer(12);
    orc.position = {11, 7};
    orc.stats = {4, 4, 2, 9};
    orc.state = PlayerState::STANDING;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros, /*dauntlessInOffer=*/true);

    bool offered = false;
    for (auto& m : macros) {
        if (m.type == MacroType::BLOCK && m.playerId == 1 && m.targetId == 12) offered = true;
    }
    EXPECT_FALSE(offered) << "ST3 into ST4 with no way to equalise is the defender's block";
}

TEST(MacroActions, BranchingFactorReasonable) {
    // Full game state should produce ~10-25 macros, not ~200
    GameState state;
    setupHalf(state, getHumanRoster(), getOrcRoster());
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 1;
    state.homeTeam.rerolls = 3;
    state.awayTeam.rerolls = 3;
    state.weather = Weather::NICE;
    state.ball = BallState::onGround({13, 7});

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // Should be far fewer than ~200 low-level actions
    EXPECT_GT(macros.size(), 1u);
    EXPECT_LT(macros.size(), 50u);

    // Compare against low-level actions
    std::vector<Action> actions;
    getAvailableActions(state, actions);

    // Macros should be significantly fewer
    EXPECT_LT(macros.size(), actions.size());
}

// =============================================================
// Macro Expansion Tests
// =============================================================

TEST(MacroExpansion, EndTurnProducesOneAction) {
    GameState state = makeMinimalState();
    DiceRoller dice(42);

    Macro macro{MacroType::END_TURN, -1, -1, {-1, -1}};
    auto result = greedyExpandMacro(state, macro, dice);

    EXPECT_EQ(result.actions.size(), 1u);
    EXPECT_EQ(result.actions[0].type, ActionType::END_TURN);
    EXPECT_FALSE(result.turnover);
}

TEST(MacroExpansion, ScoreProducesMoveActions) {
    GameState state = makeScoringState();
    DiceRoller dice(42);

    Macro macro{MacroType::SCORE, 1, -1, {-1, -1}};
    auto result = greedyExpandMacro(state, macro, dice);

    EXPECT_GE(result.actions.size(), 1u);
    for (auto& a : result.actions) {
        EXPECT_EQ(a.type, ActionType::MOVE);
        EXPECT_EQ(a.playerId, 1);
    }
}

TEST(MacroExpansion, AdvanceProducesMoveActions) {
    GameState state = makeAdvanceState();
    DiceRoller dice(42);

    Macro macro{MacroType::ADVANCE, 1, -1, {-1, -1}};
    auto result = greedyExpandMacro(state, macro, dice);

    EXPECT_GE(result.actions.size(), 1u);
    for (auto& a : result.actions) {
        EXPECT_EQ(a.type, ActionType::MOVE);
        EXPECT_EQ(a.playerId, 1);
    }
}

// Carrier at x=5 with MA6 on turn 6: 20 squares from the endzone with 3 turns
// left wants 7 steps, but the stall throttle caps it at half the remaining
// movement (3). The throttle keeps movement in reserve so teammates can cage
// the carrier next turn -- which is only worth anything if the carrier is
// still standing there next turn.
GameState makeThrottledCarrierState() {
    GameState state = makeAdvanceState();
    state.homeTeam.turnNumber = 6;
    return state;
}

TEST(MacroExpansion, AdvanceThrottlesCarrierWhenNoBlitzThreat) {
    GameState state = makeThrottledCarrierState();
    DiceRoller dice(42);

    // Nearest opponent is at {20,7}: 15 squares away, far out of blitz range.
    Macro macro{MacroType::ADVANCE, 1, -1, {-1, -1}};
    auto result = greedyExpandMacro(state, macro, dice);

    EXPECT_EQ(result.actions.size(), 3u);
    EXPECT_EQ(state.getPlayer(1).position.x, 8);
    EXPECT_EQ(state.getPlayer(1).movementRemaining, 3);
}

// Blocker on the sideline, blitzer pushing from one square in: every push
// square is off-pitch, so any push/POW result crowd-surfs the blocker and
// leaves its position at the {-1,-1} sentinel. Pins the surf outcome: no
// follow-up MOVE may chase the sentinel (today doubly prevented -- the
// blitzer's hasActed plus the loop's isOnPitch guard; this test can't tell
// the two apart, so it is a tripwire for the pair, not proof of the guard
// alone) and the carrier's Step-2 scoring run must still happen.
TEST(MacroExpansion, BlitzAndScoreStopsFollowUpWhenBlockerSurfedOffPitch) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 1;
    state.homeTeam.rerolls = 0;
    state.awayTeam.rerolls = 0;
    state.weather = Weather::NICE;

    Player& carrier = state.getPlayer(1);
    carrier.id = 1;
    carrier.teamSide = TeamSide::HOME;
    carrier.state = PlayerState::STANDING;
    carrier.position = {21, 7};
    carrier.stats = {6, 3, 3, 8};
    carrier.movementRemaining = 6;

    Player& blitzer = state.getPlayer(2);
    blitzer.id = 2;
    blitzer.teamSide = TeamSide::HOME;
    blitzer.state = PlayerState::STANDING;
    blitzer.position = {20, 1};
    blitzer.stats = {6, 6, 3, 8};
    blitzer.movementRemaining = 6;

    Player& blocker = state.getPlayer(12);
    blocker.id = 12;
    blocker.teamSide = TeamSide::AWAY;
    blocker.state = PlayerState::STANDING;
    blocker.position = {20, 0};
    blocker.stats = {6, 1, 3, 7};
    blocker.movementRemaining = 6;

    state.ball = BallState::carried({21, 7}, 1);

    DiceRoller dice(42);
    Macro macro{MacroType::BLITZ_AND_SCORE, 1, 12, {-1, -1}};
    auto result = greedyExpandMacro(state, macro, dice);

    // Precondition, not the assertion under test: ST6 vs ST1 is a 3-die
    // attacker-chooses block and all three push squares are off-pitch, so the
    // blocker must end up surfed. If a dice-seed change ever breaks this,
    // re-pick the seed rather than weakening the assertions below.
    ASSERT_FALSE(state.getPlayer(12).isOnPitch());

    // No follow-up MOVE may chase the {-1,-1} sentinel.
    for (auto& a : result.actions) {
        if (a.type == ActionType::MOVE && a.playerId == 2) {
            ADD_FAILURE() << "blitzer chased the off-pitch sentinel to ("
                          << int(a.target.x) << "," << int(a.target.y) << ")";
        }
    }

    // break, not return: the carrier's Step-2 scoring run must still happen.
    int carrierMoves = 0;
    for (auto& a : result.actions) {
        if (a.type == ActionType::MOVE && a.playerId == 1) carrierMoves++;
    }
    EXPECT_GT(carrierMoves, 0);
}

// Item 14: expandBlitz historically picked the blitzer with the most dice
// and shortest raw distance, with zero regard for the mover's agility/Dodge
// or how many enemy tackle zones the approach crosses -- root-caused item
// 10's S2 test state's 82% BLITZ turnover rate (a low-agility, no-Dodge
// player sent through a crowded midfield purely because it had good block
// dice). This fixture reproduces that shape: blitzer 1 has better dice and
// is "closer" by raw distance, but must dodge through 3 tackle zones with
// AG2/no-Dodge to get there; blitzer 2 is already adjacent with worse dice
// but zero approach risk. The old diceCount*10-dist formula picks blitzer 1
// (20-6=14 beats 10-1=9); the fixed fail-probability estimate must pick
// blitzer 2 instead (~96% combined fail vs ~33%).
TEST(MacroExpansion, BlitzPrefersSaferBlitzerOverRiskyApproach) {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 1;
    state.homeTeam.rerolls = 0;
    state.awayTeam.rerolls = 0;
    state.weather = Weather::NICE;

    // Risky blitzer: good dice (ST5 + Block vs ST3 -> 2 dice, attacker
    // chooses, ~2.8% block-fail), but AG2/no-Dodge and 6 squares from the
    // target -- straight-line path crosses 3 enemy tackle zones.
    Player& risky = state.getPlayer(1);
    risky.id = 1;
    risky.teamSide = TeamSide::HOME;
    risky.state = PlayerState::STANDING;
    risky.position = {12, 7};
    risky.stats = {6, 5, 2, 8};
    risky.movementRemaining = 6;
    risky.skills.add(SkillName::Block);

    // Safe blitzer: already adjacent to the target (zero approach risk),
    // but even dice (ST3 vs ST3 -> 1 die, attacker chooses, ~33% block-fail)
    // and no Block skill.
    Player& safe = state.getPlayer(2);
    safe.id = 2;
    safe.teamSide = TeamSide::HOME;
    safe.state = PlayerState::STANDING;
    safe.position = {19, 7};
    safe.stats = {6, 3, 3, 8};
    safe.movementRemaining = 6;

    Player& target = state.getPlayer(12);
    target.id = 12;
    target.teamSide = TeamSide::AWAY;
    target.state = PlayerState::STANDING;
    target.position = {18, 7};
    target.stats = {6, 3, 3, 8};
    target.movementRemaining = 6;

    // Tackle-zone wall across the risky blitzer's approach. Since the item7
    // review unified estimateApproachFailChance with the executor's
    // TZ-scored walk (pickApproachStep), a single row of guards no longer
    // makes the approach risky -- the walk (correctly) routes around it.
    // Guards on BOTH flanking rows (y=6 and y=8) leave no dodge-free route,
    // preserving this fixture's original intent: an AG2/no-Dodge blitzer
    // that must dodge repeatedly to arrive. All guards sit >=2 squares from
    // both blitzers and the target so they don't perturb block-dice assists,
    // only the approach.
    int guardIds[] = {13, 14, 15, 16, 17, 18};
    int guardXs[] = {14, 15, 16, 14, 15, 16};
    int guardYs[] = {6, 6, 6, 8, 8, 8};
    for (int i = 0; i < 6; ++i) {
        Player& g = state.getPlayer(guardIds[i]);
        g.id = guardIds[i];
        g.teamSide = TeamSide::AWAY;
        g.state = PlayerState::STANDING;
        g.position = {static_cast<int8_t>(guardXs[i]),
                      static_cast<int8_t>(guardYs[i])};
        g.stats = {6, 3, 3, 8};
        g.movementRemaining = 6;
    }

    state.ball = BallState::onGround({5, 7});

    DiceRoller dice(42);
    Macro macro{MacroType::BLITZ, -1, 12, {-1, -1}};
    auto result = greedyExpandMacro(state, macro, dice);

    ASSERT_FALSE(result.actions.empty());
    EXPECT_EQ(result.actions[0].type, ActionType::BLITZ);
    EXPECT_EQ(result.actions[0].playerId, 2)
        << "expandBlitz picked the risky no-Dodge blitzer despite a safe "
           "already-adjacent alternative";
}

TEST(MacroExpansion, AdvanceSprintsWhenCarrierIsBlitzable) {
    GameState state = makeThrottledCarrierState();
    DiceRoller dice(42);

    // Standing opponent 5 squares from the carrier, inside its MA6 blitz
    // range, but off the carrier's movement lane (y=7) so it neither blocks
    // the path nor puts the carrier in a tacklezone.
    Player& threat = state.getPlayer(13);
    threat.id = 13;
    threat.teamSide = TeamSide::AWAY;
    threat.state = PlayerState::STANDING;
    threat.position = {6, 12};
    threat.stats = {6, 3, 3, 8};
    threat.movementRemaining = 6;

    Macro macro{MacroType::ADVANCE, 1, -1, {-1, -1}};
    auto result = greedyExpandMacro(state, macro, dice);

    // Reserving movement buys nothing once the carrier can already be blitzed,
    // so it spends the full budget instead of the throttled 3 steps.
    EXPECT_EQ(result.actions.size(), 6u);
    EXPECT_EQ(state.getPlayer(1).position.x, 11);
    EXPECT_EQ(state.getPlayer(1).movementRemaining, 0);
}

TEST(MacroExpansion, BlockProducesBlockAction) {
    GameState state = makeMinimalState();
    state.getPlayer(1).stats.strength = 4;
    state.getPlayer(1).position = {10, 7};
    state.getPlayer(12).stats.strength = 3;
    state.getPlayer(12).position = {11, 7};

    DiceRoller dice(42);
    Macro macro{MacroType::BLOCK, 1, 12, {-1, -1}};
    auto result = greedyExpandMacro(state, macro, dice);

    EXPECT_EQ(result.actions.size(), 1u);
    EXPECT_EQ(result.actions[0].type, ActionType::BLOCK);
    EXPECT_EQ(result.actions[0].playerId, 1);
    EXPECT_EQ(result.actions[0].targetId, 12);
}

TEST(MacroExpansion, PickupMoveTowardBall) {
    GameState state = makeMinimalState();
    state.ball = BallState::onGround({12, 7});

    DiceRoller dice(42);
    Macro macro{MacroType::PICKUP, 1, -1, {12, 7}};
    auto result = greedyExpandMacro(state, macro, dice);

    EXPECT_GE(result.actions.size(), 1u);
    // All actions should be MOVE for player 1
    for (auto& a : result.actions) {
        EXPECT_EQ(a.type, ActionType::MOVE);
        EXPECT_EQ(a.playerId, 1);
    }
}

// Regression for the REPOSITION step cap (research_fable_20260709 section
// 3b, same bug class as the PICKUP step cap fix 2899cd5): candidate
// generation hands out REPOSITION targets with no reach check, so the
// expansion must walk the player's real movement budget, not a fixed 4.
// NEGATIVE CONTROL: pre-patch expandReposition hard-caps the walk at 4
// steps, so this MA8 player stops at x=14 and the last three EXPECTs
// below fail (actions.size()==4, position=={14,7}).
TEST(MacroExpansion, RepositionWalksFullMovementBudget) {
    GameState state = makeMinimalState();
    Player& p1 = state.getPlayer(1);
    p1.position = {10, 7};
    p1.stats = {8, 3, 3, 8};
    p1.movementRemaining = 8;
    // Ball held by the far-away opponent so no PICKUP/loose-ball logic runs.
    state.getPlayer(12).position = {22, 11};
    state.ball = BallState::carried({22, 11}, 12);

    DiceRoller dice(42);
    // Screen spot 7 squares away across an open lane (no TZ, no GFI needed).
    Macro macro{MacroType::REPOSITION, 1, -1, {17, 7}};
    auto result = greedyExpandMacro(state, macro, dice);

    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(result.actions.size(), 7u);
    EXPECT_EQ(state.getPlayer(1).position.x, 17);
    EXPECT_EQ(state.getPlayer(1).position.y, 7);
}

TEST(MacroExpansion, FoulProducesFoulAction) {
    GameState state = makeMinimalState();
    state.getPlayer(12).state = PlayerState::PRONE;
    state.getPlayer(12).position = {11, 7};
    state.homeTeam.foulUsedThisTurn = false;

    DiceRoller dice(42);
    Macro macro{MacroType::FOUL, 1, 12, {-1, -1}};
    auto result = greedyExpandMacro(state, macro, dice);

    EXPECT_EQ(result.actions.size(), 1u);
    EXPECT_EQ(result.actions[0].type, ActionType::FOUL);
}

// =============================================================
// Macro Feature Extraction Tests
// =============================================================

TEST(MacroFeatures, EndTurnOneHot) {
    GameState state = makeMinimalState();
    float feats[NUM_ACTION_FEATURES];
    Macro macro{MacroType::END_TURN, -1, -1, {-1, -1}};
    extractMacroFeatures(state, macro, feats);

    EXPECT_FLOAT_EQ(feats[9], 1.0f);  // END_TURN = index 9
    EXPECT_FLOAT_EQ(feats[0], 0.0f);  // SCORE = index 0
}

TEST(MacroFeatures, ScoreOneHotAndScoringPotential) {
    GameState state = makeScoringState();
    float feats[NUM_ACTION_FEATURES];
    Macro macro{MacroType::SCORE, 1, -1, {-1, -1}};
    extractMacroFeatures(state, macro, feats);

    EXPECT_FLOAT_EQ(feats[0], 1.0f);   // SCORE = index 0
    EXPECT_FLOAT_EQ(feats[10], 1.0f);  // scoring_potential = 1 for SCORE
    EXPECT_FLOAT_EQ(feats[14], 1.0f);  // positional_gain = 1 for SCORE
}

TEST(MacroFeatures, BlockDiceQuality) {
    GameState state = makeMinimalState();
    state.getPlayer(1).stats.strength = 4;
    state.getPlayer(1).position = {10, 7};
    state.getPlayer(12).stats.strength = 3;
    state.getPlayer(12).position = {11, 7};

    float feats[NUM_ACTION_FEATURES];
    Macro macro{MacroType::BLOCK, 1, 12, {-1, -1}};
    extractMacroFeatures(state, macro, feats);

    EXPECT_FLOAT_EQ(feats[4], 1.0f);  // BLOCK = index 4
    EXPECT_NEAR(feats[11], 2.0f / 3.0f, 0.01f);  // 2 dice / 3
}

TEST(MacroFeatures, RiskLevel) {
    GameState state = makeMinimalState();
    float feats[NUM_ACTION_FEATURES];

    // END_TURN: no risk
    Macro endTurn{MacroType::END_TURN, -1, -1, {-1, -1}};
    extractMacroFeatures(state, endTurn, feats);
    EXPECT_FLOAT_EQ(feats[13], 0.0f);

    // BLOCK: low risk
    Macro block{MacroType::BLOCK, 1, 12, {-1, -1}};
    extractMacroFeatures(state, block, feats);
    EXPECT_GT(feats[13], 0.0f);
    EXPECT_LT(feats[13], 0.3f);
}

TEST(MacroFeatures, PlayerStrength) {
    GameState state = makeMinimalState();
    state.getPlayer(1).stats.strength = 4;

    float feats[NUM_ACTION_FEATURES];
    Macro macro{MacroType::REPOSITION, 1, -1, {15, 7}};
    extractMacroFeatures(state, macro, feats);

    EXPECT_NEAR(feats[12], 4.0f / 7.0f, 0.01f);
}

TEST(MacroFeatures, AllOneHotExclusive) {
    GameState state = makeMinimalState();
    float feats[NUM_ACTION_FEATURES];

    for (int i = 0; i < static_cast<int>(MacroType::MACRO_COUNT); ++i) {
        MacroType type = static_cast<MacroType>(i);
        Macro macro{type, 1, -1, {-1, -1}};
        extractMacroFeatures(state, macro, feats);

        // Exactly one of feats[0..9] should be 1.0
        int oneHotCount = 0;
        for (int j = 0; j < 10; ++j) {
            if (feats[j] > 0.5f) oneHotCount++;
        }
        EXPECT_EQ(oneHotCount, 1) << "MacroType " << i << " has non-exclusive one-hot";
    }
}

TEST(MacroFeatures, FeatureCountMatchesActionFeatures) {
    // Verify macro features output size matches NUM_ACTION_FEATURES.
    // This ensures policy network compatibility. NUM_ACTION_FEATURES was
    // deliberately expanded 15->23 (2026-06-19 Phase 1, see
    // project_neural_policy_rootcause memory) to fix ~50% best-action
    // collisions from the original coarse featurization -- this assertion
    // was stale from before that change.
    GameState state = makeMinimalState();
    float feats[NUM_ACTION_FEATURES];
    Macro macro{MacroType::END_TURN, -1, -1, {-1, -1}};
    extractMacroFeatures(state, macro, feats);

    // If we got here without segfault, the size is correct
    EXPECT_EQ(NUM_ACTION_FEATURES, 23);
}

// =============================================================
// Vrstva 4: Defensive Strategy Tests
// =============================================================

// Helper: create a defensive state (opponent has ball)
GameState makeDefensiveState() {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 3;
    state.homeTeam.rerolls = 3;
    state.awayTeam.rerolls = 3;
    state.weather = Weather::NICE;

    // Home player 1 (free, fast)
    Player& p1 = state.getPlayer(1);
    p1.id = 1;
    p1.teamSide = TeamSide::HOME;
    p1.state = PlayerState::STANDING;
    p1.position = {5, 7};
    p1.stats = {7, 3, 3, 8};
    p1.movementRemaining = 7;
    p1.hasMoved = false;
    p1.hasActed = false;

    // Home player 2 (free)
    Player& p2 = state.getPlayer(2);
    p2.id = 2;
    p2.teamSide = TeamSide::HOME;
    p2.state = PlayerState::STANDING;
    p2.position = {6, 4};
    p2.stats = {6, 3, 3, 8};
    p2.movementRemaining = 6;
    p2.hasMoved = false;
    p2.hasActed = false;

    // Home player 3 (free)
    Player& p3 = state.getPlayer(3);
    p3.id = 3;
    p3.teamSide = TeamSide::HOME;
    p3.state = PlayerState::STANDING;
    p3.position = {4, 10};
    p3.stats = {6, 3, 3, 8};
    p3.movementRemaining = 6;
    p3.hasMoved = false;
    p3.hasActed = false;

    // Away player 12 — ball carrier at x=15
    Player& p12 = state.getPlayer(12);
    p12.id = 12;
    p12.teamSide = TeamSide::AWAY;
    p12.state = PlayerState::STANDING;
    p12.position = {15, 7};
    p12.stats = {6, 3, 3, 8};
    p12.movementRemaining = 6;

    // Ball held by away player 12
    state.ball = BallState::carried({15, 7}, 12);

    return state;
}

TEST(MacroActions, BlitzDefensePrioritizesCarrier) {
    GameState state = makeDefensiveState();
    state.homeTeam.blitzUsedThisTurn = false;

    // Add another away player far away
    Player& p13 = state.getPlayer(13);
    p13.id = 13;
    p13.teamSide = TeamSide::AWAY;
    p13.state = PlayerState::STANDING;
    p13.position = {20, 10};
    p13.stats = {6, 3, 3, 8};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // First BLITZ macro should target the ball carrier (player 12)
    ASSERT_TRUE(hasMacroType(macros, MacroType::BLITZ));
    for (auto& m : macros) {
        if (m.type == MacroType::BLITZ) {
            EXPECT_EQ(m.targetId, 12) << "First BLITZ should target ball carrier";
            break;
        }
    }
}

TEST(MacroActions, BlitzDefenseMultipleTargets) {
    GameState state = makeDefensiveState();
    state.homeTeam.blitzUsedThisTurn = false;

    // Add second away player
    Player& p13 = state.getPlayer(13);
    p13.id = 13;
    p13.teamSide = TeamSide::AWAY;
    p13.state = PlayerState::STANDING;
    p13.position = {18, 5};
    p13.stats = {6, 3, 3, 8};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // On defense with multiple targets, should have 2 BLITZ macros
    int blitzCount = countMacroType(macros, MacroType::BLITZ);
    EXPECT_EQ(blitzCount, 2);
}

TEST(MacroActions, BlitzOffenseSingleTarget) {
    GameState state = makeMinimalState();
    // Offense: we have the ball
    state.ball = BallState::carried({10, 7}, 1);
    state.homeTeam.blitzUsedThisTurn = false;

    // Add second away player
    Player& p13 = state.getPlayer(13);
    p13.id = 13;
    p13.teamSide = TeamSide::AWAY;
    p13.state = PlayerState::STANDING;
    p13.position = {18, 5};
    p13.stats = {6, 3, 3, 8};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // On offense: only 1 BLITZ macro (best target)
    int blitzCount = countMacroType(macros, MacroType::BLITZ);
    EXPECT_EQ(blitzCount, 1);
}

TEST(MacroActions, RepositionDefenseMarksCarrier) {
    GameState state = makeDefensiveState();

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // One REPOSITION macro should target near the carrier position (marker)
    bool hasMarkerRepo = false;
    for (auto& m : macros) {
        if (m.type == MacroType::REPOSITION) {
            if (m.targetPos.distanceTo(state.getPlayer(12).position) <= 1) {
                hasMarkerRepo = true;
            }
        }
    }
    EXPECT_TRUE(hasMarkerRepo) << "Should have a REPOSITION targeting carrier (marker)";
}

TEST(MacroActions, RepositionDefenseSafetyPlayer) {
    GameState state = makeDefensiveState();

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // One REPOSITION should target near our endzone (safety)
    // Home endzone is at x=0
    bool hasSafety = false;
    for (auto& m : macros) {
        if (m.type == MacroType::REPOSITION && m.targetPos.x <= 2) {
            hasSafety = true;
        }
    }
    EXPECT_TRUE(hasSafety) << "Should have a safety REPOSITION near endzone";
}

TEST(MacroActions, RepositionDefenseEndzoneGuard) {
    GameState state = makeDefensiveState();

    // Add opponent with scoring threat (near our endzone, fast, uncontested)
    Player& p13 = state.getPlayer(13);
    p13.id = 13;
    p13.teamSide = TeamSide::AWAY;
    p13.state = PlayerState::STANDING;
    p13.position = {4, 3};  // near home endzone (x=0)
    p13.stats = {7, 3, 3, 8};  // MA 7, can score (dist=4, MA+2=9)
    p13.movementRemaining = 7;

    // Add more home players for endzone guard assignment
    Player& p4 = state.getPlayer(4);
    p4.id = 4;
    p4.teamSide = TeamSide::HOME;
    p4.state = PlayerState::STANDING;
    p4.position = {3, 12};
    p4.stats = {6, 3, 3, 8};
    p4.movementRemaining = 6;
    p4.hasMoved = false;
    p4.hasActed = false;

    Player& p5 = state.getPlayer(5);
    p5.id = 5;
    p5.teamSide = TeamSide::HOME;
    p5.state = PlayerState::STANDING;
    p5.position = {2, 2};
    p5.stats = {6, 3, 3, 8};
    p5.movementRemaining = 6;
    p5.hasMoved = false;
    p5.hasActed = false;

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // Should have endzone guard REPOSITION(s) targeting x=4 area (4 sq from EZ)
    bool hasGuard = false;
    for (auto& m : macros) {
        if (m.type == MacroType::REPOSITION && m.targetPos.x >= 3 && m.targetPos.x <= 5 &&
            (m.targetPos.y == 5 || m.targetPos.y == 9)) {
            hasGuard = true;
        }
    }
    EXPECT_TRUE(hasGuard) << "Should have endzone guard REPOSITION";
}

TEST(MacroExpansion, BlitzSelectsBestBlitzer) {
    GameState state = makeMinimalState();
    // Two home players that can blitz the target
    state.getPlayer(1).position = {10, 7};
    state.getPlayer(1).stats.strength = 3;
    state.getPlayer(1).movementRemaining = 6;

    Player& p2 = state.getPlayer(2);
    p2.id = 2;
    p2.teamSide = TeamSide::HOME;
    p2.state = PlayerState::STANDING;
    p2.position = {12, 7};  // closer to target
    p2.stats = {6, 4, 3, 8}; // ST4 = better dice
    p2.movementRemaining = 6;
    p2.hasMoved = false;
    p2.hasActed = false;

    state.getPlayer(12).position = {14, 7};
    state.getPlayer(12).stats.strength = 3;
    state.homeTeam.blitzUsedThisTurn = false;
    state.ball = BallState::onGround({20, 7});

    DiceRoller dice(42);
    Macro macro{MacroType::BLITZ, -1, 12, {-1, -1}};
    auto result = greedyExpandMacro(state, macro, dice);

    ASSERT_GE(result.actions.size(), 1u);
    EXPECT_EQ(result.actions[0].type, ActionType::BLITZ);
    // Should select player 2 (ST4 = 2-dice, closer)
    EXPECT_EQ(result.actions[0].playerId, 2);
}

TEST(MacroActions, DefensiveScreenEvenSpread) {
    GameState state = makeDefensiveState();

    // Add many free home players (no adjacent enemies)
    for (int i = 4; i <= 8; ++i) {
        Player& p = state.getPlayer(i);
        p.id = i;
        p.teamSide = TeamSide::HOME;
        p.state = PlayerState::STANDING;
        p.position = {static_cast<int8_t>(2 + i), static_cast<int8_t>(2)};
        p.stats = {5, 3, 3, 8};  // slow (MA5, won't get safety)
        p.movementRemaining = 5;
        p.hasMoved = false;
        p.hasActed = false;
    }

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // Collect screen REPOSITION Y targets (exclude safety and marker)
    std::set<int> screenYs;
    for (auto& m : macros) {
        if (m.type != MacroType::REPOSITION) continue;
        // Exclude safety (near endzone) and marker (near carrier)
        if (m.targetPos.x <= 2) continue;  // safety
        if (m.targetPos.distanceTo(state.getPlayer(12).position) <= 1) continue;  // marker
        screenYs.insert(m.targetPos.y);
    }

    // Screen should have diverse Y values (not all the same)
    EXPECT_GE(screenYs.size(), 3u) << "Screen should spread across Y values";
}

// =============================================================
// Dodge-around Scoring Tests
// =============================================================

TEST(MacroActions, ScoreAvoidsEnemyTZ) {
    // Carrier at y=7, enemies blocking y=7 path to endzone
    // expandScore should route around (y=5 or y=9)
    GameState state = makeMinimalState();
    Player& carrier = state.getPlayer(1);
    carrier.position = {22, 7};
    carrier.movementRemaining = 6;
    state.ball = BallState::carried({22, 7}, 1);

    // Enemies blocking direct path at y=7
    Player& e1 = state.getPlayer(12);
    e1.position = {23, 7};
    e1.state = PlayerState::STANDING;

    Player& e2 = state.getPlayer(13);
    e2.id = 13;
    e2.teamSide = TeamSide::AWAY;
    e2.state = PlayerState::STANDING;
    e2.position = {24, 7};
    e2.stats = {6, 3, 3, 8};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);
    ASSERT_TRUE(hasMacroType(macros, MacroType::SCORE));

    // Expand and verify carrier moves toward endzone (should route around)
    Macro scoreMacro;
    for (auto& m : macros) {
        if (m.type == MacroType::SCORE) { scoreMacro = m; break; }
    }

    FixedDiceRoller dice({6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6});
    GameState sim = state;
    auto result = greedyExpandMacro(sim, scoreMacro, dice);

    // Should have moved (not empty expansion)
    EXPECT_FALSE(result.actions.empty());
}

TEST(MacroExpansion, AdvanceTargetPulledBackFromEnemyTZ) {
    // Cage technical review 2026-08-06, finding 1: ADVANCE picks its target
    // arithmetically, and since the walk's final square is TZ-exempt
    // (2026-08-04) the carrier walked right up to a defender and ended the
    // turn in his tackle zone -- a free block on the ball next turn. The
    // target must pull back to the nearest unoccupied TZ-free square.
    GameState state = makeMinimalState();
    Player& carrier = state.getPlayer(1);
    carrier.position = {10, 7};
    carrier.movementRemaining = 6;
    state.ball = BallState::carried({10, 7}, 1);
    state.homeTeam.turnNumber = 6;  // 3 turns left, dist 15 -> wants 5 steps

    // Defender straight ahead: arithmetic target {15,7} is in his TZ,
    // {14,7} is his own square, {13,7} is in TZ again -> stop at {12,7}.
    state.getPlayer(12).position = {14, 7};

    DiceRoller dice(42);
    Macro macro{MacroType::ADVANCE, 1, -1, {-1, -1}};
    greedyExpandMacro(state, macro, dice);

    const Player& after = state.getPlayer(1);
    EXPECT_EQ(after.position.x, 12);
    EXPECT_EQ(after.position.y, 7);
    EXPECT_EQ(countTacklezones(state, after.position, TeamSide::HOME), 0);
}

TEST(MacroExpansion, AdvanceWalksStraightAtEqualChebyshev) {
    // Manhattan tiebreak in scoreMoveAction (2026-08-04): at equal Chebyshev
    // distance the walk must take the straight square, not a diagonal drift
    // that wastes walk budget on exact-reach legs. Open field ahead: every
    // step of the ADVANCE walk stays on the carrier's row.
    GameState state = makeMinimalState();
    Player& carrier = state.getPlayer(1);
    carrier.position = {10, 7};
    carrier.movementRemaining = 6;
    state.ball = BallState::carried({10, 7}, 1);
    // Default enemy at {20,7} is out of blitz range: throttle caps steps at
    // min(ideal 2, MA/2) = 2 -> target {12,7}.

    DiceRoller dice(42);
    Macro macro{MacroType::ADVANCE, 1, -1, {-1, -1}};
    auto result = greedyExpandMacro(state, macro, dice);

    ASSERT_EQ(result.actions.size(), 2u);
    for (auto& a : result.actions) {
        EXPECT_EQ(a.type, ActionType::MOVE);
        EXPECT_EQ(a.target.y, 7);
    }
    EXPECT_EQ(state.getPlayer(1).position.x, 12);
}

TEST(MacroActions, ScoreEntersDefendedEndzone) {
    // Cage technical review 2026-08-06, finding 3 (positive side effect of
    // the 2026-08-04 final-square TZ exemption, previously unprotected):
    // a carrier must be willing to step INTO a defended endzone and score.
    // Before the exemption the endzone square's TZ penalty made stopping one
    // square short score better forever (walk abort via loop guard).
    GameState state = makeMinimalState();
    Player& carrier = state.getPlayer(1);
    carrier.position = {23, 7};
    carrier.movementRemaining = 6;
    state.ball = BallState::carried({23, 7}, 1);

    // Two defenders whose tackle zones cover every endzone square the SCORE
    // route probe considers (y 5..9).
    state.getPlayer(12).position = {24, 6};
    Player& e2 = state.getPlayer(13);
    e2.id = 13;
    e2.teamSide = TeamSide::AWAY;
    e2.state = PlayerState::STANDING;
    e2.position = {24, 8};
    e2.stats = {6, 3, 3, 8};

    FixedDiceRoller dice({6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6});
    Macro macro{MacroType::SCORE, 1, -1, {-1, -1}};
    greedyExpandMacro(state, macro, dice);

    EXPECT_EQ(state.getPlayer(1).position.x, 25);
}

TEST(MacroActions, ScoreOffPitchCarrierDoesNotHang) {
    // Regression test for a real ~100-200s stall (confirmed via live gdb
    // repro) found in the 2026-07-08 gate run: replayToNode replays a cached
    // SCORE macro open-loop (fresh dice each MCTS iteration), so the player
    // referenced by macro.playerId can have been KO'd/crowd-surfed off pitch
    // by an *earlier* macro in that same replay by the time this SCORE macro
    // is expanded, even though the macro was only ever generated for an
    // on-pitch carrier. expandScore read carrier.position without checking
    // carrier.isOnPitch() first; for an AWAY carrier with the {-1,-1}
    // off-pitch sentinel position, the TZ-probe walk's cx decremented away
    // from targetX=0 and only terminated via signed-integer-overflow
    // wraparound (~4 billion iterations). This test constructs exactly that
    // state -- an AWAY carrier flagged as the ball holder but KO'd and off
    // pitch -- and asserts expansion returns immediately (empty result, no
    // stall) instead of spinning.
    GameState state = makeMinimalState();
    Player& carrier = state.getPlayer(12);  // AWAY player (id 12 from makeMinimalState)
    carrier.teamSide = TeamSide::AWAY;
    carrier.state = PlayerState::KO;
    carrier.position = {-1, -1};
    state.ball = BallState::carried({-1, -1}, 12);
    state.activeTeam = TeamSide::AWAY;

    Macro scoreMacro{MacroType::SCORE, 12, -1, {-1, -1}};
    FixedDiceRoller dice({6, 6, 6, 6, 6, 6, 6, 6, 6, 6});
    auto result = greedyExpandMacro(state, scoreMacro, dice);

    EXPECT_TRUE(result.actions.empty());
}


// Item 11: the loose-ball "surround it" REPOSITION historically targeted the
// ball's EXACT square -- movePlayerToward walked the player onto it and
// move_handler auto-triggered a real pickup roll (failure = turnover),
// silently breaking REPOSITION's dice-free contract and making a genuine
// "deny without grabbing" play impossible. The target must be a square
// ADJACENT to the ball, and a player already adjacent should stay put.
TEST(MacroActions, RepositionLooseBallTargetsAdjacentSquareNotBall) {
    GameState state = makeMinimalState();
    // Ball loose at (13,7); free home player 1 at (10,7); away player far.
    state.getPlayer(12).position = {20, 2};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    bool found = false;
    for (auto& m : macros) {
        if (m.type != MacroType::REPOSITION || m.playerId != 1) continue;
        found = true;
        EXPECT_NE(m.targetPos, state.ball.position)
            << "loose-ball REPOSITION still targets the ball's own square";
        EXPECT_EQ(m.targetPos.distanceTo(state.ball.position), 1);
    }
    EXPECT_TRUE(found) << "no REPOSITION macro generated for the free player";
}

TEST(MacroActions, RepositionLooseBallAlreadyAdjacentStaysPut) {
    GameState state = makeMinimalState();
    state.getPlayer(1).position = {12, 7};  // adjacent to the loose ball
    state.getPlayer(12).position = {20, 2};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    for (auto& m : macros) {
        if (m.type != MacroType::REPOSITION || m.playerId != 1) continue;
        EXPECT_EQ(m.targetPos, (Position{12, 7}))
            << "already-denying player sent on a pointless walk";
    }
}

// Item 11, waypoint variant: retargeting alone is not enough -- when the only
// free square adjacent to the ball lies on the far side, the step-by-step
// walk must not cross the ball's square en route (any step landing there
// triggers the auto-pickup). Corridor fixture: every square adjacent to the
// ball is occupied by friendly statues except (14,7); the mover approaches
// from the right, so the straight-line path runs (16,7)->(15,7)->(14,7)
// straight over the ball. The fixed walk must go around and leave the ball
// untouched on the ground.
TEST(MacroActions, RepositionLooseBallNeverStepsOntoBallSquare) {
    GameState state = makeMinimalState();
    state.ball = BallState::onGround({15, 7});
    state.getPlayer(12).position = {24, 1};  // away player far off

    Player& mover = state.getPlayer(1);
    mover.position = {17, 7};
    mover.stats = {8, 3, 3, 8};
    mover.movementRemaining = 8;

    // Friendly statues sealing off every ball-adjacent square except (14,7).
    int sx[] = {14, 15, 16, 14, 15, 16, 16};
    int sy[] = {6, 6, 6, 8, 8, 8, 7};
    for (int i = 0; i < 7; ++i) {
        Player& s = state.getPlayer(2 + i);
        s.id = 2 + i;
        s.teamSide = TeamSide::HOME;
        s.state = PlayerState::STANDING;
        s.position = {static_cast<int8_t>(sx[i]), static_cast<int8_t>(sy[i])};
        s.stats = {6, 3, 3, 8};
        s.movementRemaining = 6;
        s.hasMoved = true;  // occupancy only, not macro candidates
    }

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);
    const Macro* repo = nullptr;
    for (auto& m : macros) {
        if (m.type == MacroType::REPOSITION && m.playerId == 1) repo = &m;
    }
    ASSERT_NE(repo, nullptr);
    ASSERT_EQ(repo->targetPos, (Position{14, 7}));

    DiceRoller dice(42);
    Macro macro = *repo;
    greedyExpandMacro(state, macro, dice);

    // The ball must still be loose and untouched -- the old walk stepped on
    // (15,7) and rolled a real pickup (held ball or bounce+turnover).
    EXPECT_FALSE(state.ball.isHeld);
    EXPECT_EQ(state.ball.position, (Position{15, 7}));
    EXPECT_NE(state.getPlayer(1).position, (Position{15, 7}));
}

// --- Picker ranking by computed pickup chance (2026-08-07) ---

TEST(MacroActions, PickupTargetPricedAtBallSquare) {
    // Generation must price the roll where the pickup HAPPENS: tackle zones
    // on the ball's square, not on the picker's current one. The picker here
    // stands in the clear; the ball is marked by two opponents.
    GameState state = makeMinimalState();
    Player& picker = state.getPlayer(1);
    picker.position = {10, 7};
    picker.stats = {6, 3, 3, 8};

    state.getPlayer(12).position = {13, 6};
    Player& e2 = state.getPlayer(13);
    e2.id = 13;
    e2.teamSide = TeamSide::AWAY;
    e2.state = PlayerState::STANDING;
    e2.position = {13, 8};
    e2.stats = {6, 3, 3, 8};

    EXPECT_EQ(calculatePickupTarget(state, picker), 3);           // clear square
    EXPECT_EQ(calculatePickupTargetAt(state, picker, {13, 7}), 5); // +2 TZ
}

TEST(MacroActions, PickerRankedByComputedChanceNotAgilityProxy) {
    // The old proxy (AG*10 - dist*3 + flat skill bonuses) was blind to what
    // actually makes a pickup hard: tackle zones on the ball and weather.
    // Big Hand ignores both -- so under a marked ball in pouring rain the
    // Big Hand carrier is by far the most reliable recoverer even from
    // distance, while the proxy still nominated the nearer, higher-AG body
    // whose real chance had collapsed to 33%.
    GameState state = makeMinimalState();
    state.weather = Weather::POURING_RAIN;

    // p1: AG4, adjacent to the ball, no skills -> 6-4+2 TZ+1 rain = 5+ (33%).
    Player& near = state.getPlayer(1);
    near.position = {12, 7};
    near.stats = {6, 3, 4, 8};
    near.movementRemaining = 6;

    // p2: AG3 with Big Hand, six squares out -> 3+ regardless (67%).
    Player& handler = state.getPlayer(2);
    handler.id = 2;
    handler.teamSide = TeamSide::HOME;
    handler.state = PlayerState::STANDING;
    handler.position = {7, 7};
    handler.stats = {6, 3, 3, 8};
    handler.skills.add(SkillName::BigHand);
    handler.movementRemaining = 6;

    // Two markers on the ball at {13,7}.
    state.getPlayer(12).position = {13, 6};
    Player& e2 = state.getPlayer(13);
    e2.id = 13;
    e2.teamSide = TeamSide::AWAY;
    e2.state = PlayerState::STANDING;
    e2.position = {13, 8};
    e2.stats = {6, 3, 3, 8};

    std::vector<Macro> macros;
    getAvailableMacros(state, macros);

    // Macros are emitted best-first (prior-floor ordering contract).
    const Macro* firstPickup = nullptr;
    for (const auto& m : macros) {
        if (m.type == MacroType::PICKUP) { firstPickup = &m; break; }
    }
    ASSERT_NE(firstPickup, nullptr);
    EXPECT_EQ(firstPickup->playerId, 2) << "nominated the unreliable nearer body";
}

// 2026-08-17: P13 is production. This pins the default so it cannot revert in
// silence -- the arm passed its A/B (+3.59 pp against the matchups where it
// provably never fired), and a flag that quietly flips back would leave us
// measuring one engine and shipping another. If you are here because this test
// failed, the question is not "fix the test" but "why did production change".
TEST(MacroActions, DauntlessInOfferIsOnInProduction) {
    MCTSConfig cfg;
    EXPECT_TRUE(cfg.dauntlessInOffer)
        << "MCTSConfig::dauntlessInOffer is production since 2026-08-17; "
           "see evidence/weekend_result_20260817.md";
}
