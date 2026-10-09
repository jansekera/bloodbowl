#include <gtest/gtest.h>
#include "bb/cage_advance.h"
#include "bb/pathfinder.h"
#include "bb/macro_mcts.h"
#include "bb/turn_planner.h"
#include "bb/game_state.h"
#include "bb/action_resolver.h"
#include "bb/helpers.h"
#include <algorithm>
#include <vector>

using namespace bb;

namespace {

// Hand-built dwarf-flavored cage fixture (F1, 2026-08-03): HOME carrier at
// {12,7} (MA4 AG2, longbeard-ish), full 4-corner diagonal cage:
//   p2 {11,6} +Guard, p3 {11,8}, p4 {13,6} +Guard, p5 {13,8}.
// Lone AWAY player far at {24,13} keeps generation realistic without any
// tackle zone near the cage. Turn 1: turnsLeft=8, usable (after the
// mandatory 1-turn reserve) = 7, dist to endzone 13 -> requiredPace 13/7
// ~ 1.857 -> planned step 2 (schedule-driven, never outrun). The
// role-achievable raw step is 4: every corner translation at step 4 is a
// straight 4-square walk (= MA4, no GFI), carrier likewise.
GameState makeCageState() {
    GameState state;
    state.phase = GamePhase::PLAY;
    state.activeTeam = TeamSide::HOME;
    state.half = 1;
    state.homeTeam.turnNumber = 1;
    state.homeTeam.rerolls = 0;
    state.awayTeam.rerolls = 0;
    state.weather = Weather::NICE;

    auto mk = [&](int id, TeamSide side, Position pos, int8_t ma = 4,
                  std::vector<SkillName> skills = {}) {
        Player& p = state.getPlayer(id);
        p.id = id;
        p.teamSide = side;
        p.state = PlayerState::STANDING;
        p.position = pos;
        p.stats = {ma, 3, 2, 9};
        p.movementRemaining = ma;
        p.hasMoved = false;
        p.hasActed = false;
        for (auto s : skills) p.skills.add(s);
    };
    mk(1, TeamSide::HOME, {12, 7});
    mk(2, TeamSide::HOME, {11, 6}, 4, {SkillName::Guard});
    mk(3, TeamSide::HOME, {11, 8});
    mk(4, TeamSide::HOME, {13, 6}, 4, {SkillName::Guard});
    mk(5, TeamSide::HOME, {13, 8});
    mk(12, TeamSide::AWAY, {24, 13});
    state.ball = BallState::carried({12, 7}, 1);
    return state;
}

MCTSConfig cageConfig(bool enable = true) {
    MCTSConfig cfg;
    cfg.maxIterations = 60;
    cfg.timeBudgetMs = 0;
    return cfg;
}

bool hasMacroFor(const CageAdvancePlan& plan, int playerId) {
    return std::any_of(plan.macros.begin(), plan.macros.end(),
                       [&](const Macro& m) { return m.playerId == playerId; });
}

bool actionIsAvailable(const GameState& state, const Action& a) {
    std::vector<Action> available;
    getAvailableActions(state, available);
    for (auto& av : available) {
        if (av.type == a.type && av.playerId == a.playerId &&
            av.targetId == a.targetId && av.target == a.target) {
            return true;
        }
    }
    return false;
}

} // anonymous namespace

// =============================================================
// Gate + trigger
// =============================================================

TEST(CageAdvance, NotApplicableWhenTooFewBodiesForACage) {
    GameState state = makeCageState();
    // One teammate can never make a >= 2-corner cage at any destination:
    // a formation problem -> NOT_APPLICABLE (not a tempo verdict).
    state.getPlayer(3).state = PlayerState::OFF_PITCH;
    state.getPlayer(4).state = PlayerState::OFF_PITCH;
    state.getPlayer(5).state = PlayerState::OFF_PITCH;
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    EXPECT_EQ(plan.verdict, CageAdvanceVerdict::NOT_APPLICABLE);
    EXPECT_FALSE(plan.valid);
}

TEST(CageAdvance, CageIsBuiltFromScratchAtCarrierDestination) {
    // User standard 2026-08-04: "build a proper cage, ALWAYS" -- zero
    // corners currently built, but four teammates loiter within reach of
    // the destination slots. The plan drafts them into a fresh cage around
    // the carrier's TARGET square (never around his starting square).
    GameState state = makeCageState();
    state.getPlayer(2).position = {10, 5};
    state.getPlayer(3).position = {10, 9};
    state.getPlayer(4).position = {12, 4};
    state.getPlayer(5).position = {12, 10};
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    ASSERT_TRUE(plan.valid) << "verdict=" << static_cast<int>(plan.verdict);
    EXPECT_EQ(plan.builtCorners, 0);
    EXPECT_GE(plan.filledCorners, 2);
    // Carrier still advances -- the cage forms at the destination.
    EXPECT_EQ(plan.macros.back().playerId, 1);
    EXPECT_EQ(plan.macros.back().targetPos.x, 12 + plan.step);
}

TEST(CageAdvance, NotApplicableOnLooseBall) {
    GameState state = makeCageState();
    state.ball = BallState::onGround({13, 7});
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    EXPECT_EQ(plan.verdict, CageAdvanceVerdict::NOT_APPLICABLE);
}

// =============================================================
// Tempo: computed, never a constant (binding constraint 1)
// =============================================================

TEST(CageAdvance, TempoIsComputedFromDistanceAndSchedule) {
    // P174 (08.10.2026): plánovač napřed hledá bezpečné pole po celém dosahu nosiče; tenhle test
    // zkouší ZÁLOŽNÍ postup (krok po přímce podle rozvrhu), který nastupuje, když bezpečné pole
    // nevyjde — proto je hledání po dobu testu vypnuté.
    struct OldPath { OldPath() { setCageFeaturesOff(kFeatCleanCageSearch); } ~OldPath() { setCageFeaturesOff(0); } } oldPath;
    GameState state = makeCageState();
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    ASSERT_TRUE(plan.valid) << "verdict=" << static_cast<int>(plan.verdict);
    // dist 13, turnsLeft 8, mandatory reserve 1 -> usable 7. NOT 13/8: the
    // reserve turn is part of the tempo contract.
    EXPECT_NEAR(plan.requiredPace, 13.0 / 7.0, 1e-9);
    // Bank-while-clear (user doctrine 2026-08-04): the corridor is empty,
    // so the cage rolls at MAX dice-free pace (step 4 = everyone's MA4
    // straight walk), building schedule cushion for when resistance comes.
    EXPECT_EQ(plan.step, 4);
    EXPECT_EQ(plan.rawAchievableStep, 4);
    EXPECT_EQ(plan.resistance, 0);
    EXPECT_NEAR(plan.achievablePace, 4.0, 1e-9);
    // Schedule fits within plain MA -> the carrier leg must stay dice-free.
    EXPECT_EQ(plan.carrierGfi, 0);
    EXPECT_EQ(plan.macros.back().gfiAllowance, 0);
}

// Rewritten 2026-08-11. This used to assert that falling behind schedule
// abandons the plan outright, and that is precisely the behaviour being
// replaced: the abandoned turn went back to search(), which averages 1.73
// squares where this planner averages 5.00, and the user's standing
// instruction is the hierarchy advance -> fill -> never a solo run, with the
// stated preference "move more squares forward when it is possible".
// Behind schedule now means walk as far as is safe, never give up the turn.
TEST(CageAdvance, BehindScheduleStillAdvancesInsteadOfGivingUp) {
    GameState state = makeCageState();
    state.homeTeam.turnNumber = 6;  // turnsLeft 3, usable 2 -> required 6.5
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    ASSERT_TRUE(plan.valid) << "verdict=" << static_cast<int>(plan.verdict);
    EXPECT_FALSE(plan.macros.empty());
    EXPECT_GE(plan.step, 1) << "the schedule is unmeetable, the advance is not";
    EXPECT_EQ(plan.carrierGfi, 0) << "a hopeless schedule never buys dice";
}

// Likewise: out of usable turns is not a reason to hand the turn over. Either
// the cage still walks, or it at least closes up where it stands.
TEST(CageAdvance, NoUsableTurnsLeftStillProducesAPlan) {
    GameState state = makeCageState();
    state.homeTeam.turnNumber = 8;  // turnsLeft 1, reserve eats it -> usable 0
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    ASSERT_TRUE(plan.valid) << "verdict=" << static_cast<int>(plan.verdict);
    EXPECT_TRUE(plan.verdict == CageAdvanceVerdict::PLAN_READY ||
                plan.verdict == CageAdvanceVerdict::FILL_ONLY);
}

TEST(CageAdvance, OpponentScreenInCorridorKillsPaceButNotTheTurn) {
    // P174 (08.10.2026): plánovač napřed hledá bezpečné pole po celém dosahu nosiče; tenhle test
    // zkouší ZÁLOŽNÍ postup (krok po přímce podle rozvrhu), který nastupuje, když bezpečné pole
    // nevyjde — proto je hledání po dobu testu vypnuté.
    struct OldPath { OldPath() { setCageFeaturesOff(kFeatCleanCageSearch); } ~OldPath() { setCageFeaturesOff(0); } } oldPath;
    GameState state = makeCageState();
    // Three standing opponents dead ahead in the corridor (x 13..16,
    // |dy| <= 2): a real screen -> pace penalty 2 -> achievable 0.
    auto mkOpp = [&](int id, Position pos) {
        Player& p = state.getPlayer(id);
        p.id = id;
        p.teamSide = TeamSide::AWAY;
        p.state = PlayerState::STANDING;
        p.position = pos;
        p.stats = {6, 3, 3, 8};
        p.movementRemaining = 6;
    };
    mkOpp(13, {16, 6});
    mkOpp(14, {16, 7});
    mkOpp(15, {16, 8});
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    EXPECT_EQ(plan.resistance, 3);
    // A screen still crushes the PACE -- that reading is unchanged and is
    // what the resistance penalty is for. What it no longer does is end the
    // turn: the cage walks what it can, or closes up where it stands.
    EXPECT_LE(plan.achievablePace, 2.0);
    ASSERT_TRUE(plan.valid) << "verdict=" << static_cast<int>(plan.verdict);
    EXPECT_TRUE(plan.verdict == CageAdvanceVerdict::PLAN_READY ||
                plan.verdict == CageAdvanceVerdict::FILL_ONLY);
}

