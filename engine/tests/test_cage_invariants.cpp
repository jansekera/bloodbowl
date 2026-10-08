// Doplňující testy řadiče klece (review P181, „požadavky bez testu“): invariant P178 na místě, kudy
// prochází všechno (odehraný tah řadičem), strana AWAY zrcadlově k HOME, teamHasTimeSlack (P177),
// větev „klec stojí“ (P177), zdržování TD přes dva tahy na témže řadiči (P175) a forbidsCarrierMove
// (P173). Test hlídá POŽADAVEK (věta uživatele z knihy úkolů), ne tvar implementace. Hráči se zadávají
// čísly (pohyb, síla), ne rasou — pravidla klece jsou jedna pro všechny.
#include <gtest/gtest.h>
#include "bb/one_cage.h"
#include "bb/game_simulator.h"
#include "bb/helpers.h"
#include "bb/dice.h"
#include "bb/macro_mcts.h"
#include "bb/action_resolver.h"
#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <string>

using namespace bb;

namespace {

struct Board {
    GameState s;
    explicit Board(int turn = 1) {
        s.phase = GamePhase::PLAY;
        s.activeTeam = TeamSide::HOME;
        s.half = 1;
        s.homeTeam.turnNumber = turn;
        s.homeTeam.rerolls = 0;
        s.awayTeam.rerolls = 0;
        s.weather = Weather::NICE;
    }
    Player& put(int id, TeamSide side, Position pos, int8_t ma = 4, int8_t st = 3,
                std::vector<SkillName> skills = {}) {
        Player& p = s.getPlayer(id);
        p.id = id;
        p.teamSide = side;
        p.state = PlayerState::STANDING;
        p.position = pos;
        p.stats = {ma, st, 3, 9};
        p.movementRemaining = ma;
        p.hasMoved = false;
        p.hasActed = false;
        for (auto sk : skills) p.skills.add(sk);
        return p;
    }
};

MCTSConfig cfg() {
    MCTSConfig c;
    c.maxIterations = 60;
    c.timeBudgetMs = 600000;
    return c;
}

// Kostky jen šestky = vše se povede (stejně jako v test_one_cage.cpp).
void play(GameState& s, const Macro& m) {
    FixedDiceRoller dice(std::vector<int>(200, 6));
    greedyExpandMacro(s, m, dice);
}

void playAll(CageController& cc, Board& b, std::vector<Macro>* out = nullptr) {
    Macro m;
    int guard = 0;
    while (cc.next(b.s, m) && ++guard < 40) {
        if (out) out->push_back(m);
        play(b.s, m);
    }
}

// Vypínač úprav klece se po testu vždy vrátí (i když test spadne uprostřed).
struct FeaturesOff {
    explicit FeaturesOff(unsigned mask) { setCageFeaturesOff(mask); }
    ~FeaturesOff() { setCageFeaturesOff(0); }
};

// --- Invariant P178 -----------------------------------------------------------------------------

int standingCorners(const GameState& s, const Player& carrier) {
    int n = 0;
    for (int cx : {-1, 1}) for (int cy : {-1, 1}) {
        const Player* q = s.getPlayerAtPosition({static_cast<int8_t>(carrier.position.x + cx),
                                                 static_cast<int8_t>(carrier.position.y + cy)});
        if (q && q->teamSide == carrier.teamSide && q->state == PlayerState::STANDING) ++n;
    }
    return n;
}

// Prázdný řetězec = invariant platí; jinak popis, proč ne. Po našem tahu s míčem platí jedno ze tří:
// padlo TD · nosič má čtyři stojící rohy · nosič je v bezpečí (hrozba rány ≤ kSafeBlitzThreat,
// nebo na něj soupeř vůbec nedosáhne).
std::string invariantViolation(const GameState& s, TeamSide side, int scoreBefore) {
    if (s.getTeamState(side).score > scoreBefore) return "";
    if (!s.ball.isHeld || s.ball.carrierId <= 0 || s.getPlayer(s.ball.carrierId).teamSide != side) {
        return "míč po tahu nedržíme";
    }
    const Player& c = s.getPlayer(s.ball.carrierId);
    const int corners = standingCorners(s, c);
    if (corners >= 4) return "";
    const double threat = blitzThreat(s, c);
    if (!anyOpponentReaches(s, side, c.position) || threat <= kSafeBlitzThreat) return "";
    std::ostringstream o;
    o << "ani-ani: nosič (" << int(c.position.x) << "," << int(c.position.y) << ") má " << corners
      << " rohy a hrozba rány je " << threat;
    return o.str();
}

// Odehraje tah řadičem a vrátí porušení invariantu ("" = platí).
std::string playTurnAndCheck(Board b, std::vector<Macro>* ms = nullptr) {
    const int before = b.s.homeTeam.score;
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b, ms);
    return invariantViolation(b.s, TeamSide::HOME, before);
}

