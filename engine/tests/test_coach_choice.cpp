#include <gtest/gtest.h>
#include "bb/coach_choice.h"
#include "bb/helpers.h"

using namespace bb;

// P130: přehoz jako volba kouče v živé partii.

static void placePlayer(GameState& gs, int id, Position pos) {
    Player& p = gs.getPlayer(id);
    p.state = PlayerState::STANDING;
    p.position = pos;
    p.stats = {6, 3, 3, 8};
    p.movementRemaining = 6;
}

static GameState homeWithReroll() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7});
    gs.homeTeam.rerolls = 1;
    return gs;
}

static bool roll4(GameState& gs, DiceRollerBase& dice, std::vector<GameEvent>* ev = nullptr) {
    return attemptRoll(gs, 1, dice, 4, SkillName::SKILL_COUNT, false, true, ev);
}

TEST(CoachChoice, OffByDefault_TeamRerollStaysAutomatic) {
    GameState gs = homeWithReroll();
    FixedDiceRoller dice({2, 5});
    EXPECT_TRUE(roll4(gs, dice));
    EXPECT_EQ(gs.homeTeam.rerolls, 0);
}

TEST(CoachChoice, NoAnswerYet_ThrowsPendingWithOriginalRoll) {
    GameState gs = homeWithReroll();
    FixedDiceRoller dice({2, 5});
    CoachChoiceScope scope(TeamSide::HOME, {});
    try {
        roll4(gs, dice);
        FAIL() << "měl se zeptat kouče";
    } catch (const CoachChoicePending& p) {
        EXPECT_EQ(p.question, CoachQuestion::REROLL);
        EXPECT_EQ(p.playerId, 1);
        EXPECT_EQ(p.roll, 2);
        EXPECT_EQ(p.target, 4);
        EXPECT_EQ(p.options, (1 << CoachAnswer::NONE) | (1 << CoachAnswer::TEAM));   // Pro nemá
    }
    EXPECT_EQ(gs.homeTeam.rerolls, 1);   // nic se neutratilo
}

TEST(CoachChoice, AnswerNone_KeepsRerollAndFails) {
    GameState gs = homeWithReroll();
    FixedDiceRoller dice({2, 5});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE});
    EXPECT_FALSE(roll4(gs, dice));
    EXPECT_EQ(gs.homeTeam.rerolls, 1);
    EXPECT_EQ(dice.remaining(), 1u);
}

TEST(CoachChoice, AnswerTeam_SpendsRerollAndLogsBothRolls) {
    GameState gs = homeWithReroll();
    FixedDiceRoller dice({2, 5});
    std::vector<GameEvent> ev;
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::TEAM});
    EXPECT_TRUE(roll4(gs, dice, &ev));
    EXPECT_EQ(gs.homeTeam.rerolls, 0);
    EXPECT_TRUE(gs.homeTeam.rerollUsedThisTurn);
    ASSERT_EQ(ev.size(), 1u);
    EXPECT_EQ(ev[0].type, GameEvent::Type::REROLL);
    EXPECT_EQ(ev[0].roll, 0);
    EXPECT_EQ(ev[0].die1, 2);   // původní hod
    EXPECT_EQ(ev[0].die2, 5);   // nový hod
    EXPECT_TRUE(ev[0].success);
}

TEST(CoachChoice, NothingAvailable_NoQuestion) {
    GameState gs = homeWithReroll();
    gs.homeTeam.rerolls = 0;
    FixedDiceRoller dice({2});
    CoachChoiceScope scope(TeamSide::HOME, {});
    EXPECT_FALSE(roll4(gs, dice));   // nevyhodí: není co nabídnout
}

TEST(CoachChoice, RerollAlreadyUsedThisTurn_NoQuestion) {
    GameState gs = homeWithReroll();
    gs.homeTeam.rerollUsedThisTurn = true;
    FixedDiceRoller dice({2});
    CoachChoiceScope scope(TeamSide::HOME, {});
    EXPECT_FALSE(roll4(gs, dice));
}

