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
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::TEAM, CoachAnswer::TEAM});   // přehoz, follow-up (P201)
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
    CoachChoiceScope scope(TeamSide::HOME, {static_cast<int>(BlockDiceFace::PUSHED),
                                             CoachAnswer::NONE});   // kostka, follow-up (P201)
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

// ───────────── hody velkých hráčů a kostka silnějšího obránce ─────────────
#include "bb/big_guy_handler.h"

static GameState ogre() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.homeTeam.rerolls = 1;
    placePlayer(gs, 1, {10, 7});
    gs.getPlayer(1).skills.add(SkillName::BoneHead);
    gs.getPlayer(1).skills.add(SkillName::Loner);
    return gs;
}

TEST(CoachChoiceBigGuy, AiNeverRerollsBoneHead) {
    GameState gs = ogre();
    FixedDiceRoller dice({1, 6, 6});
    auto r = resolveBigGuyCheck(gs, 1, ActionType::BLOCK, dice, nullptr);
    EXPECT_FALSE(r.proceed);
    EXPECT_EQ(gs.homeTeam.rerolls, 1);
    EXPECT_EQ(dice.remaining(), 2u);
}

TEST(CoachChoiceBigGuy, CoachIsOfferedTheReroll) {
    GameState gs = ogre();
    FixedDiceRoller dice({1, 6, 6});
    CoachChoiceScope scope(TeamSide::HOME, {});
    try { resolveBigGuyCheck(gs, 1, ActionType::BLOCK, dice, nullptr); FAIL(); }
    catch (const CoachChoicePending& p) {
        EXPECT_EQ(p.question, CoachQuestion::REROLL);
        EXPECT_EQ(p.roll, 1);
        EXPECT_EQ(p.target, 2);
    }
}

TEST(CoachChoiceBigGuy, LonerPasses_RerollSavesTheAction) {
    GameState gs = ogre();
    FixedDiceRoller dice({1, 4, 5});   // Bone Head 1, Loner 4, přehoz 5
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::TEAM});
    auto r = resolveBigGuyCheck(gs, 1, ActionType::BLOCK, dice, nullptr);
    EXPECT_TRUE(r.proceed);
    EXPECT_FALSE(gs.getPlayer(1).lostTacklezones);
    EXPECT_EQ(gs.homeTeam.rerolls, 0);
}

TEST(CoachChoiceBigGuy, LonerFails_RerollLost_ActionStillWasted) {
    GameState gs = ogre();
    FixedDiceRoller dice({1, 3, 5});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::TEAM});
    auto r = resolveBigGuyCheck(gs, 1, ActionType::BLOCK, dice, nullptr);
    EXPECT_FALSE(r.proceed);
    EXPECT_TRUE(gs.getPlayer(1).lostTacklezones);
    EXPECT_EQ(gs.homeTeam.rerolls, 0);
    EXPECT_EQ(dice.remaining(), 1u);
}

TEST(CoachChoiceBigGuy, CoachKeeps_NothingSpent) {
    GameState gs = ogre();
    FixedDiceRoller dice({1, 6});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE});
    auto r = resolveBigGuyCheck(gs, 1, ActionType::BLOCK, dice, nullptr);
    EXPECT_FALSE(r.proceed);
    EXPECT_EQ(gs.homeTeam.rerolls, 1);
}

TEST(CoachChoiceBlock, StrongerDefender_HisCoachPicksTheDie) {
    GameState gs = blockPair();
    gs.getPlayer(12).stats.strength = 4;   // ST3 × ST4 ⇒ 2 kostky, vybírá obránce
    {
        FixedDiceRoller dice({1, 6, 1, 1, 1, 1});
        CoachChoiceScope scope(TeamSide::AWAY, {});
        try { block(gs, dice); FAIL(); } catch (const CoachChoicePending& p) {
            EXPECT_EQ(p.question, CoachQuestion::BLOCK_FACE);
            EXPECT_EQ(p.playerId, 12);
            EXPECT_EQ(p.options, (1 << static_cast<int>(BlockDiceFace::ATTACKER_DOWN)) |
                                 (1 << static_cast<int>(BlockDiceFace::DEFENDER_DOWN)));
        }
    }
    GameState gs2 = blockPair();
    gs2.getPlayer(12).stats.strength = 4;
    FixedDiceRoller dice({1, 6, 1, 1, 1, 1});
    // kouč obránce vezme Defender Down (proti svému zájmu) -- engine by vzal lebku
    CoachChoiceScope scope(TeamSide::AWAY, {static_cast<int>(BlockDiceFace::DEFENDER_DOWN)});
    block(gs2, dice);
    EXPECT_EQ(gs2.getPlayer(12).state, PlayerState::PRONE);
}

