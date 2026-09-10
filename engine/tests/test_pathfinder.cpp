#include <gtest/gtest.h>
#include "bb/pathfinder.h"
#include "bb/helpers.h"
#include <cmath>

using namespace bb;

// ===========================================================================
// REROLL Z DOVEDNOSTI DODGE VE PATHFINDERU (M6/B3(a)-follow-up, 10.09.2026).
//
// Dve RUZNE veci v jednom souboru, a testy je drzi oddelene, protoze jejich
// zamena uz jednou stala korekci (`cf8634e8`):
//   · `pathFailProb` chodi po UZ ZVOLENE ceste ⇒ EXAKTNI dvoustavovy pruchod
//     R/S, presne jako `estimateApproachFailChance`.
//   · `riskWeightedDijkstra` VYBIRA cestu a ma na uzel jediny skalar ⇒
//     APROXIMACE: dve vrstvy uzlu (reroll netknuty / utraceny), prvni
//     pripustny dodge za `p²`, kazdy dalsi za hole `p`.
// ===========================================================================

namespace {

Player& mkPlayer(GameState& s, int id, TeamSide side, Position pos, int agility) {
    Player& p = s.getPlayer(id);
    p.id = id; p.teamSide = side; p.state = PlayerState::STANDING;
    p.position = pos;
    p.stats = {6, 3, static_cast<int8_t>(agility), 8};
    p.movementRemaining = 6;
    return p;
}

constexpr Position kNoBlock{-1, -1};

// ---------------------------------------------------------------------------
// FIXTURA 1 -- KORIDOR SE TREMI DODGI (pro `pathFailProb`).
// HOME AG3 MA6 na {5,0} (horni postranni), cil {8,0}, dve AWAY znacky pod
// nim na {6,1} a {8,1}. ⭐ Rozpocet je natvrdo 3 = presna Chebyshev
// vzdalenost, takze OBCHAZKA NEEXISTUJE -- Dijkstra si nemuze tacklezony
// obejit a musi projit koridorem. To je zamer: tady se meri CENA CESTY, ne
// jeji vyber.
//   {5,0}->{6,0}  TZ 1 (jen {6,1})        cil 4+  p1 = 3/6 = 1/2
//   {6,0}->{7,0}  TZ 2 (obe znacky)       cil 5+  p2 = 4/6 = 2/3
//   {7,0}->{8,0}  TZ 1 (jen {8,1})        cil 4+  p3 = 3/6 = 1/2
// (MA6, 3 kroky ⇒ zadne GFI, cislo je cista dodge cast.)
// `tackleMarkerId`: 0 = zadny Tackle · 11 = znacka {6,1}, ktera SOUSEDI se
// vsemi tremi opoustenymi poli {5,0},{6,0},{7,0} ⇒ rusi reroll VSUDE ·
// 12 = znacka {8,1}, ktera sousedi jen s {7,0} ⇒ rusi ho jen na TRETIM kroku.
GameState makeThreeDodgeCorridor(bool moverHasDodge, int tackleMarkerId = 0) {
    GameState s;
    s.phase = GamePhase::PLAY;
    s.activeTeam = TeamSide::HOME;
    Player& mover = mkPlayer(s, 1, TeamSide::HOME, {5, 0}, 3);
    if (moverHasDodge) mover.skills.add(SkillName::Dodge);
    mkPlayer(s, 11, TeamSide::AWAY, {6, 1}, 3);
    mkPlayer(s, 12, TeamSide::AWAY, {8, 1}, 3);
    if (tackleMarkerId != 0) s.getPlayer(tackleMarkerId).skills.add(SkillName::Tackle);
    return s;
}

// ---------------------------------------------------------------------------
// FIXTURA 2 -- ZKRATKA PROTI OBCHAZCE (pro VYBER cesty v Dijkstre).
// HOME AG4 MA6 na {5,9}, cil {9,5} -- diagonala, Chebyshev 4, takze
// 4-krokova cesta je JEDINA a vede {6,8},{7,7},{8,6},{9,5}. AWAY znacka na
// {7,9} da tacklezonu PRESNE na {6,8} a na nic dalsiho z te diagonaly.
//   zkratka  4 pole + 1 dodge na {6,8}, TZ 1, AG4 ⇒ cil 3+, p = 2/6 = 1/3
//   obchazka 5 poli BEZ dodge:  {5,8},{6,7},{7,6},{8,6},{9,5}
// Ceny v jednotkach klice (1 pole = 100, riziko = p*4*100, +1 za diagonalu):
//   zkratka  bez Dodge  400 + 133 + 4 = 537 · s Dodge  400 + 44 + 4 = 448
//   obchazka             500 + 3 = 503
// ⇒ bez dovednosti vyhraje OBCHAZKA (503 < 537), s dovednosti ZKRATKA
//   (448 < 503). Prvni krok se tim lisi: {5,8} proti {6,8}.
// `tackleMarker` = druha AWAY znacka na {4,10}, tedy SOUSED OPOUSTENEHO
// pole {5,9}: rusi reroll na tom kroku (r. 8566-8571) a rozdil MA ZMIZET.
// ⚠️ {4,10} je zvolene tak, aby jeji tacklezona NEsahala ani na {5,8}
//   (obchazka), ani na {6,8} (zkratka) -- jinak by test meril zdrazeni
//   obchazky, ne zruseni rerollu.
GameState makeShortcutVsDetour(bool moverHasDodge, bool withTackleMarker) {
    GameState s;
    s.phase = GamePhase::PLAY;
    s.activeTeam = TeamSide::HOME;
    Player& mover = mkPlayer(s, 1, TeamSide::HOME, {5, 9}, 4);
    if (moverHasDodge) mover.skills.add(SkillName::Dodge);
    mkPlayer(s, 11, TeamSide::AWAY, {7, 9}, 3);
    if (withTackleMarker) {
        Player& t = mkPlayer(s, 12, TeamSide::AWAY, {4, 10}, 3);
        t.skills.add(SkillName::Tackle);
    }
    return s;
}

}  // namespace

