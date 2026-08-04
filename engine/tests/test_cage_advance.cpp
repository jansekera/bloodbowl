#include <gtest/gtest.h>
#include "bb/cage_advance.h"
#include "bb/macro_mcts.h"
#include "bb/turn_planner.h"
#include "bb/game_state.h"
#include "bb/action_resolver.h"
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
    // Schedule-driven step: meet the pace, never outrun it -- even though
    // the roles could sustain step 4 (MA4 all around, straight walks).
    EXPECT_EQ(plan.step, 2);
    EXPECT_EQ(plan.rawAchievableStep, 4);
    EXPECT_EQ(plan.resistance, 0);
    EXPECT_NEAR(plan.achievablePace, 4.0, 1e-9);
    // Schedule fits within plain MA -> the carrier leg must stay dice-free.
    EXPECT_EQ(plan.carrierGfi, 0);
    EXPECT_EQ(plan.macros.back().gfiAllowance, 0);
}

TEST(CageAdvance, TempoInsufficientWhenBehindSchedule) {
    GameState state = makeCageState();
    state.homeTeam.turnNumber = 6;  // turnsLeft 3, usable 2 -> required 6.5
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    EXPECT_EQ(plan.verdict, CageAdvanceVerdict::TEMPO_INSUFFICIENT);
    EXPECT_FALSE(plan.valid);
    EXPECT_TRUE(plan.macros.empty()) << "no blind push on insufficient tempo";
}

TEST(CageAdvance, TempoInsufficientWhenNoUsableTurnsLeft) {
    GameState state = makeCageState();
    state.homeTeam.turnNumber = 8;  // turnsLeft 1, reserve eats it -> usable 0
    CageAdvancePlanner planner(nullptr, cageConfig(), 42);
    CageAdvancePlan plan = planner.build(state);
    EXPECT_EQ(plan.verdict, CageAdvanceVerdict::TEMPO_INSUFFICIENT);
}

TEST(CageAdvance, OpponentScreenInCorridorKillsTempo) {
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
    EXPECT_EQ(plan.verdict, CageAdvanceVerdict::TEMPO_INSUFFICIENT);
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

TEST(CageAdvance, CarrierTargetBlockedByTeammateGetsVacatedFirst) {
    // A teammate parked straight ahead of the carrier (post-scrum pile,
    // user doctrine: "the ones in FRONT move first so they stop blocking").
    // He is drafted into a corner slot of the NEW cage and his macro runs
    // before the carrier's.
    GameState state = makeCageState();
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

    // Carrier strictly LAST: the screen forms before the ball commits.
    const Macro& last = plan.macros.back();
    EXPECT_EQ(last.playerId, 1);
    EXPECT_EQ(last.targetPos, (Position{14, 7}));
    for (size_t i = 0; i + 1 < plan.macros.size(); ++i) {
        EXPECT_NE(plan.macros[i].playerId, 1);
    }
    // Front slots are claimed first (macro order = slot order).
    EXPECT_EQ(plan.macros[0].targetPos, (Position{15, 6}));
    EXPECT_EQ(plan.macros[1].targetPos, (Position{15, 8}));

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
    EXPECT_EQ(state.getPlayer(1).position, (Position{14, 7}));
    int corners = 0;
    for (auto& d : state.getPlayer(1).position.getAdjacent()) {
        const Player* p = state.getPlayerAtPosition(d);
        if (p && p->teamSide == TeamSide::HOME &&
            std::abs(d.x - 14) == 1 && std::abs(d.y - 7) == 1) {
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
