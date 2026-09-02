#include <gtest/gtest.h>
#include "bb/rules_engine.h"
#include "bb/helpers.h"
#include "bb/macro_actions.h"

using namespace bb;

static void placePlayer(GameState& gs, int id, Position pos, TeamSide side,
                         int ma = 6, int st = 3, int ag = 3, int av = 8) {
    Player& p = gs.getPlayer(id);
    p.state = PlayerState::STANDING;
    p.position = pos;
    p.stats = {static_cast<int8_t>(ma), static_cast<int8_t>(st),
               static_cast<int8_t>(ag), static_cast<int8_t>(av)};
    p.movementRemaining = ma;
}

static int countActionsOfType(const std::vector<Action>& actions, ActionType type) {
    int c = 0;
    for (auto& a : actions) if (a.type == type) c++;
    return c;
}

TEST(RulesEngine, EndTurnAlwaysAvailable) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    std::vector<Action> actions;
    getAvailableActions(gs, actions);
    EXPECT_EQ(countActionsOfType(actions, ActionType::END_TURN), 1);
}

TEST(RulesEngine, NoActionsWhenNotPlayPhase) {
    GameState gs;
    gs.phase = GamePhase::GAME_OVER;
    std::vector<Action> actions;
    getAvailableActions(gs, actions);
    EXPECT_EQ(actions.size(), 0);
}

TEST(RulesEngine, MoveEnumeration) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    int moveCount = countActionsOfType(actions, ActionType::MOVE);
    EXPECT_EQ(moveCount, 8); // 8 adjacent squares all empty
}

TEST(RulesEngine, MoveBlockedByOccupied) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 2, {11, 7}, TeamSide::HOME); // blocks one direction

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    // Count move actions for player 1 only
    int moveCount = 0;
    for (auto& a : actions) {
        if (a.type == ActionType::MOVE && a.playerId == 1) moveCount++;
    }
    EXPECT_EQ(moveCount, 7);
}

TEST(RulesEngine, BlockTargets) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    int blockCount = countActionsOfType(actions, ActionType::BLOCK);
    EXPECT_EQ(blockCount, 1);
}

TEST(RulesEngine, BlockNotAvailableForProneEnemy) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    int blockCount = countActionsOfType(actions, ActionType::BLOCK);
    EXPECT_EQ(blockCount, 0);
}

TEST(RulesEngine, BlitzOncePerTurn) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {15, 7}, TeamSide::AWAY); // not adjacent

    std::vector<Action> actions;
    getAvailableActions(gs, actions);
    int blitzCount = countActionsOfType(actions, ActionType::BLITZ);
    EXPECT_GE(blitzCount, 1);

    // After using blitz
    gs.homeTeam.blitzUsedThisTurn = true;
    getAvailableActions(gs, actions);
    blitzCount = countActionsOfType(actions, ActionType::BLITZ);
    EXPECT_EQ(blitzCount, 0);
}

TEST(RulesEngine, FoulTargets) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    int foulCount = countActionsOfType(actions, ActionType::FOUL);
    EXPECT_EQ(foulCount, 1);
}

TEST(RulesEngine, FoulOncePerTurn) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;

    gs.homeTeam.foulUsedThisTurn = true;
    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    int foulCount = countActionsOfType(actions, ActionType::FOUL);
    EXPECT_EQ(foulCount, 0);
}

TEST(RulesEngine, ActedPlayerCannotAct) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).hasActed = true;

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    int moveCount = countActionsOfType(actions, ActionType::MOVE);
    EXPECT_EQ(moveCount, 0);
}

TEST(RulesEngine, PronePlayerCanStandUp) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).state = PlayerState::PRONE;

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    // Should have a MOVE action to own position (stand up)
    bool found = false;
    for (auto& a : actions) {
        if (a.type == ActionType::MOVE && a.playerId == 1 &&
            a.target == (Position{10, 7})) {
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST(RulesEngine, NoMovementLeftNoMoveActions) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).movementRemaining = -2; // used all GFI already

    std::vector<Action> actions;
    getAvailableActions(gs, actions);

    int moveCount = countActionsOfType(actions, ActionType::MOVE);
    EXPECT_EQ(moveCount, 0);
}