// ===========================================================================
// CAST 1 -- `pathFailProb` MUSI BYT EXAKTNI
// ===========================================================================

TEST(PathFailProb, DodgeSkillRerollIsPricedByTheExactTwoStateWalk) {
    GameState plain = makeThreeDodgeCorridor(/*moverHasDodge=*/false);
    GameState dodgy = makeThreeDodgeCorridor(/*moverHasDodge=*/true);
    const Position target{8, 0};

    // ⭐⭐ POZITIVNI KONTROLA FIXTURY -- BEZ NI BY TEST TISE MERIL NECO JINEHO.
    //   09.09. tady prave takhle jedna fixtura vyrobila JEDEN dodge misto
    //   dvou (Dijkstra i hladovy picker se tacklezonam samy vyhybaji) a test
    //   by prosel pod spravnym i pod spatnym vzorcem.
    ASSERT_EQ(pathStepsToward(plain, plain.getPlayer(1), target, 3, kNoBlock), 3)
        << "cesta nema tri kroky -- fixtura nemeri, co tvrdi";
    Position firstStep{-1, -1};
    ASSERT_TRUE(nextStepToward(plain, plain.getPlayer(1), target, 3, kNoBlock, firstStep));
    ASSERT_EQ(firstStep, (Position{6, 0}))
        << "cesta nejde koridorem po y=0 -- fixtura nemeri, co tvrdi";

    const double p1 = 1.0 / 2.0, p2 = 2.0 / 3.0, p3 = 1.0 / 2.0;
    const double q1 = 1 - p1, q2 = 1 - p2, q3 = 1 - p3;

    // HOLA CENA (bez dovednosti): 1 - Pi(1-p_i) = 1 - 1/12 = 11/12.
    // Tohle je zaroven regresni kotva -- hrac bez Dodge nesmi videt zmenu.
    const double bare = pathFailProb(plain, plain.getPlayer(1), target, 3, kNoBlock);
    ASSERT_NEAR(bare, 1.0 - q1 * q2 * q3, 1e-12)
        << "hola cena nesedi -- fixtura nema prave ty tri dodge kroky";
    ASSERT_NEAR(bare, 11.0 / 12.0, 1e-12);

    // ⭐ EXAKTNI DVOUSTAVOVY PRUCHOD. Vsechny tri kroky jsou pripustne (zadny
    //   Tackle), takze se to musi zredukovat na uzavreny tvar
    //   Pi(1-p_i)*(1+Sum p_i) = (1/12)*(8/3) = 2/9 preziti ⇒ 7/9 riziko.
    const double exactSurvive = q1 * q2 * q3 * (1.0 + p1 + p2 + p3);
    const double withDodge = pathFailProb(dodgy, dodgy.getPlayer(1), target, 3, kNoBlock);
    EXPECT_NEAR(withDodge, 1.0 - exactSurvive, 1e-12)
        << "dvoustavovy pruchod nesedi na uzavreny tvar Pi(1-p_i)*(1+Sum p_i)";
    EXPECT_NEAR(withDodge, 7.0 / 9.0, 1e-12);

    // ⛔ A TRI HODNOTY, KTERYM SE TO NESMI ROVNAT -- kazda je jeden ze
    //   spatnych vzorcu, ktere by testem prosly, kdyby se neuvedly jmenem:
    // (1) zadny reroll (stav pred touhle zmenou) = 11/12
    EXPECT_GT(std::abs(withDodge - bare), 0.05)
        << "cena vysla jako bez dovednosti -- reroll se do ni nedostal";
    // (2) "kazdy dodge ma svuj reroll" = 1 - Pi(1-p_i²) = 0,6875. Prehnane
    //     zlevneni: reroll je JEDEN NA TAH, ne jeden na krok.
    const double naiveEveryStep = 1.0 - (1 - p1 * p1) * (1 - p2 * p2) * (1 - p3 * p3);
    EXPECT_NEAR(naiveEveryStep, 0.6875, 1e-12);
    EXPECT_GT(std::abs(withDodge - naiveEveryStep), 0.05)
        << "reroll se rozdal kazdemu kroku -- limit 'jeden za tah' zmizel";
    // (3) APROXIMACE Z DIJKSTRY (prvni pripustny dodge za p², dal hole p)
    //     = 1 - (1-p1²)q2q3 = 7/8. ⛔ Tohle je hodnota, kterou vraci VYBER
    //     cesty, a `pathFailProb` ji vratit NESMI -- je to jina uloha.
    const double dijkstraApprox = 1.0 - (1 - p1 * p1) * q2 * q3;
    EXPECT_NEAR(dijkstraApprox, 7.0 / 8.0, 1e-12);
    EXPECT_GT(std::abs(withDodge - dijkstraApprox), 0.05)
        << "pathFailProb pocita aproximaci z Dijkstry -- dve ulohy se zamenily";
    EXPECT_LT(withDodge, bare);
}