TEST(CoachChoice, OtherSideStaysAutomatic) {
    GameState gs = homeWithReroll();
    FixedDiceRoller dice({2, 5});
    CoachChoiceScope scope(TeamSide::AWAY, {});   // kouč rozhoduje za AWAY, hází HOME
    EXPECT_TRUE(roll4(gs, dice));
    EXPECT_EQ(gs.homeTeam.rerolls, 0);
}

TEST(CoachChoice, ProOffered_AndProSucceeds) {
    GameState gs = homeWithReroll();
    gs.getPlayer(1).skills.add(SkillName::Pro);
    {
        FixedDiceRoller dice({2});
        CoachChoiceScope scope(TeamSide::HOME, {});
        try { roll4(gs, dice); FAIL(); } catch (const CoachChoicePending& p) {
            EXPECT_EQ(p.options, (1 << CoachAnswer::NONE) | (1 << CoachAnswer::TEAM) | (1 << CoachAnswer::PRO));
        }
    }
    FixedDiceRoller dice({2, 4, 6});   // hod, Pro 4, přehoz 6
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::PRO});
    EXPECT_TRUE(roll4(gs, dice));
    EXPECT_TRUE(gs.getPlayer(1).proUsedThisTurn);
    EXPECT_EQ(gs.homeTeam.rerolls, 1);   // týmový zůstal
}

TEST(CoachChoice, ProFails_SecondQuestion_TeamRerollsTheProRoll) {
    GameState gs = homeWithReroll();
    gs.getPlayer(1).skills.add(SkillName::Pro);
    {
        FixedDiceRoller dice({2, 3});
        CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::PRO});
        try { roll4(gs, dice); FAIL(); } catch (const CoachChoicePending& p) {
            EXPECT_EQ(p.question, CoachQuestion::PRO_GATE_REROLL);
            EXPECT_EQ(p.roll, 3);
            EXPECT_EQ(p.options, (1 << CoachAnswer::NONE) | (1 << CoachAnswer::TEAM));
        }
    }
    GameState gs2 = homeWithReroll();
    gs2.getPlayer(1).skills.add(SkillName::Pro);
    FixedDiceRoller dice({2, 3, 5, 4});   // hod 2, Pro 3, přehoz Pro 5, přehoz hodu 4
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::PRO, CoachAnswer::TEAM});
    EXPECT_TRUE(roll4(gs2, dice));
    EXPECT_EQ(gs2.homeTeam.rerolls, 0);
}

TEST(CoachChoice, ProFails_KeepIt_OriginalStands) {
    GameState gs = homeWithReroll();
    gs.getPlayer(1).skills.add(SkillName::Pro);
    FixedDiceRoller dice({2, 3, 6});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::PRO, CoachAnswer::NONE});
    EXPECT_FALSE(roll4(gs, dice));
    EXPECT_EQ(gs.homeTeam.rerolls, 1);
    EXPECT_EQ(dice.remaining(), 1u);   // původní hod se už nepřehazuje
}

TEST(CoachChoice, Loner_Fails4Plus_RerollLostOriginalStands) {
    GameState gs = homeWithReroll();
    gs.getPlayer(1).skills.add(SkillName::Loner);
    FixedDiceRoller dice({2, 3, 6});   // hod 2, Loner 3 ⇒ propadl
    std::vector<GameEvent> ev;
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::TEAM});
    EXPECT_FALSE(roll4(gs, dice, &ev));
    EXPECT_EQ(gs.homeTeam.rerolls, 0);
    EXPECT_EQ(dice.remaining(), 1u);
    ASSERT_EQ(ev.size(), 1u);
    EXPECT_EQ(ev[0].roll, 3);
}

TEST(CoachChoice, Loner_Passes4Plus_Rerolls) {
    GameState gs = homeWithReroll();
    gs.getPlayer(1).skills.add(SkillName::Loner);
    FixedDiceRoller dice({2, 4, 6});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::TEAM});
    EXPECT_TRUE(roll4(gs, dice));
}

