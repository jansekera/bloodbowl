// P126 — jedna klec ve třech fázích (uživatel 01.–02.10.2026, paměť
// project_bloodbowl_one_cage_phases_20261002). Testy hlídají POŽADAVKY
// uživatele, ne tvar kódu: kdo se kdy hýbe, kam a co se nesmí stát.
#include <gtest/gtest.h>
#include "bb/one_cage.h"
#include "bb/game_simulator.h"
#include "bb/helpers.h"
#include "bb/dice.h"
#include "bb/roster.h"
#include "bb/macro_mcts.h"
#include "bb/action_resolver.h"
#include "bb/turn_planner.h"   // classifyTurnGoal (P154)
#include <algorithm>
#include <cstdlib>

using namespace bb;

namespace {

struct Board {
    GameState s;
    Board(int turn = 1) {
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

int distToAwayEz(Position p) { return 25 - p.x; }

// Odehraje makro v rychlé kopii (kostky jen šestky = vše se povede).
void play(GameState& s, const Macro& m) {
    FixedDiceRoller dice(std::vector<int>(200, 6));
    greedyExpandMacro(s, m, dice);
}

// Vypuštění nosiče: nosič MA6, rohy MA4 (klec ujde nejvýš 4 pole za tah),
// míč 17 polí od TD zóny. Viz výpočet u testů decideRelease.
Board releaseBoard(int turn) {
    Board b(turn);
    b.put(1, TeamSide::HOME, {8, 7}, 6, 3, {SkillName::SureHands});
    b.put(2, TeamSide::HOME, {7, 6});
    b.put(3, TeamSide::HOME, {7, 8});
    b.put(4, TeamSide::HOME, {9, 6});
    b.put(5, TeamSide::HOME, {9, 8});
    // dva volní hráči vpředu u soupeřů: na rohy klece nedosáhnou, markovat můžou
    b.put(6, TeamSide::HOME, {19, 0});
    b.put(7, TeamSide::HOME, {19, 14});
    b.put(12, TeamSide::AWAY, {20, 3}, 6);
    b.put(13, TeamSide::AWAY, {20, 11}, 6);
    b.s.ball = BallState::carried({8, 7}, 1);
    return b;
}

}  // namespace

// --- Start: kdo stojí nejhlouběji -------------------------------------------

// Uživatel 02.10.: „ostatní musí být rozestaveni míň vzadu než nosič — je to
// start." Při příjmu výkopu stojí na nejhlubším poli JEDINÝ hráč, a je to
// nejlepší držitel míče (Sure Hands, má-li ho sestava).
TEST(OneCageStart, NobodyStandsDeeperThanTheBallHandlerAtReceive) {
    for (const char* name : {"human", "orc", "skaven", "dwarf", "wood-elf"}) {
        const TeamRoster* r = getDevelopedRoster(name, 1200);
        ASSERT_NE(r, nullptr);
        GameState s;
        setupHalf(s, *r, *r, TeamSide::AWAY, nullptr);   // HOME přijímá
        int deepest = 99, atDeepest = 0;
        const Player* deep = nullptr;
        s.forEachOnPitch(TeamSide::HOME, [&](const Player& p) {
            if (p.position.x < deepest) { deepest = p.position.x; atDeepest = 0; }
            if (p.position.x == deepest) { ++atDeepest; deep = &s.getPlayer(p.id); }
        });
        EXPECT_EQ(atDeepest, 1) << name;
        ASSERT_NE(deep, nullptr);
        bool rosterHasSureHands = false;
        for (int t = 0; t < r->positionalCount; ++t)
            rosterHasSureHands |= r->positionals[t].skills.has(SkillName::SureHands);
        if (rosterHasSureHands)   // wood-elf Sure Hands nemá: hlouběji jde nejobratnější
            EXPECT_TRUE(deep->hasSkill(SkillName::SureHands)) << name << ": " << deep->positionName;
    }
}

// --- Cíl nosiče ---------------------------------------------------------------

TEST(OneCageForward, GoesAsFarAsTheMovementAllowsInTheOpen) {
    Board b;
    Player& c = b.put(1, TeamSide::HOME, {5, 7}, 6);
    b.put(12, TeamSide::AWAY, {24, 1});
    const Position d = farthestSafeForward(b.s, c, 6);
    EXPECT_EQ(d.x, 11) << "6 polí vpřed";
}

TEST(OneCageForward, NeverEndsNextToAStandingOpponentNorDodges) {
    Board b;
    Player& c = b.put(1, TeamSide::HOME, {5, 7}, 6);
    // zeď přes celou šířku na x=10: dál než x=8 se bez hodu nedá
    for (int y = 0, id = 12; y <= 14; y += 2, ++id) b.put(id, TeamSide::AWAY, {10, static_cast<int8_t>(y)});
    const Position d = farthestSafeForward(b.s, c, 6);
    EXPECT_EQ(d.x, 8);
    EXPECT_EQ(countTacklezones(b.s, d, TeamSide::HOME, 1), 0) << "nosič nesmí skončit u soupeře";
}

// --- Fáze 3: kdy pustit nosiče ---------------------------------------------------

// Tah 6 (zbývají 3 tahy včetně tohoto), 17 polí. Klec: teď 4, další tah 4 ⇒ 9
// polí, a to je víc než dosah nosiče na TD (6 + 2) ⇒ v příštím tahu by se
// ukázalo, že nedoběhne. Sám: 6 + 6 a poslední tah na TD ⇒ stihne. Pustit.
TEST(OneCageRelease, ReleasesTheTurnBeforeTheCageWouldFail) {
    Board b = releaseBoard(6);
    CageAdvancePlanner planner(nullptr, cfg(), 1);
    const ReleaseDecision r = decideRelease(b.s, b.s.getPlayer(1), planner);
    EXPECT_EQ(r.dist, 17);
    EXPECT_EQ(r.turnsLeft, 3);
    EXPECT_EQ(r.cageStep, 4);
    EXPECT_FALSE(r.cageMakesIt);
    EXPECT_TRUE(r.soloMakesIt);
    EXPECT_TRUE(r.release);
}

// Pozitivní kontrola k předchozímu: o tah dřív klec ještě stihne ⇒ nepouštět.
TEST(OneCageRelease, KeepsTheCageWhileItStillMakesIt) {
    Board b = releaseBoard(5);
    CageAdvancePlanner planner(nullptr, cfg(), 1);
    const ReleaseDecision r = decideRelease(b.s, b.s.getPlayer(1), planner);
    EXPECT_TRUE(r.cageMakesIt);
    EXPECT_FALSE(r.release);
}

// Když nestihne ani sám, klec drží míč dál.
TEST(OneCageRelease, NoReleaseWhenEvenTheSoloRunCannotScore) {
    Board b = releaseBoard(7);   // 2 tahy, 17 polí: sám 6 + TD nestihne
    CageAdvancePlanner planner(nullptr, cfg(), 1);
    const ReleaseDecision r = decideRelease(b.s, b.s.getPlayer(1), planner);
    EXPECT_FALSE(r.soloMakesIt);
    EXPECT_FALSE(r.release);
}

// Fáze 3 v tahu: nosič vyrazí sám co nejdál, pak ostatní MARKUJÍ soupeře, kteří
// by k němu doběhli — přesunem vedle nich, bez bloku.
TEST(OneCageRelease, CarrierRunsAloneThenOthersMarkTheThreats) {
    Board b = releaseBoard(6);
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    ASSERT_TRUE(cc.next(b.s, m));
    EXPECT_EQ(cc.phase(), CagePhase::RELEASE);
    ASSERT_EQ(m.type, MacroType::REPOSITION);
    ASSERT_EQ(m.playerId, 1) << "napřed nosič";
    EXPECT_EQ(distToAwayEz(m.targetPos), 17 - 6) << "co nejdál: celý pohyb";
    play(b.s, m);

    std::vector<int> marked;
    while (cc.next(b.s, m)) {
        EXPECT_EQ(m.type, MacroType::REPOSITION) << "jen markovat, žádný blok";
        EXPECT_NE(m.playerId, 1) << "nosič už se nehýbe";
        const Player* opp = nullptr;
        for (int id : {12, 13})
            if (b.s.getPlayer(id).position.distanceTo(m.targetPos) == 1) opp = &b.s.getPlayer(id);
        ASSERT_NE(opp, nullptr) << "marker se staví vedle soupeře";
        marked.push_back(opp->id);
        play(b.s, m);
    }
    std::sort(marked.begin(), marked.end());
    EXPECT_EQ(marked, (std::vector<int>{12, 13})) << "oba soupeři by k nosiči doběhli";
}

// Jednou vypuštěný nosič se do klece nevrací, ani když by klec zase stíhala.
TEST(OneCageRelease, StaysReleasedForTheRestOfThePossession) {
    Board b = releaseBoard(6);
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    ASSERT_TRUE(cc.next(b.s, m));
    ASSERT_EQ(cc.phase(), CagePhase::RELEASE);
    // další náš tah, nosič o kus dál, rohy zase kolem něj
    b.s.homeTeam.turnNumber = 7;
    for (int id = 1; id <= 7; ++id) {
        Player& p = b.s.getPlayer(id);
        p.position.x = static_cast<int8_t>(p.position.x + 3);
        p.movementRemaining = p.stats.movement;
        p.hasMoved = p.hasActed = false;
    }
    b.s.ball = BallState::carried(b.s.getPlayer(1).position, 1);
    ASSERT_TRUE(cc.next(b.s, m));
    EXPECT_EQ(cc.phase(), CagePhase::RELEASE);
}

// --- Fáze 2 ---------------------------------------------------------------

// Uživatel 02.10.: soupeř na poli rohu ⇒ „napřed shodit soupeře".
TEST(OneCageAdvance, KnocksDownAnOpponentStandingOnACornerFirst) {
    Board b(1);
    b.put(1, TeamSide::HOME, {12, 7});
    b.put(2, TeamSide::HOME, {11, 6});
    b.put(3, TeamSide::HOME, {11, 8});
    b.put(5, TeamSide::HOME, {13, 8});
    b.put(4, TeamSide::HOME, {14, 6}, 4, 3, {SkillName::Block});
    b.put(13, TeamSide::AWAY, {13, 6});   // stojí na předním rohu
    b.put(14, TeamSide::AWAY, {24, 13});
    b.s.ball = BallState::carried({12, 7}, 1);
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    ASSERT_TRUE(cc.next(b.s, m));
    EXPECT_EQ(cc.phase(), CagePhase::CAGE);
    EXPECT_EQ(m.type, MacroType::BLOCK);
    EXPECT_EQ(m.targetId, 13);
    EXPECT_NE(m.playerId, 1) << "nosič nebojuje";
}

TEST(OneCageAdvance, MovesTheCageForwardCornersFirstCarrierLast) {
    Board b(1);
    b.put(1, TeamSide::HOME, {12, 7});
    b.put(2, TeamSide::HOME, {11, 6});
    b.put(3, TeamSide::HOME, {11, 8});
    b.put(4, TeamSide::HOME, {13, 6});
    b.put(5, TeamSide::HOME, {13, 8});
    b.put(14, TeamSide::AWAY, {24, 13});
    b.s.ball = BallState::carried({12, 7}, 1);
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    std::vector<int> order;
    while (cc.next(b.s, m)) {
        EXPECT_EQ(cc.phase(), CagePhase::CAGE);
        order.push_back(m.playerId);
        play(b.s, m);
    }
    ASSERT_FALSE(order.empty());
    EXPECT_EQ(order.back(), 1) << "nosič poslední";
    EXPECT_GT(b.s.getPlayer(1).position.x, 12) << "klec postoupila";
}

// --- Fáze 1 ---------------------------------------------------------------

// Nosič zvedne míč, jde co nejdál dopředu (přednost má on), pak rohy kolem něj.
TEST(OneCagePickup, PicksUpRunsForwardThenCornersFormAroundHim) {
    Board b(1);
    b.put(1, TeamSide::HOME, {4, 7}, 6, 3, {SkillName::SureHands});
    b.put(2, TeamSide::HOME, {8, 5});
    b.put(3, TeamSide::HOME, {8, 9});
    b.put(4, TeamSide::HOME, {10, 5});
    b.put(5, TeamSide::HOME, {10, 9});
    b.put(14, TeamSide::AWAY, {24, 13});
    b.s.ball = BallState::onGround({5, 7});
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    ASSERT_TRUE(cc.next(b.s, m));
    EXPECT_EQ(cc.phase(), CagePhase::PICKUP);
    ASSERT_EQ(m.type, MacroType::PICKUP);
    EXPECT_EQ(m.playerId, 1);
    play(b.s, m);
    ASSERT_TRUE(b.s.ball.isHeld);
    const Position cp = b.s.getPlayer(1).position;
    EXPECT_EQ(cp.x, 4 + 6) << "1 pole k míči + 5 dopředu = celý pohyb";

    int corners = 0;
    while (cc.next(b.s, m)) {
        EXPECT_NE(m.playerId, 1);
        play(b.s, m);
    }
    for (const Position& d : cp.getAdjacent()) {
        if (std::abs(d.x - cp.x) != 1 || std::abs(d.y - cp.y) != 1) continue;
        const Player* p = b.s.getPlayerAtPosition(d);
        if (p && p->teamSide == TeamSide::HOME) ++corners;
    }
    EXPECT_EQ(corners, 4) << "rohy kolem nosiče";
}

// Skutečný hráč AI (MacroMCTSPolicy, výchozí nastavení — klec není za
// přepínačem): v tahu s klecí jednají napřed rohy, nosič až po nich, a klec
// postoupí. Dřív to platilo jen se zapnutým F1, který produkce nezapínala.
TEST(OneCagePolicy, ProductionPlayerMovesTheCageCornersBeforeCarrier) {
    Board b(1);
    b.put(1, TeamSide::HOME, {12, 7});
    b.put(2, TeamSide::HOME, {11, 6});
    b.put(3, TeamSide::HOME, {11, 8});
    b.put(4, TeamSide::HOME, {13, 6});
    b.put(5, TeamSide::HOME, {13, 8});
    b.put(14, TeamSide::AWAY, {24, 13});
    b.s.ball = BallState::carried({12, 7}, 1);
    MacroMCTSPolicy policy(nullptr, cfg(), 42);
    DiceRoller dice(123);
    std::vector<int> order;
    for (int step = 0; step < 80; ++step) {
        if (b.s.phase != GamePhase::PLAY || b.s.activeTeam != TeamSide::HOME) break;
        Action a = policy(b.s);
        if (a.playerId > 0 && (order.empty() || order.back() != a.playerId)) order.push_back(a.playerId);
        executeAction(b.s, a, dice, nullptr);
        if (a.type == ActionType::END_TURN) break;
    }
    auto it = std::find(order.begin(), order.end(), 1);
    ASSERT_NE(it, order.end()) << "nosič nejednal";
    EXPECT_NE(it, order.begin()) << "nosič jednal první";
    EXPECT_GT(b.s.getPlayer(1).position.x, 12);
    EXPECT_GE(policy.cagePlansAdopted(), 1);
}

// --- P154 (06.10.2026): klec nesmí zůstat stát jen proto, že je nosič „v dosahu TD“ -------

namespace {
// Nosič (MA6) 8 polí od zóny = dosah jen se dvěma GFI ⇒ tah je SCORE_BALL, ale bez hodu
// nosič nedojde. Klec stojí kolem něj, soupeř daleko.
Board scoringRangeBoard(int turn) {
    Board b(turn);
    b.put(1, TeamSide::HOME, {17, 7}, 6);
    b.put(2, TeamSide::HOME, {16, 6});
    b.put(3, TeamSide::HOME, {16, 8});
    b.put(4, TeamSide::HOME, {18, 6});
    b.put(5, TeamSide::HOME, {18, 8});
    b.put(14, TeamSide::AWAY, {3, 1});
    b.s.ball = BallState::carried({17, 7}, 1);
    return b;
}
}  // namespace

TEST(OneCageScoringRange, CageKeepsAdvancingWhenTheCarrierCannotWalkInWithoutDice) {
    Board b = scoringRangeBoard(4);
    ASSERT_EQ(classifyTurnGoal(b.s), TurnGoal::SCORE_BALL) << "předpoklad testu: tah je označený jako skórovací";
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    std::vector<int> order;
    while (cc.next(b.s, m)) {
        order.push_back(m.playerId);
        play(b.s, m);
    }
    ASSERT_FALSE(order.empty()) << "řadič klece tah nepřenechal hledání";
    EXPECT_EQ(order.back(), 1) << "nosič poslední";
    EXPECT_GT(b.s.getPlayer(1).position.x, 17) << "klec postoupila";
    EXPECT_LT(b.s.getPlayer(1).position.x, 25) << "a neskórovala přes GFI";
}

TEST(OneCageScoringRange, LastTurnOfTheHalfIsLeftToTheSearch) {
    Board b = scoringRangeBoard(8);          // poslední kolo: skórovat se musí zkusit
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    EXPECT_FALSE(cc.next(b.s, m));
}

TEST(OneCageScoringRange, CarrierWhoWalksInWithoutDiceIsLeftToTheSearch) {
    Board b = scoringRangeBoard(4);
    b.s.getPlayer(1).position = {20, 7};     // 5 polí, MA6 ⇒ dojde bez hodu
    b.s.ball = BallState::carried({20, 7}, 1);
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    EXPECT_FALSE(cc.next(b.s, m));
}

// Hledání chce tah ukončit, nosič už popošel a rohy zůstaly stát ⇒ řadič je dotáhne.
TEST(OneCageBeforeEndTurn, CornersFollowACarrierWhoAlreadyMoved) {
    Board b(3);
    b.put(1, TeamSide::HOME, {12, 7}, 6);
    b.put(2, TeamSide::HOME, {9, 6});
    b.put(3, TeamSide::HOME, {9, 8});
    b.put(4, TeamSide::HOME, {11, 6});
    b.put(5, TeamSide::HOME, {11, 8});
    b.put(14, TeamSide::AWAY, {24, 13});
    b.s.ball = BallState::carried({12, 7}, 1);
    b.s.getPlayer(1).hasMoved = true;        // nosič odešel o dvě pole z klece kolem (10,7)
    b.s.getPlayer(1).movementRemaining = 4;

    CageController cc(nullptr, cfg(), 1);
    Macro m;
    ASSERT_TRUE(cc.beforeEndTurn(b.s, m));
    int moves = 0;
    do {
        EXPECT_NE(m.playerId, 1) << "nosič se podruhé nehýbe";
        play(b.s, m);
        ++moves;
    } while (cc.next(b.s, m));
    EXPECT_GE(moves, 2);
    int corners = 0;
    for (int dx : {-1, 1}) for (int dy : {-1, 1}) {
        const Player* p = b.s.getPlayerAtPosition({static_cast<int8_t>(12 + dx), static_cast<int8_t>(7 + dy)});
        if (p && p->teamSide == TeamSide::HOME) ++corners;
    }
    EXPECT_EQ(corners, 4) << "rohy stojí kolem nosiče";
    EXPECT_FALSE(cc.beforeEndTurn(b.s, m)) << "jen jednou za tah";
}