TEST(CageAdvance, SingleStrayMarkerSlowsButStillAdvances) {
    // P174 (08.10.2026): plánovač napřed hledá bezpečné pole po celém dosahu nosiče; tenhle test
    // zkouší ZÁLOŽNÍ postup (krok po přímce podle rozvrhu), který nastupuje, když bezpečné pole
    // nevyjde — proto je hledání po dobu testu vypnuté.
    struct OldPath { OldPath() { setCageFeaturesOff(kFeatCleanCageSearch); } ~OldPath() { setCageFeaturesOff(0); } } oldPath;
    // Closer carrier (dist 7, usable 7 -> requiredPace exactly 1.0): one
    // opponent in the corridor costs one square of pace (2 -> 1), which
    // still meets the schedule -> plan fires with step 1.
    GameState state = makeCageState();
    state.getPlayer(1).position = {18, 7};
    state.ball = BallState::carried({18, 7}, 1);
    state.getPlayer(2).position = {17, 6};
    state.getPlayer(3).position = {17, 8};
    state.getPlayer(4).position = {19, 6};
    state.getPlayer(5).position = {19, 8};
    Player& opp = state.getPlayer(13);
    opp.id = 13;
    opp.teamSide = TeamSide::AWAY;
    opp.state = PlayerState::STANDING;
    opp.position = {22, 5};  // ahead 4, |dy| 2 -> in corridor
    opp.stats = {6, 3, 3, 8};
    opp.movementRemaining = 6;

    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    ASSERT_TRUE(plan.valid) << "verdict=" << static_cast<int>(plan.verdict);
    EXPECT_EQ(plan.resistance, 1);
    EXPECT_EQ(plan.step, 1);
    // One corridor marker costs exactly one square of pace off the raw
    // role-achievable step (MA-computed, here 4 -> 3).
    EXPECT_NEAR(plan.achievablePace, plan.rawAchievableStep - 1.0, 1e-9);
}

// =============================================================
// 2026-08-04 user doctrine: MA-computed ceiling, carrier GFI in a
// tempo emergency, untangling a blocked carrier lane
// =============================================================

TEST(CageAdvance, CarrierGfiFiresOnlyInTempoEmergency) {
    // P174 (08.10.2026): plánovač napřed hledá bezpečné pole po celém dosahu nosiče; tenhle test
    // zkouší ZÁLOŽNÍ postup (krok po přímce podle rozvrhu), který nastupuje, když bezpečné pole
    // nevyjde — proto je hledání po dobu testu vypnuté.
    struct OldPath { OldPath() { setCageFeaturesOff(kFeatCleanCageSearch); } ~OldPath() { setCageFeaturesOff(0); } } oldPath;
    // Carrier MA4 far from the endzone late in the half: dist 20, turn 4
    // -> turnsLeft 5, usable 4 -> requiredPace 5.0 > MA4. Corners are MA6
    // blitzer-ish so the formation sustains step 5-6; the carrier must take
    // ONE real GFI (user doctrine: he MUST arrive, even at dice cost).
    GameState state = makeCageState();
    state.homeTeam.turnNumber = 4;
    state.getPlayer(1).position = {5, 7};
    state.ball = BallState::carried({5, 7}, 1);
    auto fast = [&](int id, Position pos) {
        Player& p = state.getPlayer(id);
        p.position = pos;
        p.stats.movement = 6;
        p.movementRemaining = 6;
    };
    fast(2, {4, 6});
    fast(3, {4, 8});
    fast(4, {6, 6});
    fast(5, {6, 8});
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    ASSERT_TRUE(plan.valid) << "verdict=" << static_cast<int>(plan.verdict);
    EXPECT_NEAR(plan.requiredPace, 5.0, 1e-9);
    EXPECT_EQ(plan.step, 5);
    EXPECT_EQ(plan.carrierGfi, 1);  // exactly the emergency top-up, not 2
    // The GFI allowance rides on the carrier's macro (last in the plan).
    EXPECT_EQ(plan.macros.back().playerId, 1);
    EXPECT_EQ(plan.macros.back().gfiAllowance, 1);
}

TEST(CageAdvance, BankWhileClearRevertsToScheduleUnderResistance) {
    // P174 (08.10.2026): plánovač napřed hledá bezpečné pole po celém dosahu nosiče; tenhle test
    // zkouší ZÁLOŽNÍ postup (krok po přímce podle rozvrhu), který nastupuje, když bezpečné pole
    // nevyjde — proto je hledání po dobu testu vypnuté.
    struct OldPath { OldPath() { setCageFeaturesOff(kFeatCleanCageSearch); } ~OldPath() { setCageFeaturesOff(0); } } oldPath;
    // Same geometry twice; the only difference is one opponent in the
    // corridor. Clear corridor -> bank at max dice-free pace (4);
    // resistance -> grind at schedule pace only.
    GameState clear = makeCageState();
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan bankPlan = planner.build(clear);
    ASSERT_TRUE(bankPlan.valid);
    EXPECT_EQ(bankPlan.step, 4);
    EXPECT_EQ(bankPlan.carrierGfi, 0) << "banking must never buy GFI risk";

    GameState contested = makeCageState();
    Player& opp = contested.getPlayer(13);
    opp.id = 13;
    opp.teamSide = TeamSide::AWAY;
    opp.state = PlayerState::STANDING;
    opp.position = {16, 8};  // ahead 4, |dy| 1 -> in corridor
    opp.stats = {6, 3, 3, 8};
    opp.movementRemaining = 6;
    CageAdvancePlan grindPlan = planner.build(contested);
    ASSERT_TRUE(grindPlan.valid) << "verdict=" << static_cast<int>(grindPlan.verdict);
    EXPECT_EQ(grindPlan.resistance, 1);
    EXPECT_EQ(grindPlan.step, 2) << "schedule pace (ceil 13/7), no banking into bodies";
}

TEST(CageAdvance, FasterCornerPreferredWhenTempoDemandsIt) {
    // User design input 2026-08-03 (wired 2026-08-04): a corner slower than
    // the planned step throttles the rolling cage next turn. A closer MA4
    // longbeard must lose the front slot to a farther MA6 runner once the
    // planned step exceeds 4.
    GameState state = makeCageState();
    state.homeTeam.turnNumber = 4;  // usable 4, dist 20 -> required 5
    state.getPlayer(1).position = {5, 7};
    state.getPlayer(1).stats.movement = 6;
    state.getPlayer(1).movementRemaining = 6;
    state.ball = BallState::carried({5, 7}, 1);
    auto put = [&](int id, Position pos, int8_t ma) {
        Player& p = state.getPlayer(id);
        p.id = id;
        p.teamSide = TeamSide::HOME;
        p.state = PlayerState::STANDING;
        p.position = pos;
        p.stats = {ma, 3, 2, 9};
        p.movementRemaining = ma;
    };
    put(2, {4, 6}, 6);
    put(3, {4, 8}, 6);
    put(5, {7, 9}, 6);
    put(4, {10, 6}, 4);  // slow longbeard CLOSER to the front slot...
    put(6, {7, 5}, 6);   // ...must lose it to the sustainable runner

    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    ASSERT_TRUE(plan.valid) << "verdict=" << static_cast<int>(plan.verdict);
    ASSERT_GE(plan.step, 5);
    Position frontTop{static_cast<int8_t>(5 + plan.step + 1), 6};
    bool fastGotIt = false;
    for (const auto& m : plan.macros) {
        if (m.targetPos == frontTop) fastGotIt = (m.playerId == 6);
    }
    EXPECT_TRUE(fastGotIt) << "MA6 must outrank the closer MA4 for a step-"
                           << plan.step << " rolling cage";
}

TEST(CageAdvance, FreeBodyPreferredOverMarkedCandidateForNewCorner) {
    // Corner substitution (user 2026-08-04): the marked teammate would have
    // to dodge out (probe would veto the plan) -- a farther FREE body takes
    // the new corner instead, and the engaged one stays put binding his
    // marker. The marker sits outside the advance corridor so tempo math
    // stays untouched.
    GameState state = makeCageState();
    state.getPlayer(3).state = PlayerState::OFF_PITCH;
    auto put = [&](int id, TeamSide side, Position pos) {
        Player& p = state.getPlayer(id);
        p.id = id;
        p.teamSide = side;
        p.state = PlayerState::STANDING;
        p.position = pos;
        p.stats = {4, 3, 2, 9};
        p.movementRemaining = 4;
    };
    put(7, TeamSide::HOME, {14, 9});   // closer to the open back slot, but...
    put(13, TeamSide::AWAY, {15, 10}); // ...marked by this opponent
    put(6, TeamSide::HOME, {11, 10});  // farther and FREE -> must win

    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    ASSERT_TRUE(plan.valid) << "verdict=" << static_cast<int>(plan.verdict);
    Position backBottom{static_cast<int8_t>(12 + plan.step - 1), 8};
    bool freeGotIt = false, markedDrafted = false;
    for (const auto& m : plan.macros) {
        if (m.targetPos == backBottom && m.playerId == 6) freeGotIt = true;
        if (m.playerId == 7) markedDrafted = true;
    }
    EXPECT_TRUE(freeGotIt) << "free body must take the new corner (step="
                           << plan.step << ")";
    EXPECT_FALSE(markedDrafted) << "engaged teammate stays put, no dodge";
}