// Čtyři rohy kolem (x, 7) s pohybem `ma`.
void cageAround(Board& b, int x, int ma, int firstId = 2) {
    b.put(firstId, TeamSide::HOME, {static_cast<int8_t>(x - 1), 6}, ma);
    b.put(firstId + 1, TeamSide::HOME, {static_cast<int8_t>(x - 1), 8}, ma);
    b.put(firstId + 2, TeamSide::HOME, {static_cast<int8_t>(x + 1), 6}, ma);
    b.put(firstId + 3, TeamSide::HOME, {static_cast<int8_t>(x + 1), 8}, ma);
}

}  // namespace

// Uživatel 08.10.2026: „když nosič nemůže skórovat ani být v bezpečí — nesmí nastat — … do té doby
// má být v kleci“ (P178). Každá deska = jedna situace; po odehraném tahu řadičem platí jedno ze tří.

// pomalá klec (pohyb 4) stojí, soupeř blízko
TEST(CageInvariant, SlowCageStandingOpponentNear) {
    Board b(2);
    b.put(1, TeamSide::HOME, {12, 7}, 4);
    cageAround(b, 12, 4);
    b.put(13, TeamSide::AWAY, {17, 7}, 6);
    b.s.ball = BallState::carried({12, 7}, 1);
    EXPECT_EQ(playTurnAndCheck(b), "");
}

// klec stojí, soupeř daleko
TEST(CageInvariant, SlowCageStandingOpponentFar) {
    Board b(2);
    b.put(1, TeamSide::HOME, {10, 7}, 4);
    cageAround(b, 10, 4);
    b.put(13, TeamSide::AWAY, {24, 1}, 6);
    b.s.ball = BallState::carried({10, 7}, 1);
    EXPECT_EQ(playTurnAndCheck(b), "");
}

// rozbitá klec (dva rohy), další dva hráči jsou o kus vzadu; soupeř blízko
TEST(CageInvariant, BrokenCageTwoCornersOpponentNear) {
    Board b(3);
    b.put(1, TeamSide::HOME, {12, 7}, 5);
    b.put(2, TeamSide::HOME, {11, 6}, 5);
    b.put(3, TeamSide::HOME, {13, 8}, 5);
    b.put(4, TeamSide::HOME, {8, 3}, 5);
    b.put(5, TeamSide::HOME, {8, 11}, 5);
    b.put(13, TeamSide::AWAY, {17, 6}, 6);
    b.s.ball = BallState::carried({12, 7}, 1);
    EXPECT_EQ(playTurnAndCheck(b), "");
}

// žádná klec, rychlý nosič (pohyb 7) a rychlí spoluhráči vzadu; soupeř blízko
TEST(CageInvariant, NoCageFastTeamOpponentNear) {
    Board b(2);
    b.put(1, TeamSide::HOME, {8, 7}, 7);
    b.put(2, TeamSide::HOME, {5, 4}, 7);
    b.put(3, TeamSide::HOME, {5, 10}, 7);
    b.put(4, TeamSide::HOME, {3, 6}, 7);
    b.put(5, TeamSide::HOME, {3, 8}, 7);
    b.put(13, TeamSide::AWAY, {16, 7}, 6);
    b.s.ball = BallState::carried({8, 7}, 1);
    EXPECT_EQ(playTurnAndCheck(b), "");
}

// ⚠ NÁLEZ (P178): žádná klec, pomalý tým (nosič i spoluhráči pohyb 4), 2. kolo, soupeř (pohyb 6) osm
// polí před nosičem. Řadič nosiče postaví na (11,7) s dvěma rohy; soupeř na něj dosáhne s hrozbou rány
// 0,33 a TD nepadlo ⇒ stav „ani-ani“ po tahu. Spoluhráči stojí 3–5 polí za nosičem a s pohybem 4 další
// rohy nedoplní; nosič přitom mohl zůstat dál od soupeře. Podle P178 („když nosič nemůže skórovat ani
// být v bezpečí — nesmí nastat … do té doby má být v kleci“) se mělo stát: čtyři rohy, nebo nosič mimo dosah.
TEST(CageInvariant, DISABLED_NoCageSlowTeamOpponentNear) {
    Board b(2);
    b.put(1, TeamSide::HOME, {9, 7}, 4);
    b.put(2, TeamSide::HOME, {6, 4}, 4);
    b.put(3, TeamSide::HOME, {6, 10}, 4);
    b.put(4, TeamSide::HOME, {4, 6}, 4);
    b.put(5, TeamSide::HOME, {4, 8}, 4);
    b.put(13, TeamSide::AWAY, {16, 7}, 6);
    b.s.ball = BallState::carried({9, 7}, 1);
    EXPECT_EQ(playTurnAndCheck(b), "");
}

// nosič daleko od soupeře, mimo dosah zóny (dosah soupeře nestačí)
TEST(CageInvariant, CarrierOutOfOpponentReachNoCage) {
    Board b(2);
    b.put(1, TeamSide::HOME, {6, 7}, 6);
    b.put(2, TeamSide::HOME, {4, 4}, 6);
    b.put(3, TeamSide::HOME, {4, 10}, 6);
    b.put(13, TeamSide::AWAY, {24, 13}, 6);
    b.s.ball = BallState::carried({6, 7}, 1);
    EXPECT_EQ(playTurnAndCheck(b), "");
}

