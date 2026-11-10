#include <gtest/gtest.h>
#include "bb/bomb_handler.h"
#include "bb/helpers.h"
#include "bb/pass_handler.h"

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

TEST(BombHandler, AccurateExplosion) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME); // Bombardier AG3
    gs.getPlayer(1).skills.add(SkillName::Bombardier);
    placePlayer(gs, 12, {13, 7}, TeamSide::AWAY); // Target

    // Distance 3 = quick pass. AG3: target = 7-3-1 = 3. Roll 5 >= 3 → accurate
    // Explosion: player 12 at (13,7) is in 3x3 around (13,7) → knocked down
    // Armor: 3+3=6 ≤ 8 not broken
    // P264 (9.11.2026): hráč 12 v cíli bombu zkouší chytit (ř. 7959-7961) — 1 = nechytí ⇒ výbuch.
    FixedDiceRoller dice({5, 1, 3, 3});
    auto result = resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.turnover); // Never turnover
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BombHandler, InaccurateTripleScatter) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME, 6, 3, 2, 8); // AG2
    gs.getPlayer(1).skills.add(SkillName::Bombardier);
    placePlayer(gs, 12, {13, 7}, TeamSide::AWAY);

    // AG2: target = 7-2-1 = 4. Roll 2 < 4 → inaccurate
    // 3x scatter from target (13,7): D8=1,1,1 → each (1,0) → (16,7)
    // No player at explosion → no effect
    FixedDiceRoller dice({2, 1, 1, 1});
    auto result = resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING); // Not hit
}

// ============================================================================
// TA4 (24.08.2026): tri z peti puvodnich testu certifikovaly vadu, a jeden se
// tak i JMENOVAL -- `NeverTurnover`, s komentarem "Even if fumble with ball
// carrier, never turnover". BB2016 l. 7956-7958 rika presny opak.
// ============================================================================

TEST(BombHandler, FumbleExplodesInTheThrowersOwnSquareAndIsATurnover) {
    // l. 7967-7968: "If the bomb is fumbled it explodes IN THE BOMB THROWER'S
    // SQUARE." Puvodne se rozptylovala D8 od hazece, takze si hazec vetsinou
    // ublizit nemohl. A l. 7969-7970: kdo je ve stejnem poli, JDE K ZEMI.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Bombardier);
    placePlayer(gs, 12, {13, 7}, TeamSide::AWAY);

    // hod 1 => fumble; vybuch v (10,7); hazec sam = automaticky srazen,
    // zbroj 2D6 = 2+2 = 4 <= AV8 => neproraženo, ale SRAZEN uz je.
    FixedDiceRoller dice({1, 2, 2});
    auto result = resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE)
        << "hazec zadnou imunitu nema -- pravidlo o ni nikde nemluvi";
    EXPECT_TRUE(result.turnover)
        << "l. 7956-7958: vybuch, ktery srazi hráče aktivniho tymu, JE turnover";
}

TEST(BombHandler, AdjacentPlayersAreOnlyKnockedDownOnFourPlus) {
    // l. 7970-7971: "players in ADJACENT squares are Knocked Down ON A ROLL OF
    // 4+". Puvodne se srazelo cele okoli 3x3 automaticky.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Bombardier);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);   // soused vybuchu

    // hod 1 => fumble, vybuch v (10,7).
    // Pole (10,7) je hazec: automaticky srazen, zbroj 2+2 = 4 => neproraženo.
    // Soused (11,7): hod 3 => MENE nez 4 => nezasazen.
    FixedDiceRoller dice({1, 2, 2, 3});
    resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING);
}

TEST(BombHandler, TheBombHitsPRONEPlayersToo) {
    // l. 7972-7974: "Players can be hit by a bomb and treated as Knocked Down
    // EVEN IF THEY ARE ALREADY PRONE OR STUNNED." Puvodne se preskakovali.
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Bombardier);
    placePlayer(gs, 12, {11, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::PRONE;

    // fumble => vybuch v (10,7); hazec zbroj 2+2; soused hod 5 => zasazen,
    // jeho zbroj 3+3 = 6 <= AV8 => neproraženo, ale hod na nej PADL.
    FixedDiceRoller dice({1, 2, 2, 5, 3, 3});
    resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);   // zustava na zemi
}