TEST(CageAdvance, CarrierTargetBlockedByTeammateGetsVacatedFirst) {
    // A teammate parked straight ahead of the carrier (post-scrum pile,
    // user doctrine: "the ones in FRONT move first so they stop blocking").
    // He is drafted into a corner slot of the NEW cage and his macro runs
    // before the carrier's.
    GameState state = makeCageState();
    // Pin the carrier's reach to 2 so the plan's target is deterministically
    // {14,7} -- this test is about the vacate mechanics, not step choice.
    state.getPlayer(1).stats.movement = 2;
    state.getPlayer(1).movementRemaining = 2;
    auto& blocker = state.getPlayer(6);
    blocker.id = 6;
    blocker.teamSide = TeamSide::HOME;
    blocker.state = PlayerState::STANDING;
    blocker.position = {14, 7};  // carrier 12,7 + step 2 -> exactly the target
    blocker.stats = {4, 3, 2, 9};
    blocker.movementRemaining = 4;
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    ASSERT_TRUE(plan.valid) << "verdict=" << static_cast<int>(plan.verdict);
    EXPECT_EQ(plan.step, 2);
    ASSERT_TRUE(hasMacroFor(plan, 6));
    size_t blockerAt = 0, carrierAt = 0;
    for (size_t i = 0; i < plan.macros.size(); ++i) {
        if (plan.macros[i].playerId == 6) blockerAt = i;
        if (plan.macros[i].playerId == 1) carrierAt = i;
    }
    EXPECT_LT(blockerAt, carrierAt) << "blocker must vacate before the carrier walks";
    EXPECT_EQ(plan.macros[carrierAt].targetPos, (Position{14, 7}));
}

// =============================================================
// The plan itself: whole cage shifts, corners first, carrier last
// =============================================================

TEST(CageAdvance, PlanShiftsWholeCageCornersFirstCarrierLast) {
    GameState state = makeCageState();
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);

    ASSERT_TRUE(plan.valid);
    ASSERT_EQ(plan.macros.size(), 5u);  // 4 corner movers + carrier
    for (const auto& m : plan.macros) EXPECT_EQ(m.type, MacroType::REPOSITION);

    // Step-agnostic geometry: the new cage centre is carrier.x + step.
    const int nx = 12 + plan.step;
    // Carrier strictly LAST: the screen forms before the ball commits.
    const Macro& last = plan.macros.back();
    EXPECT_EQ(last.playerId, 1);
    EXPECT_EQ(last.targetPos, (Position{static_cast<int8_t>(nx), 7}));
    for (size_t i = 0; i + 1 < plan.macros.size(); ++i) {
        EXPECT_NE(plan.macros[i].playerId, 1);
    }
    // Front slots are claimed first (macro order = slot order).
    EXPECT_EQ(plan.macros[0].targetPos, (Position{static_cast<int8_t>(nx + 1), 6}));
    EXPECT_EQ(plan.macros[1].targetPos, (Position{static_cast<int8_t>(nx + 1), 8}));

    EXPECT_EQ(plan.filledCorners, 4);
    EXPECT_EQ(plan.openCorners, 0);
    EXPECT_EQ(plan.gfiCorners, 0);

    // Execute the plan for real: the whole cage must arrive intact.
    DiceRoller dice(7);
    for (const auto& m : plan.macros) {
        ASSERT_TRUE(stagedMacroStillValid(state, m))
            << "plan macro invalid at execution for p" << m.playerId;
        auto res = greedyExpandMacro(state, m, dice);
        ASSERT_FALSE(res.turnover);
    }
    EXPECT_EQ(state.getPlayer(1).position, (Position{static_cast<int8_t>(nx), 7}));
    int corners = 0;
    for (auto& d : state.getPlayer(1).position.getAdjacent()) {
        const Player* p = state.getPlayerAtPosition(d);
        if (p && p->teamSide == TeamSide::HOME &&
            std::abs(d.x - nx) == 1 && std::abs(d.y - 7) == 1) {
            corners++;
        }
    }
    EXPECT_EQ(corners, 4) << "cage must be re-formed around the new carrier square";
}

// =============================================================
// Corner selection (binding constraint 2)
// =============================================================

TEST(CageAdvance, GuardPreferredAtEqualDistanceAndReliability) {
    GameState state = makeCageState();
    // Pin the carrier to step 2 (this test is about the Guard tiebreak,
    // not step choice under the bank policy).
    state.getPlayer(1).stats.movement = 2;
    state.getPlayer(1).movementRemaining = 2;
    // Back corners stay (already acted); the two front slots {15,6}/{15,8}
    // must be drafted from two fresh players equidistant to {15,6}:
    // p6 (no skills) and p7 (+Guard) -> Guard wins the first slot.
    state.getPlayer(2).state = PlayerState::OFF_PITCH;
    state.getPlayer(3).state = PlayerState::OFF_PITCH;
    state.getPlayer(4).hasMoved = true;  // stays as a body on the back slot
    state.getPlayer(5).hasMoved = true;
    auto mk = [&](int id, Position pos, std::vector<SkillName> skills) {
        Player& p = state.getPlayer(id);
        p.id = id;
        p.teamSide = TeamSide::HOME;
        p.state = PlayerState::STANDING;
        p.position = pos;
        p.stats = {4, 3, 2, 9};
        p.movementRemaining = 4;
        for (auto s : skills) p.skills.add(s);
    };
    mk(6, {17, 7}, {});
    mk(7, {17, 5}, {SkillName::Guard});

    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    ASSERT_TRUE(plan.valid) << "verdict=" << static_cast<int>(plan.verdict);
    ASSERT_GE(plan.macros.size(), 3u);  // 2 front movers + carrier
    EXPECT_EQ(plan.macros[0].targetPos, (Position{15, 6}));
    EXPECT_EQ(plan.macros[0].playerId, 7) << "Guard must win the equal-distance tie";
    EXPECT_EQ(plan.macros[1].playerId, 6);
    EXPECT_EQ(plan.filledCorners, 4);  // 2 movers + 2 stay-put bodies
}

TEST(CageAdvance, NegaTraitTreemanNeverDraftedDespiteGuardStandFirm) {
    // P174 (08.10.2026): plánovač napřed hledá bezpečné pole po celém dosahu nosiče; tenhle test
    // zkouší ZÁLOŽNÍ postup (krok po přímce podle rozvrhu), který nastupuje, když bezpečné pole
    // nevyjde — proto je hledání po dobu testu vypnuté.
    struct OldPath { OldPath() { setCageFeaturesOff(kFeatCleanCageSearch); } ~OldPath() { setCageFeaturesOff(0); } } oldPath;
    GameState state = makeCageState();
    state.getPlayer(2).state = PlayerState::OFF_PITCH;
    state.getPlayer(3).state = PlayerState::OFF_PITCH;
    state.getPlayer(4).hasMoved = true;
    state.getPlayer(5).hasMoved = true;
    auto mk = [&](int id, Position pos, std::vector<SkillName> skills) {
        Player& p = state.getPlayer(id);
        p.id = id;
        p.teamSide = TeamSide::HOME;
        p.state = PlayerState::STANDING;
        p.position = pos;
        p.stats = {4, 3, 2, 9};
        p.movementRemaining = 4;
        for (auto s : skills) p.skills.add(s);
    };
    mk(6, {17, 7}, {});
    // Treeman-profile: Guard+StandFirm say "corner", TakeRoot says NEVER.
    mk(7, {17, 5}, {SkillName::Guard, SkillName::StandFirm, SkillName::TakeRoot,
                    SkillName::Loner});

    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    ASSERT_TRUE(plan.valid);
    EXPECT_FALSE(hasMacroFor(plan, 7)) << "nega-trait player must never be drafted";
    EXPECT_TRUE(hasMacroFor(plan, 6));
    EXPECT_EQ(plan.filledCorners, 3);
    EXPECT_EQ(plan.openCorners, 1);
}

TEST(CageAdvance, EligibilityIsGenericOverSkills) {
    Player p;
    p.state = PlayerState::STANDING;
    EXPECT_TRUE(CageAdvancePlanner::eligibleCornerPlayer(p));
    auto with = [](SkillName s) {
        Player q;
        q.skills.add(s);
        return CageAdvancePlanner::eligibleCornerPlayer(q);
    };
    // Activation nega-traits + drive-limited + unpositionable: excluded.
    EXPECT_FALSE(with(SkillName::BoneHead));      // human Ogre profile
    EXPECT_FALSE(with(SkillName::ReallyStupid));
    EXPECT_FALSE(with(SkillName::WildAnimal));
    EXPECT_FALSE(with(SkillName::TakeRoot));      // wood-elf Treeman profile
    EXPECT_FALSE(with(SkillName::SecretWeapon));  // Deathroller profile
    EXPECT_FALSE(with(SkillName::BallAndChain));
    // NOT excluded on their own: hands are irrelevant to corner duty and
    // Loner only taxes rerolls, not the activation itself.
    EXPECT_TRUE(with(SkillName::NoHands));
    EXPECT_TRUE(with(SkillName::Loner));
    EXPECT_TRUE(with(SkillName::Guard));
}