TEST(PathFailProb, TackleNegatesTheRerollPerSquareNotForTheWholePath) {
    const Position target{8, 0};
    const double p1 = 1.0 / 2.0, p2 = 2.0 / 3.0, p3 = 1.0 / 2.0;
    const double q1 = 1 - p1, q2 = 1 - p2, q3 = 1 - p3;

    // (i) Tackle na {8,1} -- sousedi JEN s {7,0}, tedy s vychozim polem
    //     TRETIHO kroku ⇒ pripustnost e = [ano, ano, NE].
    //     R/S pruchod: preziti 13/72 ⇒ riziko 59/72.
    GameState lateT = makeThreeDodgeCorridor(/*moverHasDodge=*/true,  /*tackleMarkerId=*/12);
    GameState latePlain = makeThreeDodgeCorridor(false, 12);
    ASSERT_NEAR(pathFailProb(latePlain, latePlain.getPlayer(1), target, 3, kNoBlock),
                1.0 - q1 * q2 * q3, 1e-12)
        << "fixtura s Tackle zmenila cestu -- meri se neco jineho";
    const double late = pathFailProb(lateT, lateT.getPlayer(1), target, 3, kNoBlock);
    EXPECT_NEAR(late, 59.0 / 72.0, 1e-12)
        << "Tackle se neuplatnil PO POLICH -- bud rusi moc, nebo malo";
    // ⭐ PRISNE MEZI: kdyby se Tackle bral jako vlastnost CELE cesty, vyslo by
    //   bud 11/12 = 66/72 (rusi vsude) nebo 7/9 = 56/72 (nerusi nikde).
    EXPECT_GT(late, 7.0 / 9.0)  << "treti krok slevu dostat NEMEL";
    EXPECT_LT(late, 11.0 / 12.0) << "prvni dva kroky slevu dostat MELY";

    // (ii) Tackle na {6,1} -- sousedi se VSEMI tremi opoustenymi poli ⇒
    //      e = [ne, ne, ne], reroll se neuplatni ani raz a cena musi spadnout
    //      PRESNE na holou hodnotu bez dovednosti.
    GameState allT = makeThreeDodgeCorridor(/*moverHasDodge=*/true, /*tackleMarkerId=*/11);
    EXPECT_NEAR(pathFailProb(allT, allT.getPlayer(1), target, 3, kNoBlock),
                11.0 / 12.0, 1e-12)
        << "Tackle u kazdeho opousteneho pole a sleva se presto uplatnila";
}