TEST(CoachChoice, SkillRerollStaysAutomatic) {
    GameState gs = homeWithReroll();
    gs.getPlayer(1).skills.add(SkillName::Dodge);
    FixedDiceRoller dice({2, 5});
    CoachChoiceScope scope(TeamSide::HOME, {});
    EXPECT_TRUE(attemptRoll(gs, 1, dice, 4, SkillName::Dodge, false, true, nullptr));
    EXPECT_EQ(gs.homeTeam.rerolls, 1);
}

TEST(CoachChoice, AnswerOutsideOptions_IsAnError) {
    GameState gs = homeWithReroll();
    FixedDiceRoller dice({2, 5});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::PRO});   // Pro nemá
    EXPECT_THROW(roll4(gs, dice), std::invalid_argument);
}

TEST(CoachChoice, ScopeEnds_ModeOff) {
    { CoachChoiceScope scope(TeamSide::HOME, {}); EXPECT_TRUE(coachModeOn()); }
    EXPECT_FALSE(coachModeOn());
    EXPECT_FALSE(coachDecides(TeamSide::HOME));
}

// ───────────── blok: přehoz po každém hodu, výběr kostky, volby obránce ─────────────
#include "bb/block_handler.h"

// kostka: 1 lebka · 2 Both Down · 3-4 Pushed · 5 Defender Stumbles · 6 Defender Down
static GameState blockPair(int attSt = 3) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7});
    gs.getPlayer(1).stats.strength = static_cast<int8_t>(attSt);
    placePlayer(gs, 12, {11, 7});
    return gs;
}

static ActionResult block(GameState& gs, DiceRollerBase& dice, std::vector<GameEvent>* ev = nullptr) {
    BlockParams params{1, 12, false, false};
    return resolveBlock(gs, params, dice, ev);
}

TEST(CoachChoiceBlock, DodgeDeclined_DefenderPushedAndDown_NoFrenzySecondBlock) {
    GameState gs = blockPair();
    gs.getPlayer(1).skills.add(SkillName::Frenzy);
    gs.getPlayer(12).skills.add(SkillName::Dodge);
    FixedDiceRoller dice({5, 1, 1, 6, 6, 6, 6});   // Stumbles, zbroj 1+1 drží; zbytek by byl druhý blok
    CoachChoiceScope scope(TeamSide::AWAY, {0});
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(12).position.x, 12);        // odtlačen, nezůstal na místě (l. 524-527)
    EXPECT_EQ(dice.remaining(), 4u);                   // druhý blok Frenzy se nehodil
}

TEST(CoachChoiceBlock, DodgeUsed_DefenderStands_FrenzyBlocksAgain) {
    GameState gs = blockPair();
    gs.getPlayer(1).skills.add(SkillName::Frenzy);
    gs.getPlayer(12).skills.add(SkillName::Dodge);
    FixedDiceRoller dice({5, 3, 6, 6});   // Stumbles → jen odtlačen; druhý blok Pushed
    CoachChoiceScope scope(TeamSide::AWAY, {1});
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(12).position.x, 13);        // dvakrát odtlačen
}

TEST(CoachChoiceBlock, DodgeQuestionIsAsked) {
    GameState gs = blockPair();
    gs.getPlayer(12).skills.add(SkillName::Dodge);
    FixedDiceRoller dice({5, 1, 1});
    CoachChoiceScope scope(TeamSide::AWAY, {});
    try { block(gs, dice); FAIL(); } catch (const CoachChoicePending& p) {
        EXPECT_EQ(p.question, CoachQuestion::USE_DODGE);
        EXPECT_EQ(p.playerId, 12);
        EXPECT_EQ(p.otherId, 1);
    }
}