TEST(CageAdvance, GfiAllowanceAtMostOneCornerRestOpen) {
    GameState state = makeCageState();
    // Pin the carrier to step 2 so the allowance branch is exercised
    // deterministically (bank policy would otherwise pick a farther step
    // whose slots these candidates cannot reach at all).
    state.getPlayer(1).stats.movement = 2;
    state.getPlayer(1).movementRemaining = 2;
    state.getPlayer(2).state = PlayerState::OFF_PITCH;
    state.getPlayer(3).state = PlayerState::OFF_PITCH;
    state.getPlayer(4).hasMoved = true;
    state.getPlayer(5).hasMoved = true;
    // Both front-slot candidates sit at distance 5 = MA4+1: each would need
    // one GFI. Exactly ONE gets the allowance; the other slot stays open.
    auto mk = [&](int id, Position pos) {
        Player& p = state.getPlayer(id);
        p.id = id;
        p.teamSide = TeamSide::HOME;
        p.state = PlayerState::STANDING;
        p.position = pos;
        p.stats = {4, 3, 2, 9};
        p.movementRemaining = 4;
    };
    mk(6, {10, 5});   // dist 5 to {15,6}
    mk(7, {10, 9});   // dist 5 to {15,8}

    // Od 07.10.2026 (P154, krok šikmo) by plán tutéž pozici vyřešil krokem o řádek vedle, kde
    // GFI není potřeba — pravidlo „nejvýš jeden roh na GFI“ se proto čte přímo z přidělení
    // rohů pro krok 2 rovně, kudy prochází každý plán.
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    auto a = planner.tryAssign(state, state.getPlayer(1), 2, {});
    ASSERT_TRUE(a.feasible);
    EXPECT_EQ(a.gfi, 1);
    EXPECT_EQ(a.open, 1);
    EXPECT_EQ(a.filled, 3);
    int with6 = 0, with7 = 0;
    for (const auto& sa : a.slots) { with6 += sa.playerId == 6; with7 += sa.playerId == 7; }
    EXPECT_EQ(with6, 1);
    EXPECT_EQ(with7, 0);
}

// =============================================================
// Role-aware shared budget (binding constraint 3)
// =============================================================

TEST(CageAdvance, ReservedPlayersAreNeverDrafted) {
    GameState state = makeCageState();
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);

    CageAdvancePlan base = planner.build(state);
    ASSERT_TRUE(base.valid);
    ASSERT_TRUE(hasMacroFor(base, 4));

    // Reserve p4 (a Guard corner) for "another job": the plan must adapt
    // without him -- and never emit a macro for a reserved player.
    CageAdvancePlan plan = planner.build(state, {4});
    ASSERT_TRUE(plan.valid) << "verdict=" << static_cast<int>(plan.verdict);
    EXPECT_FALSE(hasMacroFor(plan, 4));
    // p4 still counts where he already STANDS (a reserved body on a slot is
    // still a body), but he must not be moved.
    EXPECT_GE(plan.filledCorners, 3);
}

// =============================================================
// MacroMCTSPolicy integration (config-gated, default off)
// =============================================================

// 2026-08-11: the planner had no notion of tackle zones -- corner slots were
// picked purely geometrically -- so the cage happily parked itself inside
// them. Measured on the replay corpus: the carrier ended marked at the end of
// 40% of our advance turns. The standing rule (user, since 08-04) is that a
// marked corner is no corner at all: the opponent blocks it out and the cage
// opens.
//
// Fixture: one AWAY marker at {17,7}, deliberately just OUTSIDE the resistance
// corridor (ahead=5 > CORRIDOR_DEPTH), so the corridor still reads as clear
// and the bank policy reaches for the full step of 4 -- which parks the
// carrier on {16,7}, right beside him, with both front corners marked too.
// Stepping 2 instead meets the schedule exactly and touches nobody. Banking is
// a bonus; not standing next to an opponent is not.
TEST(CageAdvance, CarrierAvoidsEndingInsideATacklezoneWhenItIsFree) {
    // P174 (08.10.2026): plánovač napřed hledá bezpečné pole po celém dosahu nosiče; tenhle test
    // zkouší ZÁLOŽNÍ postup (krok po přímce podle rozvrhu), který nastupuje, když bezpečné pole
    // nevyjde — proto je hledání po dobu testu vypnuté.
    struct OldPath { OldPath() { setCageFeaturesOff(kFeatCleanCageSearch); } ~OldPath() { setCageFeaturesOff(0); } } oldPath;
    GameState state = makeCageState();
    Player& marker = state.getPlayer(13);
    marker.id = 13;
    marker.teamSide = TeamSide::AWAY;
    marker.state = PlayerState::STANDING;
    marker.position = {17, 7};
    marker.stats = {6, 3, 3, 8};
    marker.movementRemaining = 6;

    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    ASSERT_TRUE(plan.valid) << "verdict=" << static_cast<int>(plan.verdict);

    Position dest{static_cast<int8_t>(12 + plan.step), 7};
    EXPECT_EQ(countTacklezones(state, dest, TeamSide::HOME), 0)
        << "carrier ends marked at {" << int(dest.x) << "," << int(dest.y) << "}"
        << " with step " << plan.step;
    // The schedule is never sacrificed for it: requiredPace ~1.86 -> step >= 2.
    EXPECT_GE(plan.step, 2) << "schedule pace must still be met";
    EXPECT_EQ(plan.carrierGfi, 0) << "tempo is never bought with dice";
}

// The other half of the bound: when every reachable square is marked there is
// nothing to choose, and the planner must still advance rather than stall.
// Tempo is the binding constraint -- we score in a minority of matches, so a
// marked carrier that keeps moving beats a clean one that does not.
TEST(CageAdvance, ExposureNeverStallsTheAdvance) {
    GameState state = makeCageState();
    int id = 13;
    for (int x = 13; x <= 17; ++x) {
        Player& m = state.getPlayer(id);
        m.id = id;
        m.teamSide = TeamSide::AWAY;
        m.state = PlayerState::STANDING;
        m.position = {static_cast<int8_t>(x), 6};
        m.stats = {6, 3, 3, 8};
        m.movementRemaining = 6;
        ++id;
    }
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    // P100: dřív `if (plan.valid)` — neplatný plán (právě to zamrznutí) test pustil.
    ASSERT_TRUE(plan.valid) << "a fully marked corridor must not freeze the cage";
    EXPECT_GE(plan.step, 1) << "a fully marked corridor must not freeze the cage";
}

// 2026-08-05 (user, binding): "fallback to search is unacceptable -- the
// carrier running out of the cage on his own is a fine fallback", said
// ironically. The mandated hierarchy is advance -> fill -> never a solo run,
// and "we cannot let the dwarves throw away the attempt at a TD in turn 1".
// Measured 08-11 with the gate forced on: the advance declined in 85% of
// ADVANCE turns and every one of those fell through to search(), which
// averages 1.73 squares against the plan's 5.00.
//
// Here the corners are too far away to reform at any forward step, so the
// advance cannot run -- but two of them can still reach the slots around the
// carrier where he stands. The plan must be that fill, not nothing.
// P120 (uživatel 01.10.2026: „přední rohy soupeř není klec“): stojí-li soupeři na předních rozích
// klece, klec tam není a doplňovat ji nemá smysl. Planner nesmí vrátit fill — a nevrací (proto
// původní fixtura se zdí na x=13 nikdy fill nedala a `if` v testu to tiše přešel).
TEST(CageAdvance, NoFillWhenOpponentsStandOnTheFrontCorners) {
    GameState state = makeCageState();
    state.getPlayer(2).position = {11, 6};
    state.getPlayer(3).position = {11, 8};
    state.getPlayer(4).position = {10, 5};
    state.getPlayer(5).position = {10, 9};
    int id = 13;
    for (int y = 5; y <= 9; ++y) {   // wall ON the front corners (13,6) and (13,8)
        Player& m = state.getPlayer(id);
        m.id = id; m.teamSide = TeamSide::AWAY;
        m.state = PlayerState::STANDING;
        m.position = {13, static_cast<int8_t>(y)};
        m.stats = {6, 3, 3, 8};
        m.movementRemaining = 6;
        ++id;
    }
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    EXPECT_NE(plan.verdict, CageAdvanceVerdict::FILL_ONLY) << "soupeř na předních rozích = není klec";
    for (const auto& m : plan.macros) {
        EXPECT_NE(m.playerId, 1) << "nosič sám nevybíhá (advance -> fill -> nikdy sólo běh)";
    }
}

TEST(CageAdvance, FillsTheCageWhenTheAdvanceCannotRun) {
    GameState state = makeCageState();
    // Strip the cage: corners parked far behind, out of reach of any
    // destination slot but within reach of the carrier's own diagonals.
    state.getPlayer(2).position = {11, 6};
    state.getPlayer(3).position = {11, 8};
    state.getPlayer(4).position = {10, 5};
    state.getPlayer(5).position = {10, 9};
    // A wall right in front: every forward step is contested, so the advance
    // arithmetic gives up.
    // P120 (01.10.2026): zeď stála na x=13 — přímo na předních rozích klece (13,6) a (13,8), takže
    // doplnit nebylo co a plán byl neplatný; `if` to tiše přešel, test nic neověřoval. Na x=14 jsou
    // přední rohy volné (jen v soupeřových zónách) a nastane přesně ten fill, o kterém test mluví.
    int id = 13;
    for (int y = 5; y <= 9; ++y) {
        Player& m = state.getPlayer(id);
        m.id = id; m.teamSide = TeamSide::AWAY;
        m.state = PlayerState::STANDING;
        m.position = {14, static_cast<int8_t>(y)};
        m.stats = {6, 3, 3, 8};
        m.movementRemaining = 6;
        ++id;
    }
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    // P100: dřív se při jiném verdiktu (právě tom, který test hlídá) neověřilo nic.
    ASSERT_TRUE(plan.valid) << "the plan must be that fill, not nothing";
    ASSERT_EQ(plan.verdict, CageAdvanceVerdict::FILL_ONLY);
    EXPECT_EQ(plan.step, 0) << "fill never moves the carrier";
    EXPECT_EQ(plan.carrierGfi, 0) << "fill never buys dice";
    EXPECT_FALSE(plan.macros.empty());
    for (const auto& m : plan.macros) {
        EXPECT_NE(m.playerId, 1) << "the carrier must not be in a fill plan";
    }
}