TEST(BombHandler, ABombInTheCrowdExplodesWithNoEffect) {
    // l. 7968-7969: "If a bomb lands in the crowd, it explodes with no effect."
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {0, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Bombardier);

    // hod 2 (nepresna, cil 4 pro AG2 na kratkou), tri rozptyly na zapad
    // z ciloveho pole (2,7) => (-1,7) = mimo hriste
    FixedDiceRoller dice({2, 7, 7, 7});
    auto result = resolveBombThrow(gs, 1, {2, 7}, dice, nullptr);
    EXPECT_FALSE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
}

// P73 (30.09.2026), port PHP d9910a96.
// (a) Bomba se hazi "using the rules for throwing the ball" (r. 7952-7954)
//     a fumble je "1 OR LESS BEFORE OR AFTER MODIFICATION" (r. 1742-1744).
//     C++ fumbloval jen na prirozene 1.
TEST(BombHandler, P73FumbleCountsTheModifiedRoll) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);          // AG3
    gs.getPlayer(1).skills.add(SkillName::Bombardier);
    placePlayer(gs, 12, {11, 6}, TeamSide::AWAY);          // dve tackle zony
    placePlayer(gs, 13, {11, 8}, TeamSide::AWAY);

    // quick +1, zony -2 => modifikace -1; hod 2 => 1 = FUMBLE, vybuchne
    // v poli hazece. Pak: brneni hazece 3+3, sousedi 1 a 1 (minou).
    FixedDiceRoller dice({2, 3, 3, 1, 1, 1, 1, 1, 1});
    auto result = resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);

    EXPECT_TRUE(result.turnover);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE)
        << "hod 2 se dvema zonami se bral jako neprecny, ne jako fumble";
}

// (b) "treated as Knocked Down even if they are already Prone or Stunned"
//     (r. 7973-7974): srazenemu se hazi brneni, ale OMRACENY neni
//     otoceny licem nahoru -- zustava omraceny, pokud ho zraneni nezhorsi.
TEST(BombHandler, P73AStunnedPlayerHitByTheBombStaysStunned) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Bombardier);
    placePlayer(gs, 12, {13, 7}, TeamSide::AWAY);
    gs.getPlayer(12).state = PlayerState::STUNNED;

    FixedDiceRoller dice({5, 3, 3});                        // presne; brneni 6 neprorazi
    resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);

    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STUNNED);
}

// ============================================================================
// P251 (6.11.2026, uzivatel: „bombu jen na fumble“): fumble bomby se prehazuje,
// nepresna ne. Pravidla r. 7957-7958: "All skills that may be used when a ball
// is thrown may be used when a bomb is thrown also"; Pass r. 8336-8337; tymovy
// prehoz r. 929-936; jeden hod = nejvys jeden prehoz r. 925-927.
// ============================================================================

// Hazec AG3 na (10,7), cil (13,7), dosah quick => cil hodu 3. Hod 1 = fumble.
static GameState makeBombSetup(int rerolls) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.homeTeam.side = TeamSide::HOME;
    gs.awayTeam.side = TeamSide::AWAY;
    gs.homeTeam.rerolls = rerolls;
    gs.homeTeam.turnNumber = 1;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Bombardier);
    placePlayer(gs, 12, {13, 7}, TeamSide::AWAY);
    return gs;
}