TEST(PathFailProb, TackleNextToTheLeftSquareRemovesTheDodgeDiscount) {
    // Fixtura 2 s Tackle znackou: cesta je 5 poli BEZ dodge, takze cislo je
    // 0,0 pro oba -- a ta shoda je prave to tvrzeni. Rozdil mezi hraci se
    // meri na DELCE cesty (test nize), ne na pravdepodobnosti.
    GameState plain = makeShortcutVsDetour(/*moverHasDodge=*/false, /*withTackleMarker=*/false);
    GameState dodgy = makeShortcutVsDetour(/*moverHasDodge=*/true,  /*withTackleMarker=*/false);
    const Position target{9, 5};

    // Bez Tackle: hrac s Dodge veme ZKRATKU pres jeden dodge p = 1/3.
    // Dvoustavove s jedinym pripustnym dodgem: preziti = q*(1+p) = 8/9
    // (= 1 - p², u JEDNOHO dodge se exaktni tvar a p² potkavaji).
    const double shortcut = pathFailProb(dodgy, dodgy.getPlayer(1), target, 8, kNoBlock);
    ASSERT_EQ(pathStepsToward(dodgy, dodgy.getPlayer(1), target, 8, kNoBlock), 4)
        << "hrac s Dodge nevzal zkratku -- fixtura nemeri, co tvrdi";
    EXPECT_NEAR(shortcut, 1.0 / 9.0, 1e-12);

    // Bez dovednosti se jde OBCHAZKOU, ktera dodge nema vubec.
    ASSERT_EQ(pathStepsToward(plain, plain.getPlayer(1), target, 8, kNoBlock), 5)
        << "hrac bez Dodge nevzal obchazku -- fixtura nemeri, co tvrdi";
    EXPECT_NEAR(pathFailProb(plain, plain.getPlayer(1), target, 8, kNoBlock), 0.0, 1e-12);

    // A TED TACKLE: znacka na {4,10} sousedi s OPOUSTENYM polem {5,9}, takze
    // reroll na tom kroku neplati (r. 8566-8571). Zkratka zdrazi na plnou
    // cenu a hrac s Dodge musi skoncit na TEZE obchazce jako trpaslik.
    GameState tPlain = makeShortcutVsDetour(false, /*withTackleMarker=*/true);
    GameState tDodgy = makeShortcutVsDetour(true,  /*withTackleMarker=*/true);
    EXPECT_EQ(pathStepsToward(tDodgy, tDodgy.getPlayer(1), target, 8, kNoBlock), 5)
        << "Tackle u opousteneho pole slevu za Dodge nezrusil";
    EXPECT_EQ(pathStepsToward(tPlain, tPlain.getPlayer(1), target, 8, kNoBlock), 5);
    EXPECT_NEAR(pathFailProb(tDodgy, tDodgy.getPlayer(1), target, 8, kNoBlock), 0.0, 1e-12);
}

// ===========================================================================
// CAST 2 -- DVOUVRSTVOVA DIJKSTRA MENI VYBER CESTY
// ===========================================================================