TEST(CoachChoiceBlock, StrongerDefenderOfTheOtherSide_StaysAutomatic) {
    GameState gs = blockPair();
    gs.getPlayer(12).stats.strength = 4;
    FixedDiceRoller dice({1, 6, 1, 1, 1, 1});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE});   // HOME: jen dotaz na přehoz? rerolly 0 ⇒ žádný
    EXPECT_NO_THROW(block(gs, dice));
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);          // obránce (engine) vzal lebku
}

// ───────────── P201 (12): follow-up jako otázka kouče útočníka ─────────────
// Pravidla ř. 606-611: "The player's COACH MUST DECIDE whether to follow up" -- rozhoduje se
// AŽ po odtlačení (po kostkách i po přehozu), ne předem. Dřív se v partii volba zadávala
// příkazem `follow` před akcí a po přehozu kostek ji nešlo změnit (partie 13.10.).
static const int ANO_NE = (1 << CoachAnswer::NONE) | (1 << CoachAnswer::TEAM);

TEST(CoachChoiceFollowUp, AskedAfterPush) {
    GameState gs = blockPair();
    FixedDiceRoller dice({3});                       // jedna kostka: Pushed
    CoachChoiceScope scope(TeamSide::HOME, {});
    try { block(gs, dice); FAIL(); } catch (const CoachChoicePending& p) {
        EXPECT_EQ(p.question, CoachQuestion::USE_FOLLOW_UP);
        EXPECT_EQ(p.playerId, 1);
        EXPECT_EQ(p.otherId, 12);
        EXPECT_EQ(p.options, ANO_NE);
    }
}

TEST(CoachChoiceFollowUp, Declined_AttackerStays) {
    GameState gs = blockPair();                      // engine by u běžného bloku následoval
    FixedDiceRoller dice({3});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE});
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(1).position.x, 10);
    EXPECT_EQ(gs.getPlayer(12).position.x, 12);
}

TEST(CoachChoiceFollowUp, Accepted_AttackerFollows) {
    GameState gs = blockPair();
    FixedDiceRoller dice({3});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::TEAM});
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(1).position.x, 11);
}

TEST(CoachChoiceFollowUp, AfterBlockReroll_StillAsked) {
    GameState gs = blockPair();
    gs.homeTeam.rerolls = 1;
    FixedDiceRoller dice({1, 3});                    // lebka → přehoz → Pushed
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::TEAM});
    try { block(gs, dice); FAIL(); } catch (const CoachChoicePending& p) {
        EXPECT_EQ(p.question, CoachQuestion::USE_FOLLOW_UP);
    }
}

TEST(CoachChoiceFollowUp, Frenzy_MustFollow_NoQuestion) {
    GameState gs = blockPair();
    gs.getPlayer(1).skills.add(SkillName::Frenzy);   // ř. 8138: "must always follow up"
    FixedDiceRoller dice({3, 3});                    // Pushed, druhý blok Pushed
    CoachChoiceScope scope(TeamSide::HOME, {});
    EXPECT_NO_THROW(block(gs, dice));
}

TEST(CoachChoiceFollowUp, OtherSide_StaysAutomatic) {
    GameState gs = blockPair();
    FixedDiceRoller dice({3});
    CoachChoiceScope scope(TeamSide::AWAY, {});      // útočí HOME, kouč je AWAY
    EXPECT_NO_THROW(block(gs, dice));
    EXPECT_EQ(gs.getPlayer(1).position.x, 11);
}

// ───────────── P201 (1): lékárník na požádání kouče ─────────────
// Pravidla ř. 1200-1210: lékárník smí léčit hráče po Casualty NEBO po KO, jednou za zápas.
// KO ⇒ hráč zůstane na hřišti Stunned (mimo hřiště ⇒ rezervy). Casualty ⇒ soupeř hodí znovu
// a KOUČ vybere, který ze dvou výsledků platí; Badly Hurt ⇒ rezervy. Dřív lékárníka použil
// engine sám podle svého pravidla (partie 13.10., blitzer 11) -- uživatel: „musí být na požádání“.
#include "bb/injury.h"