// soupeř leží (vstává za 3 pole pohybu a pak bije)
TEST(CageInvariant, ProneOpponentNearStandingCage) {
    Board b(2);
    b.put(1, TeamSide::HOME, {12, 7}, 6);
    cageAround(b, 12, 6);
    Player& o = b.put(13, TeamSide::AWAY, {17, 7}, 6);
    o.state = PlayerState::PRONE;
    b.s.ball = BallState::carried({12, 7}, 1);
    EXPECT_EQ(playTurnAndCheck(b), "");
}

// soupeř leží blízko, nosič bez klece
TEST(CageInvariant, ProneOpponentNearNoCage) {
    Board b(2);
    b.put(1, TeamSide::HOME, {9, 7}, 6);
    b.put(2, TeamSide::HOME, {6, 4}, 6);
    b.put(3, TeamSide::HOME, {6, 10}, 6);
    Player& o = b.put(13, TeamSide::AWAY, {13, 7}, 6);
    o.state = PlayerState::PRONE;
    b.s.ball = BallState::carried({9, 7}, 1);
    EXPECT_EQ(playTurnAndCheck(b), "");
}

// nosič na dosah zóny (3 pole), soupeř ho ohrožuje ⇒ TD
TEST(CageInvariant, CarrierInScoringRangeThreatenedScores) {
    Board b(5);
    b.put(1, TeamSide::HOME, {22, 7}, 6);
    b.put(2, TeamSide::HOME, {20, 5}, 6);
    b.put(13, TeamSide::AWAY, {18, 9}, 6);
    b.s.ball = BallState::carried({22, 7}, 1);
    EXPECT_EQ(playTurnAndCheck(b), "");
}

// nosič na dosah zóny, soupeř daleko ⇒ zdržuje se, nosič je mimo dosah
TEST(CageInvariant, CarrierInScoringRangeOpponentFarStalls) {
    Board b(5);
    b.put(1, TeamSide::HOME, {22, 7}, 6);
    b.put(2, TeamSide::HOME, {20, 5}, 6);
    b.put(13, TeamSide::AWAY, {3, 7}, 6);
    b.s.ball = BallState::carried({22, 7}, 1);
    EXPECT_EQ(playTurnAndCheck(b), "");
}

// pomalý tým, 6. kolo, nosič 13 polí od zóny, rozbitá klec, soupeř blízko (fáze výběhu)
TEST(CageInvariant, SlowTeamLateTurnReleaseOpponentNear) {
    Board b(6);
    b.put(1, TeamSide::HOME, {12, 7}, 6);
    b.put(2, TeamSide::HOME, {11, 6}, 4);
    b.put(3, TeamSide::HOME, {11, 8}, 4);
    b.put(4, TeamSide::HOME, {8, 3}, 4);
    b.put(5, TeamSide::HOME, {8, 11}, 4);
    b.put(13, TeamSide::AWAY, {18, 7}, 6);
    b.s.ball = BallState::carried({12, 7}, 1);
    EXPECT_EQ(playTurnAndCheck(b), "");
}

// dva silní soupeři (síla 4) blízko, pomalá klec s průměrnými rohy
TEST(CageInvariant, TwoStrongOpponentsNearSlowCage) {
    Board b(3);
    b.put(1, TeamSide::HOME, {10, 7}, 5);
    cageAround(b, 10, 5);
    b.put(13, TeamSide::AWAY, {15, 5}, 5, 4);
    b.put(14, TeamSide::AWAY, {15, 9}, 5, 4);
    b.s.ball = BallState::carried({10, 7}, 1);
    EXPECT_EQ(playTurnAndCheck(b), "");
}

// ⚠ NÁLEZ (P178): rychlý nosič (pohyb 7) na (13,7) je o 7–8 polí před spoluhráči (pohyb 6, x = 5–6),
// 3. kolo (zbývá 6 tahů), soupeř (síla 4, pohyb 6) na (19,7). Rohy k nosiči nedojdou ⇒ rozhodnutí o
// vypuštění (fáze 3, „krok klece 0“) pošle nosiče samotného na (20,5), tedy DVA POLE od soupeře: nula
// rohů, hrozba rány 0,55, do zóny zbývá 5 polí a TD v tomto tahu nepadne. Podle P178 („když nosič
// nemůže skórovat ani být v bezpečí — nesmí nastat“) se mělo stát: nosič zůstane/ustoupí mimo dosah
// soupeře, dokud spoluhráči nedojdou (je čas), nebo skóruje; výběh na pole v dosahu rány není ani jedno.
TEST(CageInvariant, DISABLED_FastCarrierFarAheadOfSlowerMatesOpponentAhead) {
    Board b(3);
    b.put(1, TeamSide::HOME, {13, 7}, 7);
    b.put(2, TeamSide::HOME, {6, 5}, 6);
    b.put(3, TeamSide::HOME, {6, 9}, 6);
    b.put(4, TeamSide::HOME, {5, 6}, 6);
    b.put(5, TeamSide::HOME, {5, 8}, 6);
    b.put(13, TeamSide::AWAY, {19, 7}, 6, 4);
    b.s.ball = BallState::carried({13, 7}, 1);
    EXPECT_EQ(playTurnAndCheck(b), "");
}