// ---------------------------------------------------------------------------
// P154 (07.10.2026) — čistá klec na konci tahu. Uživatel: „zkus dokončit co nejvíc možností ke
// zlepšení stavění čisté klece na konci tahu“; „ležící spoluhráč může vstát a dojít stát se
// rohem, pokud nevyžaduje dodge nebo riskantní hod“.

namespace {
Player& putPlayer(GameState& state, int id, TeamSide side, Position pos, int8_t ma = 4) {
    Player& p = state.getPlayer(id);
    p.id = id; p.teamSide = side; p.state = PlayerState::STANDING; p.position = pos;
    p.stats = {ma, 3, 2, 9}; p.movementRemaining = ma; p.hasMoved = false; p.hasActed = false;
    return p;
}
int slotOwner(const CageAdvancePlanner::AssignmentResult& a, Position sq) {
    for (const auto& sa : a.slots) if (sa.slot == sq) return sa.playerId;
    return -2;
}
}  // namespace

// Roh nedostane hráč, který by na něj musel z kontaktu uhýbat: roh zůstane otevřený a zbylé tři
// stojí. (Dřív ho dostal, kontrola „bez hodu“ pak zahodila celý krok a klec se nestavěla.)
TEST(CageAdvance, CornerIsNotGivenToAPlayerWhoWouldHaveToDodge) {
    GameState state = makeCageState();
    state.getPlayer(5).position = {14, 10};                 // jediný kandidát na roh (13,8)
    putPlayer(state, 13, TeamSide::AWAY, {15, 11}, 6);      // drží ho v zóně, na roh nedosahuje
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    auto a = planner.tryAssign(state, state.getPlayer(1), 0, {});
    EXPECT_EQ(a.filled, 3);
    EXPECT_EQ(slotOwner(a, {13, 8}), -1) << "roh zůstává otevřený, vázaný hráč o něj nesoutěží";

    // pozitivní kontrola: bez soupeře tentýž hráč roh dostane
    state.getPlayer(13).state = PlayerState::OFF_PITCH;
    auto b = planner.tryAssign(state, state.getPlayer(1), 0, {});
    EXPECT_EQ(b.filled, 4);
    EXPECT_EQ(slotOwner(b, {13, 8}), 5);
}

// Omráčený spoluhráč na poli rohu není roh (dřív se počítal: „a body on the slot is a corner“).
TEST(CageAdvance, StunnedTeammateOnTheCornerSquareIsNotACorner) {
    GameState state = makeCageState();
    state.getPlayer(5).state = PlayerState::STUNNED;
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    auto a = planner.tryAssign(state, state.getPlayer(1), 0, {});
    EXPECT_EQ(a.filled, 3);
    EXPECT_EQ(a.open, 1);
}

// Ležící spoluhráč vstane bez hodu (MA ≥ 3, pravidla ř. 690-695) a je rohem: na místě, i když
// k rohu po vstání dojde. S MA 2 by vstával na 4+ (riskantní hod) ⇒ rohem není.
TEST(CageAdvance, ProneTeammateStandsUpToBecomeACorner) {
    GameState state = makeCageState();
    state.getPlayer(5).state = PlayerState::PRONE;          // leží přímo na rohu (13,8)
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan fill = planner.buildFillOnly(state, {});
    ASSERT_TRUE(fill.valid);
    ASSERT_EQ(fill.macros.size(), 1u);
    EXPECT_EQ(fill.macros[0].playerId, 5);
    EXPECT_EQ(fill.macros[0].targetPos, (Position{13, 8}));
    ASSERT_TRUE(stagedMacroStillValid(state, fill.macros[0], true)) << "řadič makro pro ležícího nezahodí";
    DiceRoller dice(1);
    auto r = greedyExpandMacro(state, fill.macros[0], dice);
    EXPECT_FALSE(r.turnover);
    EXPECT_EQ(state.getPlayer(5).state, PlayerState::STANDING);
    EXPECT_EQ(state.getPlayer(5).position, (Position{13, 8}));

    // leží pole od rohu: MA 4 − 3 za vstání = 1 pole ⇒ dojde; dvě pole od rohu už ne
    GameState near = makeCageState();
    near.getPlayer(5).state = PlayerState::PRONE;
    near.getPlayer(5).position = {14, 9};
    EXPECT_EQ(slotOwner(planner.tryAssign(near, near.getPlayer(1), 0, {}), {13, 8}), 5);
    near.getPlayer(5).position = {15, 10};
    EXPECT_EQ(slotOwner(planner.tryAssign(near, near.getPlayer(1), 0, {}), {13, 8}), -1);

    // MA 2: vstání je hod na 4+ ⇒ rohem se nepočítá
    GameState slow = makeCageState();
    slow.getPlayer(5).state = PlayerState::PRONE;
    slow.getPlayer(5).stats.movement = 2;
    slow.getPlayer(5).movementRemaining = 2;
    EXPECT_EQ(planner.tryAssign(slow, slow.getPlayer(1), 0, {}).filled, 3);
}

// Krok šikmo: na řádku předních rohů leží soupeři, rovně má každý krok nejvýš tři rohy; o řádek
// vedle vyjdou čtyři.
TEST(CageAdvance, StepEndsOneRowAsideWhenACornerSquareIsTaken) {
    GameState state = makeCageState();
    for (int i = 0; i < 4; ++i) {
        Player& o = putPlayer(state, 13 + i, TeamSide::AWAY, {static_cast<int8_t>(14 + i), 6}, 6);
        o.state = PlayerState::PRONE;
    }
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    for (int step = 1; step <= 4; ++step) {                 // pozitivní kontrola fixture
        ASSERT_LE(planner.tryAssign(state, state.getPlayer(1), step, {}).filled, 3) << "krok " << step;
    }
    CageAdvancePlan plan = planner.build(state);
    ASSERT_TRUE(plan.valid);
    ASSERT_GE(plan.step, 1);
    EXPECT_EQ(plan.filledCorners, 4);
    Position carrierTarget{-1, -1};
    for (const auto& m : plan.macros) if (m.playerId == 1) carrierTarget = m.targetPos;
    EXPECT_EQ(carrierTarget.y, 8) << "nosič končí o řádek vedle, kde jsou všechna čtyři pole rohů volná";
}

// Uživatel 08.10.2026: „pokud má klec dva nebo tři rohy tak, ať soupeř nedosáhne na nosiče — tak je
// to také validní — ale nutné je rozlišit soupeř dosáhne / nedosáhne na nosiče“. Zadní roh už hrál,
// takže každý krok vpřed má jen tři rohy. Bez soupeře v dosahu klec jde dál i se třemi; když by
// soupeř po kroku měl na nosiče dobrou ránu, klec zůstane stát se čtyřmi.
TEST(CageAdvance, ThreeCornersAdvanceWhenNobodyReachesTheCarrierAndStayWhenSomeoneDoes) {
    auto board = [](bool opponentNear) {
        GameState state = makeCageState();
        state.getPlayer(3).position = {13, 8};
        state.getPlayer(5).position = {11, 8};
        state.getPlayer(5).hasMoved = true;                     // zadní roh stojí, ale dál už nejde
        if (opponentNear) {                                     // dva soupeři za klecí: po kroku mají díru vzadu
            putPlayer(state, 13, TeamSide::AWAY, {9, 9}, 6);
            putPlayer(state, 14, TeamSide::AWAY, {9, 10}, 6);
        }
        return state;
    };
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    {
        GameState state = board(false);
        ASSERT_EQ(planner.tryAssign(state, state.getPlayer(1), 0, {}).filled, 4);
        CageAdvancePlan plan = planner.build(state);
        ASSERT_TRUE(plan.valid);
        EXPECT_GE(plan.step, 1) << "nikdo na nosiče nedosáhne ⇒ tři rohy stačí a klec jde dál";
        EXPECT_DOUBLE_EQ(plan.blitzThreat, 0.0);
    }
    {
        GameState state = board(true);
        CageAdvancePlan plan = planner.build(state);
        if (plan.valid && plan.step >= 1) {
            EXPECT_LE(plan.blitzThreat, 0.15) << "když už jde dál, tak jen tam, kde soupeř dobrou ránu nemá";
        }
    }
}

// P131 / P169 krok 7: klec se od kraje hřiště odtahuje — nosič na řádku 1 udělá krok šikmo ke
// středu (řádek 2), ne rovně po řádku 1; a nosič na řádku 2 ke kraji nejde.
TEST(CageAdvance, CageStepsAwayFromTheSideline) {
    GameState state = makeCageState();
    for (int id = 1; id <= 5; ++id) state.getPlayer(id).position.y -= 6;   // nosič (12,1), rohy řádky 0 a 2
    state.ball = BallState::carried({12, 1}, 1);
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    ASSERT_TRUE(plan.valid);
    ASSERT_GE(plan.step, 1);
    Position target{-1, -1};
    for (const auto& m : plan.macros) if (m.playerId == 1) target = m.targetPos;
    EXPECT_EQ(target.y, 2) << "krok šikmo od kraje";
}