TEST(BombHandler, P251FumbleIsRerolledByTeamReroll) {
    GameState gs = makeBombSetup(2);
    // 1 = fumble, tymovy prehoz -> 5 = presne; vybuch v cili, brneni 3+3 neprorazi.
    // Stary kod: fumble stoji, vybuch u hazece (hazec PRONE, prehoz neutracen).
    FixedDiceRoller dice({1, 5, 1, 3, 3});   // P264: 1 = hráč 12 v cíli bombu nechytí
    auto result = resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);

    EXPECT_EQ(gs.homeTeam.rerolls, 1) << "r. 929-936: tymovy prehoz se utratil";
    EXPECT_TRUE(gs.homeTeam.rerollUsedThisTurn);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE) << "bomba dopadla podle noveho hodu";
    EXPECT_FALSE(result.turnover);
}

TEST(BombHandler, P251InaccurateBombIsNotRerolled) {
    // Rozhodnuti uzivatele: nepresna bomba se NEPREHAZUJE, ani kdyz prehoz je.
    // (Hlidaci test: na starem kodu take prochazi.)
    GameState gs = makeBombSetup(2);
    FixedDiceRoller dice({2, 1, 1, 1});    // 2 < 3 nepresna; 3x rozptyl na vychod -> (16,7)
    auto result = resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);

    EXPECT_EQ(gs.homeTeam.rerolls, 2);
    EXPECT_FALSE(gs.homeTeam.rerollUsedThisTurn);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING);
    EXPECT_FALSE(result.turnover);
}

TEST(BombHandler, P251PassSkillRerollsFumbleWithoutSpendingTeamReroll) {
    GameState gs = makeBombSetup(2);
    gs.getPlayer(1).skills.add(SkillName::Pass);
    FixedDiceRoller dice({1, 5, 1, 3, 3});    // fumble -> Pass: 5 presne; P264: 1 = 12 nechytí
    auto result = resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);

    EXPECT_EQ(gs.homeTeam.rerolls, 2) << "r. 8336-8337: Pass je zdarma";
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
    EXPECT_FALSE(result.turnover);
}

TEST(BombHandler, P251ProRerollsFumbleBeforeTeamReroll) {
    GameState gs = makeBombSetup(2);
    gs.getPlayer(1).skills.add(SkillName::Pro);
    FixedDiceRoller dice({1, 4, 5, 1, 3, 3});  // fumble, Pro 4+ projde, novy hod 5; P264: 1 = 12 nechytí
    auto result = resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);

    EXPECT_TRUE(gs.getPlayer(1).proUsedThisTurn);
    EXPECT_EQ(gs.homeTeam.rerolls, 2);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
    EXPECT_FALSE(result.turnover);
}

TEST(BombHandler, P251FumbleStandsWhenTeamRerollAlreadyUsedThisTurn) {
    // r. 929-936: jeden tymovy prehoz za kolo. (Hlidaci test.)
    GameState gs = makeBombSetup(2);
    gs.homeTeam.rerollUsedThisTurn = true;
    FixedDiceRoller dice({1, 2, 2});
    auto result = resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);

    EXPECT_EQ(gs.homeTeam.rerolls, 2);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_TRUE(result.turnover);
}

TEST(BombHandler, P251LonerBomberDoesNotSpendTeamRerollOnFumble) {
    // Uzivatel 6.11.: „jestli ma bomber loner - tak se prikloni k neprehozeni
    // ani fumble“. (Hlidaci test: stary kod take neprehazuje.)
    GameState gs = makeBombSetup(2);
    gs.getPlayer(1).skills.add(SkillName::Loner);
    FixedDiceRoller dice({1, 2, 2});
    auto result = resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);

    EXPECT_EQ(gs.homeTeam.rerolls, 2);
    EXPECT_FALSE(gs.homeTeam.rerollUsedThisTurn);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_TRUE(result.turnover);
}

TEST(BombHandler, P251LonerBomberWithPassSkillStillRerollsFumble) {
    // Loner omezuje jen TYMOVE prehozy (r. 8287-8290); Pass zustava.
    GameState gs = makeBombSetup(2);
    gs.getPlayer(1).skills.add(SkillName::Loner);
    gs.getPlayer(1).skills.add(SkillName::Pass);
    FixedDiceRoller dice({1, 5, 1, 3, 3});   // P264: 1 = 12 nechytí
    auto result = resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);

    EXPECT_EQ(gs.homeTeam.rerolls, 2);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
    EXPECT_FALSE(result.turnover);
}