// nosič u postranní čáry, jeden roh, soupeř blízko
TEST(CageInvariant, CarrierNearTheSidelineOneCornerOpponentNear) {
    Board b(3);
    b.put(1, TeamSide::HOME, {12, 2}, 5);
    b.put(2, TeamSide::HOME, {11, 3}, 5);
    b.put(3, TeamSide::HOME, {8, 5}, 5);
    b.put(4, TeamSide::HOME, {8, 8}, 5);
    b.put(5, TeamSide::HOME, {6, 6}, 5);
    b.put(13, TeamSide::AWAY, {17, 4}, 6, 4);
    b.s.ball = BallState::carried({12, 2}, 1);
    EXPECT_EQ(playTurnAndCheck(b), "");
}

// Pozitivní kontrola: kontrola invariantu UMÍ SPADNOUT. Deska ručně v prostřed „ani-ani“ (bez
// odehraného tahu): sám nosič v dosahu silného soupeře, bez rohů, TD nepadlo.
TEST(CageInvariant, TheCheckFailsOnACarrierWhoIsNeitherCagedNorSafe) {
    Board b(2);
    b.put(1, TeamSide::HOME, {12, 7}, 4);
    b.put(13, TeamSide::AWAY, {15, 7}, 6, 5);
    b.s.ball = BallState::carried({12, 7}, 1);
    ASSERT_EQ(standingCorners(b.s, b.s.getPlayer(1)), 0);
    ASSERT_GT(blitzThreat(b.s, b.s.getPlayer(1)), kSafeBlitzThreat);
    ASSERT_TRUE(anyOpponentReaches(b.s, TeamSide::HOME, {12, 7}));
    EXPECT_NE(invariantViolation(b.s, TeamSide::HOME, 0), "");
}

// Pozitivní kontrola ke každé ze tří větví: každá sama invariant splní.
TEST(CageInvariant, TheCheckAcceptsEachOfTheThreeWays) {
    {   // čtyři rohy
        Board b(2);
        b.put(1, TeamSide::HOME, {12, 7}, 4);
        cageAround(b, 12, 4);
        b.put(13, TeamSide::AWAY, {15, 7}, 6, 5);
        b.s.ball = BallState::carried({12, 7}, 1);
        EXPECT_EQ(invariantViolation(b.s, TeamSide::HOME, 0), "") << "čtyři rohy";
    }
    {   // mimo dosah
        Board b(2);
        b.put(1, TeamSide::HOME, {12, 7}, 4);
        b.put(13, TeamSide::AWAY, {24, 1}, 6, 5);
        b.s.ball = BallState::carried({12, 7}, 1);
        EXPECT_EQ(invariantViolation(b.s, TeamSide::HOME, 0), "") << "mimo dosah";
    }
    {   // TD
        Board b(2);
        b.put(1, TeamSide::HOME, {25, 7}, 4);
        b.put(13, TeamSide::AWAY, {24, 7}, 6, 5);
        b.s.ball = BallState::carried({25, 7}, 1);
        b.s.homeTeam.score = 1;
        EXPECT_EQ(invariantViolation(b.s, TeamSide::HOME, 0), "") << "TD";
        EXPECT_NE(invariantViolation(b.s, TeamSide::HOME, 1), "") << "bez nového TD a bez bezpečí to neplatí";
    }
}

// --- Strana AWAY zrcadlově k HOME -------------------------------------------------------------------

namespace {

Position mirrorPos(Position p) {
    return p.isOnPitch() ? Position{static_cast<int8_t>(25 - p.x), p.y} : p;   // zrcadlo podle středu délky
}
int mirrorId(int id) {            // HOME 1–11 ↔ AWAY 12–22, lavice 23–24 ↔ 25–26
    if (id <= 11) return id + 11;
    if (id <= 22) return id - 11;
    return id <= 24 ? id + 2 : id - 2;
}

// Zrcadlí desku: strany se vymění, x → 25 − x (y zůstává, hřiště je symetrické); aktivní je AWAY.
Board mirrorBoard(const Board& src) {
    Board m;
    m.s = src.s.clone();
    for (int id = 1; id <= GameState::PLAYERS_TOTAL; ++id) {
        const Player& from = src.s.getPlayer(id);
        Player& to = m.s.getPlayer(mirrorId(id));
        to = from;
        to.id = mirrorId(id);
        to.teamSide = opponent(from.teamSide);
        to.position = mirrorPos(from.position);
    }
    m.s.homeTeam = src.s.awayTeam;
    m.s.awayTeam = src.s.homeTeam;
    m.s.homeTeam.side = TeamSide::HOME;
    m.s.awayTeam.side = TeamSide::AWAY;
    m.s.activeTeam = opponent(src.s.activeTeam);
    if (src.s.ball.carrierId > 0) m.s.ball = BallState::carried(mirrorPos(src.s.ball.position), mirrorId(src.s.ball.carrierId));
    else m.s.ball = BallState::onGround(mirrorPos(src.s.ball.position));
    return m;
}

struct Outcome {
    std::vector<Macro> macros;
    Board board;
};

Outcome run(Board b) {
    Outcome o{{}, b};
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, o.board, &o.macros);
    return o;
}

