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
#include <random>

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
// N3 (uživatel 23.11.2026: „dřívější premisa byla založena na tom, že už nejde dát TD — ta zůstává … vpřed s rizikem
//   se jde pouze, pokud je šance dát TD“): dokud TD v poločase stihnout jde (tdStillReachable před tahem), invariant
//   „ani-ani“ neplatí — místo něj nosič NESMÍ ZŮSTAT STÁT V RÁNĚ: padlo TD, nebo šel vpřed, nebo je po tahu v bezpečí.
std::string playTurnAndCheck(Board b, std::vector<Macro>* ms = nullptr) {
    const int before = b.s.homeTeam.score;
    const bool reachable = b.s.ball.isHeld && tdStillReachable(b.s, b.s.getPlayer(b.s.ball.carrierId));
    const int x0 = b.s.ball.isHeld ? b.s.getPlayer(b.s.ball.carrierId).position.x : -1;
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b, ms);
    const std::string v = invariantViolation(b.s, TeamSide::HOME, before);
    if (v.empty() || !reachable || !b.s.ball.isHeld) return v;
    if (b.s.getPlayer(b.s.ball.carrierId).position.x > x0) return "";   // vpřed s rizikem, TD je stihnutelné
    return "stojí v ráně, ač TD stihnout jde: " + v;
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