// ---------------------------------------------------------------------------
// K9b (2026-08-18): corridorResistance must live OUTSIDE the planner.
//
// Until today the number existed only when the cage gate ran. The gate is OFF
// in production (NOT_CONSULTED in 100% of turns on the 3000-game corpus), so
// check K9b -- which needs it -- could never run, and it was parked as
// "BLOCKED on T3.1". T3.1 was REJECTED on 2026-08-18, which would have turned
// that temporary blocker into a permanent one. This pins the hoisted function
// so it cannot quietly slide back inside the planner.
TEST(CorridorResistance, CountsOnlyStandingOpponentsInTheCorridor) {
    GameState state = makeCageState();
    const Player& carrier = state.getPlayer(1);
    // The fixture's only AWAY body sits far away at {24,13} -> outside.
    EXPECT_EQ(corridorResistance(state, carrier, TeamSide::HOME), 0);

    auto place = [&](int id, Position pos, PlayerState st) {
        Player& p = state.getPlayer(id);
        p.id = id;
        p.teamSide = TeamSide::AWAY;
        p.state = st;
        p.position = pos;
        p.stats = {6, 3, 3, 8};
        p.movementRemaining = 6;
        p.hasMoved = false;
        p.hasActed = false;
    };
    // HOME advances with dx = +1; the carrier stands at {12,7}.
    place(13, Position{14, 7}, PlayerState::STANDING);   // ahead 2, dy 0
    place(14, Position{15, 9}, PlayerState::STANDING);   // ahead 3, dy 2
    EXPECT_EQ(corridorResistance(state, carrier, TeamSide::HOME), 2);

    place(15, Position{16, 7}, PlayerState::PRONE);      // prone       -> out
    place(16, Position{18, 7}, PlayerState::STANDING);   // ahead 6 > 4 -> out
    place(17, Position{14, 11}, PlayerState::STANDING);  // dy 4 > 2    -> out
    place(18, Position{10, 7}, PlayerState::STANDING);   // behind us   -> out
    EXPECT_EQ(corridorResistance(state, carrier, TeamSide::HOME), 2)
        << "prone, too deep, too wide and behind must all be excluded";
}

// ---------------------------------------------------------------------------
// MĚŘENÍ PŘIDANÉ 21.08. — tempo a síla koridoru jako VLASTNOSTI DESKY.
// Důvod: plan.* je v produkci celé nula (NOT_CONSULTED ve 100 % kol), takže
// "stíháme dojít?" a "jak silná je zeď?" nešlo z korpusu zjistit vůbec.
// ---------------------------------------------------------------------------

TEST(BoardMetrics, CorridorStrengthWeighsStrengthNotBodies) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    Player& car = gs.getPlayer(1);
    car.id = 1; car.teamSide = TeamSide::HOME; car.state = PlayerState::STANDING;
    car.position = {10, 7}; car.stats = {6, 3, 3, 8}; car.movementRemaining = 6;

    // dvě slabá těla v koridoru (ST 2 + ST 2)
    for (int i = 0; i < 2; ++i) {
        Player& e = gs.getPlayer(12 + i);
        e.id = 12 + i; e.teamSide = TeamSide::AWAY; e.state = PlayerState::STANDING;
        e.position = {static_cast<int8_t>(12 + i), 7}; e.stats = {6, 2, 3, 7};
    }
    EXPECT_EQ(corridorResistance(gs, car, TeamSide::HOME), 2);
    EXPECT_EQ(corridorStrength(gs, car, TeamSide::HOME), 4);

    // tytéž dvě těla, ale silná (ST 4 + ST 4): počet STEJNÝ, síla DVOJNÁSOBNÁ
    gs.getPlayer(12).stats = {6, 4, 3, 9};
    gs.getPlayer(13).stats = {6, 4, 3, 9};
    EXPECT_EQ(corridorResistance(gs, car, TeamSide::HOME), 2);
    EXPECT_EQ(corridorStrength(gs, car, TeamSide::HOME), 8);
}

TEST(BoardMetrics, TempoSnapshotExistsWithoutThePlanner) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.homeTeam.turnNumber = 5;              // turnsLeft = 9-5 = 4, usable = 3
    Player& car = gs.getPlayer(1);
    car.id = 1; car.teamSide = TeamSide::HOME; car.state = PlayerState::STANDING;
    car.position = {13, 7}; car.stats = {6, 3, 3, 8}; car.movementRemaining = 6;

    TempoSnapshot t = tempoSnapshot(gs, car, TeamSide::HOME);
    EXPECT_EQ(t.distToEndzone, 12);          // 25 - 13
    EXPECT_EQ(t.turnsLeft, 4);
    EXPECT_FLOAT_EQ(t.required, 12.0f / 3.0f);
    // prázdný koridor => žádná přirážka => MA 6 + 2 GFI
    EXPECT_FLOAT_EQ(t.achievable, 8.0f);
}

TEST(BoardMetrics, CageSnapshotExistsWithoutThePlanner) {
    // T5.34: krytí nosiče je vlastnost DESKY. Plánovač v produkci neběží
    // (NOT_CONSULTED ve 100 % kol), takže `plan.filled_corners` je trvale 0 --
    // a nula se pak čte jako „klec nemá rohy" místo „nikdo to nepočítal".
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    Player& car = gs.getPlayer(1);
    car.id = 1; car.teamSide = TeamSide::HOME; car.state = PlayerState::STANDING;
    car.position = {13, 7}; car.stats = {6, 3, 3, 8};

    // čtyři naše diagonály = plná klec, ortogonály prázdné
    int id = 2;
    for (int sx : {-1, 1}) {
        for (int sy : {-1, 1}) {
            Player& p = gs.getPlayer(id);
            p.id = id; p.teamSide = TeamSide::HOME; p.state = PlayerState::STANDING;
            p.position = {static_cast<int8_t>(13 + sx), static_cast<int8_t>(7 + sy)};
            p.stats = {6, 3, 3, 8};
            ++id;
        }
    }
    CageSnapshot c = cageSnapshot(gs, car, TeamSide::HOME);
    EXPECT_EQ(c.corners, 4);
    EXPECT_EQ(c.cornersMarked, 0);
    EXPECT_EQ(c.orthoOccupied, 0) << "plná klec má ortogonály PRÁZDNÉ (pravidlo 11.08.)";
    EXPECT_EQ(c.aheadOccupied, 0);
    EXPECT_EQ(c.carrierTz, 0);

    // ležící roh roh NEDRŽÍ
    gs.getPlayer(2).state = PlayerState::PRONE;
    EXPECT_EQ(cageSnapshot(gs, car, TeamSide::HOME).corners, 3);
}

TEST(BoardMetrics, CageSnapshotSeesTheBodyDirectlyAhead) {
    // M11 (26.08.): cestu vpřed zavírají VLASTNÍ ve 149 ze 149 případů, a pole
    // přímo vpřed rohem klece NENÍ. Proto se vede zvlášť od ostatních ortogonál.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    Player& car = gs.getPlayer(1);
    car.id = 1; car.teamSide = TeamSide::HOME; car.state = PlayerState::STANDING;
    car.position = {13, 7}; car.stats = {6, 3, 3, 8};

    Player& own = gs.getPlayer(2);           // NÁŠ hráč přímo vpřed (HOME jde na +x)
    own.id = 2; own.teamSide = TeamSide::HOME; own.state = PlayerState::STANDING;
    own.position = {14, 7}; own.stats = {6, 3, 3, 8};

    CageSnapshot c = cageSnapshot(gs, car, TeamSide::HOME);
    EXPECT_EQ(c.orthoOccupied, 1);
    EXPECT_EQ(c.orthoOurs, 1);
    EXPECT_EQ(c.aheadOccupied, 1);
    EXPECT_EQ(c.aheadOurs, 1) << "tělo navíc PŘÍMO VPŘED je ta vada z M11";
    EXPECT_EQ(c.corners, 0);

    // týž hráč o pole vedle (ortogonála, ale ne dopředu) => ahead spadne na 0
    own.position = {13, 8};
    CageSnapshot c2 = cageSnapshot(gs, car, TeamSide::HOME);
    EXPECT_EQ(c2.orthoOccupied, 1);
    EXPECT_EQ(c2.aheadOccupied, 0);
}

TEST(BoardMetrics, CageSnapshotCountsMarkedCorners) {
    // Označený roh není slabší roh: soupeř ho vyblokuje a klec se otevře
    // (doktrína 4.08., bbtactics „Cage Basics"). Proto se počítá zvlášť.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    Player& car = gs.getPlayer(1);
    car.id = 1; car.teamSide = TeamSide::HOME; car.state = PlayerState::STANDING;
    car.position = {13, 7}; car.stats = {6, 3, 3, 8};

    Player& corner = gs.getPlayer(2);
    corner.id = 2; corner.teamSide = TeamSide::HOME;
    corner.state = PlayerState::STANDING;
    corner.position = {12, 6}; corner.stats = {6, 3, 3, 8};

    EXPECT_EQ(cageSnapshot(gs, car, TeamSide::HOME).cornersMarked, 0);

    Player& foe = gs.getPlayer(12);          // soupeř sousedící s rohem, ne s nosičem
    foe.id = 12; foe.teamSide = TeamSide::AWAY; foe.state = PlayerState::STANDING;
    foe.position = {11, 5}; foe.stats = {6, 3, 3, 8};

    CageSnapshot c = cageSnapshot(gs, car, TeamSide::HOME);
    EXPECT_EQ(c.corners, 1);
    EXPECT_EQ(c.cornersMarked, 1);
    EXPECT_EQ(c.carrierTz, 0) << "soupeř na nosiče nedosahuje";
}