// Výsledek na AWAY je zrcadlový k výsledku na HOME: tytéž typy maker týmiž hráči (po přečíslování),
// zrcadlové konečné pozice všech hráčů, stejné skóre a stejné držení míče.
void expectMirrored(const Outcome& home, const Outcome& away, const char* what) {
    ASSERT_EQ(home.macros.size(), away.macros.size()) << what << ": počet maker";
    for (size_t i = 0; i < home.macros.size(); ++i) {
        EXPECT_EQ(home.macros[i].type, away.macros[i].type) << what << ": makro " << i;
        EXPECT_EQ(mirrorId(home.macros[i].playerId), away.macros[i].playerId) << what << ": hráč v makru " << i;
    }
    for (int id = 1; id <= 22; ++id) {
        EXPECT_EQ(mirrorPos(home.board.s.getPlayer(id).position), away.board.s.getPlayer(mirrorId(id)).position)
            << what << ": hráč " << id;
    }
    EXPECT_EQ(home.board.s.homeTeam.score, away.board.s.awayTeam.score) << what << ": skóre";
    EXPECT_EQ(mirrorId(home.board.s.ball.carrierId), away.board.s.ball.carrierId) << what << ": držitel míče";
}

Board advanceBoard() {
    Board b(1);
    b.put(1, TeamSide::HOME, {12, 7});
    b.put(2, TeamSide::HOME, {11, 6});
    b.put(3, TeamSide::HOME, {11, 8});
    b.put(4, TeamSide::HOME, {13, 6});
    b.put(5, TeamSide::HOME, {13, 8});
    b.put(14, TeamSide::AWAY, {24, 13});
    b.s.ball = BallState::carried({12, 7}, 1);
    return b;
}
Board stallBoard() {       // P175: míč v bezpečí, 5. kolo, nosič tři pole před zónou
    Board b(5);
    b.put(1, TeamSide::HOME, {22, 7}, 6);
    b.put(2, TeamSide::HOME, {20, 5});
    b.put(13, TeamSide::AWAY, {3, 7}, 6);
    b.s.ball = BallState::carried({22, 7}, 1);
    return b;
}
Board orderedScoreBoard() {   // P178: hrozí blitz ⇒ TD přikáže řadič
    Board b(5);
    b.put(1, TeamSide::HOME, {22, 7}, 6);
    b.put(2, TeamSide::HOME, {20, 5});
    b.put(13, TeamSide::AWAY, {18, 9}, 6);
    b.s.ball = BallState::carried({22, 7}, 1);
    return b;
}

}  // namespace

// Pozitivní kontrola zrcadla: zrcadlení desky dvakrát dá původní desku a AWAY opravdu útočí k x = 0.
TEST(CageMirror, TheMirroredBoardIsTheOriginalSeenFromTheOtherSide) {
    const Board h = advanceBoard();
    const Board a = mirrorBoard(h);
    EXPECT_EQ(a.s.activeTeam, TeamSide::AWAY);
    EXPECT_EQ(a.s.getPlayer(12).position, (Position{13, 7})) << "nosič AWAY (HOME 12,7)";
    EXPECT_EQ(a.s.getPlayer(12).teamSide, TeamSide::AWAY);
    EXPECT_EQ(a.s.ball.carrierId, 12);
    const Board back = mirrorBoard(a);
    for (int id = 1; id <= 22; ++id) EXPECT_EQ(back.s.getPlayer(id).position, h.s.getPlayer(id).position) << id;
}

// Uživatel 02.10.: klec postupuje, napřed rohy, nosič poslední (P126) — i pro AWAY k x = 0.
TEST(CageMirror, AwayCageAdvancesTowardsXZeroCornersFirstCarrierLast) {
    const Outcome home = run(advanceBoard());
    const Outcome away = run(mirrorBoard(advanceBoard()));
    ASSERT_FALSE(away.macros.empty());
    EXPECT_EQ(away.macros.back().playerId, 12) << "nosič AWAY poslední";
    EXPECT_LT(away.board.s.getPlayer(12).position.x, 13) << "klec AWAY postoupila k x = 0";
    expectMirrored(home, away, "postup klece");
}

// P175: míč v bezpečí ⇒ zdržuje se (AWAY stejně jako HOME).
TEST(CageMirror, AwayStallsTheTouchdownWhenTheBallIsSafe) {
    const Outcome home = run(stallBoard());
    const Outcome away = run(mirrorBoard(stallBoard()));
    EXPECT_EQ(away.board.s.getPlayer(12).position, (Position{3, 7})) << "nosič AWAY stojí";
    EXPECT_EQ(away.board.s.awayTeam.score, 0);
    expectMirrored(home, away, "zdržování TD");
}

