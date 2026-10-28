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

// P177: „dokud má tým časovou rezervu, klec jde bezpečně, ne co nejdál“. Zeď soupeřů na x = 11 vpřed
// nepustí (každé pole vpřed leží v jejich zóně), nosič je tam, kde stojí, v bezpečí ⇒ klec stojí:
// nosič se nehne a chybějící roh se dostaví.
// P228 (27.10.2026): zeď stála na x = 10, přímo u předních rohů (9,6)/(9,8) — ty pak stály v zóně
// soupeřů, hrozba přes špinavé rohy je 0,33 a předpoklad „nosič je v bezpečí“ přestal platit. Na
// x = 11 jsou rohy čisté a pole vpřed pořád v zóně zdi (vpřed by rohy stály špinavé — i to hlídá
// předpoklad níž; bez P228 by plán bezpečné pole vpřed našel). Bez P177 (maska 16384) test padá.
TEST(CageStands, WithTimeToSpareAndNoSafeSquareAheadTheCarrierStaysAndTheCornerComes) {
    Board b = standBoard(11, 3);

    b.s.getPlayer(3).position = {6, 9};                     // jeden roh vzadu
    ASSERT_TRUE(teamHasTimeSlack(b.s, b.s.getPlayer(1))) << "předpoklad: tým má rezervu";
    ASSERT_LE(blitzThreat(b.s, b.s.getPlayer(1)), kSafeBlitzThreat) << "předpoklad: nosič je v bezpečí tam, kde stojí";
    ASSERT_EQ(standingCorners(b.s, b.s.getPlayer(1)), 3);
    {   // předpoklad (review 27.10., nález 6): bezpečné pole vpřed není — plán klece nosičem dopředu nehne
        CageAdvancePlanner planner(nullptr, cfg(), 1);
        ASSERT_LE(planner.buildImpl(b.s, {}, false).step, 0) << "předpoklad: bezpečné pole vpřed není";
    }
    CageController cc(nullptr, cfg(), 1);
    std::vector<Macro> ms;
    playAll(cc, b, &ms);
    EXPECT_EQ(b.s.getPlayer(1).position, (Position{8, 7})) << "nosič se nehnul";
    EXPECT_EQ(standingCorners(b.s, b.s.getPlayer(1)), 4) << "rohy se dostavěly";
    for (const Macro& m : ms) EXPECT_NE(m.playerId, 1) << "nosič nedostal žádné makro";
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
    EXPECT_LE(blitzThreat(b.s, b.s.getPlayer(1)), kSafeBlitzThreat) << "a nosič po tahu není vystaven dobré ráně";
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

// Pozice vybrané sondou 28.10. tak, aby pokryly všechny větve; každá větev má pozitivní kontrolu (počet ≥ 1), takže
// když se pozice změnou enginu rozpadnou, test spadne, místo aby tiše prošel.
TEST(RiskyForward, ForwardUpToTheEdgeNeverAboveIt_NotWorseThanStandingOnlyWithoutASaferOption) {
    int riskyForward = 0, cappedOldStep = 0, saferOptionKept = 0, slackBoards = 0, notWorseThanStanding = 0;
    // pomalý tým (pohyb 4): bez rezervy; rychlý (pohyb 7): s rezervou — tam smí vpřed jen do bezpečí;
    // „kolem“ = soupeři i vedle a za klecí (jinak je vždy bezpečný krok stranou a „není horší než stát“ nenastane)
    struct Pos { unsigned seed; int8_t ma; bool around; };
    for (const Pos& q : std::vector<Pos>{{0, 4, false}, {5, 4, false}, {9, 4, false}, {14, 4, false}, {33, 4, false},
                                         {36, 4, false}, {43, 4, false}, {73, 4, false}, {79, 4, false}, {0, 7, false},
                                         {1, 7, false}, {2, 7, false}, {36, 4, true}, {56, 4, true}, {3, 4, true}}) {
        const unsigned seed = q.seed;
        const int8_t ma = q.ma;
        Board b = riskBoard(seed, 8, ma, q.around);
        const Player& c = b.s.getPlayer(1);
        const double edge = riskEdge(b.s, c);
        if (edge == kSafeBlitzThreat) ++slackBoards;
        CageAdvancePlanner planner(nullptr, cfg(), 1);
        const CageAdvancePlan fill = planner.buildFillOnly(b.s, {});
        const double stay = fill.valid ? fill.blitzThreat : blitzThreat(b.s, c);
        const RiskOutcome now = planOutcome(b.s, 0), old = planOutcome(b.s, kP232RiskyForward);
        const bool oldAltWithinEdge = old.moves && old.threat <= edge;
        SCOPED_TRACE("seed " + std::to_string(seed) + " pohyb " + std::to_string(ma) + (q.around ? " kolem" : "") + ": vpřed o " + std::to_string(now.progress) +
                     " hrozba " + std::to_string(now.threat) + ", dřív o " + std::to_string(old.progress) + " hrozba " +
                     std::to_string(old.threat) + ", stát " + std::to_string(stay) + ", hranice " + std::to_string(edge));
        if (now.moves && now.progress >= 1 && now.threat > edge) {
            // nad hranicí jen „není horší než stát“ a jen když jiné pole do hranice není
            EXPECT_LE(now.threat, stay + 1e-9);
            EXPECT_FALSE(oldAltWithinEdge);
            ++notWorseThanStanding;
        }
        if (now.moves && now.progress >= 1 && now.threat > kSafeBlitzThreat && now.threat <= edge) ++riskyForward;
        if (old.moves && old.progress >= 1 && old.threat > std::max(edge, stay)) {
            ++cappedOldStep;
            // klec stojí a dostaví se (krok 0), nebo vpřed do hranice / ne hůř než stát
            EXPECT_TRUE(!now.moves || (now.progress >= 1 && now.threat <= std::max(edge, stay) + 1e-9)) << "krok nad hranicí i nad stáním";
        }
        if (stay > edge && old.moves && old.progress <= 0 && old.threat <= kSafeBlitzThreat) {
            ++saferOptionKept;
            EXPECT_TRUE(now.moves && now.threat <= edge) << "bezpečný krok stranou má přednost před „není horší než stát“";
        }
    }
    EXPECT_GT(riskyForward, 0) << "vpřed i přes riziko (do hranice)";
    EXPECT_GT(cappedOldStep, 0) << "starý krok nad hranicí i nad stáním";
    EXPECT_GT(saferOptionKept, 0) << "stát nad hranicí a bezpečný krok stranou";
    EXPECT_GT(slackBoards, 0) << "pozice s časovou rezervou";
    EXPECT_GT(notWorseThanStanding, 0) << "vpřed nad hranicí, protože není horší než stát a jiné pole do hranice není";
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

// Krok vpřed by měl hrozbu 0,49 (nad hranicí i nad stáním 0,02) ⇒ klec stojí. Bez P232 tam šla.
TEST(RiskyForward, AStepAboveTheEdgeAndAboveStandingIsNotPlayed) {
    for (unsigned off : {0u, static_cast<unsigned>(kP232RiskyForward)}) {
        setExtraFeaturesOff(off);
        Board b = wallAhead(8, 11, 4, 3);
        CageController cc(nullptr, cfg(), 1);
        playAll(cc, b);
        setExtraFeaturesOff(0);
        const Player& c = b.s.getPlayer(1);
        if (off) {
            EXPECT_GT(blitzThreat(b.s, c), kRiskEdgeNoSlack) << "pozitivní kontrola: bez P232 krok do rány";
        } else {
            EXPECT_EQ(c.position, (Position{8, 7})) << "klec stojí";
            EXPECT_LE(blitzThreat(b.s, c), kSafeBlitzThreat);
        }
    }
}

// Táž zeď, ale tým má časovou rezervu (nosič blíž zóně) ⇒ hranice 0,15: vpřed jen do bezpečí.
TEST(RiskyForward, WithTimeSlackOnlyASafeStep) {
    Board b = wallAhead(16, 19, 3, 3);
    ASSERT_EQ(riskEdge(b.s, b.s.getPlayer(1)), kSafeBlitzThreat);
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    EXPECT_LE(blitzThreat(b.s, b.s.getPlayer(1)), kSafeBlitzThreat);
}
// P232 + sonda 28.10.: plán klece přijal krok vpřed s hrozbou 0,24, ale zaostalec pak odešel soupeři z cesty
// k nosiči a po tahu byla hrozba 0,55. Hrozba, se kterou se tah rozhodl, musí platit i po tahu — zaostalec ani
// záloha se nepřesunou, když by ji zvedli. A řadič přijatý krok vpřed nepřebije útěkem nosiče stranou.
TEST(RiskyForward, LaggardsDoNotRaiseTheAcceptedThreat) {
    for (unsigned off : {0u, static_cast<unsigned>(kP232KeepThreat)}) {
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
    for (unsigned off : {0u, static_cast<unsigned>(kP232KeepThreat)}) {
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

// Krok vpřed nad hranicí i nad stáním se zahodí: plán „klec stojí“ nese hrozbu toho, co se stane — stání (review 2).
TEST(RiskyForward, TheStandPlanCarriesTheStandingThreat) {
    Board b = wallAhead(8, 11, 4, 3);
    CageAdvancePlanner planner(nullptr, cfg(), 1);
    const CageAdvancePlan old = planner.buildImpl(b.s, {});
    ASSERT_TRUE(old.valid && old.step >= 1);
    ASSERT_GT(old.blitzThreat, kRiskEdgeNoSlack) << "předpoklad: starý krok nad hranicí";
    const CageAdvancePlan r = planner.riskyForward(b.s, old, {}, false);
    EXPECT_FALSE(r.valid);
    EXPECT_DOUBLE_EQ(r.blitzThreat, planner.stayThreat(b.s, {}));
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