TEST(BoardMetrics, TempoAchievableDropsWithCorridorResistance) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.homeTeam.turnNumber = 1;
    Player& car = gs.getPlayer(1);
    car.id = 1; car.teamSide = TeamSide::HOME; car.state = PlayerState::STANDING;
    car.position = {10, 7}; car.stats = {6, 3, 3, 8}; car.movementRemaining = 6;

    float clean = tempoSnapshot(gs, car, TeamSide::HOME).achievable;
    for (int i = 0; i < 3; ++i) {
        Player& e = gs.getPlayer(12 + i);
        e.id = 12 + i; e.teamSide = TeamSide::AWAY; e.state = PlayerState::STANDING;
        e.position = {static_cast<int8_t>(11 + i), 7}; e.stats = {6, 3, 3, 8};
    }
    float blocked = tempoSnapshot(gs, car, TeamSide::HOME).achievable;
    EXPECT_LT(blocked, clean) << "zeď musí srazit dosažitelné tempo";
    EXPECT_FLOAT_EQ(blocked, clean - 2.0f);   // přirážka min(2, (3+1)/2) = 2
}

// P154/K2 (06.10.2026): když nejdelší krok nevyjde bez hodu, klec udělá kratší — nezahodí se
// celý plán. Soupeři na (14,5) a (14,9): krok 4 by vedl rohy přes jejich zóny (úhyb), krok 2
// postaví klec kolem (14,7) bez jediného hodu a nosič nestojí v ničí zóně.
TEST(CageAdvance, TakesAShorterStepWhenTheLongestNeedsDice) {
    GameState state = makeCageState();
    for (int id : {13, 14}) {
        Player& m = state.getPlayer(id);
        m.id = id; m.teamSide = TeamSide::AWAY;
        m.state = PlayerState::STANDING;
        m.position = {14, static_cast<int8_t>(id == 13 ? 5 : 9)};
        m.stats = {6, 3, 3, 8};
        m.movementRemaining = 6;
    }
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    ASSERT_TRUE(plan.valid);
    ASSERT_EQ(plan.verdict, CageAdvanceVerdict::PLAN_READY) << "postup, ne jen dostavění na místě";
    // pozitivní kontrola: krok je kratší než čtyři pole, která by nosič ušel (dřív = konec,
    // žádný plán). Od 07.10.2026 hráč, který by na roh musel uhýbat, o roh nesoutěží, takže
    // delší krok vypadne už při přidělování rohů a `shortenedFromStep` se nenastaví.
    EXPECT_LT(plan.step, 4);
    EXPECT_GE(plan.step, 1);

    // odehrát plán se skutečnými kostkami: žádný hod se nesmí pokazit, protože žádný není
    struct Ones : DiceRollerBase {
        int rollD6() override { return 1; }
        int rollD8() override { return 1; }
    } ones;
    for (const Macro& m : plan.macros) {
        auto r = greedyExpandMacro(state, m, ones);
        EXPECT_FALSE(r.turnover) << "makro klece hodilo kostkou (hráč " << m.playerId << ")";
    }
    const Player& carrier = state.getPlayer(1);
    EXPECT_EQ(carrier.position.x, 12 + plan.step) << "nosič postoupil o krok plánu";
    EXPECT_EQ(countTacklezones(state, carrier.position, TeamSide::HOME), 0) << "nosič nesmí stát v zóně";
    int corners = 0;
    for (int dx : {-1, 1}) for (int dy : {-1, 1}) {
        const Player* p = state.getPlayerAtPosition({static_cast<int8_t>(carrier.position.x + dx),
                                                     static_cast<int8_t>(carrier.position.y + dy)});
        if (p && p->teamSide == TeamSide::HOME && p->state == PlayerState::STANDING) ++corners;
    }
    EXPECT_EQ(corners, 4) << "klec má po tahu všechny čtyři rohy";
}

// P165 (07.10.2026): na roh klece jde přednostně hráč, kterého z něj soupeř neodtlačí
// (Stand Firm, u agilních Side Step), i když je o pole dál než hráč bez té dovednosti.
TEST(CageAdvance, CornerGoesToThePlayerWhoCannotBePushedOffIt) {
    auto assign = [](bool farPlayerHoldsCorner) {
        GameState state = makeCageState();
        // jen přední horní roh (13,6) je volný; kandidáti: hráč 4 hned vedle, hráč 6 o pole dál
        state.getPlayer(4).position = {14, 5};
        Player& far = state.getPlayer(6);
        far.id = 6; far.teamSide = TeamSide::HOME; far.state = PlayerState::STANDING;
        far.position = {15, 4}; far.stats = {4, 3, 2, 9}; far.movementRemaining = 4;
        if (farPlayerHoldsCorner) far.skills.add(SkillName::StandFirm);
        CageAdvancePlanner planner(nullptr, cageConfig(), 42);
        auto a = planner.tryAssign(state, state.getPlayer(1), 0, {});
        for (const auto& sa : a.slots) {
            if (sa.slot == Position{13, 6}) return sa.playerId;
        }
        return -1;
    };
    EXPECT_EQ(assign(true), 6) << "hráč se Stand Firm má přednost";
    EXPECT_EQ(assign(false), 4) << "pozitivní kontrola: bez dovednosti rozhoduje vzdálenost";
}

// P165 (07.10.2026): „MB na dvou protilehlých rozích“ — dva rohoví hráči s Mighty Blow skončí
// na úhlopříčně protilehlých rozích klece, když na ně oba dosáhnou.
TEST(CageAdvance, MightyBlowCornersEndUpOppositeEachOther) {
    auto mbCorners = [](bool swapPossible) {
        GameState state = makeCageState();
        // všichni čtyři budoucí rohoví hráči stojí opodál; MB mají dva, kteří by „nejbližším
        // polem“ skončili oba vpředu (stejný sloupec), ne proti sobě
        state.getPlayer(2).position = {14, 5};   // nejblíž přednímu hornímu (13,6)
        state.getPlayer(3).position = {14, 9};   // nejblíž přednímu dolnímu (13,8)
        state.getPlayer(4).position = {10, 5};   // nejblíž zadnímu hornímu (11,6)
        state.getPlayer(5).position = {10, 9};   // nejblíž zadnímu dolnímu (11,8)
        state.getPlayer(2).skills.add(SkillName::MightyBlow);
        state.getPlayer(3).skills.add(SkillName::MightyBlow);
        if (!swapPossible) {                      // nikdo nedosáhne dál než na svůj nejbližší roh
            for (int id : {2, 3, 4, 5}) state.getPlayer(id).movementRemaining = 1;
        }
        CageAdvancePlanner planner(nullptr, cageConfig(), 42);
        auto a = planner.tryAssign(state, state.getPlayer(1), 0, {});
        std::vector<Position> mb;
        for (const auto& sa : a.slots) {
            if (sa.playerId >= 0 && state.getPlayer(sa.playerId).hasSkill(SkillName::MightyBlow)) mb.push_back(sa.slot);
        }
        return mb;
    };
    {
        auto mb = mbCorners(true);
        ASSERT_EQ(mb.size(), 2u);
        EXPECT_NE(mb[0].x, mb[1].x) << "ne oba vpředu / oba vzadu";
        EXPECT_NE(mb[0].y, mb[1].y) << "a ne na téže straně ⇒ úhlopříčně proti sobě";
    }
    {   // pozitivní kontrola: když se prohodit nedá (nedosáhnou), zůstanou oba vpředu
        auto mb = mbCorners(false);
        ASSERT_EQ(mb.size(), 2u);
        EXPECT_EQ(mb[0].x, mb[1].x);
    }
}

// ---------------------------------------------------------------------------
// P174 — „soupeř dosáhne / nedosáhne na nosiče“ (uživatel 08.10.2026: „když postavíme klec se
// třemi rohy a necháme volný ten ve směru, odkud přijde blitz, tak je to víc chyba než správně
// postavené dva rohy“).
// ---------------------------------------------------------------------------
namespace {
GameState threatBoard(std::vector<Position> corners, Position opponent, int8_t oppMa) {
    GameState s;
    s.phase = GamePhase::PLAY;
    s.activeTeam = TeamSide::HOME;
    s.half = 1;
    s.weather = Weather::NICE;
    putPlayer(s, 1, TeamSide::HOME, {12, 7}, 6);
    int id = 2;
    for (Position c : corners) putPlayer(s, id++, TeamSide::HOME, c, 4);
    putPlayer(s, 12, TeamSide::AWAY, opponent, oppMa);
    s.ball = BallState::carried({12, 7}, 1);
    return s;
}
}  // namespace

TEST(BlitzThreat, OutOfReachIsZeroAndALoneCarrierInReachFacesAOneDieHit) {
    GameState far = threatBoard({}, {24, 7}, 6);              // 12 polí, dosah 6 + 2 − 1 = 7
    EXPECT_DOUBLE_EQ(blitzThreat(far, far.getPlayer(1)), 0.0);
    GameState near = threatBoard({}, {16, 7}, 6);             // dojde bez GFI, síla 3 proti 3 = jedna kostka
    EXPECT_NEAR(blitzThreat(near, near.getPlayer(1)), 0.33, 1e-9);
}