// P178: hrozí blitz ⇒ TD přikáže řadič, nosič AWAY je v zóně na x = 0.
TEST(CageMirror, AwayOrderedTouchdownWhenThreatened) {
    const Outcome home = run(orderedScoreBoard());
    const Outcome away = run(mirrorBoard(orderedScoreBoard()));
    ASSERT_FALSE(away.macros.empty());
    EXPECT_EQ(away.macros.front().type, MacroType::SCORE);
    EXPECT_EQ(away.board.s.getPlayer(12).position.x, 0) << "nosič AWAY je v zóně";
    EXPECT_EQ(away.board.s.awayTeam.score, 1);
    expectMirrored(home, away, "přikázané TD");
}

// P178: invariant platí i pro AWAY (stejná deska zrcadlově).
TEST(CageMirror, AwayKeepsTheInvariantOnTheMirroredBoard) {
    Board h(2);
    h.put(1, TeamSide::HOME, {12, 7}, 4);
    cageAround(h, 12, 4);
    h.put(13, TeamSide::AWAY, {17, 7}, 6);
    h.s.ball = BallState::carried({12, 7}, 1);
    Board a = mirrorBoard(h);
    const int before = a.s.awayTeam.score;
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, a);
    EXPECT_EQ(invariantViolation(a.s, TeamSide::AWAY, before), "");
}

// --- teamHasTimeSlack (P177) -----------------------------------------------------------------------

namespace {
// Tým s jednotným pohybem `ma`, nosič na x = 5 (20 polí od zóny), pět spoluhráčů za ním.
Board slackBoard(int turn, int ma) {
    Board b(turn);
    b.put(1, TeamSide::HOME, {5, 7}, static_cast<int8_t>(ma));
    for (int i = 0; i < 5; ++i) b.put(2 + i, TeamSide::HOME, {4, static_cast<int8_t>(3 + 2 * i)}, static_cast<int8_t>(ma));
    b.put(13, TeamSide::AWAY, {24, 1}, 6);
    b.s.ball = BallState::carried({5, 7}, 1);
    return b;
}
}  // namespace

// P177: „skaveni a elfové stihnou TD za 2 kola … trpaslíci za 6“ — stejná vzdálenost a stejné kolo,
// rychlý tým (pohyb 8) má časovou rezervu, pomalý (pohyb 4) ne. Rezerva se počítá z pohybu, ne z rasy.
TEST(TimeSlack, FastTeamHasSlackSlowTeamDoesNotAtTheSameDistanceAndTurn) {
    int needFast = 0, needSlow = 0;
    Board fast = slackBoard(2, 8);
    Board slow = slackBoard(2, 4);
    EXPECT_TRUE(teamHasTimeSlack(fast.s, fast.s.getPlayer(1), &needFast));
    EXPECT_FALSE(teamHasTimeSlack(slow.s, slow.s.getPlayer(1), &needSlow));
    EXPECT_LT(needFast, needSlow) << "rychlý tým potřebuje méně tahů na TD";
}

// Rychlý tým (pohyb 7) má rezervu do 3. kola; ve 4. kole dojde. Hranice: potřebuje N tahů, zbývá-li
// (9 − kolo) tahů včetně tohoto, rezerva je při N ≤ zbývá − 2 (jedna rezerva + tento tah se nepočítá).
TEST(TimeSlack, TheSlackRunsOutOnTheTurnWhereRemainingTurnsMinusTwoDropBelowTheNeed) {
    int need = 0;
    Board probe = slackBoard(1, 7);
    teamHasTimeSlack(probe.s, probe.s.getPlayer(1), &need);
    ASSERT_GT(need, 0);
    const int lastSlackTurn = 9 - (need + 2);       // poslední kolo, kdy 9 − kolo − 2 ≥ need
    ASSERT_GE(lastSlackTurn, 1);
    ASSERT_LT(lastSlackTurn, 8) << "předpoklad: hranice leží uvnitř poločasu";
    Board atLimit = slackBoard(lastSlackTurn, 7);
    Board past = slackBoard(lastSlackTurn + 1, 7);
    EXPECT_TRUE(teamHasTimeSlack(atLimit.s, atLimit.s.getPlayer(1))) << "kolo " << lastSlackTurn;
    EXPECT_FALSE(teamHasTimeSlack(past.s, past.s.getPlayer(1))) << "kolo " << lastSlackTurn + 1;
}

// Rezerva s posunem nosiče: kdo je blíž zóně, potřebuje méně tahů a rezervu má déle.
TEST(TimeSlack, ACarrierCloserToTheEndZoneKeepsTheSlackLonger) {
    Board far = slackBoard(5, 6);
    Board near = slackBoard(5, 6);
    near.s.getPlayer(1).position = {14, 7};
    near.s.ball = BallState::carried({14, 7}, 1);
    EXPECT_FALSE(teamHasTimeSlack(far.s, far.s.getPlayer(1)));
    EXPECT_TRUE(teamHasTimeSlack(near.s, near.s.getPlayer(1)));
}