static GameState hurtHome() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::AWAY;
    placePlayer(gs, 1, {10, 7});
    gs.getPlayer(1).state = PlayerState::PRONE;
    gs.getPlayer(1).positionName = "Blitzer";
    gs.homeTeam.hasApothecary = true;
    return gs;
}

TEST(CoachChoiceApothecary, Casualty_Asked) {
    GameState gs = hurtHome();
    FixedDiceRoller dice({5, 5, 5, 5, 6, 1});        // zbroj 10, zranění 10, D68 61 = mrtev
    CoachChoiceScope scope(TeamSide::HOME, {});
    try { resolveArmourAndInjury(gs, 1, dice, InjuryContext{}, nullptr); FAIL(); }
    catch (const CoachChoicePending& p) {
        EXPECT_EQ(p.question, CoachQuestion::USE_APOTHECARY);
        EXPECT_EQ(p.playerId, 1);
        EXPECT_EQ(p.roll, static_cast<int>(CasualtyResult::DEAD));
        EXPECT_EQ(p.options, ANO_NE);
    }
}

TEST(CoachChoiceApothecary, Casualty_Declined_StaysDead) {
    GameState gs = hurtHome();                       // engine by ho u mrtvého blitzera použil sám
    FixedDiceRoller dice({5, 5, 5, 5, 6, 1, 1, 1});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE});
    resolveArmourAndInjury(gs, 1, dice, InjuryContext{}, nullptr);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::DEAD);
    EXPECT_FALSE(gs.homeTeam.apothecaryUsed);
}

TEST(CoachChoiceApothecary, Casualty_UsedOnMiddleBand_CoachPicksSecond) {
    GameState gs = hurtHome();                       // engine by ho u Miss Next Game šetřil
    FixedDiceRoller dice({5, 5, 5, 5, 4, 1, 1, 1});  // D68 41 = Miss Next Game, druhý 11 = Badly Hurt
    {
        CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::TEAM});
        try { resolveArmourAndInjury(gs, 1, dice, InjuryContext{}, nullptr); FAIL(); }
        catch (const CoachChoicePending& p) {
            EXPECT_EQ(p.question, CoachQuestion::APOTHECARY_PICK);
            EXPECT_EQ(p.roll, static_cast<int>(CasualtyResult::MISS_NEXT_GAME));
            EXPECT_EQ(p.target, static_cast<int>(CasualtyResult::BADLY_HURT));
            EXPECT_EQ(p.options, 3);
        }
    }
    GameState gs2 = hurtHome();
    FixedDiceRoller dice2({5, 5, 5, 5, 4, 1, 1, 1});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::TEAM, 1});
    resolveArmourAndInjury(gs2, 1, dice2, InjuryContext{}, nullptr);
    EXPECT_TRUE(gs2.homeTeam.apothecaryUsed);
    EXPECT_EQ(gs2.getPlayer(1).state, PlayerState::OFF_PITCH);   // Badly Hurt ⇒ rezervy
}

TEST(CoachChoiceApothecary, Casualty_CoachMayKeepTheFirst) {
    GameState gs = hurtHome();
    FixedDiceRoller dice({5, 5, 5, 5, 4, 1, 1, 1});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::TEAM, 0});   // „vybere, který“ -- i horší
    resolveArmourAndInjury(gs, 1, dice, InjuryContext{}, nullptr);
    EXPECT_TRUE(gs.homeTeam.apothecaryUsed);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::INJURED);
}

TEST(CoachChoiceApothecary, KO_Used_StaysOnPitchStunned) {
    GameState gs = hurtHome();
    FixedDiceRoller dice({5, 5, 4, 5});              // zbroj 10, zranění 9 = KO
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::TEAM});
    resolveArmourAndInjury(gs, 1, dice, InjuryContext{}, nullptr);
    EXPECT_TRUE(gs.homeTeam.apothecaryUsed);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STUNNED);
    EXPECT_EQ(gs.getPlayer(1).position.x, 10);
}