// ============================================================================
// M13, KROK A (31.08.2026): LEŽÍCÍ HRÁČ SMÍ DEKLAROVAT AKCI
//
// BB2016 ř. 669-676: „While Prone, the player ... may do nothing before
// standing up at a cost of three squares of his movement WHEN HE NEXT TAKES AN
// ACTION. ... The player may take any Action other than a Block Action."
//
// Do 31.08. vracel `getAvailableActions` ležícímu JEDINOU akci: MOVE na vlastní
// pole (vstát a skončit). Resolver přitom vstávání před pohybem i před blitzem
// uměl už dřív -- chyběla NABÍDKA. Krok A otevírá právě a jen MOVE.
// ============================================================================
namespace {

// Ležící HOME hráč na {10,7}, kolem samé prázdno.
// ⚠️ M13 je od 31.08. za vypínačem (`setProneActionArm`), aby šla změřit
// zvlášť od noci P35 — uživatel 31.08.: „zkus to rozdělit na dvě měření".
// Fixtura ho proto zapíná; test `ProneArmOffRestoresTheOldOfferSet` níž hlídá,
// že vypnuté rameno vrací PŘESNĚ původní stav.
// ⭐ 02.09.2026: rameno odebrano po zmereni (noc 2 400 paru: +0,0048 +- 0,0084).
// Struktura zustava PRAZDNA schvalne — testy nize ji drzi v tele, takze se
// nemusely prepisovat, a kdyby se M13 nekdy zase schovavalo za vypinac,
// je kam sahnout. Prazdny RAII je levnejsi nez patnact editaci testu.
struct ProneArmOn { };

GameState makeProneState(int ma, bool jumpUp = false, bool rooted = false) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, ma);
    Player& p = gs.getPlayer(1);
    p.state = PlayerState::PRONE;
    p.rooted = rooted;
    if (jumpUp) p.skills.add(SkillName::JumpUp);
    return gs;
}

bool hasMoveTo(const std::vector<Action>& as, Position dest) {
    for (auto& a : as)
        if (a.type == ActionType::MOVE && a.target == dest) return true;
    return false;
}

} // namespace

TEST(RulesEngineProne, ProneMa6IsOfferedRealStepsNotJustStandingUp) {
    ProneArmOn _arm;
    GameState gs = makeProneState(/*ma=*/6);
    std::vector<Action> as;
    getAvailableActions(gs, as);

    // vstání na místě tu bylo i před M13 -- musí zůstat
    EXPECT_TRUE(hasMoveTo(as, {10, 7})) << "vstání na místě se ztratilo";
    // ⭐ jádro kroku A: po vstání (6-3=3) mu zbývá pohyb, takže smí i krok
    EXPECT_TRUE(hasMoveTo(as, {11, 7})) << "ležící nedostal žádný skutečný krok";
    EXPECT_EQ(countActionsOfType(as, ActionType::MOVE), 9);  // 8 sousedů + vlastní pole
}

TEST(RulesEngineProne, ProneIsNeverOfferedBlockOrAnythingBelowTheStepGate) {
    ProneArmOn _arm;
    GameState gs = makeProneState(/*ma=*/6);
    // soused, na kterého by se dalo útočit, kdyby to šlo
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);

    std::vector<Action> as;
    getAvailableActions(gs, as);

    // ř. 674-676 zakazuje blok z lehu VÝSLOVNĚ
    EXPECT_EQ(countActionsOfType(as, ActionType::BLOCK), 0);
    EXPECT_EQ(countActionsOfType(as, ActionType::FOUL), 0);
    EXPECT_EQ(countActionsOfType(as, ActionType::HYPNOTIC_GAZE), 0);
    EXPECT_EQ(countActionsOfType(as, ActionType::LEAP), 0);
}

TEST(RulesEngineProne, StunnedPlayerGetsNothing) {
    ProneArmOn _arm;
    GameState gs = makeProneState(/*ma=*/6);
    gs.getPlayer(1).state = PlayerState::STUNNED;
    std::vector<Action> as;
    getAvailableActions(gs, as);
    // ř. 690 pouští vstání jen z Prone
    EXPECT_EQ(countActionsOfType(as, ActionType::MOVE), 0);
}