// NÁLEZ z 08.10.2026, OPRAVENO týž den (P187: klec se staví i vzadu). Původní popis:
// ⚠ NÁLEZ (P178): žádná klec, pomalý tým (nosič i spoluhráči pohyb 4), 2. kolo, soupeř (pohyb 6) osm
// polí před nosičem. Řadič nosiče postaví na (11,7) s dvěma rohy; soupeř na něj dosáhne s hrozbou rány
// 0,33 a TD nepadlo ⇒ stav „ani-ani“ po tahu. Spoluhráči stojí 3–5 polí za nosičem a s pohybem 4 další
// rohy nedoplní; nosič přitom mohl zůstat dál od soupeře. Podle P178 („když nosič nemůže skórovat ani
// být v bezpečí — nesmí nastat … do té doby má být v kleci“) se mělo stát: čtyři rohy, nebo nosič mimo dosah.
TEST(CageInvariant, NoCageSlowTeamOpponentNear) {
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

// NÁLEZ z 08.10.2026, OPRAVENO týž den (P187: s časovou rezervou se nosič nevypouští). Původní popis:
// ⚠ NÁLEZ (P178): rychlý nosič (pohyb 7) na (13,7) je o 7–8 polí před spoluhráči (pohyb 6, x = 5–6),
// 3. kolo (zbývá 6 tahů), soupeř (síla 4, pohyb 6) na (19,7). Rohy k nosiči nedojdou ⇒ rozhodnutí o
// vypuštění (fáze 3, „krok klece 0“) pošle nosiče samotného na (20,5), tedy DVA POLE od soupeře: nula
// rohů, hrozba rány 0,55, do zóny zbývá 5 polí a TD v tomto tahu nepadne. Podle P178 („když nosič
// nemůže skórovat ani být v bezpečí — nesmí nastat“) se mělo stát: nosič zůstane/ustoupí mimo dosah
// soupeře, dokud spoluhráči nedojdou (je čas), nebo skóruje; výběh na pole v dosahu rány není ani jedno.
TEST(CageInvariant, FastCarrierFarAheadOfSlowerMatesOpponentAhead) {
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

// N3 (uživatel 23.11.2026): když TD v poločase stihnout nejde, platí dosavadní ochrana (P178/P187) — riziko vpřed se
// nepodstupuje. Táž deska jako NoCageSlowTeamOpponentNear, ale v 7. kole: 2 tahy na 16 polí s pohybem 4 nestačí.
TEST(CageInvariant, WithNoTimeLeftForATouchdownTheCarrierIsCagedOrSafe) {
    Board b(7);
    b.put(1, TeamSide::HOME, {9, 7}, 4);
    b.put(2, TeamSide::HOME, {6, 4}, 4);
    b.put(3, TeamSide::HOME, {6, 10}, 4);
    b.put(4, TeamSide::HOME, {4, 6}, 4);
    b.put(5, TeamSide::HOME, {4, 8}, 4);
    b.put(13, TeamSide::AWAY, {16, 7}, 6);
    b.s.ball = BallState::carried({9, 7}, 1);
    ASSERT_FALSE(tdStillReachable(b.s, b.s.getPlayer(1))) << "předpoklad: TD se nestihne";
    EXPECT_EQ(playTurnAndCheck(b), "");
}

// N3: s celými 8 koly je TD stihnutelné vždy (uživatel: „nesmí nastat, že bychom řekli, že už nestihneme“).
TEST(CageInvariant, WithAWholeHalfLeftATouchdownIsAlwaysReachable) {
    Board b(1);
    b.put(1, TeamSide::HOME, {1, 7}, 4);
    b.s.ball = BallState::carried({1, 7}, 1);
    EXPECT_TRUE(tdStillReachable(b.s, b.s.getPlayer(1)));
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

// N3 (uživatel 23.11.2026: „rezerva nesmí dovolit stání“; „zůstat stát je vždy horší než podstoupit riziko a jít
// kupředu“). Dřív (P177) tu klec s časovou rezervou stála, když bezpečné pole vpřed nebylo. Zeď soupeřů na x = 11
// bezpečný krok vpřed nepustí, nosič je tam, kde stojí, v bezpečí, tým má rezervu ⇒ klec přesto jde vpřed.
TEST(CageStands, WithTimeToSpareAndNoSafeSquareAheadTheCageStillStepsForward) {
    Board b = standBoard(11, 3);
    b.s.getPlayer(3).position = {6, 9};                     // jeden roh vzadu
    ASSERT_TRUE(teamHasTimeSlack(b.s, b.s.getPlayer(1))) << "předpoklad: tým má rezervu";
    ASSERT_LE(blitzThreat(b.s, b.s.getPlayer(1)), kSafeBlitzThreat) << "předpoklad: nosič je v bezpečí tam, kde stojí";
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    EXPECT_GT(b.s.getPlayer(1).position.x, 8) << "klec nestojí, jde vpřed i s rizikem";
    EXPECT_GT(blitzThreat(b.s, b.s.getPlayer(1)), kSafeBlitzThreat) << "předpoklad: bezpečně vpřed nešlo (krok s rizikem)";
}

// P228 (review 27.10.2026, nález 6): táž klec u zdi na x = 10 — přední rohy (9,6)/(9,8) stojí v zóně
// soupeřů, takže je soupeř může napřed srazit a pak udeřit na nosiče (hrozba 0,29). Čtyři rohy tu
// bezpečí neznamenají; vzadu stojí dva volní hráči, klec se dá postavit bezpečně o pole vzadu ⇒ po tahu
// je nosič v bezpečí. (Bez volných hráčů klec stojí: přední rohy se bez úhybu nepohnou a P217 dává
// stání nejvyšší šanci 0,71 — to je nejmenší zlo, ne vada.) Uživatel 27.10.: „primární je, aby klec
// byla bezpečná — zjednodušení je, aby klec byla čistá“.
TEST(CageStands, FourDirtyCornersInReachOfAGoodHitAreNotASafeCage) {
    Board b = standBoard(10, 3);
    b.put(6, TeamSide::HOME, {4, 6}, 8);                          // volní hráči vzadu: klec se dá postavit jinde
    b.put(7, TeamSide::HOME, {4, 8}, 8);
    ASSERT_EQ(standingCorners(b.s, b.s.getPlayer(1)), 4);
    ASSERT_GT(blitzThreat(b.s, b.s.getPlayer(1)), kSafeBlitzThreat) << "předpoklad: špinavé rohy, nosič v dosahu rány";
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    EXPECT_LE(blitzThreat(b.s, b.s.getPlayer(1)), kSafeBlitzThreat) << "po tahu je nosič v bezpečí";
}

// Opak: nosič je v dosahu dobré rány (dva silní soupeři dvě pole před dosahem, hrozba 0,55) a bezpečné
// pole vpřed také není. Čekáním o míč přijde („pokud hrozí blitz na nosiče a ztráta — je lepší dát
// TD dříve“) ⇒ klec nestojí: jde tam, kde je nosič v bezpečí (vpřed, stranou nebo vzad ke spoluhráčům).
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
    // Od P187 (09.10.2026) smí klec před dobrou ranou i ustoupit ke spoluhráčům; požadavek je, že
    // nosič NEZŮSTANE STÁT v dosahu dobré rány — pohne se a po tahu je v bezpečí.
    EXPECT_NE(b.s.getPlayer(1).position, (Position{8, 7})) << "klec nestála";
    // N3 (uživatel 23.11.2026): TD je stihnutelné ⇒ místo ústupu do bezpečí smí klec jít vpřed s rizikem.
    EXPECT_TRUE(blitzThreat(b.s, b.s.getPlayer(1)) <= kSafeBlitzThreat || b.s.getPlayer(1).position.x > 8)
        << "po tahu v bezpečí, nebo vpřed";
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

// --- P232 vpřed i s rizikem (uživatel 28.10.2026) ------------------------------------------------------
// „jde o princip donutit se pohnout kupředu s míčem i přes riziko a to riziko popsat … cílem je dojít a dát TD“;
// „ano“ k: hranice podle času (s rezervou do bezpečí 0,15, bez ní 0,30) a „krok vpřed, který není rizikovější
// než stát, se zahraje“. Pořadí vpřed > do strany > stát > dozadu. Místo, kudy prochází každý krok klece: plán
// postupu (CageAdvancePlanner::build). Dřív postup „co nejdál bez hodu“ riziko nevážil — klec šla na hrozbu 0,49,
// ačkoli stát vycházelo 0,02.
namespace {
// Klec (pohyb `ma`) kolem nosiče na x = `cx` ve 2. kole, dva hráči vzadu, 4–9 soupeřů (pohyb 6, síla 3–5)
// náhodně 2–7 sloupců před nosičem. Pomalý tým (4) na x = 8 časovou rezervu nemá; na x = 16 nebo s pohybem 7 ano.
Board riskBoard(unsigned seed, int cx, int8_t ma = 4, bool around = false) {
    std::mt19937 rng(seed);
    Board b(2);
    b.put(1, TeamSide::HOME, {static_cast<int8_t>(cx), 7}, ma);
    cageAround(b, cx, ma);
    for (int i = 0; i < 2; ++i) b.put(6 + i, TeamSide::HOME, {static_cast<int8_t>(cx - 4), static_cast<int8_t>(4 + 6 * i)}, ma);
    const int k = 4 + static_cast<int>(rng() % 6);
    for (int i = 0; i < k; ++i) {
        for (;;) {
            const Position p{static_cast<int8_t>(around ? cx - 5 + static_cast<int>(rng() % 13) : cx + 2 + static_cast<int>(rng() % 6)),
                             static_cast<int8_t>(2 + rng() % 11)};
            if (b.s.getPlayerAtPosition(p) || (around && p.distanceTo({static_cast<int8_t>(cx), 7}) < 2)) continue;
            b.put(13 + i, TeamSide::AWAY, p, 6, static_cast<int8_t>(3 + rng() % 3));
            break;
        }
    }
    b.s.ball = BallState::carried({static_cast<int8_t>(cx), 7}, 1);
    return b;
}
struct RiskOutcome { bool moves = false; int progress = 0; double threat = -1.0; };
RiskOutcome planOutcome(const GameState& s, unsigned featuresOff) {
    setExtraFeaturesOff(featuresOff);
    CageAdvancePlanner planner(nullptr, cfg(), 1);
    const CageAdvancePlan plan = planner.build(s);
    setExtraFeaturesOff(0);
    RiskOutcome o;
    if (!plan.valid || plan.step < 1) return o;
    o.moves = true;
    o.threat = plan.blitzThreat;
    for (const Macro& m : plan.macros)
        if (m.playerId == 1 && m.type == MacroType::REPOSITION) o.progress = m.targetPos.x - s.getPlayer(1).position.x;
    return o;
}
}  // namespace

// Hranice podle času: tým bez rezervy 0,30, s rezervou jen do bezpečí.
TEST(RiskyForward, TheEdgeFollowsTheTimeSlack) {
    Board slow = riskBoard(1, 8);
    Board near = riskBoard(1, 16);
    ASSERT_FALSE(teamHasTimeSlack(slow.s, slow.s.getPlayer(1)));
    ASSERT_TRUE(teamHasTimeSlack(near.s, near.s.getPlayer(1)));
    EXPECT_DOUBLE_EQ(riskEdge(slow.s, slow.s.getPlayer(1)), kRiskEdgeNoSlack);
    EXPECT_DOUBLE_EQ(riskEdge(near.s, near.s.getPlayer(1)), kSafeBlitzThreat);
}

// N3 (uživatel 23.11.2026: „odstupňovat podle chybějících polí“): hranice kroku klece vpřed nezávisí na rezervě — je
// 0,30, když se TD tempem klece stihne, a roste o 0,10 za každé chybějící pole (nejvýš 0,70).
TEST(RiskyForward, TheForwardEdgeGrowsWithTheMissingSquares) {
    Board near = riskBoard(1, 16);
    EXPECT_EQ(missingSquares(near.s, near.s.getPlayer(1)), 0);
    EXPECT_DOUBLE_EQ(forwardRiskEdge(near.s, near.s.getPlayer(1)), kRiskEdgeNoSlack) << "s rezervou taky 0,30, ne 0,15";
    Board late(6);                                   // 6. kolo, nosič (pohyb 4) 15 polí před zónou: 3 tahy, tempo 2
    late.put(1, TeamSide::HOME, {10, 7}, 4);
    cageAround(late, 10, 4);
    late.s.ball = BallState::carried({10, 7}, 1);
    const int m = missingSquares(late.s, late.s.getPlayer(1));
    ASSERT_GT(m, 0) << "předpoklad: nouze";
    EXPECT_NEAR(forwardRiskEdge(late.s, late.s.getPlayer(1)), std::min(kForwardEdgeMax, kRiskEdgeNoSlack + 0.10 * m), 1e-9);
}

// N3 (uživatel 23.11.2026: „zůstat stát je vždy horší než podstoupit riziko a jít kupředu“). Na pozicích ze sondy 28.10.
// (2. kolo — TD je stihnutelné): kde šel dřívější plán bez P232 vpřed, jde vpřed i teď; vpřed do hranice
// forwardRiskEdge má přednost; nad hranicí se bere krok z nejnižšího pásma hrozby — nikdy horší než dřívější krok
// „co nejdál bez hodu“. Pozitivní kontroly: obě větve (do hranice, nad ní) nastanou.
TEST(RiskyForward, AlwaysForwardUpToTheEdgeFirst_AboveItInTheLowestBand) {
    int withinEdge = 0, aboveEdge = 0;
    struct Pos { unsigned seed; int8_t ma; bool around; };
    for (const Pos& q : std::vector<Pos>{{0, 4, false}, {5, 4, false}, {9, 4, false}, {14, 4, false}, {33, 4, false},
                                         {36, 4, false}, {43, 4, false}, {73, 4, false}, {79, 4, false}, {0, 7, false},
                                         {1, 7, false}, {2, 7, false}, {36, 4, true}, {56, 4, true}, {3, 4, true}}) {
        Board b = riskBoard(q.seed, 8, q.ma, q.around);
        const Player& c = b.s.getPlayer(1);
        ASSERT_TRUE(tdStillReachable(b.s, c));
        const double edge = forwardRiskEdge(b.s, c);
        const RiskOutcome now = planOutcome(b.s, 0), old = planOutcome(b.s, kP232RiskyForward);
        SCOPED_TRACE("seed " + std::to_string(q.seed) + " pohyb " + std::to_string(q.ma) + (q.around ? " kolem" : "") +
                     ": vpřed o " + std::to_string(now.progress) + " hrozba " + std::to_string(now.threat) + ", dřív o " +
                     std::to_string(old.progress) + " hrozba " + std::to_string(old.threat) + ", hranice " + std::to_string(edge));
        if (old.moves && old.progress >= 1) {
            EXPECT_TRUE(now.moves && now.progress >= 1) << "klec nestojí, když vpřed jde";
            EXPECT_LE(now.threat, std::max(edge, old.threat) + 1e-9) << "nad hranicí ne hůř než dřívější krok";
        }
        if (now.moves && now.progress >= 1) (now.threat <= edge ? withinEdge : aboveEdge)++;
    }
    EXPECT_GT(withinEdge, 0) << "vpřed do hranice";
    EXPECT_GT(aboveEdge, 0) << "vpřed nad hranicí (dřív by klec stála)";
}

// Ručně postavené pozice se známým výsledkem, odehrané celým řadičem klece (review P232: řadič nesmí přijatý krok
// vpřed přebít útěkem nosiče stranou). Pomalá klec (pohyb 4) kolem nosiče na x = `cx`, dva hráči vzadu, `k` soupeřů
// (pohyb 6, síla `st`) ve sloupci `wallX` uprostřed.
namespace {
Board wallAhead(int cx, int wallX, int8_t st, int k, int turn = 2) {
    Board b(turn);
    b.put(1, TeamSide::HOME, {static_cast<int8_t>(cx), 7}, 4);
    cageAround(b, cx, 4);
    for (int i = 0; i < 2; ++i) b.put(6 + i, TeamSide::HOME, {static_cast<int8_t>(cx - 4), static_cast<int8_t>(4 + 6 * i)}, 4);
    for (int i = 0; i < k; ++i)
        b.put(13 + i, TeamSide::AWAY, {static_cast<int8_t>(wallX), static_cast<int8_t>(7 - k + 2 * i + 1)}, 6, st);
    b.s.ball = BallState::carried({static_cast<int8_t>(cx), 7}, 1);
    return b;
}
}  // namespace

// Bez časové rezervy, bezpečné pole vpřed není, krok vpřed s hrozbou do 0,30 ano ⇒ klec jde vpřed.
TEST(RiskyForward, NoSlackTheCageStepsForwardDespiteRisk) {
    Board b = wallAhead(8, 11, 3, 3);
    ASSERT_EQ(riskEdge(b.s, b.s.getPlayer(1)), kRiskEdgeNoSlack);
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    const Player& c = b.s.getPlayer(1);
    EXPECT_GT(c.position.x, 8) << "vpřed";
    const double t = blitzThreat(b.s, c);
    EXPECT_GT(t, kSafeBlitzThreat) << "předpoklad: bezpečně vpřed nešlo";
    EXPECT_LE(t, kRiskEdgeNoSlack);
}

// N3 (uživatel 23.11.2026): krok vpřed s hrozbou 0,49 (nad hranicí i nad stáním 0,02) se dřív nehrál a klec stála.
// Teď jde klec vpřed — a ne do vyšší hrozby, než měl krok „co nejdál bez hodu“ (bez P232).
TEST(RiskyForward, AStepAboveTheEdgeAndAboveStandingIsPlayedInTheLowestBand) {
    double offThreat = -1.0;
    for (unsigned off : {static_cast<unsigned>(kP232RiskyForward), 0u}) {
        setExtraFeaturesOff(off);
        Board b = wallAhead(8, 11, 4, 3);
        CageController cc(nullptr, cfg(), 1);
        playAll(cc, b);
        setExtraFeaturesOff(0);
        const Player& c = b.s.getPlayer(1);
        if (off) {
            offThreat = blitzThreat(b.s, c);
            EXPECT_GT(offThreat, kRiskEdgeNoSlack) << "pozitivní kontrola: krok co nejdál je do rány";
        } else {
            EXPECT_GT(c.position.x, 8) << "klec nestojí";
            EXPECT_LE(blitzThreat(b.s, c), offThreat + 1e-9);
        }
    }
}

// Táž zeď, tým s časovou rezervou (nosič blíž zóně): dřív hranice 0,15 a vpřed jen do bezpečí; teď (N3) jde klec
// vpřed i s rizikem — rezerva už krok vpřed nebrzdí.
TEST(RiskyForward, WithTimeSlackTheCageStillStepsForward) {
    Board b = wallAhead(16, 19, 3, 3);
    ASSERT_TRUE(teamHasTimeSlack(b.s, b.s.getPlayer(1)));
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    EXPECT_GT(b.s.getPlayer(1).position.x, 16) << "vpřed";
}
// P232 + sonda 28.10.: plán klece přijal krok vpřed s hrozbou 0,24, ale zaostalec pak odešel soupeři z cesty
// k nosiči a po tahu byla hrozba 0,55. Hrozba, se kterou se tah rozhodl, musí platit i po tahu — zaostalec ani
// záloha se nepřesunou, když by ji zvedli. A řadič přijatý krok vpřed nepřebije útěkem nosiče stranou.
TEST(RiskyForward, LaggardsDoNotRaiseTheAcceptedThreat) {
    // pozitivní kontrola i bez N2b: blitz do pruhu by jinak zaostalce předběhl a hrozbu by neměl kdo zvednout
    for (uint64_t off : {uint64_t{0}, uint64_t{kP232KeepThreat} | uint64_t{kN2bLaneBlitzPlay}}) {
        setExtraFeaturesOff(off);
        Board b = riskBoard(0, 8, 4);
        CageController cc(nullptr, cfg(), 1);
        playAll(cc, b);
        setExtraFeaturesOff(0);
        const Player& c = b.s.getPlayer(1);
        const double t = blitzThreat(b.s, c);
        if (off) {
            EXPECT_GT(t, kRiskEdgeNoSlack) << "pozitivní kontrola: bez pojistky zaostalec hrozbu zvedne";
        } else {
            EXPECT_GT(c.position.x, 8) << "klec vpřed (řadič krok nepřebil)";
            EXPECT_GT(t, kSafeBlitzThreat) << "předpoklad: krok s rizikem";
            EXPECT_LE(t, kRiskEdgeNoSlack) << "po tahu nejvýš hranice";
        }
    }
}

// Hráč, který zůstal stát, protože by odchodem zvedl hrozbu na nosiče, nesmí po tahu klece odvést ani hledání
// (review 2 P232). V téže pozici chtěl zaostalec 2 z (7,6) na (11,3).
TEST(RiskyForward, SearchDoesNotMoveAPlayerHeldForTheCarriersSafety) {
    // pozitivní kontrola i bez N2b: bez pojistky by hráče 2 jinak odvedl blitz do pruhu, ne zaostalec
    for (uint64_t off : {uint64_t{0}, uint64_t{kP232KeepThreat} | uint64_t{kN2bLaneBlitzPlay}}) {
        setExtraFeaturesOff(off);
        Board b = riskBoard(0, 8, 4);
        CageController cc(nullptr, cfg(), 1);
        playAll(cc, b);
        const Position at = b.s.getPlayer(2).position;
        const bool banned = cc.forbidsCarrierMove(b.s, Macro{MacroType::REPOSITION, 2, -1, {11, 3}});
        setExtraFeaturesOff(0);
        if (off) {
            EXPECT_EQ(at, (Position{11, 3})) << "pozitivní kontrola: bez pojistky zaostalec odešel";
        } else {
            ASSERT_EQ(at, (Position{7, 6})) << "zůstal stát";
            EXPECT_TRUE(banned);
        }
    }
}
// Společný filtr přesunů (P232): přesun, který otevře soupeři cestu k nosiči, vypadne a hráč je „držený“; přesun,
// který hrozbu nezvedne, zůstane; přesuny před `from` se nesoudí; záloha, jejíž cesta na desce po přijatých přesunech
// chce hod, vypadne taky.
TEST(RiskyForward, TheMoveFilterDropsOnlyThreatRaisingMovesAndBlockedBackups) {
    Board b = riskBoard(0, 8, 4);                      // hráč 2 na (7,6) stojí soupeři v cestě (viz test výš)
    CageAdvancePlanner planner(nullptr, cfg(), 1);
    CageAdvancePlan plan = planner.build(b.s);
    ASSERT_TRUE(plan.valid);
    for (const Macro& m : plan.macros) play(b.s, m);   // deska po kroku klece
    const double base = blitzThreat(b.s, b.s.getPlayer(1));
    GameState probe = b.s.clone();
    probe.getPlayer(2).position = {11, 3};
    ASSERT_GT(blitzThreat(probe, probe.getPlayer(1)), base + 0.01) << "předpoklad: odchod hráče 2 hrozbu zvedne";
    {
        std::vector<Macro> q{Macro{MacroType::REPOSITION, 2, -1, {11, 3}}, Macro{MacroType::REPOSITION, 6, -1, {5, 4}}};
        std::vector<int> held;
        dropThreatRaisingMoves(b.s, q, 0, {}, held);
        ASSERT_EQ(q.size(), 1u);
        EXPECT_EQ(q[0].playerId, 6) << "neškodný přesun zůstal";
        EXPECT_EQ(held, (std::vector<int>{2}));
    }
    {
        std::vector<Macro> q{Macro{MacroType::REPOSITION, 2, -1, {11, 3}}};
        std::vector<int> held;
        dropThreatRaisingMoves(b.s, q, 1, {}, held);
        EXPECT_EQ(q.size(), 1u) << "před `from` se nesoudí";
    }
    {
        // záloha 6 z (4,4) na (4,2) přes pole (4,3): cestu zatarasí soupeř postavený vedle — chce úhyb
        GameState s = b.s.clone();
        Player& o = s.getPlayer(20);
        o.id = 20; o.teamSide = TeamSide::AWAY; o.state = PlayerState::STANDING; o.position = {5, 3}; o.stats = {6, 3, 3, 9};
        std::vector<Macro> q{Macro{MacroType::REPOSITION, 6, -1, {4, 2}}};
        std::vector<int> held;
        dropThreatRaisingMoves(s, q, 0, {6}, held);
        EXPECT_TRUE(q.empty()) << "záloha s cestou přes hod vypadne";
        std::vector<Macro> q2{Macro{MacroType::REPOSITION, 6, -1, {4, 2}}};
        std::vector<int> held2;
        dropThreatRaisingMoves(s, q2, 0, {}, held2);
        EXPECT_EQ(q2.size(), 1u) << "stejný přesun, který zálohou není, se cestou nesoudí";
    }
}

// N3: krok vpřed nad hranicí i nad stáním se dřív zahodil (plán „klec stojí“). Teď riskyForward vrátí krok vpřed
// z nejnižšího pásma hrozby — ne horší než původní krok.
TEST(RiskyForward, AStepAboveTheEdgeIsReplacedByTheLowestBandStep) {
    Board b = wallAhead(8, 11, 4, 3);
    CageAdvancePlanner planner(nullptr, cfg(), 1);
    const CageAdvancePlan old = planner.buildImpl(b.s, {});
    ASSERT_TRUE(old.valid && old.step >= 1);
    ASSERT_GT(old.blitzThreat, kRiskEdgeNoSlack) << "předpoklad: starý krok nad hranicí";
    const CageAdvancePlan r = planner.riskyForward(b.s, old, {}, false);
    EXPECT_TRUE(r.valid && r.step >= 1) << "vpřed";
    EXPECT_LE(r.blitzThreat, old.blitzThreat + 1e-9);
}

// Ani Wild Animal zaostalec před koncem tahu nezvedne hrozbu na nosiče (review 2 P232). Pozice ze sondy 28.10.:
// soupeři i kolem klece, zaostalci 2 a 3 (Wild Animal) by šli na zadní rohy do zóny soupeře — špinavé rohy (P228)
// by hrozbu zvedly 0,37 → 0,46, proto zůstanou.
TEST(RiskyForward, WildAnimalLaggardsDoNotRaiseTheThreatEither) {
    for (unsigned off : {0u, static_cast<unsigned>(kP232KeepThreat)}) {
        setExtraFeaturesOff(off);
        Board b = riskBoard(22, 8, 4, true);
        for (int id : {2, 3, 6, 7}) b.s.getPlayer(id).skills.add(SkillName::WildAnimal);
        CageController cc(nullptr, cfg(), 1);
        playAll(cc, b);
        Macro m;
        if (cc.beforeEndTurn(b.s, m)) { play(b.s, m); playAll(cc, b); }
        setExtraFeaturesOff(0);
        const Position p2 = b.s.getPlayer(2).position;
        if (off) EXPECT_EQ(p2, (Position{11, 6})) << "pozitivní kontrola: bez pojistky Wild Animal jde na roh";
        else EXPECT_EQ(p2, (Position{7, 6})) << "zůstal stát";
    }
}

// N4a (uživatel 27.11.2026: „B nepodceňovat riziko vysurfování, bránit se mu“): hrozba rány na nosiče počítá
// odtlačení do davu. Nosič na lajně (y = 0), soupeř dosáhne na pole pod ním — všechna tři pole odtlačení jsou
// mimo hřiště ⇒ míč pryč při Pushed i Stumbles i Down (ř. 650-651).
namespace {
double crowdThreat(bool n4a, int carrierY, bool fourCorners, std::vector<SkillName> carrierSkills = {},
                   std::vector<Position> fillers = {}) {
    const uint64_t saved = extraFeaturesOff();
    setExtraFeaturesOff(n4a ? 0 : static_cast<uint64_t>(kN4aCrowdThreat));
    Board b;
    b.put(1, TeamSide::HOME, {10, static_cast<int8_t>(carrierY)}, 4, 3, carrierSkills);
    b.s.ball.isHeld = true;
    b.s.ball.carrierId = 1;
    b.s.ball.position = b.s.getPlayer(1).position;
    if (fourCorners) {
        int id = 2;
        for (int cx : {-1, 1}) for (int cy : {-1, 1})
            b.put(id++, TeamSide::HOME, {static_cast<int8_t>(10 + cx), static_cast<int8_t>(carrierY + cy)});
    }
    int fid = 6;   // 2-5 rohy; 1-11 = naše sloty, 12-22 soupeřovy
    // Jen zaplní pole, nic jiného: ležící hráči neblokují (špinavé rohy P228 počítají jen stojící) a pole drží obsazené.
    for (const Position& f : fillers) b.put(fid++, TeamSide::HOME, f).state = PlayerState::PRONE;
    b.put(12, TeamSide::AWAY, {10, static_cast<int8_t>(carrierY + 3)}, 6, 3);
    const double t = blitzThreat(b.s, b.s.getPlayer(1));
    setExtraFeaturesOff(saved);
    return t;
}
}  // namespace

TEST(CrowdThreat, SidelineCarrierIsThreatenedMoreWithN4a) {
    const double off = crowdThreat(false, 0, false);
    const double on = crowdThreat(true, 0, false);
    EXPECT_GT(off, 0.0) << "pozitivní kontrola: soupeř na nosiče dosáhne";
    EXPECT_GT(on, off + 0.05) << "vysurfování se počítá";
}

TEST(CrowdThreat, MidPitchCarrierIsUnchanged) {
    const double off = crowdThreat(false, 7, true);
    EXPECT_GT(off, 0.0);
    EXPECT_DOUBLE_EQ(crowdThreat(true, 7, true), off);
}

TEST(CrowdThreat, StandFirmCarrierAtTheSidelineIsUnchanged) {
    const double off = crowdThreat(false, 0, false, {SkillName::StandFirm});
    EXPECT_GT(off, 0.0);
    EXPECT_DOUBLE_EQ(crowdThreat(true, 0, false, {SkillName::StandFirm}), off);
}

// Side Step (ř. 8470-8480; uživatel 27.11.2026: „u sideline pozor na kombinaci sidestep a žádné volné pole k ústupu“):
// nosič volí pole odsunu sám, takže dav obejde — ale jen s volným sousedním polem na hřišti.
TEST(CrowdThreat, SideStepCarrierWithAnOpenAdjacentSquareAvoidsTheCrowd) {
    const double off = crowdThreat(false, 0, false, {SkillName::SideStep});
    EXPECT_GT(off, 0.0);
    EXPECT_DOUBLE_EQ(crowdThreat(true, 0, false, {SkillName::SideStep}), off);
}

// Všechna sousední pole na hřišti obsazená kromě pole (10,1), odkud soupeř bije ⇒ Side Step nelze použít.
TEST(CrowdThreat, SideStepCarrierWithNoOpenAdjacentSquareIsPushedIntoTheCrowd) {
    const std::vector<Position> ring = {{9, 0}, {11, 0}, {9, 1}, {11, 1}};
    const double off = crowdThreat(false, 0, false, {SkillName::SideStep}, ring);
    const double on = crowdThreat(true, 0, false, {SkillName::SideStep}, ring);
    EXPECT_GT(off, 0.0) << "pozitivní kontrola: soupeř na (10,1) dojde";
    EXPECT_GT(on, off + 0.05) << "bez volného pole Side Step neplatí, dav hrozí";
    EXPECT_DOUBLE_EQ(on, crowdThreat(true, 0, false, {}, ring)) << "totéž jako bez Side Step";
}

// N4a, revize 27.11.2026: číselné hodnoty, domovské pole útočníka, Juggernaut / Grab / Wrestle, koncová čára a roh hřiště,
// jedno volné pole odtlačení u kraje, P228. Jeden útočník (pohyb 6), nosič bez rohů; `ours` / `opps` = ležící hráči,
// kteří jen drží pole obsazená (neblokují, nemají zónu ani asistenci).
namespace {
struct HitSetup {
    Position carrier{10, 0};
    int8_t carrierSt = 3;
    std::vector<SkillName> carrierSkills;
    Position att{10, 3};
    int8_t attSt = 3;
    std::vector<SkillName> attSkills;
    std::vector<Position> ours, opps;
};
double hitThreat(const HitSetup& h, bool n4a) {
    const uint64_t saved = extraFeaturesOff();
    setExtraFeaturesOff(n4a ? 0 : static_cast<uint64_t>(kN4aCrowdThreat));
    Board b;
    b.put(1, TeamSide::HOME, h.carrier, 4, h.carrierSt, h.carrierSkills);
    b.s.ball = BallState::carried(h.carrier, 1);
    int id = 2;                                                  // sloty: naši 1-11, soupeřovi 12-22 (útočník 12)
    for (const Position& p : h.ours) b.put(id++, TeamSide::HOME, p).state = PlayerState::PRONE;
    id = 13;
    for (const Position& p : h.opps) b.put(id++, TeamSide::AWAY, p).state = PlayerState::PRONE;
    b.put(12, TeamSide::AWAY, h.att, 6, h.attSt, h.attSkills);
    const double t = blitzThreat(b.s, b.s.getPlayer(1));
    setExtraFeaturesOff(saved);
    return t;
}
}  // namespace

// (i) čísla: 1 kostka útočníka bez Block nosiče = 5/6, s Block = 4/6; 2 kostky obránce (útočník síla 2) = (5/6)²
TEST(CrowdThreat, NumbersOneDieNoBlockOneDieBlockTwoDiceDefender) {
    HitSetup h;
    EXPECT_NEAR(hitThreat(h, true), 5.0 / 6.0, 1e-9);
    h.carrierSkills = {SkillName::Block};
    EXPECT_NEAR(hitThreat(h, true), 4.0 / 6.0, 1e-9);
    h.carrierSkills = {};
    h.attSt = 2;
    EXPECT_NEAR(hitThreat(h, true), 25.0 / 36.0, 1e-9);
    EXPECT_LT(hitThreat(h, false), 0.2) << "pozitivní kontrola: bez N4a jen sražení (2 kostky obránce)";
}

// (ii) domovské pole útočníka je po jeho přesunu volné ⇒ když je polem odtlačení, dav nehrozí. Útočník z (9,0) jde blitzem na
// (11,1)/(11,0) (pole (10,1) je obsazené, obchází se přes řádek 2); odtlačení z (11,1) míří na (9,0) = jeho vyklizený domov.
// Kontrola: tentýž útočník začíná jinde a na (9,0) leží cizí tělo ⇒ dav.
TEST(CrowdThreat, TheAttackersVacatedHomeSquareIsAFreePushSquare) {
    HitSetup h;
    h.att = {9, 0};
    h.ours = {{10, 1}};
    EXPECT_LT(hitThreat(h, true), 0.4) << "domov (9,0) se uvolní ⇒ dav nehrozí (jen sražení)";
    HitSetup c;
    c.att = {9, 2};
    c.ours = {{10, 1}};
    c.opps = {{9, 0}};
    EXPECT_NEAR(hitThreat(c, true), 5.0 / 6.0, 1e-9) << "pozitivní kontrola: na (9,0) leží tělo ⇒ dav";
}

// (ii) Side Step nosiče: jediné volné sousední pole (kromě pole rány) je vyklizený domov útočníka ⇒ uteče. Útočník stojí na (9,0),
// (10,1) je pole rány, ostatní sousedé obsazení. Kontrola: útočník jinde, na (9,0) tělo ⇒ žádné volné pole ⇒ dav.
TEST(CrowdThreat, SideStepEscapesToTheAttackersVacatedHomeSquare) {
    HitSetup h;
    h.carrierSkills = {SkillName::SideStep};
    h.att = {9, 0};
    h.ours = {{11, 0}, {9, 1}, {11, 1}};
    EXPECT_DOUBLE_EQ(hitThreat(h, true), hitThreat(h, false));
    HitSetup c = h;
    c.att = {10, 4};
    c.opps = {{9, 0}};
    EXPECT_GT(hitThreat(c, true), hitThreat(c, false) + 0.3) << "pozitivní kontrola: bez volného pole Side Step neplatí";
}

// (iii) Juggernaut v blitzu (ř. 8190-8195): Stand Firm nosiče nepomůže, Block taky ne (Both Down = Pushed); v bloku z místa ano
TEST(CrowdThreat, JuggernautInABlitzBeatsStandFirmAndBlock) {
    HitSetup h;
    h.carrierSkills = {SkillName::StandFirm};
    h.attSkills = {SkillName::Juggernaut};
    EXPECT_NEAR(hitThreat(h, true), 5.0 / 6.0, 1e-9) << "blitz s Juggernautem: Stand Firm neplatí";
    h.att = {10, 1};                                             // útočník už stojí vedle nosiče = blok, ne blitz
    EXPECT_DOUBLE_EQ(hitThreat(h, true), hitThreat(h, false)) << "blok bez blitzu: Juggernaut nepůsobí, Stand Firm drží";
    HitSetup g;
    g.carrierSkills = {SkillName::Block};
    g.attSkills = {SkillName::Juggernaut};
    EXPECT_NEAR(hitThreat(g, true), 5.0 / 6.0, 1e-9) << "Juggernaut: Both Down jako Pushed, Block nepomůže";
}

// (iii) Grab a Side Step se ruší (ř. 8151-8153): útočník s Grabem Side Step nosiče vypíná
TEST(CrowdThreat, GrabCancelsTheCarriersSideStep) {
    HitSetup h;
    h.carrierSkills = {SkillName::SideStep};
    EXPECT_DOUBLE_EQ(hitThreat(h, true), hitThreat(h, false)) << "pozitivní kontrola: bez Grabu Side Step dav obejde";
    h.attSkills = {SkillName::Grab};
    EXPECT_NEAR(hitThreat(h, true), 5.0 / 6.0, 1e-9);
}

// (iii) Wrestle útočníka přebíjí Block nosiče: Both Down = oba na zem ⇒ míč pryč
TEST(CrowdThreat, AttackersWrestleBeatsTheCarriersBlock) {
    HitSetup h;
    h.carrierSkills = {SkillName::Block};
    EXPECT_NEAR(hitThreat(h, true), 4.0 / 6.0, 1e-9) << "pozitivní kontrola: bez Wrestle drží Block";
    h.attSkills = {SkillName::Wrestle};
    EXPECT_NEAR(hitThreat(h, true), 5.0 / 6.0, 1e-9);
}

// (iv) jiná osa než y = 0: koncová čára (x = 25) a roh hřiště (0,0)
TEST(CrowdThreat, EndLineAndPitchCornerAreCrowdsToo) {
    HitSetup h;
    h.carrier = {25, 7};
    h.att = {22, 7};
    EXPECT_NEAR(hitThreat(h, true), 5.0 / 6.0, 1e-9) << "koncová čára";
    EXPECT_LT(hitThreat(h, false), 0.4);
    HitSetup c;
    c.carrier = {0, 0};
    c.att = {3, 3};
    EXPECT_NEAR(hitThreat(c, true), 5.0 / 6.0, 1e-9) << "roh hřiště";
}

// (v) u kraje jedno (onPitch = 1) i dvě (onPitch = 2) pole odtlačení na hřišti volná ⇒ dav nehrozí; všechna obsazená ⇒ dav.
// Sousedé nosiče (10,0) jsou zaplněni tak, aby šlo bít jen z (11,1), resp. z (11,0).
TEST(CrowdThreat, AFreeOnPitchPushSquareMeansNoCrowd) {
    HitSetup one;                                                // bije se z (11,1): pole odtlačení na hřišti jen (9,0)
    one.att = {12, 3};
    one.ours = {{9, 1}, {10, 1}, {11, 0}};
    EXPECT_LT(hitThreat(one, true), 0.4) << "(9,0) volné ⇒ dav nehrozí";
    HitSetup oneTaken = one;
    oneTaken.opps = {{9, 0}};
    EXPECT_NEAR(hitThreat(oneTaken, true), 5.0 / 6.0, 1e-9) << "pozitivní kontrola: (9,0) obsazené ⇒ dav";
    HitSetup two;                                                // bije se z (11,0): pole odtlačení na hřišti (9,0), (9,1)
    two.att = {13, 3};
    two.ours = {{9, 1}, {10, 1}, {11, 1}};
    EXPECT_LT(hitThreat(two, true), 0.4) << "(9,0) volné ⇒ dav nehrozí";
    HitSetup twoTaken = two;
    twoTaken.opps = {{9, 0}};
    EXPECT_NEAR(hitThreat(twoTaken, true), 5.0 / 6.0, 1e-9) << "pozitivní kontrola: obě pole obsazená ⇒ dav";
}

// (vi) P228: náš roh na poli odtlačení (9,0) stojí v zóně soupeře (soupeř na (8,1)); roh se napadne a buď zmizí, nebo leží —
// sražený roh pole odtlačení OBSAZUJE, takže dav hrozí (nikdy ne méně, než když roh stojí).
TEST(CrowdThreat, ADirtyCornerOnThePushSquareStillMakesTheCrowd) {
    const uint64_t saved = extraFeaturesOff();
    for (bool n4a : {false, true}) {
        setExtraFeaturesOff(n4a ? 0 : static_cast<uint64_t>(kN4aCrowdThreat));
        Board b;
        b.put(1, TeamSide::HOME, {10, 0}, 4);
        b.put(2, TeamSide::HOME, {9, 0}, 4);                    // roh na boku
        b.put(3, TeamSide::HOME, {9, 1}, 4).state = PlayerState::PRONE;
        b.put(4, TeamSide::HOME, {10, 1}, 4).state = PlayerState::PRONE;
        b.put(5, TeamSide::HOME, {11, 0}, 4).state = PlayerState::PRONE;
        b.put(13, TeamSide::AWAY, {8, 1}, 6, 4);                // dotýká se rohu (9,0)
        b.put(12, TeamSide::AWAY, {12, 3}, 6, 3);               // bije z (11,1)
        b.s.ball = BallState::carried({10, 0}, 1);
        const double t = blitzThreat(b.s, b.s.getPlayer(1));
        if (n4a) EXPECT_GE(t, 5.0 / 6.0 - 1e-9) << "dav se počítá i s rohem na poli odtlačení";
        else EXPECT_LT(t, 0.7) << "pozitivní kontrola: bez N4a dav nehrozí";
    }
    setExtraFeaturesOff(saved);
}

// N4b (uživatel 26.11./27.11.2026 — citace u kN4bEdgeCage): v tísni smí nosič stát na kraji jen se dvěma vnitřními rohy.
namespace {
// Trpaslíci (pohyb 4, síla 3) proti silnějším a rychlejším (pohyb 6, síla 4); 7. kolo ⇒ chybí pole k TD a rezerva není.
// Uprostřed neprostupná zeď (zóny soupeřů na x = 16..18, řádky 2..14), u kraje volno.
Board edgeBoard(int carrierY = 3, std::vector<Position> extraOpp = {}, std::vector<Position> extraOurs = {}, int turn = 7) {
    Board b(turn);
    b.put(1, TeamSide::HOME, {15, static_cast<int8_t>(carrierY)}, 4);
    const Position mates[] = {{14, 2}, {14, 4}, {13, 3}, {13, 1}, {15, 5}, {13, 5}};
    int id = 2;
    for (const Position& p : mates) b.put(id++, TeamSide::HOME, p, 4);
    for (const Position& p : extraOurs) b.put(id++, TeamSide::HOME, p, 4);
    int oid = 13;
    for (int y = 3; y <= 13; y += 2) b.put(oid++, TeamSide::AWAY, {17, static_cast<int8_t>(y)}, 6, 4);
    for (const Position& p : extraOpp) b.put(oid++, TeamSide::AWAY, p, 6, 4);
    b.s.ball = BallState::carried({15, static_cast<int8_t>(carrierY)}, 1);
    return b;
}
Position edgeTarget(const GameState& s, bool n4b, double* threat = nullptr) {
    const uint64_t saved = extraFeaturesOff();
    setExtraFeaturesOff(n4b ? 0 : static_cast<uint64_t>(kN4bEdgeCage));
    CageAdvancePlanner planner(nullptr, cfg(), 1);
    const CageAdvancePlan plan = planner.build(s);
    setExtraFeaturesOff(saved);
    if (threat) *threat = plan.blitzThreat;
    if (plan.valid)
        for (const Macro& m : plan.macros)
            if (m.playerId == 1 && m.type == MacroType::REPOSITION) return m.targetPos;
    return {-1, -1};
}
}  // namespace

// (a) tíseň: bez N4b nosič kraj nevybere (ustoupí), s N4b jde na y = 0 se dvěma rohy a prázdnými boky i dírou
TEST(EdgeCage, InDistressTheCarrierGoesToTheEdgeWithTwoCorners) {
    Board b = edgeBoard();
    ASSERT_GE(missingSquares(b.s, b.s.getPlayer(1)), 1) << "předpoklad: chybí pole, rezerva není";
    ASSERT_TRUE(tdStillReachable(b.s, b.s.getPlayer(1)));
    const Position off = edgeTarget(b.s, false);
    EXPECT_NE(off.y, 0) << "pozitivní kontrola: bez N4b se kraj nevybere";
    const Position on = edgeTarget(b.s, true);
    EXPECT_EQ(on.y, 0) << "s N4b nosič stojí na kraji";
    EXPECT_GE(on.x, 16) << "a je to krok vpřed";
}

// (b) soupeř z díry (zóna jen na pole vedle kraje) dojde bez úhybu a bije na dvě kostky ⇒ vysurfování nad mezí ⇒ kraj ne
TEST(EdgeCage, AnOpponentReachingTheHoleKeepsTheCarrierOffTheEdge) {
    Board b = edgeBoard(3, {{17, 2}});
    Player& hunter = b.s.getPlayer(19);                  // 7. soupeř = ten na (17,2); obratný, úhyb z naší zóny mu vyjde
    ASSERT_EQ(hunter.position, (Position{17, 2}));
    hunter.stats.agility = 5;
    hunter.skills.add(SkillName::Dodge);
    ASSERT_GE(missingSquares(b.s, b.s.getPlayer(1)), 1);
    EXPECT_NE(edgeTarget(b.s, true).y, 0);
}

// (c) s časovou rezervou není tíseň ⇒ kraj ne (rezerva = brzké kolo, TD se stihne po klidném postupu)
TEST(EdgeCage, WithTimeToSpareThereIsNoDistressAndNoEdge) {
    Board b = edgeBoard(3, {}, {}, 2);
    ASSERT_EQ(missingSquares(b.s, b.s.getPlayer(1)), 0) << "předpoklad: nic nechybí";
    EXPECT_NE(edgeTarget(b.s, true).y, 0);
}

// (d) náš hráč na boku cílového pole ⇒ kraj ne (klec jen rohy a prázdno, aby nešel řetěz); všechna pole vpřed na kraji mají
//     náš bok
TEST(EdgeCage, OurPlayerOnTheFlankKeepsTheCarrierOffTheEdge) {
    Board b = edgeBoard(3, {}, {{15, 0}, {16, 0}, {18, 0}, {19, 0}});
    ASSERT_GE(missingSquares(b.s, b.s.getPlayer(1)), 1);
    EXPECT_NE(edgeTarget(b.s, true).y, 0);
}

// (e) nosič už na kraji stojí ⇒ s N4b se klec dostaví na místě dvěma rohy, bez N4b ne
TEST(EdgeCage, ACarrierAlreadyOnTheEdgeGetsHisTwoCornersInPlace) {
    Board b(7);
    b.put(1, TeamSide::HOME, {16, 0}, 4);
    b.put(2, TeamSide::HOME, {14, 1}, 4);
    b.put(3, TeamSide::HOME, {16, 3}, 4);
    b.put(13, TeamSide::AWAY, {23, 8}, 6, 4);
    b.s.ball = BallState::carried({16, 0}, 1);
    for (bool on : {false, true}) {
        const uint64_t saved = extraFeaturesOff();
        setExtraFeaturesOff(on ? 0 : static_cast<uint64_t>(kN4bEdgeCage));
        CageAdvancePlanner planner(nullptr, cfg(), 1);
        const CageAdvancePlan fill = planner.buildFillOnly(b.s, {});
        setExtraFeaturesOff(saved);
        if (on) {
            ASSERT_TRUE(fill.valid) << "s N4b se dostaví";
            EXPECT_EQ(fill.filledCorners, 2);
            EXPECT_EQ(fill.macros.size(), 2u);
        } else {
            EXPECT_FALSE(fill.valid) << "pozitivní kontrola: bez N4b na kraji dostavba není";
        }
    }
}