TEST(RiskWeightedPath, DodgeSkillMakesTheShortcutWorthTakingAndChangesTheRoute) {
    GameState plain = makeShortcutVsDetour(/*moverHasDodge=*/false, /*withTackleMarker=*/false);
    GameState dodgy = makeShortcutVsDetour(/*moverHasDodge=*/true,  /*withTackleMarker=*/false);
    const Position target{9, 5};

    Position sPlain{-1, -1}, sDodge{-1, -1};
    ASSERT_TRUE(nextStepToward(plain, plain.getPlayer(1), target, 8, kNoBlock, sPlain));
    ASSERT_TRUE(nextStepToward(dodgy, dodgy.getPlayer(1), target, 8, kNoBlock, sDodge));

    // ⭐ POZITIVNI KONTROLA FIXTURY: obchazka MUSI byt o presne jedno pole
    //   delsi a bez dodge, jinak test nemeri prevahu ceny, ale neco jineho.
    ASSERT_EQ(pathStepsToward(plain, plain.getPlayer(1), target, 8, kNoBlock), 5)
        << "obchazka nema pet poli -- fixtura nemeri, co tvrdi";
    ASSERT_NEAR(pathFailProb(plain, plain.getPlayer(1), target, 8, kNoBlock), 0.0, 1e-12)
        << "obchazka neni bez dodge -- fixtura nemeri, co tvrdi";

    // Trpaslik obchazi, elf s Dodge zkracuje. RUZNY PRVNI KROK, ne jen ruzna
    // celkova cena -- na tomhle stoji cely bod zdvojeneho stavu.
    EXPECT_EQ(sPlain, (Position{5, 8})) << "bez dovednosti se melo obchazet";
    EXPECT_EQ(sDodge, (Position{6, 8})) << "s dovednosti se mela vzit zkratka";
    EXPECT_NE(sPlain, sDodge);
    EXPECT_EQ(pathStepsToward(dodgy, dodgy.getPlayer(1), target, 8, kNoBlock), 4);
}

TEST(RiskWeightedPath, TackleNextToTheLeftSquareSuppressesTheRouteChange) {
    GameState plain = makeShortcutVsDetour(/*moverHasDodge=*/false, /*withTackleMarker=*/true);
    GameState dodgy = makeShortcutVsDetour(/*moverHasDodge=*/true,  /*withTackleMarker=*/true);
    const Position target{9, 5};

    Position sPlain{-1, -1}, sDodge{-1, -1};
    ASSERT_TRUE(nextStepToward(plain, plain.getPlayer(1), target, 8, kNoBlock, sPlain));
    ASSERT_TRUE(nextStepToward(dodgy, dodgy.getPlayer(1), target, 8, kNoBlock, sDodge));

    // ⭐ Znacka s Tackle nesmi zdrazit ani obchazku, ani zkratku -- jen zrusit
    //   reroll na kroku z {5,9}. Overeno tim, ze bez dovednosti se nic
    //   nezmenilo proti fixture bez Tackle.
    ASSERT_EQ(sPlain, (Position{5, 8}))
        << "znacka s Tackle zmenila i cestu bez dovednosti -- meri se neco jineho";
    ASSERT_EQ(pathStepsToward(plain, plain.getPlayer(1), target, 8, kNoBlock), 5);

    EXPECT_EQ(sDodge, sPlain)
        << "Tackle u opousteneho pole zlevneni zkratky nezrusil";
    EXPECT_EQ(pathStepsToward(dodgy, dodgy.getPlayer(1), target, 8, kNoBlock), 5);
}