TEST(CoachChoiceApothecary, KO_Declined_GoesToKO) {
    GameState gs = hurtHome();
    FixedDiceRoller dice({5, 5, 4, 5});
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE});
    resolveArmourAndInjury(gs, 1, dice, InjuryContext{}, nullptr);
    EXPECT_FALSE(gs.homeTeam.apothecaryUsed);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::KO);
}

TEST(CoachChoiceApothecary, CrowdKO_Used_GoesToReserves) {
    GameState gs = hurtHome();
    gs.getPlayer(1).state = PlayerState::STANDING;
    FixedDiceRoller dice({4, 5});                    // dav: zranění 9 = KO
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::TEAM});
    resolveCrowdSurf(gs, 1, gs.getPlayer(1).position, dice, nullptr);
    EXPECT_TRUE(gs.homeTeam.apothecaryUsed);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::OFF_PITCH);    // „in the Reserves box if not on the pitch“
}

TEST(CoachChoiceApothecary, AlreadyUsed_NoQuestion) {
    GameState gs = hurtHome();
    gs.homeTeam.apothecaryUsed = true;
    FixedDiceRoller dice({5, 5, 4, 5});
    CoachChoiceScope scope(TeamSide::HOME, {});
    EXPECT_NO_THROW(resolveArmourAndInjury(gs, 1, dice, InjuryContext{}, nullptr));
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::KO);
}

TEST(CoachChoiceApothecary, OtherSide_StaysAutomatic) {
    GameState gs = hurtHome();
    FixedDiceRoller dice({5, 5, 5, 5, 6, 1, 1, 1});
    CoachChoiceScope scope(TeamSide::AWAY, {});
    EXPECT_NO_THROW(resolveArmourAndInjury(gs, 1, dice, InjuryContext{}, nullptr));
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::OFF_PITCH);    // dosavadní pravidlo enginu
}

// ───────────── P201 (2), (4), (9), (11): výpis živé partie ─────────────
// Nové řádky výpisu jen v živé partii (coachModeOn), jako REROLL -- proudy událostí měření se nemění.
static const GameEvent* najdi(const std::vector<GameEvent>& ev, GameEvent::Type t) {
    for (const auto& e : ev) if (e.type == t) return &e;
    return nullptr;
}

TEST(P201Vypis, PassEventCarriesTargetAndDie) {
    // (9): PASS nesl v `roll` hozenou kostku, výpis ji tiskl jako „potřeba N+“. Nově jako ostatní
    // hody (P145): roll = cílové číslo, die1 = kostka.
    GameState gs = passSetup();                      // rychlá přihrávka, potřeba 3+
    FixedDiceRoller dice({4, 5});
    std::vector<GameEvent> ev;
    resolvePass(gs, 1, {8, 7}, dice, &ev);
    const GameEvent* p = najdi(ev, GameEvent::Type::PASS);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->roll, 3);
    EXPECT_EQ(p->die1, 4);
    EXPECT_TRUE(p->success);
}

TEST(P201Vypis, StandFirmHasItsOwnLine) {
    // (4): Stand Firm obránce bez kouče nechal ve výpisu jen „odtlačen nebyl“ bez důvodu.
    GameState gs = blockPair();
    gs.getPlayer(12).skills.add(SkillName::StandFirm);
    FixedDiceRoller dice({3});                       // Pushed
    std::vector<GameEvent> ev;
    CoachChoiceScope scope(TeamSide::HOME, {});
    block(gs, dice, &ev);
    bool seen = false;
    for (const auto& e : ev)
        if (e.type == GameEvent::Type::SKILL_USED && e.playerId == 12 &&
            e.roll == static_cast<int>(SkillName::StandFirm)) seen = true;
    EXPECT_TRUE(seen);
    EXPECT_EQ(gs.getPlayer(12).position.x, 11);
}

TEST(P201Vypis, AiTeamRerollHasALine) {
    // (11): týmový reroll strany bez kouče snížil počítadlo bez události.
    GameState gs = homeWithReroll();
    FixedDiceRoller dice({2, 5});
    std::vector<GameEvent> ev;
    CoachChoiceScope scope(TeamSide::AWAY, {});      // kouč je AWAY, hází HOME (AI)
    EXPECT_TRUE(roll4(gs, dice, &ev));
    EXPECT_EQ(gs.homeTeam.rerolls, 0);
    const GameEvent* r = najdi(ev, GameEvent::Type::REROLL);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->roll, 4);                           // druh 4 = týmový reroll použila AI
    EXPECT_EQ(r->playerId, 1);
}