TEST(BlitzThreat, FourCornersTurnTheHitIntoTwoDiceChosenByTheCarrier) {
    GameState s = threatBoard({{11, 6}, {11, 8}, {13, 6}, {13, 8}}, {16, 7}, 6);
    // Na každém poli u nosiče asistují dva rohy (2 kostky, vybírá nosič = 0,11) a soupeř se na něj
    // musí z pole v zónách rohů teprve proúhýbat (AG 2 do tří zón: 6+), takže dojde jen v 1 z 6.
    const double t = blitzThreat(s, s.getPlayer(1));
    EXPECT_NEAR(t, 0.11 / 6.0, 1e-9);
    EXPECT_LT(t, 0.33) << "klec je bezpečnější než nosič sám";
}

TEST(BlitzThreat, TwoCornersTowardTheBlitzBeatThreeCornersWithTheHoleFacingIt) {
    // soupeř přichází zprava a s pohybem 2 dojde (se dvěma GFI) jen na pole před nosičem
    GameState two = threatBoard({{13, 6}, {13, 8}}, {16, 7}, 2);
    GameState threeWithHole = threatBoard({{11, 6}, {11, 8}, {13, 8}}, {16, 7}, 2);   // díra na (13,6)
    const double a = blitzThreat(two, two.getPlayer(1));
    const double b = blitzThreat(threeWithHole, threeWithHole.getPlayer(1));
    EXPECT_LT(a, b) << "dva rohy ve směru blitzu " << a << " × tři rohy s dírou k soupeři " << b;
    EXPECT_LE(a, 0.11 + 1e-9);
    EXPECT_GE(b, 0.20);
}

// Plánovač vybírá pole, kde je nosič po tahu v bezpečí před dobrou ranou, a mezi nimi to nejdál —
// i šikmo: na řádku nosiče stojí zeď, o tři řádky níž je volno.
TEST(CageAdvance, PicksTheFarthestSquareWhereTheCarrierIsSafeNotJustStraightAhead) {
    GameState state = makeCageState();
    for (int i = 0; i < 3; ++i) putPlayer(state, 13 + i, TeamSide::AWAY, {15, static_cast<int8_t>(5 + i)}, 6);
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    ASSERT_TRUE(plan.valid);
    ASSERT_GE(plan.step, 1);
    EXPECT_LE(plan.blitzThreat, 0.15) << "nosič po tahu není vystaven dobré ráně";
    Position target{-1, -1};
    for (const auto& m : plan.macros) if (m.playerId == 1) target = m.targetPos;
    ASSERT_TRUE(target.isOnPitch());
    EXPECT_EQ(countTacklezones(state, target, TeamSide::HOME, 1), 0) << "nosič nekončí v zóně soupeře";
}

// P177 (uživatel 08.10.2026: „rychlejší týmy mohou s klecí dojít spíše bezpečně než co nejdál —
// např. skaveni proti orkům, protože pak jim zbyde dost pohybu na TD později“; „skaveni a elfové
// stihnou TD za 2 kola … trpaslíci za 6“). Stejná pozice, jiný pohyb: rychlý tým má ve 2. kole
// časovou rezervu a vezme nejbezpečnější pole; pomalý ji nemá a jde co nejdál mezi bezpečnými.
TEST(CageAdvance, ATeamWithTimeToSpareTakesTheSafestSquareASlowTeamTheFarthest) {
    auto plan = [](int8_t ma) {
        GameState state = makeCageState();
        state.homeTeam.turnNumber = 2;
        for (int id = 1; id <= 5; ++id) {
            state.getPlayer(id).stats.movement = ma;
            state.getPlayer(id).movementRemaining = ma;
        }
        putPlayer(state, 13, TeamSide::AWAY, {20, 7}, 4);       // soupeř před klecí: čím dál, tím blíž k němu
        CageAdvancePlanner planner(nullptr, cageConfig(), 42);
        return planner.build(state);
    };
    const CageAdvancePlan slow = plan(4);    // do zóny 13 polí, tempo 2 ⇒ potřebuje 5 tahů ze 7: rezerva těsná
    const CageAdvancePlan fast = plan(8);    // tempo 4 ⇒ potřebuje 2 tahy: rezerva velká
    ASSERT_TRUE(slow.valid);
    EXPECT_GE(slow.step, 1) << "pomalý tým postupuje";
    if (fast.valid && fast.step >= 1) {
        EXPECT_LE(fast.blitzThreat, slow.blitzThreat + 1e-9) << "rychlý tým nebere větší riziko než pomalý";
    }
    EXPECT_LE(fast.valid ? fast.blitzThreat : 0.0, 0.15);
}

// P178 (uživatel 08.10.2026: „když nosič nemůže skórovat ani být v bezpečí — nesmí nastat … do té
// doby má být v kleci“). Rychlý nosič (MA 9) jen se třemi spoluhráči: čistá klec se čtyřmi rohy
// nevyjde nikde, takže předvýběr dřív prošel jen 24 polí NEJDÁL vpřed — všechna v dosahu dobré
// rány — a bezpečná pole blíž vůbec nezkusil; plán spadl do „nejmenšího zla“. Bezpečné pole přitom
// existuje (soupeř s pohybem 3 dosáhne jen na vzdálená pole).
TEST(CageAdvance, ScreenReachesTheSafeNearSquaresWhenTheFarOnesAreAllUnderAGoodHit) {
    auto build = [](unsigned off) {
        GameState state = makeCageState();
        state.homeTeam.turnNumber = 7;                          // bez časové rezervy ⇒ „stát“ není volba
        state.getPlayer(5).state = PlayerState::OFF_PITCH;      // jen tři spoluhráči
        state.getPlayer(5).position = {-1, -1};
        for (int id = 1; id <= 4; ++id) {
            state.getPlayer(id).stats.movement = 9;
            state.getPlayer(id).movementRemaining = 9;
        }
        for (int i = 0; i < 7; ++i) {
            Player& o = putPlayer(state, 13 + i, TeamSide::AWAY, {21, static_cast<int8_t>(1 + 2 * i)}, 3);
            o.stats.strength = 13;   // rána na tři kostky i proti třem rohům
            o.stats.agility = 6;     // a z dotyku s rohem uhne na 2+
        }
        setCageFeaturesOff(off);
        CageAdvancePlanner planner(nullptr, cageConfig(), 42);
        CageAdvancePlan plan = planner.build(state);
        setCageFeaturesOff(0);
        return plan;
    };
    const CageAdvancePlan on = build(0);
    ASSERT_TRUE(on.valid);
    EXPECT_GE(on.step, 1) << "klec postupuje";
    EXPECT_LE(on.blitzThreat, 0.15) << "a nosič po tahu není vystaven dobré ráně";
    const CageAdvancePlan off = build(kFeatScreenSpread);
    EXPECT_GT(off.valid ? off.blitzThreat : 1.0, 0.15) << "pozitivní kontrola: bez úpravy plán bezpečné pole nenašel";
}

// P190 (uživatel 07.10.2026: „ležící spoluhráč může vstát a dojít stát se rohem, pokud nevyžaduje
// dodge nebo riskantní hod“). Ani hráč s Dodge na roh z kontaktu neuhýbá: úhyb na 2+ s přehozem
// nevyjde jednou z 36 a turnover uprostřed stavby klece nechá nosiče samotného.
TEST(CageAdvance, CornerIsNotGivenEvenToADodgerWhoWouldHaveToDodge) {
    GameState state = makeCageState();
    state.getPlayer(5).position = {14, 10};
    state.getPlayer(5).stats.agility = 4;
    state.getPlayer(5).skills.add(SkillName::Dodge);
    putPlayer(state, 13, TeamSide::AWAY, {15, 11}, 6);
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    auto a = planner.tryAssign(state, state.getPlayer(1), 0, {});
    EXPECT_EQ(slotOwner(a, {13, 8}), -1) << "roh zůstává otevřený";
    setCageFeaturesOff(kFeatCornerNoDodge);
    auto b = planner.tryAssign(state, state.getPlayer(1), 0, {});
    setCageFeaturesOff(0);
    EXPECT_EQ(slotOwner(b, {13, 8}), 5) << "pozitivní kontrola: dřív ho hráč s Dodge dostal";
}

// P190: volné pole rohu, na které se hráč v rozpočtu pohybu nedostane (mezi ním a rohem stojí zeď
// soupeřů), se nepřiděluje — dřív hráč vyrazil, uvízl v půli cesty a roh zůstal prázdný.
TEST(CageAdvance, AnEmptyCornerBehindAWallOfOpponentsIsNotAssigned) {
    GameState state = makeCageState();
    state.getPlayer(5).position = {13, 12};                 // k rohu (13,8) rovně 4 pole
    state.getPlayer(5).stats.movement = 4;
    state.getPlayer(5).movementRemaining = 4;
    int id = 13;                                            // souvislá zeď na řádku 10: obejít ji je dál než 4 pole
    for (int x = 9; x <= 17; ++x) putPlayer(state, id++, TeamSide::AWAY, {static_cast<int8_t>(x), 10}, 6);
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    ASSERT_LT(pathFailProb(state, state.getPlayer(5), {13, 8}, 4, Position{-1, -1}), 0.0)
        << "předpoklad: hledač cest hlásí „v rozpočtu nedosažitelné“ (−1), ne cestu přes hod";
    auto a = planner.tryAssign(state, state.getPlayer(1), 0, {});
    EXPECT_EQ(slotOwner(a, {13, 8}), -1);
}
