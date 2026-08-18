#include <gtest/gtest.h>
#include "bb/cage_advance.h"
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
    cfg.cageAdvance = enable;
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

TEST(CageAdvance, ConfigDefaultIsOff) {
    MCTSConfig cfg;
    EXPECT_FALSE(cfg.cageAdvance);
}

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

    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    ASSERT_TRUE(plan.valid) << "verdict=" << static_cast<int>(plan.verdict);
    EXPECT_EQ(plan.gfiCorners, 1);
    EXPECT_EQ(plan.openCorners, 1);
    EXPECT_EQ(plan.filledCorners, 3);
    // REPOSITION never actually GFIs (dice-free contract): the allowance
    // corner walks and may stop one square short -- but it IS in the plan.
    EXPECT_TRUE(hasMacroFor(plan, 6));
    EXPECT_FALSE(hasMacroFor(plan, 7));
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

TEST(CageAdvancePolicy, CornersActBeforeCarrierWhenEnabled) {
    GameState state = makeCageState();
    MacroMCTSPolicy policy(nullptr, cageConfig(true), 42);
    DiceRoller dice(123);

    std::vector<int> actingOrder;
    for (int step = 0; step < 80; ++step) {
        if (state.phase != GamePhase::PLAY || state.activeTeam != TeamSide::HOME) break;
        Action a = policy(state);
        ASSERT_TRUE(actionIsAvailable(state, a)) << "step " << step;
        if (a.playerId > 0 &&
            (actingOrder.empty() || actingOrder.back() != a.playerId)) {
            actingOrder.push_back(a.playerId);
        }
        executeAction(state, a, dice, nullptr);
        if (a.type == ActionType::END_TURN) break;
    }

    auto carrierIt = std::find(actingOrder.begin(), actingOrder.end(), 1);
    ASSERT_NE(carrierIt, actingOrder.end()) << "carrier never activated";
    EXPECT_NE(carrierIt, actingOrder.begin())
        << "carrier acted first -- corners were not sequenced before the ball";
    // The staged plan's carrier leg landed (>= plan target x=14). After the
    // plan completes, the rest of the turn belongs to production search(),
    // which may legitimately spend the carrier's leftover movement on a
    // further ADVANCE -- so assert at-least, not exact (open question for
    // the A/B harness, see evidence report).
    EXPECT_GE(state.getPlayer(1).position.x, 14);
    EXPECT_EQ(state.getPlayer(1).position.y, 7);
    // Diagnostics counter the A/B harness reports ("did the gate fire?").
    EXPECT_EQ(policy.stagedPlansAdopted(), 1);
}

TEST(CageAdvancePolicy, DisabledGateMatchesSearchPath) {
    // Gate off vs gate on-but-inert (no cage built -> NOT_APPLICABLE) must
    // both go through plain search(): first action identical at equal seed.
    GameState noCage = makeCageState();
    noCage.getPlayer(2).state = PlayerState::OFF_PITCH;
    noCage.getPlayer(3).state = PlayerState::OFF_PITCH;
    noCage.getPlayer(4).state = PlayerState::OFF_PITCH;
    noCage.getPlayer(5).state = PlayerState::OFF_PITCH;

    MacroMCTSPolicy off(nullptr, cageConfig(false), 42);
    MacroMCTSPolicy onButInert(nullptr, cageConfig(true), 42);

    GameState s1 = noCage.clone();
    GameState s2 = noCage.clone();
    Action a1 = off(s1);
    Action a2 = onButInert(s2);
    EXPECT_EQ(a1.type, a2.type);
    EXPECT_EQ(a1.playerId, a2.playerId);
    EXPECT_EQ(a1.target, a2.target);
}

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
    if (plan.valid) {
        EXPECT_GE(plan.step, 1) << "a fully marked corridor must not freeze the cage";
    }
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
    int id = 13;
    for (int y = 5; y <= 9; ++y) {
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
    if (plan.valid && plan.verdict == CageAdvanceVerdict::FILL_ONLY) {
        EXPECT_EQ(plan.step, 0) << "fill never moves the carrier";
        EXPECT_EQ(plan.carrierGfi, 0) << "fill never buys dice";
        EXPECT_FALSE(plan.macros.empty());
        for (const auto& m : plan.macros) {
            EXPECT_NE(m.playerId, 1) << "the carrier must not be in a fill plan";
        }
    }
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