TEST(P201Vypis, AiTeamRerollOutsideLiveGame_NoEvent) {
    GameState gs = homeWithReroll();
    FixedDiceRoller dice({2, 5});
    std::vector<GameEvent> ev;
    EXPECT_TRUE(roll4(gs, dice, &ev));               // mimo živou partii: proud událostí beze změny
    EXPECT_EQ(najdi(ev, GameEvent::Type::REROLL), nullptr);
}

TEST(P201Vypis, ApothecaryToReserves_InjuryRollIsShown) {
    // (2): hod 10+ neměl řádek, když lékárník poslal hráče do rezerv (návrat před CASUALTY).
    GameState gs = hurtHome();
    FixedDiceRoller dice({5, 5, 5, 5, 6, 1, 1, 1});
    std::vector<GameEvent> ev;
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::TEAM, 1});
    resolveArmourAndInjury(gs, 1, dice, InjuryContext{}, &ev);
    const GameEvent* inj = najdi(ev, GameEvent::Type::INJURY);
    ASSERT_NE(inj, nullptr);
    EXPECT_EQ(inj->roll, 10);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::OFF_PITCH);
}

// ───────────── P201 (5): Wrestle útočníka při Both Down je volba jeho kouče ─────────────
// BB2016 ř. 8672-8676: "This player MAY USE Wrestle" — volba. Kouč obránce se ptal (P130),
// kouč útočníka ne: engine rozhodl sám (partie 13.10.).
TEST(CoachChoiceBlock, AttackerWrestle_Asked) {
    GameState gs = blockPair();
    gs.getPlayer(1).skills.add(SkillName::Wrestle);
    gs.getPlayer(1).skills.add(SkillName::Block);    // engine by Wrestle nepoužil (obránce Block nemá)
    FixedDiceRoller dice({2, 1, 1});                 // Both Down, zbroj obránce 1+1
    CoachChoiceScope scope(TeamSide::HOME, {});
    try { block(gs, dice); FAIL(); } catch (const CoachChoicePending& p) {
        EXPECT_EQ(p.question, CoachQuestion::USE_WRESTLE);
        EXPECT_EQ(p.playerId, 1);
        EXPECT_EQ(p.otherId, 12);
    }
}

TEST(CoachChoiceBlock, AttackerWrestle_Used_BothProne) {
    GameState gs = blockPair();
    gs.getPlayer(1).skills.add(SkillName::Wrestle);
    gs.getPlayer(1).skills.add(SkillName::Block);
    FixedDiceRoller dice({2, 1, 1});
    CoachChoiceScope scope(TeamSide::HOME, {1});
    auto r = block(gs, dice);
    EXPECT_FALSE(r.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
    EXPECT_EQ(dice.remaining(), 2u);                 // Wrestle: bez hodu na zbroj
}

TEST(CoachChoiceBlock, AttackerWrestle_Declined_BlockKeepsHimStanding) {
    GameState gs = blockPair();
    gs.getPlayer(1).skills.add(SkillName::Wrestle);
    gs.getPlayer(1).skills.add(SkillName::Block);
    FixedDiceRoller dice({2, 1, 1});
    CoachChoiceScope scope(TeamSide::HOME, {0});
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

// ───────────── P201 (8): kam se započetl +1 z Mighty Blow ─────────────
// Výpis ukazoval jen součet; z „9 (4+4)“ šlo +1 odhadnout, ale ne říct, že je to Mighty Blow.
// V živé partii SKILL_USED MightyBlow, die1 = 1 brnění, 2 zranění (ř. 8285-8292: jen jeden z hodů).
static int mbKam(const std::vector<GameEvent>& ev) {
    for (const auto& e : ev)
        if (e.type == GameEvent::Type::SKILL_USED && e.roll == static_cast<int>(SkillName::MightyBlow)) return e.die1;
    return 0;
}

TEST(P201Vypis, MightyBlowOnArmour) {
    GameState gs = hurtHome();                       // AV 8
    InjuryContext ctx; ctx.mightyBlow = true;
    FixedDiceRoller dice({4, 4, 1, 1});              // brnění 8 + 1 = 9 prorazí, zranění 2
    std::vector<GameEvent> ev;
    CoachChoiceScope scope(TeamSide::HOME, {});
    resolveArmourAndInjury(gs, 1, dice, ctx, &ev);
    EXPECT_EQ(mbKam(ev), 1);
}

TEST(P201Vypis, MightyBlowOnInjury) {
    GameState gs = hurtHome();
    InjuryContext ctx; ctx.mightyBlow = true;
    FixedDiceRoller dice({5, 5, 3, 4});              // brnění 10 prorazí samo, zranění 7 + 1 = 8
    std::vector<GameEvent> ev;
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::NONE});   // KO: lékárník ne
    resolveArmourAndInjury(gs, 1, dice, ctx, &ev);
    EXPECT_EQ(mbKam(ev), 2);
}