// ───────────── P251b (uživatel 7.11.2026: „přehoz bomby se má člověka ptát“) ─────────────
// Živá partie: kouč-člověk rozhoduje o přehozu fumble bomby sám (nechat / tým / Pro), jako u přihrávky
// (CoachQuestion::PASS_REROLL). Dřív se ho engine neptal a fumble nechal stát (kromě Pass zdarma).
#include "bb/coach_choice.h"

static GameState bombCoachSetup() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.homeTeam.rerolls = 3;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);   // AG3, rychlý hod na (13,7): 3+
    gs.getPlayer(1).skills.add(SkillName::Bombardier);
    placePlayer(gs, 12, {13, 7}, TeamSide::AWAY);
    return gs;
}

TEST(CoachChoiceBomb, P251bFumble_CoachIsAsked) {
    GameState gs = bombCoachSetup();
    FixedDiceRoller dice({1, 5, 3, 3});
    CoachChoiceScope scope(TeamSide::HOME, {});
    try { resolveBombThrow(gs, 1, {13, 7}, dice, nullptr); FAIL(); } catch (const CoachChoicePending& p) {
        EXPECT_EQ(p.question, CoachQuestion::BOMB_REROLL);
        EXPECT_EQ(p.roll, 1);
        EXPECT_EQ(p.target, 3);
    }
}

TEST(CoachChoiceBomb, P251bInaccurateBomb_CoachIsNotAsked) {
    // nepřesná bomba se nepřehazuje (uživatel 6.11.) ⇒ ani se neptá
    GameState gs = bombCoachSetup();
    gs.getPlayer(1).stats.agility = 2;               // cíl 4+, hod 2 = nepřesná, ne fumble
    FixedDiceRoller dice({2, 1, 1, 1});
    CoachChoiceScope scope(TeamSide::HOME, {});
    EXPECT_NO_THROW(resolveBombThrow(gs, 1, {13, 7}, dice, nullptr));
    EXPECT_EQ(gs.homeTeam.rerolls, 3);
}

TEST(CoachChoiceBomb, P251bTeamRerollOnFumble_BombHitsTheTarget) {
    GameState gs = bombCoachSetup();
    FixedDiceRoller dice({1, 5, 1, 3, 3});           // fumble → týmový přehoz 5 → přesná; P264: 12 nechytí (1); zbroj 3+3
    CoachChoiceScope scope(TeamSide::HOME, {CoachAnswer::TEAM});
    resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);
    EXPECT_EQ(gs.homeTeam.rerolls, 2);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);       // vybuchla na cíli
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::STANDING);      // ne u házeče
}

// ───────────── P256 (review dávky 7.11.2026): bomba = hod míčem (ř. 7952-7958) ─────────────
// „The bomb is thrown using the rules for throwing the ball (including weather effects …)“ a „All skills that may
// be used when a ball is thrown may be used when a bomb is thrown“ ⇒ Accurate, Strong Arm, vánice jako u přihrávky.
// Dřív C++ bomba Accurate ani Strong Arm nebrala a ve vánici házela i dlouhé (PHP Accurate bralo přes getAccuracyTarget).
static GameState bombP256() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {10, 7}, TeamSide::HOME);   // AG3
    gs.getPlayer(1).skills.add(SkillName::Bombardier);
    placePlayer(gs, 12, {13, 7}, TeamSide::AWAY);   // rychlý hod na (13,7)
    return gs;
}