// ===========================================================================
// CAST 3 -- REGRESNI ZAMEK NA HRACE BEZ DOVEDNOSTI
//
// ⛔⛔ `nextStepTowardAdjacent` je NASAZENA A UZ ZMERENA cesta M14b (parove
//   A/B, 4 800 dvojic dw-dw). Trpaslici (a vubec vsichni bez Dodge) se nesmi
//   pohnout ani o jednotku klice, jinak by zmena tise premerila neco, co uz
//   je zmerene. Layer 1 je pro ne nedosazitelna (prechod je za `hasDodge`),
//   takze to MA byt bit za bitem tentyz jednovrstvovy Dijkstra.
// ⭐ Zamek je PLOSNY, ne bodovy: prochazi CELE hriste jako cil a michá
//   vysledky vsech ctyr vstupnich bodu do jednoho kontrolniho souctu. Jedna
//   literalni konstanta chrani stovky dotazu. Poridila se spustenim TOHOTO
//   testu proti binarce PRED zmenou.
// ===========================================================================
TEST(RiskWeightedPath, NonDodgeMoverIsBitIdenticalAcrossTheWholePitch) {
    GameState s;
    s.phase = GamePhase::PLAY;
    s.activeTeam = TeamSide::HOME;
    mkPlayer(s, 1, TeamSide::HOME, {9, 7}, 3);        // BEZ dovednosti Dodge
    mkPlayer(s, 2, TeamSide::HOME, {8, 5}, 3);        // vlastni telo v ceste
    mkPlayer(s, 11, TeamSide::AWAY, {11, 6}, 3);
    mkPlayer(s, 12, TeamSide::AWAY, {11, 8}, 3);
    mkPlayer(s, 13, TeamSide::AWAY, {7, 9}, 3);
    mkPlayer(s, 14, TeamSide::AWAY, {13, 7}, 3);
    const Player& mover = s.getPlayer(1);

    uint64_t acc = 1469598103934665603ull;
    auto mix = [&acc](int64_t v) { acc = (acc ^ static_cast<uint64_t>(v)) * 1099511628211ull; };

    for (int y = 0; y < Position::PITCH_HEIGHT; ++y) {
        for (int x = 0; x < Position::PITCH_WIDTH; ++x) {
            const Position tgt{static_cast<int8_t>(x), static_cast<int8_t>(y)};
            mix(pathStepsToward(s, mover, tgt, 8, kNoBlock));
            const double pf = pathFailProb(s, mover, tgt, 8, kNoBlock);
            mix(std::llround(pf * 1e9));
            Position st{-1, -1};
            mix(nextStepToward(s, mover, tgt, 8, kNoBlock, st) ? (st.y * 100 + st.x) : -1);
            Position adj{-1, -1};
            mix(nextStepTowardAdjacent(s, mover, tgt, adj) ? (adj.y * 100 + adj.x) : -1);
        }
    }
    (void)takeBlitzPathPicksInSearch();   // citac ramene se tu nesmi vlecet dal

    // Konstanta ZMERENA na binarce z `f4237f57` (tedy PRED zdvojenim stavu),
    // ne odvozena z noveho kodu -- jinak by zamek hlidal sam sebe.
    EXPECT_EQ(acc, 10758018192652500411ull)
        << "chovani hrace BEZ dovednosti Dodge se zmenilo -- zdvojeny stav "
           "prosakuje do vrstvy 0 a nasazena cesta M14b se tise premerila";
}

// ===========================================================================
// CAST 4 -- ODSUN OD NOSICE JAKO TIEBREAK VE VYBERU POLE DOSEDNUTI
//          (B-ROUND / pripad 1, 10.09.2026)
//
// Uzivatel: "predřaď ten blitz s vyberem odkud, at jej odsuneš od nosiče."
// Pole dosednuti urcuje UTOCNOU LINII, a odstrceni jde po ni dozadu ⇒ pole
// fixuje MNOZINU kandidatu na odsun, mezi kterymi az potom vybira `P9c`
// (`pushDestScore`, block_handler.cpp). Do dneska se pole dosednuti vybiralo
// VYHRADNE podle ceny cesty a to, k cemu je pole potom dobre, nevazilo nic.
//
// ⛔⛔ ROZSAH JE NOSNY: `nextStepTowardAdjacent` je NASAZENA A ZMERENA cesta
//   M14b. Nova preference smi rozhodovat VYHRADNE pri PRESNE STEJNE cene
//   (`key`), nikdy cenu neprebijet. Test `CheaperSquareWinsEvenWhenItPushesWorse`
//   je presne ten, ktery to hlida -- bez nej je zmena neomezena.
// ===========================================================================