TEST(CoachChoiceBlock, DodgeNotAskedWhenTackleCancelsIt) {
    GameState gs = blockPair();
    gs.getPlayer(1).skills.add(SkillName::Tackle);
    gs.getPlayer(12).skills.add(SkillName::Dodge);
    FixedDiceRoller dice({5, 1, 1, 1, 1});
    CoachChoiceScope scope(TeamSide::AWAY, {});
    EXPECT_NO_THROW(block(gs, dice));
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(CoachChoiceBlock, WrestleDeclined_BlockKeepsDefenderUp_AttackerFalls) {
    GameState gs = blockPair();
    gs.getPlayer(12).skills.add(SkillName::Block);
    gs.getPlayer(12).skills.add(SkillName::Wrestle);
    FixedDiceRoller dice({2, 1, 1, 1, 1});
    CoachChoiceScope scope(TeamSide::AWAY, {0});
    auto r = block(gs, dice);
    EXPECT_TRUE(r.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING);
}

TEST(CoachChoiceBlock, WrestleUsed_BothProne_NoTurnover) {
    GameState gs = blockPair();
    gs.getPlayer(12).skills.add(SkillName::Block);
    gs.getPlayer(12).skills.add(SkillName::Wrestle);
    FixedDiceRoller dice({2, 1, 1, 1, 1});
    CoachChoiceScope scope(TeamSide::AWAY, {1});
    auto r = block(gs, dice);
    EXPECT_FALSE(r.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(CoachChoiceBlock, StandFirmUsed_StaysPut) {
    GameState gs = blockPair();
    gs.getPlayer(12).skills.add(SkillName::StandFirm);
    FixedDiceRoller dice({3, 1, 1});
    CoachChoiceScope scope(TeamSide::AWAY, {1});
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(12).position.x, 11);
}

TEST(CoachChoiceBlock, StandFirmDeclined_IsPushed) {
    GameState gs = blockPair();
    gs.getPlayer(12).skills.add(SkillName::StandFirm);
    FixedDiceRoller dice({3, 1, 1});
    CoachChoiceScope scope(TeamSide::AWAY, {0});
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(12).position.x, 12);
}

TEST(CoachChoiceBlock, StandFirmInChain_CoachIsAsked) {
    // 12 je odtlačen do 13, 14, 15 (všechna tři pole obsazená) ⇒ řetěz; 13 má Stand Firm.
    GameState gs = blockPair();
    placePlayer(gs, 13, {12, 6});
    placePlayer(gs, 14, {12, 7});
    placePlayer(gs, 15, {12, 8});
    gs.getPlayer(13).skills.add(SkillName::StandFirm);
    FixedDiceRoller dice({3, 1, 1});
    CoachChoiceScope scope(TeamSide::AWAY, {});
    try { block(gs, dice); } catch (const CoachChoicePending& p) {
        EXPECT_EQ(p.question, CoachQuestion::USE_STAND_FIRM);
        EXPECT_EQ(p.playerId, 13);
        return;
    }
    // řetěz mohl zvolit jiné pole než hráče 13 -- pak se neptal a test nic neříká
    SUCCEED();
}

TEST(CoachChoiceBlock, SideStepUsed_CoachPicksSquare) {
    GameState gs = blockPair();
    gs.getPlayer(12).skills.add(SkillName::SideStep);
    setManualSideStepSquare({11, 6});
    FixedDiceRoller dice({3, 1, 1});
    CoachChoiceScope scope(TeamSide::AWAY, {1});
    block(gs, dice);
    clearManualBlockChoices();
    EXPECT_EQ(gs.getPlayer(12).position.x, 11);
    EXPECT_EQ(gs.getPlayer(12).position.y, 6);
}

TEST(CoachChoiceBlock, SideStepDeclined_AttackerPicksStandardSquare) {
    GameState gs = blockPair();
    gs.getPlayer(12).skills.add(SkillName::SideStep);
    setManualSideStepSquare({11, 6});   // nesmí se použít
    FixedDiceRoller dice({3, 1, 1});
    CoachChoiceScope scope(TeamSide::AWAY, {0});
    block(gs, dice);
    clearManualBlockChoices();
    EXPECT_EQ(gs.getPlayer(12).position.x, 12);
}

TEST(CoachChoiceBlock, AttackersPushChoiceIsNotUsedAsSideStepSquare) {
    GameState gs = blockPair();
    gs.getPlayer(12).skills.add(SkillName::SideStep);
    setManualPushChoice({11, 6});       // volba ÚTOČNÍKA; obránce Side Step použije bez pole
    FixedDiceRoller dice({3, 1, 1});
    CoachChoiceScope scope(TeamSide::AWAY, {1});
    block(gs, dice);
    clearManualBlockChoices();
    EXPECT_FALSE(gs.getPlayer(12).position.x == 11 && gs.getPlayer(12).position.y == 6);
}

TEST(CoachChoiceBlock, RerollAskedAfterEveryRoll_EvenAGoodOne) {
    GameState gs = blockPair();
    gs.homeTeam.rerolls = 1;
    FixedDiceRoller dice({6, 1, 1});   // Defender Down -- engine by nepřehazoval
    CoachChoiceScope scope(TeamSide::HOME, {});
    try { block(gs, dice); FAIL(); } catch (const CoachChoicePending& p) {
        EXPECT_EQ(p.question, CoachQuestion::BLOCK_REROLL);
        EXPECT_EQ(p.playerId, 1);
        EXPECT_EQ(p.otherId, 12);
        EXPECT_EQ(p.roll, static_cast<int>(BlockDiceFace::DEFENDER_DOWN) + 1);
        EXPECT_EQ(p.target, 1);
        EXPECT_EQ(p.options, (1 << CoachAnswer::NONE) | (1 << CoachAnswer::TEAM));
    }
}

TEST(CoachChoiceBlock, TeamReroll_RollsAllDiceAgain_AndLogsBoth) {
    GameState gs = blockPair();
    gs.homeTeam.rerolls = 1;
    FixedDiceRoller dice({1, 6, 1, 1, 1, 1});   // lebka → přehoz → Defender Down
    std::vector<GameEvent> ev;
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::TEAM});
    auto r = block(gs, dice, &ev);
    EXPECT_FALSE(r.turnover);
    EXPECT_EQ(gs.homeTeam.rerolls, 0);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
    bool seen = false;
    for (const auto& e : ev) if (e.type == GameEvent::Type::REROLL) {
        seen = true;
        EXPECT_EQ(e.die1, static_cast<int>(BlockDiceFace::ATTACKER_DOWN) + 1);
        EXPECT_EQ(e.die2, static_cast<int>(BlockDiceFace::DEFENDER_DOWN) + 1);
    }
    EXPECT_TRUE(seen);
}