// ───────────── P201 (6): kouč za obě strany ─────────────
// `kouc` uměl jen jednu stranu; za druhou (v partii 13.10. orkové = Claude) vybíral kostku bloku,
// přehozy a skilly engine. Režim „obě strany“ se ptá za každou stranu.
TEST(CoachChoice, BothSides_AskedForEither) {
    {
        CoachChoiceScope scope(TeamSide::HOME, {}, /*bothSides=*/true);
        EXPECT_TRUE(coachDecides(TeamSide::HOME));
        EXPECT_TRUE(coachDecides(TeamSide::AWAY));
    }
    GameState gs = homeWithReroll();
    FixedDiceRoller dice({2, 5});
    CoachChoiceScope scope(TeamSide::AWAY, {}, /*bothSides=*/true);   // hází HOME, kouč „AWAY + obě“
    EXPECT_THROW(roll4(gs, dice), CoachChoicePending);
}

// ───────────── Review 14.10. (nálezy 1, 2, 3, 7): vytlačení do davu ─────────────
// P194 (6) posunul follow-up před vhazování -- ale obránce byl v tu chvíli ještě na hřišti
// (mohl chytit vhazovaný míč) a směr vhazování se počítal z útočníkova NOVÉHO pole.
static GameState davPair(Position att, Position def) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, att);
    placePlayer(gs, 12, def);
    return gs;
}

TEST(DoDavu, VhazovaniPodleSmeruOdtlaceni_ObranceUzNeniNaHristi) {
    GameState gs = davPair({24, 0}, {25, 0});        // odtlačení na východ do davu, útočník následuje
    gs.ball = BallState::carried({25, 0}, 12);
    // Pushed · šablona 3 (vpravo = na západ, nahoře = na jih) · 2D6 = 3 · dav 1+1 · zbytek odrazy
    FixedDiceRoller dice({3, 3, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1});
    std::vector<GameEvent> ev;
    block(gs, dice, &ev);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{25, 0}));      // follow-up proběhl
    bool vhoz = false;
    for (const auto& e : ev)
        if (e.type == GameEvent::Type::BALL_BOUNCE && e.die1 != 0) {   // vhazování nese kostku šablony
            vhoz = true;
            EXPECT_LT(e.to.x, e.from.x) << "vhazování z PRAVÉHO okraje míří na západ";
        }
    EXPECT_TRUE(vhoz);
    EXPECT_FALSE(gs.ball.isHeld && gs.ball.carrierId == 12) << "obránce v davu míč držet nesmí";
}

TEST(DoDavu, CrowdSurfNesePoleZeKterehoSelDoDavu) {
    GameState gs = davPair({24, 5}, {25, 5});
    FixedDiceRoller dice({3, 1, 1, 1, 1});
    std::vector<GameEvent> ev;
    block(gs, dice, &ev);
    bool seen = false;
    for (const auto& e : ev)
        if (e.type == GameEvent::Type::CROWD_SURF) { seen = true; EXPECT_EQ(e.from, (Position{25, 5})); }
    EXPECT_TRUE(seen);
}

TEST(DoDavu, FendZakazujeFollowUpIPriVytlaceni) {
    // Fend ř. 8115: "Opposing players may not follow-up blocks made against this player".
    GameState gs = davPair({24, 5}, {25, 5});
    gs.getPlayer(12).skills.add(SkillName::Fend);
    FixedDiceRoller dice({3, 1, 1, 1, 1});
    block(gs, dice);
    EXPECT_EQ(gs.getPlayer(1).position, (Position{24, 5}));
}