TEST(BombHandler, P256AccurateHelpsTheBomb) {
    GameState gs = bombP256();
    gs.getPlayer(1).skills.add(SkillName::Accurate);
    // cíl 7-3-1(rychlá)-1(Accurate) = 2: hod 2 = přesná ⇒ výbuch na (13,7); zbroj 3+3. Bez Accurate cíl 3 ⇒ rozptyl.
    FixedDiceRoller dice({2, 1, 3, 3});   // P264: 1 = 12 nechytí
    resolveBombThrow(gs, 1, {13, 7}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BombHandler, P256StrongArmHelpsAShortBomb) {
    GameState gs = bombP256();
    gs.getPlayer(1).skills.add(SkillName::StrongArm);
    gs.getPlayer(12).position = {15, 7};             // 5 polí = krátká (0); Strong Arm ⇒ jako rychlá (+1): cíl 3
    FixedDiceRoller dice({3, 1, 3, 3});               // hod 3 = přesná (bez Strong Arm cíl 4 ⇒ rozptyl); P264: 1 = nechytí
    resolveBombThrow(gs, 1, {15, 7}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BombHandler, P256NoLongBombInABlizzard) {
    GameState gs = bombP256();
    gs.weather = Weather::BLIZZARD;
    gs.getPlayer(12).position = {19, 7};             // 9 polí = dlouhá ⇒ ve vánici nejde
    FixedDiceRoller dice({});
    auto result = resolveBombThrow(gs, 1, {19, 7}, dice, nullptr);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING);
}

TEST(BombHandler, P262PassEventCarriesTheTargetAndTheDie) {
    // konvence P201/P145: roll = cílové číslo, die1 = kostka (dřív u bomby obráceně)
    GameState gs = bombP256();
    std::vector<GameEvent> ev;
    FixedDiceRoller dice({5, 1, 3, 3});              // rychlá, cíl 3; hod 5; P264: 1 = 12 nechytí
    resolveBombThrow(gs, 1, {13, 7}, dice, &ev);
    bool seen = false;
    for (const auto& e : ev) if (e.type == GameEvent::Type::PASS) { seen = true; EXPECT_EQ(e.roll, 3); EXPECT_EQ(e.die1, 5); }
    EXPECT_TRUE(seen);
}

// ───────────── P263 (uživatel 7.11.2026: „OK“): Hail Mary u bomby ─────────────
// Ř. 7952-7954: bomba se hází „using the rules for throwing the ball (including … use of Hail Mary Pass)“.
// Ř. 8164-8169: cíl kdekoli na hřišti, pravítko se nepoužívá; D6: 1 = fumble (bomba vybuchne u házeče, ř. 7966-7967),
// 2-6 = hod projde, ale „never accurate“ ⇒ vždy 3 rozptyly od cíle; nejde zachytit. Ve vánici ne (ř. 8170).
// Hod NENÍ test obratnosti s modifikátory (tak to tvrdil podklad z webu) — jen 1 vs 2-6.
static GameState bombHmp() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {3, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Bombardier);
    gs.getPlayer(1).skills.add(SkillName::HailMaryPass);
    placePlayer(gs, 12, {23, 7}, TeamSide::AWAY);     // 20 polí — mimo pravítko
    return gs;
}

TEST(BombHandler, P263HailMaryBombScattersThreeTimesFromTheTarget) {
    GameState gs = bombHmp();
    // hod 2 (projde); rozptyly D8=3 (V) ×3 z (20,7) → (23,7) ⇒ výbuch na hráči 12; zbroj 3+3
    FixedDiceRoller dice({2, 3, 3, 3, 1, 3, 3});      // P264: po rozptylech 1 = 12 nechytí
    auto result = resolveBombThrow(gs, 1, {20, 7}, dice, nullptr);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
}

TEST(BombHandler, P263HailMaryBombIsNeverAccurate) {
    // i šestka se rozptýlí: hod 6; rozptyly D8=7 (Z) ×3 z (23,7) → (20,7) ⇒ hráč 12 na (23,7) zůstane stát
    GameState gs = bombHmp();
    FixedDiceRoller dice({6, 7, 7, 7});
    // review 9.11. (M3): bez `success` a spotřeby kostek byl test zelený i na starém kódu (hod odmítnut) i s přesnou
    // šestkou (12 by bombu chytil a hodil jinam)
    auto r = resolveBombThrow(gs, 1, {23, 7}, dice, nullptr);
    EXPECT_TRUE(r.success);
    EXPECT_EQ(dice.remaining(), 0u);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING);
}