TEST(RulesEngineProne, SubThreeMovementStillGetsAStepBecauseItIsAGfi) {
    // MA2 (treeman): vstání je hod na 4+ a pohyb se pak nuluje, ale ř. 693
    // dovoluje jít dál "unless he Goes For It" => krok se nabídnout MÁ.
    ProneArmOn _arm;
    GameState gs = makeProneState(/*ma=*/2);
    std::vector<Action> as;
    getAvailableActions(gs, as);
    EXPECT_TRUE(hasMoveTo(as, {11, 7}));
}

TEST(RulesEngineProne, RootedProneGetsNoStepBecauseHeMayNotGoForIt) {
    // Take Root (ř. 8577-8578) bere GFI. MA3 => po vstání 0 a bez GFI
    // není čím krok zaplatit. Hlídá, že se GFI nepřičetlo paušálně.
    ProneArmOn _arm;
    GameState gs = makeProneState(/*ma=*/3, /*jumpUp=*/false, /*rooted=*/true);
    std::vector<Action> as;
    getAvailableActions(gs, as);
    EXPECT_FALSE(hasMoveTo(as, {11, 7}));
    EXPECT_TRUE(hasMoveTo(as, {10, 7})) << "vstát na místě smí i zakořeněný";
}

TEST(RulesEngineProne, JumpUpStandsForFreeSoTheWholeMoveIsStillThere) {
    // ř. 8196-8198: vstání zdarma při jiné akci než blok.
    ProneArmOn _arm;
    GameState gsPlain = makeProneState(/*ma=*/3);
    GameState gsJump  = makeProneState(/*ma=*/3, /*jumpUp=*/true);
    std::vector<Action> a1, a2;
    getAvailableActions(gsPlain, a1);
    getAvailableActions(gsJump, a2);
    // bez Jump Up: 3-3 = 0 a krok je GFI (nabídne se)
    // s Jump Up:   zůstávají 3 pole -- taky se nabídne, ale ne z GFI
    EXPECT_TRUE(hasMoveTo(a1, {11, 7}));
    EXPECT_TRUE(hasMoveTo(a2, {11, 7}));
}

// ============================================================================
// M13, KROK B (31.08.2026): BLITZ Z LEHU
//
// Blitz NENÍ Block Action, takže spadá pod ř. 676 („may take any Action other
// than a Block Action"). Do teď ho ležící nedostal nikdy — a právě o něj šlo
// v uživatelově tahu z 27.08. („začal bych blitz z (14,2) na (14,3)").
// ============================================================================

TEST(RulesEngineProneBlitz, ProneIsOfferedBlitzOnAnAdjacentEnemy) {
    ProneArmOn _arm;
    GameState gs = makeProneState(/*ma=*/6);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    std::vector<Action> as;
    getAvailableActions(gs, as);
    EXPECT_EQ(countActionsOfType(as, ActionType::BLITZ), 1);
    EXPECT_EQ(countActionsOfType(as, ActionType::BLOCK), 0) << "blok z lehu je zakázaný";
}

TEST(RulesEngineProneBlitz, StandingPlayerReachesFurtherThanTheSameProneOne) {
    // ⭐ Tenhle test drží celý smysl kroku B: TÁŽ pozice, TÝŽ hráč, jen leh.
    // Kdyby se vstání do dosahu nezapočítalo, obě čísla by si byla rovna.
    ProneArmOn _arm;
    GameState up = makeProneState(/*ma=*/6);
    up.getPlayer(1).state = PlayerState::STANDING;
    placePlayer(up, 12, {16, 7}, TeamSide::AWAY);
    GameState down = makeProneState(/*ma=*/6);
    placePlayer(down, 12, {16, 7}, TeamSide::AWAY);
    std::vector<Action> au, ad;
    getAvailableActions(up, au);
    getAvailableActions(down, ad);
    EXPECT_EQ(countActionsOfType(au, ActionType::BLITZ), 1) << "stojící dosáhne";
    EXPECT_EQ(countActionsOfType(ad, ActionType::BLITZ), 0)
        << "ležící dosáhl stejně daleko => vstání se do dosahu nezapočítalo";
}

TEST(RulesEngineProneBlitz, RootedProneNextToEnemyGetsNoBlitzBecauseTheBlockIsUnpayable) {
    // MA3 zakořeněný: po vstání 0 pohybu a bez GFI (ř. 8577-8578) není čím
    // zaplatit blok (ř. 549-550) => blitz by se utratil za nic.
    ProneArmOn _arm;
    GameState gs = makeProneState(/*ma=*/3, /*jumpUp=*/false, /*rooted=*/true);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    std::vector<Action> as;
    getAvailableActions(gs, as);
    EXPECT_EQ(countActionsOfType(as, ActionType::BLITZ), 0);
}