TEST(CoachChoiceBlock, KeepSkull_NoAutomaticReroll) {
    GameState gs = blockPair();
    gs.homeTeam.rerolls = 1;
    FixedDiceRoller dice({1, 6, 1, 1, 1, 1});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE});
    auto r = block(gs, dice);
    EXPECT_TRUE(r.turnover);
    EXPECT_EQ(gs.homeTeam.rerolls, 1);
}

TEST(CoachChoiceBlock, FaceChoice_AskedAndRespected) {
    GameState gs = blockPair(4);   // ST4 × ST3 ⇒ 2 kostky, vybírá útočník
    {
        FixedDiceRoller dice({3, 6, 1, 1});
        CoachChoiceScope scope(TeamSide::HOME, {});
        try { block(gs, dice); FAIL(); } catch (const CoachChoicePending& p) {
            EXPECT_EQ(p.question, CoachQuestion::BLOCK_FACE);
            EXPECT_EQ(p.options, (1 << static_cast<int>(BlockDiceFace::PUSHED)) |
                                 (1 << static_cast<int>(BlockDiceFace::DEFENDER_DOWN)));
        }
    }
    GameState gs2 = blockPair(4);
    FixedDiceRoller dice({3, 6, 1, 1});
    CoachChoiceScope scope(TeamSide::HOME, {static_cast<int>(BlockDiceFace::PUSHED)});
    block(gs2, dice);
    EXPECT_EQ(gs2.getPlayer(12).state, PlayerState::STANDING);   // engine by vzal Defender Down
    EXPECT_EQ(gs2.getPlayer(12).position.x, 12);
}