TEST(BombHandler, P263HailMaryBombFumbleOnOneExplodesAtTheThrower) {
    GameState gs = bombHmp();
    gs.homeTeam.rerolls = 0;
    FixedDiceRoller dice({1, 3, 3});                  // 1 = fumble ⇒ výbuch u házeče; zbroj 3+3
    auto result = resolveBombThrow(gs, 1, {20, 7}, dice, nullptr);
    EXPECT_TRUE(result.turnover);                     // ř. 7956-7958: srazí vlastního hráče
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
}

TEST(BombHandler, P263NoHailMaryBombInABlizzard) {
    GameState gs = bombHmp();
    gs.weather = Weather::BLIZZARD;
    FixedDiceRoller dice({});
    auto result = resolveBombThrow(gs, 1, {20, 7}, dice, nullptr);
    EXPECT_FALSE(result.success);
}

// ───────────── P264 (uživatel 9.11.2026: „přidej možnost intercept“): zachycení a chycení bomby ─────────────
// Ř. 7959-7966: „A bomb may be intercepted or caught using the same rules for catching the ball, in which case the
// player catching it must throw it again immediately … The bomb explodes when it lands in an empty square or an
// opportunity to catch the bomb fails or is declined.“ Zachycení se hlásí před hodem (ř. 1773-1774); nepovedené
// zachycení ⇒ bomba letí dál (uživatel). Hail Mary zachytit nejde (ř. 8167). Zachycená bomba není turnover (ř. 7955),
// výbuch, který srazí hráče týmu na tahu, ano (ř. 7956-7958).
static GameState bombP264() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {5, 7}, TeamSide::HOME);
    gs.getPlayer(1).skills.add(SkillName::Bombardier);
    placePlayer(gs, 12, {10, 7}, TeamSide::AWAY);     // cíl (5 polí = krátká)
    return gs;
}

TEST(BombHandler, P264InterceptedBombIsThrownBackAtUs) {
    GameState gs = bombP264();
    placePlayer(gs, 13, {8, 7}, TeamSide::AWAY);      // v dráze; zachycení 7-3+2 = 6+
    gs.getPlayer(1).skills.add(SkillName::NoHands);   // házeč nechytá ani nezachytává ⇒ hod zpět míří přímo na něj
    // 6 = zachytil ⇒ 13 hází na jediného našeho (5,7): rychlá, cíl 3, hod 6 přesně ⇒ výbuch v (5,7); zbroj 3+3
    FixedDiceRoller dice({6, 6, 3, 3});
    auto r = resolveBombThrow(gs, 1, {10, 7}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING) << "do cíle bomba nedoletěla";
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_TRUE(r.turnover) << "výbuch srazil hráče týmu na tahu";
    EXPECT_EQ(dice.remaining(), 0u);
}

TEST(BombHandler, P264FailedInterceptionLetsTheBombFlyOn) {
    GameState gs = bombP264();
    placePlayer(gs, 13, {8, 7}, TeamSide::AWAY);
    // 1 = zachycení selhalo ⇒ hod 6 přesně; 12 v cíli chytá (3+), hodí 1 ⇒ výbuch v (10,7); zbroj 12 3+3
    FixedDiceRoller dice({1, 6, 1, 3, 3});
    auto r = resolveBombThrow(gs, 1, {10, 7}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(13).state, PlayerState::STANDING);   // 2 pole od výbuchu
    EXPECT_FALSE(r.turnover);
}

TEST(BombHandler, P264NobodyInterceptsABombThatDoesNotThreatenHisTeam) {
    GameState gs = bombP264();
    gs.getPlayer(12).position = {20, 1};
    placePlayer(gs, 13, {8, 9}, TeamSide::AWAY);      // v dráze na prázdné (10,10), od cíle 2 pole
    FixedDiceRoller dice({6});                         // jen hod na přesnost — žádný pokus o zachycení
    auto r = resolveBombThrow(gs, 1, {10, 10}, dice, nullptr);
    EXPECT_TRUE(r.success);
    EXPECT_EQ(dice.remaining(), 0u);
    EXPECT_EQ(gs.getPlayer(13).state, PlayerState::STANDING);
}