// Sourozenec plosneho zamku vyse, ale s micem V RUKACH SOUPERE: vlastni nosic
// neexistuje ⇒ nova preference nesmi vratit ani jeden krok jinak.
// Konstanta ZMERENA na binarce PRED touhle zmenou (commit `734e34e7`).
TEST(RiskWeightedPath, EnemyHeldBallDoesNotMoveTheBlitzApproach) {
    GameState s;
    s.phase = GamePhase::PLAY;
    s.activeTeam = TeamSide::HOME;
    mkPlayer(s, 1, TeamSide::HOME, {9, 7}, 3);        // BEZ dovednosti Dodge
    mkPlayer(s, 2, TeamSide::HOME, {8, 5}, 3);
    mkPlayer(s, 11, TeamSide::AWAY, {11, 6}, 3);
    mkPlayer(s, 12, TeamSide::AWAY, {11, 8}, 3);
    mkPlayer(s, 13, TeamSide::AWAY, {7, 9}, 3);
    mkPlayer(s, 14, TeamSide::AWAY, {13, 7}, 3);
    s.ball = BallState::carried({11, 6}, 11);         // mic nese SOUPER
    const Player& mover = s.getPlayer(1);

    // Pojistka na fixturu: mic je drzeny, ale NE nasim hracem -- jinak by tenhle
    // zamek merily jinou vetev, nez o ktere tvrdi, ze se nesmi hnout.
    ASSERT_TRUE(s.ball.isHeld);
    ASSERT_EQ(s.getPlayer(s.ball.carrierId).teamSide, TeamSide::AWAY);

    uint64_t acc = 1469598103934665603ull;
    auto mix = [&acc](int64_t v) { acc = (acc ^ static_cast<uint64_t>(v)) * 1099511628211ull; };
    for (int y = 0; y < Position::PITCH_HEIGHT; ++y) {
        for (int x = 0; x < Position::PITCH_WIDTH; ++x) {
            Position adj{-1, -1};
            const Position tgt{static_cast<int8_t>(x), static_cast<int8_t>(y)};
            mix(nextStepTowardAdjacent(s, mover, tgt, adj) ? (adj.y * 100 + adj.x) : -1);
        }
    }
    (void)takeBlitzPathPicksInSearch();

    EXPECT_EQ(acc, 10651955102882326157ull)
        << "mic v rukach SOUPERE zmenil vyber pole dosednuti -- podminka "
           "'nosic je NAS' nedrzi a nasazena cesta M14b se tise premerila";
}

namespace {
// ---------------------------------------------------------------------------
// FIXTURA 4 -- PRSTENEC S DVEMA DIRAMI.
// AWAY cil na {10,7} je obstoupen SESTI nasimi tely, volna zustavaji jen dve
// pole dosednuti: {10,6} (na sever od cile) a {10,8} (na jih). Blitzujici
// startuje z `from`, nas nosic stoji na {10,11}, tedy JIZNE od cile:
//   · dosednuti na {10,8} tlaci cil na SEVER  ({10,6}/{11,6}/{9,6}) -- PRYC
//   · dosednuti na {10,6} tlaci cil na JIH    ({10,8}/{9,8}/{11,8}) -- K NAM
// ⭐ Prstenec je z NASICH hracu zamerne: vlastni telo netvori tacklezonu, takze
//   obe zbyla pole maji tacklezonu PRESNE JEDNU (jen cil sam) a jsou tedy
//   cenove na roven -- rozdil ceny by tiebreak nikdy nepustil ke slovu.
GameState makeRingWithTwoHoles(Position from, bool withCarrier) {
    GameState s;
    s.phase = GamePhase::PLAY;
    s.activeTeam = TeamSide::HOME;
    mkPlayer(s, 1, TeamSide::HOME, from, 3);
    const Position ring[6] = {{9,6},{9,7},{9,8},{11,6},{11,7},{11,8}};
    for (int i = 0; i < 6; ++i) mkPlayer(s, 2 + i, TeamSide::HOME, ring[i], 3);
    mkPlayer(s, 11, TeamSide::AWAY, {10, 7}, 3);
    if (withCarrier) {
        mkPlayer(s, 8, TeamSide::HOME, {10, 11}, 3);
        s.ball = BallState::carried({10, 11}, 8);
    }
    return s;
}

// Kolik poli sousedicich s cilem je vubec volnych -- pojistka, ze fixtura
// postavila to, co si o ni myslim (⭐ pravidlo projektu: fixtura se overuje,
// jinak muze test projit i pri spatnem vzorci).
int freeAdjacentCount(const GameState& s, Position target) {
    int n = 0;
    for (Position p : target.getAdjacent())
        if (p.isOnPitch() && !s.getPlayerAtPosition(p)) ++n;
    return n;
}

}  // namespace