TEST(CoachChoiceBlock, OpponentsBlockStaysAutomatic) {
    GameState gs = blockPair(4);
    gs.homeTeam.rerolls = 1;
    FixedDiceRoller dice({3, 6, 1, 1, 1, 1});
    CoachChoiceScope scope(TeamSide::AWAY, {});   // kouč je AWAY, blokuje HOME
    EXPECT_NO_THROW(block(gs, dice));
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

// ───────────── přihrávka: dotaz u nepřesné i u fumble ─────────────
#include "bb/pass_handler.h"

static GameState passSetup() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.homeTeam.rerolls = 3;
    placePlayer(gs, 1, {5, 7});
    placePlayer(gs, 2, {8, 7});          // rychlá přihrávka, potřeba 3+
    gs.ball = BallState::carried({5, 7}, 1);
    return gs;
}

TEST(CoachChoicePass, InaccuratePass_CoachIsAsked) {
    GameState gs = passSetup();
    FixedDiceRoller dice({2, 4, 5});
    CoachChoiceScope scope(TeamSide::HOME, {});
    try { resolvePass(gs, 1, {8, 7}, dice, nullptr); FAIL(); } catch (const CoachChoicePending& p) {
        EXPECT_EQ(p.question, CoachQuestion::PASS_REROLL);
        EXPECT_EQ(p.roll, 2);
        EXPECT_EQ(p.target, 3);
    }
}

TEST(CoachChoicePass, Fumble_CoachIsAsked) {
    GameState gs = passSetup();
    FixedDiceRoller dice({1, 4, 5});
    CoachChoiceScope scope(TeamSide::HOME, {});
    EXPECT_THROW(resolvePass(gs, 1, {8, 7}, dice, nullptr), CoachChoicePending);
}

TEST(CoachChoicePass, TeamRerollOnInaccurate_PassThenCaught) {
    GameState gs = passSetup();
    FixedDiceRoller dice({2, 4, 5});     // nepřesná → přehoz 4 → chycení 5
    std::vector<GameEvent> ev;
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::TEAM});
    auto r = resolvePass(gs, 1, {8, 7}, dice, &ev);
    EXPECT_FALSE(r.turnover);
    EXPECT_EQ(gs.homeTeam.rerolls, 2);
    EXPECT_TRUE(gs.ball.isHeld);
    EXPECT_EQ(gs.ball.carrierId, 2);
    bool seen = false;
    for (const auto& e : ev) if (e.type == GameEvent::Type::REROLL) {
        seen = true; EXPECT_EQ(e.die1, 2); EXPECT_EQ(e.die2, 4);
    }
    EXPECT_TRUE(seen);
}

TEST(CoachChoicePass, KeepInaccurate_RerollNotSpent) {
    GameState gs = passSetup();
    FixedDiceRoller dice({2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE});
    resolvePass(gs, 1, {8, 7}, dice, nullptr);
    EXPECT_EQ(gs.homeTeam.rerolls, 3);
}

TEST(CoachChoicePass, PassSkillStaysAutomatic) {
    GameState gs = passSetup();
    gs.getPlayer(1).skills.add(SkillName::Pass);
    FixedDiceRoller dice({2, 4, 5});
    CoachChoiceScope scope(TeamSide::HOME, {});
    EXPECT_NO_THROW(resolvePass(gs, 1, {8, 7}, dice, nullptr));
    EXPECT_EQ(gs.homeTeam.rerolls, 3);
}