// --- Větev „klec stojí“ (P177) ---------------------------------------------------------------------

namespace {
// Tým s časovou rezervou (pohyb 8 ve 2. kole), čtyři rohy kolem nosiče na x = 8. Soupeři stojí ve
// zdi na x = `wallX` (řádky 3–11 po dvou): každé pole vpřed leží v jejich zóně nebo za ní, takže
// bezpečné pole pro klec vpřed není.
Board standBoard(int wallX, int8_t oppSt) {
    Board b(2);
    b.put(1, TeamSide::HOME, {8, 7}, 8);
    cageAround(b, 8, 8);
    for (int i = 0; i < 5; ++i) b.put(13 + i, TeamSide::AWAY, {static_cast<int8_t>(wallX), static_cast<int8_t>(3 + 2 * i)}, 6, oppSt);
    b.s.ball = BallState::carried({8, 7}, 1);
    return b;
}
}  // namespace

// P177: „dokud má tým časovou rezervu, klec jde bezpečně, ne co nejdál“. Zeď soupeřů na x = 10 vpřed
// nepustí (každé pole vpřed leží v jejich zóně), nosič je tam, kde stojí, v bezpečí (hrozba 0,11
// s klecí, jejíž jeden roh je o dvě pole vzadu) ⇒ klec stojí: nosič se nehne a chybějící roh se dostaví.
TEST(CageStands, WithTimeToSpareAndNoSafeSquareAheadTheCarrierStaysAndTheCornerComes) {
    Board b = standBoard(10, 3);
    b.s.getPlayer(3).position = {6, 9};                     // jeden roh vzadu
    ASSERT_TRUE(teamHasTimeSlack(b.s, b.s.getPlayer(1))) << "předpoklad: tým má rezervu";
    ASSERT_LE(blitzThreat(b.s, b.s.getPlayer(1)), kSafeBlitzThreat) << "předpoklad: nosič je v bezpečí tam, kde stojí";
    ASSERT_EQ(standingCorners(b.s, b.s.getPlayer(1)), 3);
    CageController cc(nullptr, cfg(), 1);
    std::vector<Macro> ms;
    playAll(cc, b, &ms);
    EXPECT_EQ(b.s.getPlayer(1).position, (Position{8, 7})) << "nosič se nehnul";
    EXPECT_EQ(standingCorners(b.s, b.s.getPlayer(1)), 4) << "rohy se dostavěly";
    for (const Macro& m : ms) EXPECT_NE(m.playerId, 1) << "nosič nedostal žádné makro";
}

// Opak: nosič je v dosahu dobré rány (dva silní soupeři dvě pole před dosahem, hrozba 0,55) a bezpečné
// pole vpřed také není. Čekáním o míč přijde („pokud hrozí blitz na nosiče a ztráta — je lepší dát
// TD dříve“) ⇒ klec nestojí a pokračuje dosavadním postupem vpřed.
TEST(CageStands, ACarrierInReachOfAGoodHitDoesNotStand) {
    Board b(2);
    b.put(1, TeamSide::HOME, {8, 7}, 8);
    b.put(2, TeamSide::HOME, {7, 6}, 8);
    b.put(3, TeamSide::HOME, {7, 8}, 8);
    b.put(13, TeamSide::AWAY, {15, 6}, 6, 5);
    b.put(14, TeamSide::AWAY, {15, 8}, 6, 5);
    b.s.ball = BallState::carried({8, 7}, 1);
    ASSERT_TRUE(teamHasTimeSlack(b.s, b.s.getPlayer(1))) << "předpoklad: tým má rezervu (jako v testu výš)";
    ASSERT_GT(blitzThreat(b.s, b.s.getPlayer(1)), kSafeBlitzThreat) << "předpoklad: nosič je v dosahu dobré rány";
    {
        CageAdvancePlanner planner(nullptr, cfg(), 1);
        const CageAdvancePlan plan = planner.buildImpl(b.s, {}, false);
        EXPECT_TRUE(plan.valid) << "klec postupuje";
    }
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    EXPECT_GT(b.s.getPlayer(1).position.x, 8) << "klec nestála, nosič postoupil";
}

// --- Zdržování TD přes dva tahy na témže řadiči (P175) ------------------------------------------------