TEST(RulesEngineProneBlitz, SubThreeMovementProneStillGetsAnAdjacentBlitzViaGfi) {
    // MA2 treeman bez Take Root: vstání na 4+, pohyb 0, blok přes GFI --
    // ř. 693 to výslovně dovoluje. Dřív `maxMove -= 3` dalo -1 a blitz zmizel.
    ProneArmOn _arm;
    GameState gs = makeProneState(/*ma=*/2);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    std::vector<Action> as;
    getAvailableActions(gs, as);
    EXPECT_EQ(countActionsOfType(as, ActionType::BLITZ), 1);
}

TEST(RulesEngineProneBlitz, SubThreeMovementProneReachesAnEnemyTwoSquaresAwayOnGfi) {
    // ⭐ Tenhle test drží opravu v PATHFINDERU, ne v nabídce: MA2 ležící,
    // soupeř na 2 pole. Po vstání (hod 4+) je pohyb 0, ale GFI dovoluje
    // krok + blok = 2 pole (ř. 693 „unless he Goes For It").
    // Starý `maxMove -= 3` dal -1, rozpočet vyšel 0 a blitz zmizel --
    // pravidlo pod 3 MA žádný záporný rozpočet nezná.
    ProneArmOn _arm;
    GameState gs = makeProneState(/*ma=*/2);
    placePlayer(gs, 12, {12, 7}, TeamSide::AWAY);   // vzdálenost 2
    std::vector<Action> as;
    getAvailableActions(gs, as);
    EXPECT_EQ(countActionsOfType(as, ActionType::BLITZ), 1);
}


// ⛔ TEST `ProneArmOffRestoresTheOldOfferSet` ODSTRANEN 02.09.2026 spolu
// s ramenem: hlidal, ze VYPNUTE rameno vraci stav pred M13. Rameno uz
// neexistuje, takze test nema predmet. Chovani samo hlidaji testy vyse.

// ============================================================================
// P37b (01.09.2026): NA RÁNU MUSÍ ZBÝT POLE POHYBU
//
// ř. 549-550: „the block … costs one square of movement." Do 01.09. měli tuhle
// kontrolu jen ležící (M13); stojící ne, takže hráč, který už utratil pohyb
// i GFI, blitz DEKLAROVAL, tým přišel o svůj jediný blitz na kolo a rána se
// nehodila. Změřeno před opravou: ~9 na zápas.
//
// ⛔ ROZSAH JE ÚZKÝ SCHVÁLNĚ (uživatel 01.09.): ř. 552-553 dovoluje po ráně
// pokračovat v pohybu, takže „blitz souseda a pak útěk" je legitimní vzor —
// srazím ho, zmizí tacklezóna, odejdu bez dodge. Nabídka se proto NESMÍ zúžit
// za to, že hráč sousedí, ani za to, že mu po ráně nezbude krok.
// ============================================================================
TEST(RulesEngineBlitzBudget, AdjacentBlitzIsStillOfferedWhenOnlyTheBlockIsAffordable) {
    // Pohyb 0, ale GFI zbývá => ránu zaplatit lze. Nabídka MUSÍ zůstat:
    // hráč sice po ráně neodejde, ale rána sama má cenu.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(1).movementRemaining = 0;

    std::vector<Action> as;
    getAvailableActions(gs, as);
    EXPECT_EQ(countActionsOfType(as, ActionType::BLITZ), 1)
        << "nabídka se zúžila i tam, kde rána zaplatit jde";
}

TEST(RulesEngineBlitzBudget, AdjacentBlitzIsRefusedWhenEvenTheBlockCannotBePaid) {
    // Pohyb i GFI vyčerpané (-2 při maxGfi 2) => rána se nehodí, tým by přišel
    // o blitz za nic. A hráč se nemůže ani hnout, takže deklarace nekupuje nic.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(1).movementRemaining = -2;

    std::vector<Action> as;
    getAvailableActions(gs, as);
    EXPECT_EQ(countActionsOfType(as, ActionType::BLITZ), 0)
        << "nabídl se blitz, jehož ránu není z čeho zaplatit";
}