// TEST 1: pri STEJNE cene rozhodne smer odsunu.
TEST(BlitzApproach, EqualCostPicksTheSquareThatPushesAwayFromOurCarrier) {
    const Position kFrom{13, 7}, kTarget{10, 7};
    GameState noBall = makeRingWithTwoHoles(kFrom, /*withCarrier=*/false);
    GameState withBall = makeRingWithTwoHoles(kFrom, /*withCarrier=*/true);

    // --- POJISTKY NA FIXTURU ---
    ASSERT_EQ(freeAdjacentCount(noBall, kTarget), 2)
        << "prstenec nema presne dve diry -- test meri jinou geometrii";
    const Player& mover = noBall.getPlayer(1);
    // Ceny na obe diry se musi ROVNAT (delka i riziko), jinak tiebreak nema co
    // rozhodovat a test by prosel i pro spatny vzorec.
    const int stepsN = pathStepsToward(noBall, mover, {10, 6}, 8, kTarget);
    const int stepsS = pathStepsToward(noBall, mover, {10, 8}, 8, kTarget);
    ASSERT_EQ(stepsN, 3);
    ASSERT_EQ(stepsS, 3) << "cesty na obe diry nejsou stejne dlouhe";
    ASSERT_DOUBLE_EQ(pathFailProb(noBall, mover, {10, 6}, 8, kTarget),
                     pathFailProb(noBall, mover, {10, 8}, 8, kTarget))
        << "riziko obou cest se lisi -- klice tedy nejsou shodne";

    // Bez nosice vyhrava porad prvni nalezene minimum (severni dira {10,6},
    // prvni krok {12,6}) -- kontrolni strana paru.
    Position stepNoBall{-1, -1};
    ASSERT_TRUE(nextStepTowardAdjacent(noBall, mover, kTarget, stepNoBall));
    EXPECT_EQ(stepNoBall, Position(12, 6));

    // S nasim nosicem na JIHU se ma vzit JIZNI dira {10,8}, protoze odtud
    // odstrceni miri na sever, PRYC od nosice.
    Position step{-1, -1};
    ASSERT_TRUE(nextStepTowardAdjacent(withBall, withBall.getPlayer(1), kTarget, step));
    EXPECT_EQ(step, Position(12, 8))
        << "blitzujici dosedl na stranu, ze ktere tlaci cil NA nosice";
}

// TEST 2: ⛔ TENHLE TEST DRZI ROZSAH. Kdyz se ceny LISI, vyhrava LEVNEJSI pole,
// i kdyz tlaci hur -- jinak by z tiebreaku byla nova optimalizace a nasazena
// cesta M14b by se tise premerila.
TEST(BlitzApproach, CheaperSquareWinsEvenWhenItPushesTowardOurCarrier) {
    const Position kFrom{13, 7}, kTarget{10, 7};
    GameState s = makeRingWithTwoHoles(kFrom, /*withCarrier=*/true);
    // Jedno telo navic na {12,8} zdrazi JIZNI (lepe tlacici) cestu o pole.
    mkPlayer(s, 9, TeamSide::HOME, {12, 8}, 3);

    // --- POJISTKY NA FIXTURU ---
    ASSERT_EQ(freeAdjacentCount(s, kTarget), 2);
    const Player& mover = s.getPlayer(1);
    const int stepsN = pathStepsToward(s, mover, {10, 6}, 8, kTarget);
    const int stepsS = pathStepsToward(s, mover, {10, 8}, 8, kTarget);
    ASSERT_EQ(stepsN, 3);
    ASSERT_EQ(stepsS, 4)
        << "ceny se nelisi -- test by pak nemohl dokazat, ze cena je primarni";

    Position step{-1, -1};
    ASSERT_TRUE(nextStepTowardAdjacent(s, mover, kTarget, step));
    EXPECT_EQ(step, Position(12, 6))
        << "odsun prebil CENU cesty -- z tiebreaku se stala optimalizace";
}