TEST(BombHandler, P264CaughtBombIsThrownOnByTheCatcher) {
    GameState gs = bombP264();
    gs.getPlayer(1).skills.add(SkillName::NoHands);   // házeč nechytá ani nezachytává ⇒ hod zpět míří přímo na něj
    // hod 6 přesně; 12 chytá (přesná +1 ⇒ 3+) hodí 6 ⇒ hází na 1 (5,7): krátká, cíl 4, hod 6 ⇒ výbuch; zbroj 3+3
    FixedDiceRoller dice({6, 6, 6, 3, 3});
    auto r = resolveBombThrow(gs, 1, {10, 7}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING);
    EXPECT_EQ(gs.getPlayer(1).state, PlayerState::PRONE);
    EXPECT_TRUE(r.turnover);
    EXPECT_EQ(dice.remaining(), 0u);
}

TEST(BombHandler, P264HailMaryBombCannotBeIntercepted) {
    GameState gs = bombHmp();
    placePlayer(gs, 13, {19, 7}, TeamSide::AWAY);     // v dráze, vedle cíle (20,7) — u běžné bomby by zachytával
    // tytéž kostky jako P263HailMaryBombScattersThreeTimesFromTheTarget: hod 2, 3× V ⇒ (23,7), 12 nechytí (1), zbroj
    FixedDiceRoller dice({2, 3, 3, 3, 1, 3, 3});
    resolveBombThrow(gs, 1, {20, 7}, dice, nullptr);
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::PRONE);
    EXPECT_EQ(gs.getPlayer(13).state, PlayerState::STANDING);
}

// Review dávky 9.11. (M1): hod chycené bomby týmem, který není na tahu — týmový přehoz jen ve vlastním tahu
// (ř. 929-932). Engine to hlídal, ale kouč-člověk dostal volbu TEAM vždy.
TEST(CoachChoiceBomb, P264RethrowByTheTeamNotOnTurnOffersNoTeamReroll) {
    GameState gs = bombP264();
    placePlayer(gs, 13, {8, 7}, TeamSide::AWAY);
    gs.awayTeam.rerolls = 3;
    FixedDiceRoller dice({6, 1, 1, 3, 3});          // 13 zachytí; hod dál = fumble
    CoachChoiceScope scope(TeamSide::AWAY, {0});     // P272: kam hodit — 0 = jako AI; pak otázka na přehoz
    try {
        resolveBombThrow(gs, 1, {10, 7}, dice, nullptr);
    } catch (const CoachChoicePending& p) {
        EXPECT_EQ(p.question, CoachQuestion::BOMB_REROLL);
        EXPECT_EQ(p.options & (1 << CoachAnswer::TEAM), 0) << "AWAY není na tahu";
    }
    EXPECT_EQ(gs.awayTeam.rerolls, 3);
}

// Review 9.11.: při stejné šanci a stejném Catch zachytává nižší id (parita s PHP najdiZachycujiciho).
TEST(Interception, TieGoesToTheLowerId) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    placePlayer(gs, 1, {5, 5}, TeamSide::HOME);
    placePlayer(gs, 14, {8, 6}, TeamSide::AWAY);
    placePlayer(gs, 13, {8, 4}, TeamSide::AWAY);
    EXPECT_EQ(pickInterceptor(gs, gs.getPlayer(1), {11, 5}), 13);
}

