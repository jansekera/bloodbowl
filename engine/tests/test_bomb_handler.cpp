#include <gtest/gtest.h>
#include "bb/bomb_handler.h"
#include "bb/helpers.h"

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
    FixedDiceRoller dice({5, 3, 3});
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