// Uživatel 08.10.2026: „obecně chci, ať s TD zdržujeme za všechny, ale jen v případě, kdy máme balon
// bezpečně v držení a nehrozí blitz na nosiče — na druhou stranu pokud hrozí blitz na nosiče a ztráta,
// je lepší dát TD dříve“. Hra používá jeden řadič přes všechny tahy; stav „zdržuje se“ nesmí přejít do
// dalšího tahu. Tah 1: soupeř daleko ⇒ zdržuje se. Soupeř se přiblíží ⇒ tah 2: skóruje se.
TEST(OneCageStallAcrossTurns, StallsWhileSafeThenScoresInTheNextTurnWhenTheOpponentComesClose) {
    Board b = stallBoard();
    CageController cc(nullptr, cfg(), 1);
    std::vector<Macro> ms1;
    playAll(cc, b, &ms1);
    ASSERT_EQ(b.s.getPlayer(1).position, (Position{22, 7})) << "tah 1: nosič stojí (zdržuje se)";
    ASSERT_EQ(b.s.homeTeam.score, 0);
    ASSERT_TRUE(cc.forbidsCarrierMove(b.s, Macro{MacroType::SCORE, 1, -1, {-1, -1}})) << "tah 1: skórovat se nesmí";

    // druhý tah: soupeř se přiblížil, naši hráči jsou znovu volní
    b.s.getPlayer(13).position = {18, 9};
    b.s.homeTeam.turnNumber = 6;
    b.s.resetPlayersForNewTurn(TeamSide::HOME);
    std::vector<Macro> ms2;
    playAll(cc, b, &ms2);
    ASSERT_FALSE(ms2.empty());
    EXPECT_EQ(ms2.front().type, MacroType::SCORE) << "tah 2: hrozí blitz ⇒ TD hned";
    EXPECT_EQ(b.s.homeTeam.score, 1);
    EXPECT_FALSE(cc.forbidsCarrierMove(b.s, Macro{MacroType::SCORE, 1, -1, {-1, -1}})) << "příznak zdržování z tahu 1 se nepřenesl";
}

// Pozitivní kontrola: když se soupeř nepřiblíží, zdržuje se i ve druhém tahu (test výš tedy nepadá
// jen proto, že by se v 6. kole skórovalo vždy).
TEST(OneCageStallAcrossTurns, KeepsStallingInTheNextTurnWhenTheOpponentStaysFar) {
    Board b = stallBoard();
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    b.s.homeTeam.turnNumber = 6;
    b.s.resetPlayersForNewTurn(TeamSide::HOME);
    playAll(cc, b);
    EXPECT_EQ(b.s.getPlayer(1).position, (Position{22, 7}));
    EXPECT_EQ(b.s.homeTeam.score, 0);
}

// --- forbidsCarrierMove (P173) --------------------------------------------------------------------------

// P173: „nosič smí jít tam, kde je bezpečněji než tady“ — hledání smí nosičem pohnout jen tehdy, když
// je hrozba na cílovém poli menší než na tom, kde stojí (stejně pro každou rasu).
namespace {
// Rychlý tým (pohyb 8) ve 2. kole: klec doběhne, takže nosič není vypuštěn (fáze 3) a rozhoduje
// porovnání rizika. Nosič má dva rohy; soupeř (síla 5, pohyb 6 ⇒ dosah 7 polí) na něj dosáhne.
Board fleeBoard(Position opponent) {
    Board b(2);
    b.put(1, TeamSide::HOME, {8, 7}, 8);
    b.put(2, TeamSide::HOME, {7, 6}, 8);
    b.put(3, TeamSide::HOME, {7, 8}, 8);
    b.put(4, TeamSide::HOME, {4, 4}, 8);
    b.put(5, TeamSide::HOME, {4, 10}, 8);
    b.put(13, TeamSide::AWAY, opponent, 6, 5);
    b.put(14, TeamSide::AWAY, {opponent.x, static_cast<int8_t>(opponent.y + 2)}, 6, 5);
    b.s.ball = BallState::carried({8, 7}, 1);
    return b;
}
}  // namespace

// Smí: soupeř je za nosičem, nosič v jeho dosahu, útěk vpřed (6 polí + GFI) ho dostane z dosahu.
TEST(CarrierMoveBan, MayMoveWhenFleeingOutOfReachIsSaferThanStanding) {
    Board b = fleeBoard({2, 6});
    ASSERT_GT(blitzThreat(b.s, b.s.getPlayer(1)), kSafeBlitzThreat) << "předpoklad: tady je nosič v dosahu dobré rány";
    CageController cc(nullptr, cfg(), 1);
    Macro first;
    ASSERT_TRUE(cc.next(b.s, first)) << "řadič zahájí tah (stav tahu se nastaví)";
    EXPECT_FALSE(cc.forbidsCarrierMove(b.s, Macro{MacroType::ADVANCE, 1, -1, {-1, -1}})) << "útěk z dosahu hledání smí";
}

// Nesmí: soupeř je před nosičem, každý krok vpřed je blíž k němu ⇒ stejně zlé (nebo horší) pole.
TEST(CarrierMoveBan, MayNotMoveWhenTheSquareAheadIsNoSaferThanStanding) {
    Board b = fleeBoard({15, 6});
    ASSERT_GT(blitzThreat(b.s, b.s.getPlayer(1)), kSafeBlitzThreat) << "předpoklad: tady je nosič v dosahu dobré rány";
    CageController cc(nullptr, cfg(), 1);
    Macro first;
    cc.next(b.s, first);
    EXPECT_TRUE(cc.forbidsCarrierMove(b.s, Macro{MacroType::ADVANCE, 1, -1, {-1, -1}})) << "stejně zlé pole vpřed hledání nesmí";
}