// ───────────── P272 (uživatel 9.11.2026): kouč-člověk volí, kam hodit chycenou bombu ─────────────
// „player must try to catch and if success throw — na ptaní mi z toho vyplývá — kam hodit po chycení“. Chytání je
// povinné (ř. 7964-7966), hod dál taky (ř. 7961-7963); volba je jen cíl.
TEST(CoachChoiceBomb, P272HumanCatcherIsAskedWhereToThrow) {
    GameState gs = bombP264();                       // HOME 1 hází na AWAY 12 (10,7)
    FixedDiceRoller dice({6, 6});                     // přesně; 12 chytí
    CoachChoiceScope scope(TeamSide::AWAY, {});
    try {
        resolveBombThrow(gs, 1, {10, 7}, dice, nullptr);
        FAIL() << "kouč AWAY měl dostat otázku";
    } catch (const CoachChoicePending& p) {
        EXPECT_EQ(p.question, CoachQuestion::BOMB_RETHROW_TARGET);
        EXPECT_EQ(p.playerId, 12);
        EXPECT_EQ(p.target, 1007);
        EXPECT_GT(p.roll, 1);
        EXPECT_EQ(p.options, (1 << (p.roll + 1)) - 1);
    }
}

TEST(CoachChoiceBomb, P272EngineSideIsNotAsked) {
    GameState gs = bombP264();
    gs.getPlayer(1).skills.add(SkillName::NoHands);   // hod zpět míří na 1 (5,7), výbuch, žádné chytání
    FixedDiceRoller dice({6, 6, 6, 3, 3});
    CoachChoiceScope scope(TeamSide::HOME, {});        // člověk hraje HOME, chytá AWAY (engine)
    EXPECT_NO_THROW(resolveBombThrow(gs, 1, {10, 7}, dice, nullptr));
}

TEST(CoachChoiceBomb, P272AnswerPicksTheOfferedSquare) {
    GameState base = bombP264();
    const std::vector<Position> cand = bombRethrowCandidates(base, base.getPlayer(12));
    ASSERT_GE(cand.size(), 2u);
    EXPECT_EQ(cand[0], chooseBombTarget(base, base.getPlayer(12), true).pos) << "pole 1 = volba enginu";
    auto thrownTo = [&](int answer) {
        GameState gs = base;
        std::vector<GameEvent> ev;
        // přesně, 12 chytí, hod dál = fumble ⇒ výbuch u 12 (bez dalších voleb); zbytek jedničky na zbroj/sousedy
        std::vector<int> d{6, 6, 1};
        d.resize(40, 1);
        FixedDiceRoller dice(d);
        CoachChoiceScope scope(TeamSide::AWAY, {answer});
        resolveBombThrow(gs, 1, {10, 7}, dice, &ev);
        Position to{-1, -1};
        for (const auto& e : ev) if (e.type == GameEvent::Type::PASS && e.playerId == 12) to = e.to;
        return to;
    };
    EXPECT_EQ(thrownTo(2), cand[1]);
    EXPECT_EQ(thrownTo(0), cand[0]);
    EXPECT_EQ(thrownTo(1), cand[0]);
}

// P275: bomba se hází „using the rules for throwing the ball“ (ř. 7952-7953) ⇒ i Stunty −1 (ř. 8533-8534).
// Rychlá na (13,7): bez Stunty cíl 3 (hod 3 přesně ⇒ 12 v centru padne), se Stunty 4 ⇒ nepřesná, rozptyl 3× pryč.
TEST(BombHandler, P275StuntyBombardierThrowsWithMinusOne) {
    GameState gs = bombP256();
    gs.getPlayer(1).skills.add(SkillName::Stunty);
    std::vector<GameEvent> ev;
    FixedDiceRoller dice({3, 2, 2, 2, 6, 6, 6, 6});   // rozptyl D8 2 = sever 3× ⇒ (13,4), daleko od 12
    resolveBombThrow(gs, 1, {13, 7}, dice, &ev);
    for (const auto& e : ev)
        if (e.type == GameEvent::Type::PASS) {
            EXPECT_EQ(e.roll, 4);
            EXPECT_FALSE(e.success);
        }
    EXPECT_EQ(gs.getPlayer(12).state, PlayerState::STANDING);
}
