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
    // P176 (08.10.2026): jediný soupeř stojí daleko ⇒ pád na GFI by byl levný a nosič přidá dvě GFI
    EXPECT_EQ(d.x, 11) << "6 polí vpřed; GFI se bere jen jako útěk z dosahu soupeře, a tady na něj nikdo nedosáhne";
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

// P154 (07.10.2026): po zvednutí jde nosič jen tak daleko, aby za ním rohy došly — čtyři hráči
// s MA 4 dosáhnou na pole rohů nejdál kolem x=9; bez ohledu na klec by nosič doběhl na x=11.
TEST(OneCageForward, AfterPickupStopsWhereFourCornersCanStillReachHim) {
    auto board = [](Position opponent) {
        Board b;
        b.put(1, TeamSide::HOME, {5, 7}, 6);
        b.put(2, TeamSide::HOME, {6, 5});
        b.put(3, TeamSide::HOME, {6, 9});
        b.put(4, TeamSide::HOME, {4, 5});
        b.put(5, TeamSide::HOME, {4, 9});
        b.put(12, TeamSide::AWAY, opponent, 6);
        return b;
    };
    {   // soupeř v dosahu (MA 6 + 2 GFI + rána): bezpečněji je v kleci ⇒ jen kam dojdou čtyři rohy
        Board b = board({18, 7});
        const Player& c = b.s.getPlayer(1);
        EXPECT_EQ(farthestSafeForward(b.s, c, 6).x, 11) << "pozitivní kontrola: bez ohledu na klec co nejdál";
        const Position d = farthestSafeForward(b.s, c, 6, /*forCage=*/true);
        EXPECT_EQ(d.x, 9);
        EXPECT_EQ(d.y, 7);
    }
    {   // P173 (uživatel 08.10.2026: „útěk daleko“; „musí alespoň hlídat, že k němu nikdo ze soupeřů
        // nedojde v příštím kole“): soupeř na nosiče nedosáhne nikde ⇒ útěk je stejně bezpečný
        // jako klec a nosič jde co nejdál — stejné pravidlo pro každou rasu.
        Board b = board({24, 1});
        const Player& c = b.s.getPlayer(1);
        EXPECT_EQ(farthestSafeForward(b.s, c, 6, /*forCage=*/true).x, 11);
    }
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
    // souseda dostane každý soupeř, který na nosiče ranou dosáhne (pohyb + 2 GFI, pole stojí rána).
    // Od 08.10. nosič vybíhá tam, kam jich dosáhne nejméně — proto se počítá z desky, ne napevno.
    std::vector<int> reach;
    const Position cp = b.s.getPlayer(1).position;
    for (int id : {12, 13})
        if (b.s.getPlayer(id).position.distanceTo(cp) - 1 <= blitzReachOf(b.s.getPlayer(id)) ||
            std::find(marked.begin(), marked.end(), id) != marked.end()) reach.push_back(id);
    ASSERT_FALSE(marked.empty()) << "pozitivní kontrola: aspoň jeden soupeř na nosiče dosáhne";
    EXPECT_EQ(marked, reach) << "každý, kdo by k nosiči doběhl, má souseda";
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

// Od 08.10.2026 (P175) platí: dojde bez hodu a hrozí mu rána ⇒ skóruje hned (rozhoduje hledání);
// dojde bez hodu a je v bezpečí ⇒ TD se zdržuje (OneCageStall).
TEST(OneCageScoringRange, CarrierWhoWalksInWithoutDiceIsLeftToTheSearchWhenThreatened) {
    Board b = scoringRangeBoard(4);
    b.s.getPlayer(1).position = {20, 7};     // 5 polí, MA6 ⇒ dojde bez hodu
    b.s.ball = BallState::carried({20, 7}, 1);
    b.put(18, TeamSide::AWAY, {20, 8}, 6);   // soupeř stojí hned u nosiče ⇒ hrozí rána
    ASSERT_GT(blitzThreat(b.s, b.s.getPlayer(1)), 0.05);
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    if (cc.next(b.s, m)) EXPECT_NE(m.playerId, 1) << "řadič nosiče nedrží — smí jen rány na soupeře u něj";
    EXPECT_FALSE(cc.forbidsCarrierMove(b.s, Macro{MacroType::SCORE, 1, -1, {-1, -1}})) << "skórovat smí";
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

// --- P154 a/b/c (uživatel 07.10.2026) ------------------------------------------------------

namespace {
// Klec kolem (12,7), zeď soupeřů na x=14 ⇒ postup klece nevyjde (jako CageAdvance.FillsTheCage…).
Board walledCageBoard(int turn) {
    Board b(turn);
    b.put(1, TeamSide::HOME, {12, 7}, 4);
    b.put(2, TeamSide::HOME, {11, 6});
    b.put(3, TeamSide::HOME, {11, 8});
    b.put(4, TeamSide::HOME, {13, 6});
    b.put(5, TeamSide::HOME, {13, 8});
    for (int i = 0; i < 5; ++i) b.put(13 + i, TeamSide::AWAY, {14, static_cast<int8_t>(5 + i)}, 6);
    b.s.ball = BallState::carried({12, 7}, 1);
    return b;
}
void playAll(CageController& cc, Board& b, std::vector<Macro>* out = nullptr) {
    Macro m;
    while (cc.next(b.s, m)) {
        if (out) out->push_back(m);
        play(b.s, m);
    }
}
}  // namespace

// (a) „uvolnit rohového blokem nebo blitzem — nebo jej nahradit volným“
TEST(OneCageFreeing, SafeBlockFreesATeammateHeldByOneOpponent) {
    auto board = [](bool attackerHasBlock) {
        Board b(2);
        b.put(1, TeamSide::HOME, {12, 7}, 4);
        b.put(2, TeamSide::HOME, {11, 6});
        b.put(3, TeamSide::HOME, {11, 8});
        b.put(4, TeamSide::HOME, {14, 5});                       // drží ho jen soupeř 13
        std::vector<SkillName> sk;
        if (attackerHasBlock) sk.push_back(SkillName::Block);
        b.put(5, TeamSide::HOME, {16, 4}, 4, 3, sk);             // stojí u téhož soupeře z druhé strany
        b.put(13, TeamSide::AWAY, {15, 5}, 6);
        b.put(14, TeamSide::AWAY, {24, 13}, 6);
        b.s.ball = BallState::carried({12, 7}, 1);
        return b;
    };
    {
        Board b = board(true);
        CageController cc(nullptr, cfg(), 1);
        Macro m;
        ASSERT_TRUE(cc.next(b.s, m));
        EXPECT_EQ(m.type, MacroType::BLOCK) << "tah klece začíná uvolňovací ranou";
        EXPECT_EQ(m.playerId, 5) << "blokuje hráč s Block, který rohem nebude";
        EXPECT_EQ(m.targetId, 13);
    }
    {   // bez dovednosti Block rána bezpečná není ⇒ řadič ji neplánuje
        Board b = board(false);
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        for (const Macro& m : ms) EXPECT_NE(m.type, MacroType::BLOCK);
    }
}

// Uživatel 07.10.2026: „zkus dát všechny hráče na klec a na konci kdyžtak provést i blitz — blitz
// je sice jeden za kolo, ale bezpečí nosiče je důležitější než pravidlo využít blitz každé kolo“.
// Má-li klec čtyři rohy bez ran, řadič žádnou uvolňovací ránu ani blitz nehraje (zbydou hledání
// na konec tahu). Pozitivní kontrola: tatáž pozice bez dvou předních rohů ránou začíná
// (OneCageFreeing.SafeBlockFreesATeammateHeldByOneOpponent).
TEST(OneCageOrder, CageComesFirstAndNoFreeingHitsWhenFourCornersStand) {
    Board b(2);
    b.put(1, TeamSide::HOME, {12, 7}, 4);
    b.put(2, TeamSide::HOME, {11, 6});
    b.put(3, TeamSide::HOME, {11, 8});
    b.put(6, TeamSide::HOME, {13, 6});
    b.put(7, TeamSide::HOME, {13, 8});
    b.put(4, TeamSide::HOME, {14, 4});                           // drží ho jen soupeř 13
    b.put(5, TeamSide::HOME, {16, 3}, 4, 3, {SkillName::Block}); // bezpečná rána by byla po ruce
    b.put(13, TeamSide::AWAY, {15, 4}, 6);
    b.put(14, TeamSide::AWAY, {24, 13}, 6);
    b.s.ball = BallState::carried({12, 7}, 1);
    CageController cc(nullptr, cfg(), 1);
    std::vector<Macro> ms;
    playAll(cc, b, &ms);
    for (const Macro& m : ms) {
        EXPECT_NE(m.type, MacroType::BLOCK) << "hráč " << m.playerId;
        EXPECT_NE(m.type, MacroType::BLITZ) << "hráč " << m.playerId;
    }
    const Position cp = b.s.getPlayer(1).position;
    int corners = 0;
    for (int cx : {-1, 1}) for (int cy : {-1, 1}) {
        const Player* q = b.s.getPlayerAtPosition({static_cast<int8_t>(cp.x + cx), static_cast<int8_t>(cp.y + cy)});
        if (q && q->teamSide == TeamSide::HOME && q->state == PlayerState::STANDING) ++corners;
    }
    EXPECT_EQ(corners, 4) << "po tahu řadiče stojí čistá klec";
}

// P169 krok 3 (07.10.2026): nosič bez klece nejde „kamkoli dopředu“ (to dělalo hledání), ale na
// pole, kolem kterého se ještě v tomto tahu postaví klec — a hledání jím pak už nepohne.
// Nosič MA 6 stojí sám na (5,7), čtyři spoluhráči MA 4 stojí kolem x=12–13: na rohy u nosiče
// nedosáhnou, na rohy kolem (11,7) ano.
TEST(OneCageJoin, CarrierWithoutACageGoesWhereTheCageCanForm) {
    Board b(2);
    b.put(1, TeamSide::HOME, {5, 7}, 6);
    b.put(2, TeamSide::HOME, {12, 5});
    b.put(3, TeamSide::HOME, {12, 9});
    b.put(4, TeamSide::HOME, {13, 6});
    b.put(5, TeamSide::HOME, {13, 8});
    b.put(13, TeamSide::AWAY, {24, 13}, 6);
    b.s.ball = BallState::carried({5, 7}, 1);
    CageController cc(nullptr, cfg(), 1);
    {   // pozitivní kontrola fixture: na místě klec postavit nejde, o kus dál ano
        const Player& c = b.s.getPlayer(1);
        ASSERT_EQ(cornersWithinReach(b.s, c, c.position), 0);
        ASSERT_EQ(cornersWithinReach(b.s, c, {11, 7}), 4);
    }
    playAll(cc, b);
    const Position cp = b.s.getPlayer(1).position;
    int corners = 0;
    for (int cx : {-1, 1}) for (int cy : {-1, 1}) {
        const Player* q = b.s.getPlayerAtPosition({static_cast<int8_t>(cp.x + cx), static_cast<int8_t>(cp.y + cy)});
        if (q && q->teamSide == TeamSide::HOME && q->state == PlayerState::STANDING) ++corners;
    }
    EXPECT_GT(cp.x, 5) << "nosič se pohnul ke spoluhráčům";
    EXPECT_EQ(corners, 4) << "a kolem něj stojí klec";
}

// (b) „nosič dál jen s klecí“ + „na konci poločasu musí vyběhnout, aby stihl TD“
TEST(OneCagePin, CarrierStaysInTheCageWhenTheAdvanceFails) {
    Board b = walledCageBoard(2);
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    EXPECT_EQ(b.s.getPlayer(1).position, (Position{12, 7})) << "řadič nosičem nepohnul";
    EXPECT_TRUE(cc.forbidsCarrierMove(b.s, Macro{MacroType::ADVANCE, 1, -1, {-1, -1}}));
    EXPECT_TRUE(cc.forbidsCarrierMove(b.s, Macro{MacroType::REPOSITION, 1, -1, {12, 3}}));
    EXPECT_FALSE(cc.forbidsCarrierMove(b.s, Macro{MacroType::SCORE, 1, -1, {-1, -1}})) << "skórovat smí";
    // P169 (07.10.2026): roh smí jen BEZPEČNOU ránu (2+ kostky, které vybíráme my, a Block) —
    // rohový hráč, který při ráně spadne, je díra v kleci. Roh 4 tu má proti zdi jednu kostku.
    EXPECT_TRUE(cc.forbidsCarrierMove(b.s, Macro{MacroType::BLOCK, 4, 13, {-1, -1}})) << "riskantní rána rohu ne";
    EXPECT_TRUE(cc.forbidsCarrierMove(b.s, Macro{MacroType::FOUL, 4, 13, {-1, -1}})) << "roh nefauluje";
    b.s.getPlayer(4).stats.strength = 7;                       // silný roh s Block: dvě kostky pro nás
    b.s.getPlayer(4).skills.add(SkillName::Block);
    ASSERT_GE(blockDiceCount(b.s, b.s.getPlayer(4), b.s.getPlayer(13)), 2);
    EXPECT_FALSE(cc.forbidsCarrierMove(b.s, Macro{MacroType::BLOCK, 4, 13, {-1, -1}})) << "bezpečná rána rohu smí";
}

TEST(OneCagePin, CarrierMayRunWhenWaitingWouldCostTheTouchdown) {
    // 13 polí do zóny, MA4: sólo po čekání potřebuje ceil((13-6)/4)+1 = 3 tahy. V 6. kole zbývají
    // po tomto tahu jen 2 ⇒ čekat nejde. Ve 2. kole zbývá 6 ⇒ čeká (test výš).
    Board b = walledCageBoard(6);
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    EXPECT_FALSE(cc.forbidsCarrierMove(b.s, Macro{MacroType::ADVANCE, 1, -1, {-1, -1}}));
}

TEST(OneCagePin, CornersAreNotRepositionedAway) {
    Board b = walledCageBoard(2);
    b.put(6, TeamSide::HOME, {8, 2});                             // volný hráč mimo klec
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    ASSERT_EQ(b.s.getPlayer(2).position, (Position{11, 6}));
    EXPECT_TRUE(cc.forbidsCarrierMove(b.s, Macro{MacroType::REPOSITION, 2, -1, {9, 3}})) << "roh zůstává rohem";
    EXPECT_FALSE(cc.forbidsCarrierMove(b.s, Macro{MacroType::REPOSITION, 2, -1, {11, 6}})) << "stát na místě smí";
    const Position p6 = b.s.getPlayer(6).position;
    EXPECT_FALSE(cc.forbidsCarrierMove(b.s, Macro{MacroType::REPOSITION, 6, -1, {static_cast<int8_t>(p6.x + 1), p6.y}}))
        << "hráče mimo klec hledání přesouvat smí";
}

// (c) „pak co nejdříve dořešit pohyb zaostalců co nejvíce dopředu“
TEST(OneCageLaggards, FreePlayerLeftBehindMovesForwardWithoutContact) {
    Board b(1);
    b.put(1, TeamSide::HOME, {12, 7});
    b.put(2, TeamSide::HOME, {11, 6});
    b.put(3, TeamSide::HOME, {11, 8});
    b.put(4, TeamSide::HOME, {13, 6});
    b.put(5, TeamSide::HOME, {13, 8});
    b.put(6, TeamSide::HOME, {6, 3});                             // zaostalec
    b.put(14, TeamSide::AWAY, {24, 13});
    b.s.ball = BallState::carried({12, 7}, 1);
    CageController cc(nullptr, cfg(), 1);
    std::vector<Macro> ms;
    playAll(cc, b, &ms);
    const Player& lag = b.s.getPlayer(6);
    EXPECT_GT(lag.position.x, 6) << "zaostalec šel dopředu";
    EXPECT_EQ(lag.position.x, 6 + 4) << "o celý svůj pohyb, bez hodu";
    EXPECT_EQ(countTacklezones(b.s, lag.position, TeamSide::HOME), 0) << "ne do kontaktu";
    EXPECT_LE(lag.position.x, b.s.getPlayer(1).position.x + 3) << "nejvýš tři sloupce před nosiče";
}

// (a) pokračování — „využij i blitz a block pro uvolnění klece“, „nezapomeň na příchod pro asistenci“
namespace {
// Náš hráč 4 na (14,5) je držen jediným soupeřem 13 na (15,5). Klec kolem (12,7) má jen zadní rohy.
Board heldTeammateBoard() {
    Board b(2);
    b.put(1, TeamSide::HOME, {12, 7}, 4);
    b.put(2, TeamSide::HOME, {11, 6});
    b.put(3, TeamSide::HOME, {11, 8});
    b.put(4, TeamSide::HOME, {14, 5});
    b.put(13, TeamSide::AWAY, {15, 5}, 6);
    b.put(15, TeamSide::AWAY, {24, 13}, 6);
    b.s.ball = BallState::carried({12, 7}, 1);
    return b;
}
}  // namespace

TEST(OneCageFreeing, FreeTeammateComesToAssistSoTheBlockHasTwoDice) {
    // P174/P177 (08.10.2026): s hledáním bezpečného pole řadič v této pozici postaví čistou klec
    // i s volným pomocníkem a rány pak nejsou potřeba („klec napřed“); test zkouší samotný
    // příchod pro asistenci, proto je hledání po dobu testu vypnuté.
    struct OldPath { OldPath() { setCageFeaturesOff(kFeatCleanCageSearch); } ~OldPath() { setCageFeaturesOff(0); } } oldPath;
    Board b = heldTeammateBoard();
    b.put(5, TeamSide::HOME, {16, 4}, 4, 3, {SkillName::Block});   // u soupeře 13 z druhé strany
    b.put(14, TeamSide::AWAY, {17, 5}, 6);                          // kryje 13: bez pomoci jen 1 kostka
    b.put(6, TeamSide::HOME, {13, 2});                              // volný pomocník
    ASSERT_LT(blockDiceCount(b.s, b.s.getPlayer(5), b.s.getPlayer(13)), 2) << "předpoklad: bez příchodu není rána bezpečná";

    CageController cc(nullptr, cfg(), 1);
    Macro first, second;
    ASSERT_TRUE(cc.next(b.s, first));
    EXPECT_EQ(first.type, MacroType::REPOSITION);
    EXPECT_EQ(first.playerId, 6) << "pro asistenci jde volný hráč, ne roh klece";
    EXPECT_EQ(first.targetPos.distanceTo(b.s.getPlayer(13).position), 1) << "na pole vedle soupeře";
    play(b.s, first);
    EXPECT_GE(blockDiceCount(b.s, b.s.getPlayer(5), b.s.getPlayer(13)), 2) << "po příchodu 2+ kostky, vybíráme my";
    ASSERT_TRUE(cc.next(b.s, second));
    EXPECT_EQ(second.type, MacroType::BLOCK);
    EXPECT_EQ(second.playerId, 5);
    EXPECT_EQ(second.targetId, 13);
}

TEST(OneCageFreeing, BlitzFreesAHeldTeammateWhenNoBlockIsAvailable) {
    Board b = heldTeammateBoard();
    b.put(5, TeamSide::HOME, {12, 3}, 4, 3, {SkillName::Block});   // volný hráč s Block, dva kroky od soupeře
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    ASSERT_TRUE(cc.next(b.s, m));
    EXPECT_EQ(m.type, MacroType::BLITZ);
    EXPECT_EQ(m.playerId, 5);
    EXPECT_EQ(m.targetId, 13);
    play(b.s, m);
    EXPECT_TRUE(b.s.getPlayer(5).usedBlitz) << "blitz zahrál právě určený hráč";
    {   // už použitý blitz v tahu ⇒ řadič ho neplánuje
        Board c = heldTeammateBoard();
        c.put(5, TeamSide::HOME, {12, 3}, 4, 3, {SkillName::Block});
        c.s.homeTeam.blitzUsedThisTurn = true;
        CageController cc2(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc2, c, &ms);
        for (const Macro& q : ms) EXPECT_NE(q.type, MacroType::BLITZ);
    }
}

// --- P154: co klec rozebíralo mezi tahem řadiče a koncem tahu (07.10.2026) ------------------

TEST(OneCageKeepsCorners, CornerDoesNotFollowUpAfterABlock) {
    auto board = [](bool carrierInside) {
        Board b(2);
        b.put(1, TeamSide::HOME, carrierInside ? Position{12, 7} : Position{3, 2}, 4);
        b.put(4, TeamSide::HOME, {13, 6}, 4, 3, {SkillName::Block});   // přední roh klece kolem (12,7)
        b.put(13, TeamSide::AWAY, {14, 6}, 6);
        b.put(14, TeamSide::AWAY, {24, 13}, 6);
        b.s.ball = BallState::carried(b.s.getPlayer(1).position, 1);
        return b;
    };
    {
        Board b = board(true);
        play(b.s, Macro{MacroType::BLOCK, 4, 13, {-1, -1}});          // šestky = POW, soupeř odtlačen
        EXPECT_NE(b.s.getPlayer(13).position, (Position{14, 6})) << "předpoklad: soupeř byl odtlačen";
        EXPECT_EQ(b.s.getPlayer(4).position, (Position{13, 6})) << "roh zůstal na rohu";
    }
    {   // pozitivní kontrola: tentýž hráč mimo klec po ráně následuje
        Board b = board(false);
        play(b.s, Macro{MacroType::BLOCK, 4, 13, {-1, -1}});
        EXPECT_EQ(b.s.getPlayer(4).position, (Position{14, 6}));
    }
}

TEST(OneCageKeepsCorners, CarrierIsNeverPickedAsTheBlitzer) {
    Board b(2);
    b.put(1, TeamSide::HOME, {12, 7}, 6, 3, {SkillName::Block});       // nosič: nejblíž a s Block
    b.put(2, TeamSide::HOME, {11, 3}, 6);                              // dál, bez Block
    b.put(13, TeamSide::AWAY, {14, 7}, 6);
    b.put(14, TeamSide::AWAY, {24, 13}, 6);
    b.s.ball = BallState::carried({12, 7}, 1);
    FixedDiceRoller dice(std::vector<int>(200, 6));
    auto r = greedyExpandMacro(b.s, Macro{MacroType::BLITZ, -1, 13, {-1, -1}}, dice);
    ASSERT_FALSE(r.actions.empty());
    EXPECT_EQ(r.actions[0].playerId, 2) << "blitzuje druhý hráč, ne nosič";
    EXPECT_EQ(b.s.getPlayer(1).position, (Position{12, 7}));
}

TEST(OneCageKeepsCorners, FrenzyCornerDoesNotBlock) {
    Board b = walledCageBoard(2);
    b.s.getPlayer(4).skills.add(SkillName::Frenzy);                   // přední roh (13,6) vedle zdi na x=14
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    ASSERT_EQ(b.s.getPlayer(4).position, (Position{13, 6}));
    EXPECT_TRUE(cc.forbidsCarrierMove(b.s, Macro{MacroType::BLOCK, 4, 13, {-1, -1}})) << "po ráně by musel následovat";
    b.s.getPlayer(5).stats.strength = 7;                              // bezpečná rána: 2+ kostky a Block
    b.s.getPlayer(5).skills.add(SkillName::Block);
    ASSERT_GE(blockDiceCount(b.s, b.s.getPlayer(5), b.s.getPlayer(17)), 2);
    EXPECT_FALSE(cc.forbidsCarrierMove(b.s, Macro{MacroType::BLOCK, 5, 17, {-1, -1}})) << "roh bez Frenzy bezpečně blokovat smí";
}

// P169 (07.10.2026): blitz, který si hráče neurčil, roh klece nevezme — ani když je jediný, kdo
// na cíl dosáhne („bezpečí nosiče je důležitější než pravidlo využít blitz každé kolo“).
TEST(OneCageKeepsCorners, UnnamedBlitzNeverTakesACageCorner) {
    Board b(2);
    b.put(1, TeamSide::HOME, {12, 7}, 4);
    b.put(2, TeamSide::HOME, {11, 6});
    b.put(3, TeamSide::HOME, {11, 8});
    b.put(4, TeamSide::HOME, {13, 6}, 6, 3, {SkillName::Block});
    b.put(5, TeamSide::HOME, {13, 8});
    b.put(13, TeamSide::AWAY, {16, 6}, 6);
    b.s.ball = BallState::carried({12, 7}, 1);
    FixedDiceRoller dice(std::vector<int>(200, 6));
    auto r = greedyExpandMacro(b.s, Macro{MacroType::BLITZ, -1, 13, {-1, -1}}, dice);
    EXPECT_TRUE(r.actions.empty()) << "žádný blitz: na cíl dosáhnou jen rohy";
    EXPECT_EQ(b.s.getPlayer(4).position, (Position{13, 6}));

    b.put(6, TeamSide::HOME, {14, 2}, 6, 3, {SkillName::Block});      // pozitivní kontrola: volný hráč blitzuje
    auto r2 = greedyExpandMacro(b.s, Macro{MacroType::BLITZ, -1, 13, {-1, -1}}, dice);
    ASSERT_FALSE(r2.actions.empty());
    EXPECT_EQ(r2.actions[0].playerId, 6);
}

// P169 krok 6 (07.10.2026): když by nosič ani s klecí nestihl TD, výběh volí řadič — bez hodu a
// ne vedle soupeře (dřív ho vedlo hledání kamkoli dopředu). Kolo 7, do zóny 13 polí, nosič MA 6:
// sólo to stihne (6 teď + 8 v posledním kole), klec s rohy MA 4 ne.
TEST(OneCageRun, WhenTimeIsShortTheControllerRunsTheCarrierDiceFreeAndAwayFromOpponents) {
    Board b(7);
    b.put(1, TeamSide::HOME, {12, 7}, 6);
    b.put(2, TeamSide::HOME, {11, 6});
    b.put(3, TeamSide::HOME, {11, 8});
    b.put(4, TeamSide::HOME, {13, 6});
    b.put(5, TeamSide::HOME, {13, 8});
    b.put(13, TeamSide::AWAY, {16, 7}, 6);
    b.s.ball = BallState::carried({12, 7}, 1);
    CageController cc(nullptr, cfg(), 1);
    std::vector<Macro> ms;
    playAll(cc, b, &ms);
    const Player& c = b.s.getPlayer(1);
    EXPECT_GT(c.position.x, 12) << "nosič vyběhl";
    EXPECT_EQ(countTacklezones(b.s, c.position, TeamSide::HOME, 1), 0) << "ne vedle soupeře";
    bool carrierByController = false;
    for (const Macro& m : ms) carrierByController |= (m.playerId == 1 && m.type == MacroType::REPOSITION);
    EXPECT_TRUE(carrierByController) << "pohyb nosiče zvolil řadič";
    EXPECT_EQ(b.s.homeTeam.rerolls, 0);
}

// P179 (uživatel 08.10.2026: „vypustíme nosiče dopředu a zkusíme dát souseda všem protihráčům, co
// by na něj dosáhli — nejlépe ve směru k nosiči, ať zavazí co nejvíce“). Spoluhráč postavený vedle
// soupeřem jen „kde je to nejblíž“ nosiči nepomůže. Markující hráč si má stoupnout mezi soupeře
// a nosiče (deska z testu OneCageRelease: nosič vyběhne, dva soupeři na něj dosáhnou).
TEST(OneCageRun, TheMarkerStandsBetweenTheOpponentAndTheCarrier) {
    // vrací: kolik markerů stojí blíž nosiči než jejich soupeř / kolik markerů celkem
    auto run = [](unsigned off) {
        Board b = releaseBoard(6);
        b.s.getPlayer(6).position = {22, 1};     // volní hráči stojí ZA soupeři: nejbližší pole
        b.s.getPlayer(7).position = {22, 13};    // u soupeře je za jeho zády
        setCageFeaturesOff(off);
        CageController cc(nullptr, cfg(), 1);
        Macro m;
        int between = 0, all = 0;
        while (cc.next(b.s, m)) {
            if (m.playerId != 1) {
                const Position c = b.s.getPlayer(1).position;
                for (int id : {12, 13}) {
                    const Position o = b.s.getPlayer(id).position;
                    if (o.distanceTo(m.targetPos) != 1) continue;
                    ++all;
                    between += m.targetPos.distanceTo(c) < o.distanceTo(c);
                    break;
                }
            }
            play(b.s, m);
        }
        setCageFeaturesOff(0);
        return std::make_pair(between, all);
    };
    const auto on = run(0);
    ASSERT_GE(on.second, 1) << "aspoň jeden soupeř na nosiče dosáhne a dostal souseda";
    EXPECT_EQ(on.first, on.second) << "každý marker stojí mezi soupeřem a nosičem";
    const auto off = run(kFeatMarkerToward);
    ASSERT_GE(off.second, 1);
    EXPECT_LT(off.first, off.second) << "pozitivní kontrola: bez úpravy aspoň jeden stál jinde";
}

// P131 / P169 krok 7 (uživatel 07.10.2026: „nosič stál vedle okraje hřiště a byl vysurfován — a
// při Frenzy i z pole vedle okraje“): nosič končí aspoň dvě pole od postranní čáry.
TEST(OneCageForward, RunNeverEndsWithinTwoRowsOfTheSideline) {
    Board b;
    Player& c = b.put(1, TeamSide::HOME, {5, 3}, 6);
    // soupeři uprostřed tlačí běh ke kraji: volná pole nejdál vpředu jsou na řádcích 0 a 1
    for (int y = 3; y <= 11; y += 2) b.put(12 + y, TeamSide::AWAY, {9, static_cast<int8_t>(y)});
    const Position d = farthestSafeForward(b.s, c, 6);
    EXPECT_GE(d.y, 2) << "(" << int(d.x) << "," << int(d.y) << ")";
    EXPECT_LE(d.y, 12);

    Board e;                                       // nosič u kraje smí jen ke středu
    Player& c2 = e.put(1, TeamSide::HOME, {5, 0}, 6);
    e.put(13, TeamSide::AWAY, {24, 13});
    const Position d2 = farthestSafeForward(e.s, c2, 6);
    EXPECT_GE(d2.y, 1);
    EXPECT_GT(d2.x, 5);
}

// P169 krok 8 (07.10.2026): soupeře u nosiče blokuje ten, kdo ho odtlačí PRYČ od nosiče. Soupeř
// stojí před nosičem na (13,7). Hráč 2 za ním na (14,7) by ho tlačil k nosiči (všechna tři pole
// odtlačení s nosičem sousedí nebo jsou obsazená), hráč 3 na (12,6) ho tlačí šikmo pryč.
TEST(OneCageFreeing, TheBlockerWhoPushesTheMarkerAwayFromTheCarrierIsChosen) {
    Board b(2);
    b.put(1, TeamSide::HOME, {12, 7}, 4);
    b.put(2, TeamSide::HOME, {14, 7}, 4, 4, {SkillName::Block});   // silnější a s Block, ale tlačí k nosiči
    b.put(3, TeamSide::HOME, {12, 6}, 4, 3);
    b.put(13, TeamSide::AWAY, {13, 7}, 6);
    b.put(14, TeamSide::AWAY, {24, 13}, 6);
    b.s.ball = BallState::carried({12, 7}, 1);
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    ASSERT_TRUE(cc.next(b.s, m));
    ASSERT_EQ(m.type, MacroType::BLOCK);
    EXPECT_EQ(m.targetId, 13);
    EXPECT_EQ(m.playerId, 3) << "blokuje ten, po jehož ráně soupeř u nosiče nezůstane";
}

// P169 krok 9 (07.10.2026): míč na začátku tahu nikdo náš nedržel a řadič zvednutí nenabídl;
// zvedlo ho až hledání (tady nasimulováno). Řadič pak hned dostaví rohy kolem nosiče.
TEST(OneCageLatePickup, CornersAreBuiltRightAfterAPickupMadeByTheSearch) {
    Board b(3);
    b.put(1, TeamSide::HOME, {12, 7}, 4);
    b.put(2, TeamSide::HOME, {10, 5});
    b.put(3, TeamSide::HOME, {10, 9});
    b.put(4, TeamSide::HOME, {14, 5});
    b.put(5, TeamSide::HOME, {14, 9});
    b.put(13, TeamSide::AWAY, {24, 13}, 6);
    b.s.ball = BallState::onGround({25, 0});                  // míč mimo dosah všech: zvednutí se nenabízí
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    ASSERT_FALSE(cc.next(b.s, m)) << "pozitivní kontrola: bez míče řadič nic nehraje";

    b.s.ball = BallState::carried({12, 7}, 1);                // „hledání“ míč zvedlo hráčem 1
    b.s.getPlayer(1).hasMoved = true;
    std::vector<Macro> ms;
    playAll(cc, b, &ms);
    int corners = 0;
    for (int cx : {-1, 1}) for (int cy : {-1, 1}) {
        const Player* q = b.s.getPlayerAtPosition({static_cast<int8_t>(12 + cx), static_cast<int8_t>(7 + cy)});
        if (q && q->teamSide == TeamSide::HOME && q->state == PlayerState::STANDING) ++corners;
    }
    EXPECT_EQ(corners, 4) << "řadič po zvednutí dostavěl klec (maker " << ms.size() << ")";
}

// P172 — první fáze celotahu (uživatel 07.10.2026: „první bude klec — kde bude v obsahu i blitz pro
// proboření obrany nebo blocky na uvolnění klece — pak se provede celý pohyb klece s nosičem“).
// Soupeř stojí kleci v cestě na (15,7): bez rány klec ujde jedno pole (dál by nosič končil v jeho
// zóně). Volný hráč s Block a silou na dvě kostky ho blitzem shodí a klec pak jde dál.
TEST(OneCagePhaseOne, BlitzBreaksTheWayBeforeTheCageMoves) {
    // P174 (08.10.2026): s hledáním bezpečného pole by klec soupeře v cestě obešla šikmo a blitz
    // by potřeba nebyl; test zkouší samotný blitz na proboření, proto je hledání vypnuté.
    struct OldPath { OldPath() { setCageFeaturesOff(kFeatCleanCageSearch); } ~OldPath() { setCageFeaturesOff(0); } } oldPath;
    auto board = [](bool withBlitzer) {
        Board b(2);
        b.put(1, TeamSide::HOME, {12, 7}, 4);
        b.put(2, TeamSide::HOME, {11, 6});
        b.put(3, TeamSide::HOME, {11, 8});
        b.put(4, TeamSide::HOME, {13, 6});
        b.put(5, TeamSide::HOME, {13, 8});
        if (withBlitzer) b.put(6, TeamSide::HOME, {13, 3}, 6, 4, {SkillName::Block});
        b.put(13, TeamSide::AWAY, {15, 7}, 6);
        b.put(14, TeamSide::AWAY, {24, 13}, 6);
        b.s.ball = BallState::carried({12, 7}, 1);
        return b;
    };
    {
        Board b = board(true);
        CageController cc(nullptr, cfg(), 1);
        Macro m;
        ASSERT_TRUE(cc.next(b.s, m));
        EXPECT_EQ(m.type, MacroType::BLITZ) << "tah klece začíná blitzem na soupeře v cestě";
        EXPECT_EQ(m.playerId, 6);
        EXPECT_EQ(m.targetId, 13);
    }
    {   // pozitivní kontrola: bez hráče, který by bezpečně blitzoval, řadič blitz nehraje
        Board b = board(false);
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        for (const Macro& m : ms) EXPECT_NE(m.type, MacroType::BLITZ);
    }
}

// JEDNO MĚŘÍTKO BEZPEČÍ NOSIČE (sjednoceno 08.10.2026; uživatel: „klec má být univerzální“, „pokud …
// uteče nosič sám — musí alespoň hlídat, že k němu nikdo ze soupeřů nedojde v příštím kole“,
// „pokud má klec dva nebo tři rohy tak, ať soupeř nedosáhne na nosiče — tak je to také validní“).
// Hrozba rány na nosiče na poli, kam by došel, s rohy, které tam v tomto tahu ještě dojdou.
TEST(CarrierThreat, OutOfReachIsZeroAndTeammatesWhoWillComeLowerIt) {
    auto board = [](bool mates) {
        Board b;
        b.put(1, TeamSide::HOME, {5, 7}, 6);
        if (mates) {
            b.put(2, TeamSide::HOME, {9, 5}); b.put(3, TeamSide::HOME, {9, 9});
            b.put(4, TeamSide::HOME, {11, 4}); b.put(5, TeamSide::HOME, {11, 10});
        }
        b.put(12, TeamSide::AWAY, {20, 7}, 6);       // dosah rány: 6 + 2 GFI − 1 = 7 polí cesty
        return b;
    };
    Board lone = board(false);
    const Player& c = lone.s.getPlayer(1);
    EXPECT_DOUBLE_EQ(carrierThreatAt(lone.s, c, {5, 7}), 0.0) << "nikdo nedosáhne ⇒ útěk je stejně bezpečný jako klec";
    const double alone = carrierThreatAt(lone.s, c, {12, 7});
    EXPECT_GT(alone, kSafeBlitzThreat) << "v dosahu a sám: dobrá rána";
    Board caged = board(true);
    ASSERT_EQ(cornersWithinReach(caged.s, caged.s.getPlayer(1), {12, 7}), 4) << "předpoklad: čtyři rohy tam dojdou";
    EXPECT_LT(carrierThreatAt(caged.s, caged.s.getPlayer(1), {12, 7}), alone) << "s rohy, které tam dojdou, je hrozba menší";
}

// Review 08.10.2026: stará tabulka ležícího soupeře nepočítala — nosič „utekl z dosahu“ na pole
// dvě pole od ležícího soupeře, který vstane (3 pole pohybu) a udeří. Dosah je jeden pro všechno.
TEST(CarrierThreat, AProneOpponentWhoCanStandUpAndHitCounts) {
    Board b;
    b.put(1, TeamSide::HOME, {5, 7}, 6);
    Player& o = b.put(12, TeamSide::AWAY, {8, 7}, 6);
    o.state = PlayerState::PRONE;
    EXPECT_TRUE(anyOpponentReaches(b.s, TeamSide::HOME, {5, 7}));
    EXPECT_GT(carrierThreatAt(b.s, b.s.getPlayer(1), {5, 7}), 0.0);
    o.stats.movement = 2;                             // vstává na hod 4+ ⇒ nepočítá se
    EXPECT_FALSE(anyOpponentReaches(b.s, TeamSide::HOME, {5, 7}));
    EXPECT_DOUBLE_EQ(carrierThreatAt(b.s, b.s.getPlayer(1), {5, 7}), 0.0);
}

// P175 (uživatel 08.10.2026: „obecně chci, ať s TD zdržujeme za všechny, ale jen v případě, kdy máme
// balon bezpečně v držení a nehrozí blitz na nosiče — na druhou stranu pokud hrozí blitz na nosiče
// a ztráta, je lepší dát TD dříve — toto je obojí obecné pravidlo“).
// Nosič stojí tři pole před zónou a dojde bez hodu.
TEST(OneCageStall, HoldsTheBallWhenSafeAndScoresWhenThreatenedOrOnTheLastTurn) {
    auto board = [](int turn, Position opponent) {
        Board b(turn);
        b.put(1, TeamSide::HOME, {22, 7}, 6);
        b.put(2, TeamSide::HOME, {20, 5});
        b.put(13, TeamSide::AWAY, opponent, 6);
        b.s.ball = BallState::carried({22, 7}, 1);
        return b;
    };
    const Macro score{MacroType::SCORE, 1, -1, {-1, -1}};
    {   // 5. kolo, soupeř daleko (dosah 6 + 2 + rána nestačí): TD se zdržuje
        Board b = board(5, {3, 7});
        ASSERT_DOUBLE_EQ(blitzThreat(b.s, b.s.getPlayer(1)), 0.0);
        CageController cc(nullptr, cfg(), 1);
        playAll(cc, b);
        EXPECT_EQ(b.s.getPlayer(1).position, (Position{22, 7})) << "nosič stojí";
        EXPECT_TRUE(cc.forbidsCarrierMove(b.s, score)) << "skórovat se v tomto tahu nesmí";
        EXPECT_TRUE(cc.forbidsCarrierMove(b.s, Macro{MacroType::ADVANCE, 1, -1, {-1, -1}}));
    }
    {   // 5. kolo, soupeř na nosiče dosáhne s dobrou ranou: skóruje se hned
        Board b = board(5, {18, 9});
        ASSERT_GT(blitzThreat(b.s, b.s.getPlayer(1)), 0.05);
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        // P178 (uživatel 08.10.: „tam má být kontrola, ať raději skórují, než zůstat jako cíl pro
        // blitz“): TD přikáže řadič sám, nenechává ho na volbě hledání.
        ASSERT_FALSE(ms.empty());
        EXPECT_EQ(ms.front().type, MacroType::SCORE) << "hrozí blitz ⇒ TD hned, příkazem řadiče";
        EXPECT_EQ(b.s.getPlayer(1).position.x, 25) << "nosič je v zóně";
    }
    {   // 8. kolo: skóruje se, i když je nosič v bezpečí
        Board b = board(8, {3, 7});
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        ASSERT_FALSE(ms.empty());
        EXPECT_EQ(ms.front().type, MacroType::SCORE) << "poslední kolo poločasu";
    }
    {   // pojistka měření: s vypnutou úpravou řadič TD nepřikazuje (jako do 08.10.)
        setCageFeaturesOff(kFeatForceScore);
        Board b = board(5, {18, 9});
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        setCageFeaturesOff(0);
        for (const Macro& m : ms) EXPECT_NE(m.type, MacroType::SCORE);
    }
}

// P175, druhá polovina (uživatel 08.10.2026: „pokud hrozí blitz na nosiče a ztráta — je lepší dát TD
// dříve“). Nosič MA 6 stojí 8 polí před zónou: TD jen přes dvě GFI (vyjde v 69 %). Osamělý nosič se
// soupeřem v dosahu (rána na jednu kostku, míč přežije v 67 %) má běžet pro TD; nosič v čisté kleci
// (míč přežije skoro jistě) ne — tam vede klec.
TEST(OneCageStall, ThreatenedCarrierGoesForTheTouchdownThroughDiceACagedOneDoesNot) {
    auto board = [](bool caged) {
        Board b(5);
        b.put(1, TeamSide::HOME, {17, 7}, 6);
        if (caged) {
            b.put(2, TeamSide::HOME, {16, 6}); b.put(3, TeamSide::HOME, {16, 8});
            b.put(4, TeamSide::HOME, {18, 6}); b.put(5, TeamSide::HOME, {18, 8});
        }
        b.put(13, TeamSide::AWAY, {13, 7}, 6);
        b.s.ball = BallState::carried({17, 7}, 1);
        return b;
    };
    {
        Board b = board(false);
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        ASSERT_FALSE(ms.empty());
        EXPECT_EQ(ms.front().type, MacroType::SCORE) << "P178: řadič nosiče nedrží v kleci a TD přikáže sám";
        EXPECT_EQ(ms.front().playerId, 1);
    }
    {
        Board b = board(true);
        ASSERT_LT(blitzThreat(b.s, b.s.getPlayer(1)), 0.31) << "pozitivní kontrola: v kleci je hrozba pod 1 − 0,69";
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        // Review 08.10.: dřív stačilo „nosič dostal nějaké makro“ — prošlo by i přikázané TD přes hody.
        for (const Macro& m : ms) EXPECT_NE(m.type, MacroType::SCORE) << "nosič v kleci TD přes hody nedostane";
        EXPECT_LT(b.s.getPlayer(1).position.x, 25) << "klec vede, nosič neběží přes hody";
    }
}

// Review 08.10.2026 k P175/P178: hrozba se má brát PO našem tahu. Soupeř nosiči odtlačil dva rohy
// (na začátku tahu je hrozba velká), ale spoluhráči stojí o pole vedle a klec tento tah dostaví ⇒
// přes dvě GFI se neběží. Pozitivní kontrola: stejný nosič a soupeř bez spoluhráčů ⇒ TD hned.
TEST(OneCageStall, ThreatIsJudgedAfterTheCageIsRebuiltNotAsTheOpponentLeftIt) {
    auto board = [](bool matesNearby) {
        Board b(5);
        b.put(1, TeamSide::HOME, {17, 7}, 6);
        if (matesNearby) {
            b.put(2, TeamSide::HOME, {16, 6}); b.put(3, TeamSide::HOME, {16, 8});
            b.put(4, TeamSide::HOME, {18, 4}); b.put(5, TeamSide::HOME, {18, 10});
        }
        b.put(13, TeamSide::AWAY, {22, 7}, 6);
        b.s.ball = BallState::carried({17, 7}, 1);
        return b;
    };
    {
        Board b = board(true);
        ASSERT_GT(blitzThreat(b.s, b.s.getPlayer(1)), 0.31) << "předpoklad: před tahem je nosič vystaven dobré ráně";
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        for (const Macro& m : ms) EXPECT_NE(m.type, MacroType::SCORE) << "klec jde dostavět ⇒ žádné TD přes hody";
        EXPECT_LT(b.s.getPlayer(1).position.x, 25);
        EXPECT_LT(blitzThreat(b.s, b.s.getPlayer(1)), 0.31) << "po tahu je míč bezpečnější než TD přes dvě GFI";
    }
    {
        Board b = board(false);
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        ASSERT_FALSE(ms.empty());
        EXPECT_EQ(ms.front().type, MacroType::SCORE) << "klec dostavět nejde ⇒ TD hned";
    }
}

// Review 08.10.2026 k P178: přikázané TD jde na to pole zóny, podle kterého řadič rozhodl. Pole
// zóny v řádcích 5–9 jsou obsazená; bez hodu se dá dojít jen o tři řádky vedle. Makro SCORE si dřív
// vybíralo samo jen z řádků ±2 a obsazenost nekontrolovalo.
TEST(OneCageStall, TheOrderedTouchdownGoesToTheSquareTheControllerFound) {
    Board b(5);
    b.put(1, TeamSide::HOME, {22, 7}, 6);
    for (int y = 5; y <= 9; ++y) b.put(y - 3, TeamSide::HOME, {25, static_cast<int8_t>(y)});
    b.put(13, TeamSide::AWAY, {18, 9}, 6);
    b.s.ball = BallState::carried({22, 7}, 1);
    ASSERT_GT(blitzThreat(b.s, b.s.getPlayer(1)), 0.05);
    CageController cc(nullptr, cfg(), 1);
    std::vector<Macro> ms;
    playAll(cc, b, &ms);
    ASSERT_FALSE(ms.empty());
    ASSERT_EQ(ms.front().type, MacroType::SCORE);
    EXPECT_EQ(b.s.homeTeam.score, 1) << "nosič do zóny došel";
    EXPECT_EQ(b.s.homeTeam.rerolls, 0);
}

// Review 08.10.2026 k P175: ve zdržovacím tahu nosič stojí i tehdy, když hledání tah ukončí a řadič
// dostane slovo ještě jednou (beforeEndTurn) — dřív tam volal postup klece i s nosičem.
TEST(OneCageStall, TheStallingCarrierAlsoStandsWhenTheControllerIsAskedBeforeTheEndOfTurn) {
    Board b(5);
    b.put(1, TeamSide::HOME, {22, 7}, 6);
    b.put(2, TeamSide::HOME, {20, 5});
    b.put(3, TeamSide::HOME, {19, 6});
    b.put(4, TeamSide::HOME, {19, 8});
    b.put(5, TeamSide::HOME, {20, 9});
    b.put(13, TeamSide::AWAY, {3, 7}, 6);
    b.s.ball = BallState::carried({22, 7}, 1);
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    ASSERT_TRUE(cc.forbidsCarrierMove(b.s, Macro{MacroType::SCORE, 1, -1, {-1, -1}})) << "předpoklad: zdržuje se";
    Macro m;
    if (cc.beforeEndTurn(b.s, m)) {
        play(b.s, m);
        playAll(cc, b);
    }
    EXPECT_EQ(b.s.getPlayer(1).position, (Position{22, 7})) << "nosič stojí";
}

// P176 — hodnota rizika (uživatel 08.10.2026: „když skaven upadne na GFI daleko ode všech soupeřů
// a nezraní se — je to relativně bezpečnější“).
TEST(FallValue, AFallFarFromOpponentsIsCheapAndTheCarrierMayRushThereButNotNextToThem) {
    {   // změřená čísla
        Board b;
        b.put(1, TeamSide::HOME, {5, 7}, 6);
        b.put(13, TeamSide::AWAY, {7, 7}, 6);
        EXPECT_DOUBLE_EQ(looseBallLossRisk(b.s, TeamSide::HOME, {5, 7}), 0.56);
        EXPECT_DOUBLE_EQ(looseBallLossRisk(b.s, TeamSide::HOME, {11, 7}), 0.38);
        EXPECT_DOUBLE_EQ(looseBallLossRisk(b.s, TeamSide::HOME, {14, 7}), 0.14);
    }
    {   // GFI jen jako ÚTĚK Z DOSAHU: soupeř (MA 4, dosah rány 5 polí cesty) stojí za nosičem na (0,7). Svým
        // pohybem dojde nosič na x=11 — tam na něj soupeř nedosáhne (11 polí) ⇒ GFI netřeba.
        Board b;
        Player& c = b.put(1, TeamSide::HOME, {5, 7}, 6);
        b.put(13, TeamSide::AWAY, {0, 7}, 4);
        EXPECT_EQ(farthestSafeForward(b.s, c, 6, /*forCage=*/true).x, 11) << "mimo dosah už bez hodu ⇒ žádné GFI";
    }
    {   // soupeř MA 9 (9 + 2 GFI − 1 pole za ránu = 10 polí cesty) na (0,7): na x=11 by na nosiče
        // právě dosáhl, na x=12 už ne ⇒ jedno levné GFI (pád daleko od soupeře) ho z dosahu dostane
        Board b;
        Player& c = b.put(1, TeamSide::HOME, {5, 7}, 6);
        b.put(13, TeamSide::AWAY, {0, 7}, 9);
        EXPECT_EQ(farthestSafeForward(b.s, c, 6, /*forCage=*/true).x, 12) << "GFI jako útěk z dosahu";
    }
    {   // soupeři stojí kousek od místa, kam by GFI vedlo: pád by byl drahý ⇒ jen vlastním pohybem
        Board b;
        Player& c = b.put(1, TeamSide::HOME, {5, 7}, 6);
        b.put(13, TeamSide::AWAY, {15, 5}, 4);
        b.put(14, TeamSide::AWAY, {15, 9}, 4);
        const Position d = farthestSafeForward(b.s, c, 6);
        EXPECT_LE(d.x, 11) << "u soupeře se GFI neriskuje";
    }
}

// P178 (uživatel 08.10.2026: „trpaslíci jsou pomalí a špatně zvedají míč — u nich je cena za pád při
// GFI vysoká … pomalým týmům a týmům s malou agilitou zvedni cenu GFI, ať to nedělají“). Stejná
// pozice (soupeř daleko), jiný nosič a jiný tým: cena pádu vyjde z pohybu a obratnosti, ne z rasy.
TEST(FallValue, AFallCostsASlowClumsyTeamFarMoreThanAFastAgileOne) {
    auto cost = [](int8_t ma, int8_t ag, int turn) {
        Board b(turn);
        Player& c = b.put(1, TeamSide::HOME, {8, 7}, ma);
        c.stats.agility = ag;
        for (int i = 0; i < 4; ++i) b.put(2 + i, TeamSide::HOME, {static_cast<int8_t>(7 + (i % 2) * 2), static_cast<int8_t>(6 + (i / 2) * 2)}, ma);
        b.put(13, TeamSide::AWAY, {24, 1}, 6);
        b.s.ball = BallState::carried({8, 7}, 1);
        return carrierFallCost(b.s, b.s.getPlayer(1), {12, 7});
    };
    const double slowClumsy = cost(4, 2, 2);     // pohyb 4, obratnost 2: bez časové rezervy, zvedá na 4+
    const double fastAgile = cost(9, 4, 2);      // pohyb 9, obratnost 4: rezerva, zvedá na 2+
    EXPECT_GT(slowClumsy, 0.6);
    EXPECT_LT(fastAgile, 0.3);
    EXPECT_GT(slowClumsy * (1.0 / 6.0), 0.05) << "jedno GFI pomalého nosiče je nad mezí 5 % ⇒ nedělá ho";
    EXPECT_LE(fastAgile * (1.0 / 6.0), 0.05) << "jedno GFI rychlého obratného nosiče se vejde";
}

// ---------------------------------------------------------------------------------------------
// P180 (uživatel 08.10.2026: „rychlejší tým by měl být pouze ve stavech — klec v pořádku — a —
// nosič doběhne, případně předá nebo hodí někomu nachystanému dát TD“; rozhodnutí: „hrozbu ztráty
// míče řešíme dřívějším TD vždy“, „pokud je míč v bezpečí a máme čas — volíme zdržovat“, „pokud
// hráč dojde se chystat a nedojde tvořit roh — má se jít chystat“). Hráči jsou zadaní čísly, ne rasou.
namespace {
// nosič (MA 6) 13 polí od zóny — sám nedojde; příjemce (MA 9) stojí 8 polí od zóny, 4 pole od nosiče
Board mateBoard(int turn, Position opponent, int8_t oppSt = 3) {
    Board b(turn);
    b.put(1, TeamSide::HOME, {12, 7}, 6);
    b.put(6, TeamSide::HOME, {17, 7}, 9);
    b.put(13, TeamSide::AWAY, opponent, 6, oppSt);
    b.s.ball = BallState::carried({12, 7}, 1);
    return b;
}
}  // namespace

// Nosiči hrozí dobrá rána (soupeř dvě pole od něj, silnější) a sám do zóny nedosáhne; příjemce po
// předávce dojde bez hodu ⇒ řadič přikáže TD předávkou. Pravidla ř. 1676–1692 (předávka na sousední
// pole, pohyb před ní ano; zachycení +1), ř. 845–846 (kdo chytil a ještě nehrál, smí hrát).
TEST(OneCageMate, AThreatenedCarrierWhoCannotReachHandsOffToAReadyTeammateForTheTouchdown) {
    {
        Board b = mateBoard(4, {10, 7}, 5);
        ASSERT_GT(blitzThreat(b.s, b.s.getPlayer(1)), 0.34) << "předpoklad: míč přežije s menší šancí, než je šance předávky";
        ASSERT_GT(handOffTdChance(b.s, b.s.getPlayer(1), b.s.getPlayer(6)), 0.6);
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        ASSERT_FALSE(ms.empty());
        EXPECT_EQ(ms.front().type, MacroType::HAND_OFF_SCORE);
        EXPECT_EQ(ms.front().targetId, 6);
        EXPECT_EQ(b.s.homeTeam.score, 1) << "se šestkami na kostkách předávka i doběh vyjdou";
    }
    {   // pojistka měření: s vypnutou úpravou řadič předávku nepřikazuje
        setCageFeaturesOff(kFeatScoreViaMate);
        Board b = mateBoard(4, {10, 7}, 5);
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        setCageFeaturesOff(0);
        for (const Macro& m : ms) EXPECT_NE(m.type, MacroType::HAND_OFF_SCORE);
    }
}

// Míč je v bezpečí (soupeř nedosáhne) a rychlý tým má ve 2. kole času dost ⇒ TD předávkou se
// nehraje: řadič ho nepřikáže a hledání ho nedostane (dřív díra ve zdržování). V 7. kole týž tým
// časovou rezervu nemá ⇒ hledání předávku hrát smí.
TEST(OneCageMate, ASafeBallWithTimeToSpareIsNotHandedOffForAnEarlyTouchdown) {
    const Macro ho{MacroType::HAND_OFF_SCORE, 1, 6, {-1, -1}};
    {
        Board b = mateBoard(2, {0, 0});
        ASSERT_DOUBLE_EQ(blitzThreat(b.s, b.s.getPlayer(1)), 0.0);
        ASSERT_TRUE(teamHasTimeSlack(b.s, b.s.getPlayer(1)));
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        for (const Macro& m : ms) EXPECT_NE(m.type, MacroType::HAND_OFF_SCORE);
        EXPECT_EQ(b.s.homeTeam.score, 0);
        EXPECT_TRUE(cc.forbidsCarrierMove(b.s, ho)) << "ani hledání TD předávkou nedostane";
    }
    {
        Board b = mateBoard(7, {0, 0});
        ASSERT_FALSE(teamHasTimeSlack(b.s, b.s.getPlayer(1))) << "předpoklad: v 7. kole rezerva není";
        CageController cc(nullptr, cfg(), 1);
        playAll(cc, b);
        ASSERT_EQ(b.s.homeTeam.score, 0) << "řadič sám neskóroval (míč je v bezpečí, předávku nepřikazuje)";
        EXPECT_FALSE(cc.forbidsCarrierMove(b.s, ho)) << "bez rezervy se nezdržuje — hledání předat smí";
    }
}

// Příprava: po tahu klece stojí jeden volný hráč tak, aby příští tah došel do zóny bez hodu a nosič
// k němu došel předat. Pět spoluhráčů (MA 9) stojí na začátku 16–18 polí od zóny: čtyři jdou na rohy, pátý se chystá.
TEST(OneCageMate, AfterTheCageMovesOneFreePlayerGetsReadyWithinReachOfTheEndZoneAndOfTheCarrier) {
    auto run = [](unsigned off, bool& ready) {
        Board b(2);
        b.put(1, TeamSide::HOME, {8, 7}, 4);          // pomalý nosič: klec ujde 4 pole
        b.put(2, TeamSide::HOME, {7, 6}, 9); b.put(3, TeamSide::HOME, {7, 8}, 9);
        b.put(4, TeamSide::HOME, {9, 6}, 9); b.put(5, TeamSide::HOME, {9, 8}, 9);
        b.put(6, TeamSide::HOME, {8, 3}, 9);
        b.put(13, TeamSide::AWAY, {24, 13}, 4);
        b.s.ball = BallState::carried({8, 7}, 1);
        setCageFeaturesOff(off);
        CageController cc(nullptr, cfg(), 1);
        playAll(cc, b);
        setCageFeaturesOff(0);
        const Player& c = b.s.getPlayer(1);
        ready = false;                       // kdo z pěti zbude volný, vybírá plánovač klece — stačí kdokoli
        for (int id = 2; id <= 6; ++id) {
            const Player& m = b.s.getPlayer(id);
            const bool corner = std::abs(m.position.x - c.position.x) == 1 && std::abs(m.position.y - c.position.y) == 1;
            if (!corner && (25 - m.position.x) <= 9 && m.position.distanceTo(c.position) - 1 <= 4) ready = true;
        }
        return 25 - c.position.x;
    };
    bool ready = false;
    const int carrierDist = run(0, ready);
    ASSERT_GT(carrierDist, 4 + 2) << "předpoklad: nosič sám příští tah do zóny nedojde ani s GFI";
    EXPECT_TRUE(ready) << "volný hráč je připravený";
    run(kFeatReadyMate, ready);
    EXPECT_FALSE(ready) << "pozitivní kontrola: bez úpravy jde volný hráč jen tři sloupce před klec — do zóny by nedošel";
}

// Nosič v čisté kleci (rána nejvýš „dvě kostky, vybírá nosič“, míč přežije v 89 %) a příjemce bez
// Catch (zachycení 3+ = 67 %): předávka má menší šanci než klec ⇒ klec jde dál.
TEST(OneCageMate, ACleanCageIsNotBrokenForAHandOffWithWorseOdds) {
    Board b(4);
    b.put(1, TeamSide::HOME, {12, 7}, 6);
    b.put(2, TeamSide::HOME, {11, 6}); b.put(3, TeamSide::HOME, {11, 8});
    b.put(4, TeamSide::HOME, {13, 6}); b.put(5, TeamSide::HOME, {13, 8});
    b.put(6, TeamSide::HOME, {17, 3}, 9);
    b.put(13, TeamSide::AWAY, {8, 7}, 6);
    b.s.ball = BallState::carried({12, 7}, 1);
    const double pm = handOffTdChance(b.s, b.s.getPlayer(1), b.s.getPlayer(6));
    ASSERT_GT(pm, 0.5) << "předpoklad: předávka možná je";
    ASSERT_LT(pm, 1.0 - blitzThreat(b.s, b.s.getPlayer(1))) << "předpoklad: klec je bezpečnější";
    CageController cc(nullptr, cfg(), 1);
    std::vector<Macro> ms;
    playAll(cc, b, &ms);
    for (const Macro& m : ms) EXPECT_NE(m.type, MacroType::HAND_OFF_SCORE);
    EXPECT_EQ(b.s.homeTeam.score, 0);
}

// Laťka „míč v bezpečí“ (uživatel 08.10.2026: „pokud je klec čistá — nedostaneme se pod 11 %
// pravděpodobnost ztráty — já bych to ignoroval a postavil laťku výše — jinak budou elfové vždy
// skórovat a nikdy zdržovat“). Soupeř na nosiče dosáhne, ale jen ranou „dvě kostky, vybírá nosič“
// (11 %): to je BEZPEČÍ ⇒ zdržuje se — TD doběhem (nosič 3 pole před zónou) i TD předávkou
// jistému příjemci (Catch, 97 %). Pozitivní kontrola: silnější soupeř má ránu na jednu kostku
// (33 %) ⇒ skóruje se hned. (Do plné klece se soupeř dostane jen přes úhyb, hrozba je pak pod
// 5 % — proto má klec v testu tři rohy a nosič sílu 4: soupeř dojde volně, ale rána je slabá.)
TEST(OneCageMate, AHitOfTwoDiceChosenByTheCarrierCountsAsSafeAndTheTouchdownIsDelayed) {
    auto walkIn = [](int8_t oppSt) {
        Board b(5);
        b.put(1, TeamSide::HOME, {22, 7}, 6, 4);
        b.put(3, TeamSide::HOME, {21, 8}); b.put(4, TeamSide::HOME, {23, 6}); b.put(5, TeamSide::HOME, {23, 8});
        b.put(13, TeamSide::AWAY, {18, 4}, 6, oppSt);
        b.s.ball = BallState::carried({22, 7}, 1);
        return b;
    };
    {
        Board b = walkIn(3);
        const double t = blitzThreat(b.s, b.s.getPlayer(1));
        ASSERT_GT(t, 0.05) << "předpoklad: stará mez 0,05 by tady už skórovala";
        ASSERT_LE(t, kSafeBlitzThreat) << "předpoklad: jen rána „dvě kostky, vybírá nosič“";
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        for (const Macro& m : ms) EXPECT_NE(m.type, MacroType::SCORE);
        EXPECT_EQ(b.s.homeTeam.score, 0) << "míč v bezpečí ⇒ TD se zdržuje";
        EXPECT_TRUE(cc.forbidsCarrierMove(b.s, Macro{MacroType::SCORE, 1, -1, {-1, -1}}));
    }
    {
        Board b = walkIn(5);
        ASSERT_GT(blitzThreat(b.s, b.s.getPlayer(1)), kSafeBlitzThreat);
        CageController cc(nullptr, cfg(), 1);
        playAll(cc, b);
        EXPECT_EQ(b.s.homeTeam.score, 1) << "pozitivní kontrola: hrozí dobrá rána ⇒ TD hned";
    }
    {   // TD předávkou: jistý příjemce (Catch), slabá rána, 2. kolo rychlého týmu ⇒ nepředává se
        Board b(2);
        b.put(1, TeamSide::HOME, {12, 7}, 7, 4);
        b.put(3, TeamSide::HOME, {11, 8}, 7); b.put(4, TeamSide::HOME, {13, 6}, 7); b.put(5, TeamSide::HOME, {13, 8}, 7);
        b.put(6, TeamSide::HOME, {17, 3}, 9, 3, {SkillName::Catch}).stats.agility = 4;   // chytá na 2+ s přehozem
        b.put(13, TeamSide::AWAY, {8, 4}, 6);
        b.s.ball = BallState::carried({12, 7}, 1);
        const double t = blitzThreat(b.s, b.s.getPlayer(1));
        ASSERT_GT(t, 0.05);
        ASSERT_LE(t, kSafeBlitzThreat);
        ASSERT_GT(handOffTdChance(b.s, b.s.getPlayer(1), b.s.getPlayer(6)), 1.0 - t) << "předpoklad: předávka je jistější než čekání";
        ASSERT_TRUE(teamHasTimeSlack(b.s, b.s.getPlayer(1)));
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        for (const Macro& m : ms) EXPECT_NE(m.type, MacroType::HAND_OFF_SCORE);
        EXPECT_EQ(b.s.homeTeam.score, 0);
    }
}

// „předávka nevyjde — počítej s team rerollem, pokud je k dispozici“ (uživatel 08.10.2026).
// Příjemce bez Catch chytá předávku na 3+ (67 %); s týmovým přehozem 89 %; použitý přehoz nepomůže.
TEST(OneCageMate, TheTeamRerollCountsTowardTheHandOffChanceWhenItIsAvailable) {
    Board b = mateBoard(4, {0, 0});
    const Player& c = b.s.getPlayer(1);
    const Player& m = b.s.getPlayer(6);
    const double without = handOffTdChance(b.s, c, m);
    EXPECT_NEAR(without, 4.0 / 6.0, 1e-9);
    b.s.homeTeam.rerolls = 2;
    EXPECT_NEAR(handOffTdChance(b.s, c, m), 1.0 - (2.0 / 6.0) * (2.0 / 6.0), 1e-9);
    b.s.homeTeam.rerollUsedThisTurn = true;
    EXPECT_NEAR(handOffTdChance(b.s, c, m), without, 1e-9);
}

// Review 08.10.2026 (H1): klec nemůže postoupit a jen se dostavuje ⇒ hrozba po dostavbě se musí
// spočítat (dřív zůstala 0 = „míč přežije jistě“ a TD při hrozbě se nepřikázalo). Nosič (síla 3)
// má soupeře (síla 5) hned vedle sebe; spoluhráč dostaví jeden roh, rána zůstává dobrá.
TEST(OneCageMate, AFillOnlyPlanReportsTheThreatThatRemainsAfterTheFill) {
    Board b(4);
    b.put(1, TeamSide::HOME, {12, 7}, 6);
    b.put(2, TeamSide::HOME, {11, 6}); b.put(3, TeamSide::HOME, {11, 8});
    b.put(4, TeamSide::HOME, {14, 4});
    b.put(13, TeamSide::AWAY, {13, 7}, 6, 5);
    b.s.ball = BallState::carried({12, 7}, 1);
    CageAdvancePlanner planner(nullptr, cfg(), 1);
    const CageAdvancePlan fill = planner.buildFillOnly(b.s, {});
    ASSERT_TRUE(fill.valid) << "předpoklad: roh jde dostavět";
    EXPECT_GT(fill.blitzThreat, kSafeBlitzThreat) << "soupeř u nosiče má dobrou ránu i po dostavbě";
}

// P190 (rozbor skavenů 09.10.2026; uživatel: „do té doby má být v kleci“). Kolem nosiče stojí tři
// rohy a soupeř má jen slabou ránu (dvě kostky, vybírá nosič, 11 %). Útěk vpřed z dosahu by hrozbu
// srazil na nulu — o jedenáct setin. Kvůli tak malému zisku hledání nosiče z klece neodvede
// (v měření: klec se čtyřmi rohy opuštěna pro zisk dvou setin, po pohybu dva rohy).
TEST(OneCageKeep, TheSearchDoesNotTakeTheCarrierOutOfABuiltCageForASmallGain) {
    auto forbidden = [](unsigned off) {
        Board b(3);
        b.put(1, TeamSide::HOME, {12, 7}, 6, 4);
        b.put(3, TeamSide::HOME, {11, 8}); b.put(4, TeamSide::HOME, {13, 6}); b.put(5, TeamSide::HOME, {13, 8});
        for (int id : {3, 4, 5}) { b.s.getPlayer(id).hasMoved = true; }      // rohy už v tahu hrály
        b.put(13, TeamSide::AWAY, {8, 4}, 6);
        b.s.ball = BallState::carried({12, 7}, 1);
        const double t = blitzThreat(b.s, b.s.getPlayer(1));
        EXPECT_GT(t, 0.05);
        EXPECT_LE(t, kSafeBlitzThreat);
        setCageFeaturesOff(off);
        CageController cc(nullptr, cfg(), 1);
        Macro first;
        cc.next(b.s, first);
        const bool f = cc.forbidsCarrierMove(b.s, Macro{MacroType::ADVANCE, 1, -1, {-1, -1}});
        setCageFeaturesOff(0);
        return f;
    };
    EXPECT_TRUE(forbidden(0)) << "nosič zůstává v kleci";
    EXPECT_FALSE(forbidden(kFeatKeepBuiltCage)) << "pozitivní kontrola: dřív stačil zisk nad dvě setiny";
}

// Týž požadavek pro řadič samotný: nosič v bezpečí (tři rohy, slabá rána) sám dopředu neodbíhá.
TEST(OneCageKeep, TheControllerDoesNotRunASafeCarrierOutOfTheCageEither) {
    auto carrierX = [](unsigned off) {
        Board b(3);
        b.put(1, TeamSide::HOME, {12, 7}, 6, 4);
        b.put(3, TeamSide::HOME, {11, 8}); b.put(4, TeamSide::HOME, {13, 6}); b.put(5, TeamSide::HOME, {13, 8});
        for (int id : {3, 4, 5}) { b.s.getPlayer(id).hasMoved = true; }
        b.put(13, TeamSide::AWAY, {8, 4}, 6);
        b.s.ball = BallState::carried({12, 7}, 1);
        setCageFeaturesOff(off);
        CageController cc(nullptr, cfg(), 1);
        playAll(cc, b);
        setCageFeaturesOff(0);
        return static_cast<int>(b.s.getPlayer(1).position.x);
    };
    EXPECT_EQ(carrierX(0), 12) << "nosič v bezpečí zůstává u rohů";
    EXPECT_GT(carrierX(kFeatKeepBuiltCage), 12) << "pozitivní kontrola: dřív odběhl sám z dosahu";
}

// P190 (uživatel 08.10.2026: „u agilních týmů bude převažovat dodge a útěk daleko“; „když nosič
// nemůže skórovat ani být v bezpečí — nesmí nastat“). Nosič stojí v kontaktu se silnějším soupeřem
// (rána na jednu kostku, 33 %) a klec kolem něj postavit nejde — spoluhráči stojí o šest polí vzadu.
// Obratný nosič (úhyb na 2+, nevyjde jednou ze šesti) z kontaktu uhne ke spoluhráčům, kde kolem něj
// klec vznikne. Méně obratný (úhyb na 3+ a hůř) zůstane: neúspěch je turnover na začátku tahu.
// Z čísel hráče, ne z rasy.
TEST(OneCageEscape, AnAgileCarrierDodgesOutOfContactToWhereTheCageCanFormAClumsyOneDoesNot) {
    auto run = [](int8_t ag, unsigned off, double& threatAfter) {
        Board b(3);
        b.put(1, TeamSide::HOME, {12, 7}, 7).stats.agility = ag;
        b.put(2, TeamSide::HOME, {4, 6}); b.put(3, TeamSide::HOME, {4, 8});      // pohyb 4: k nosiči nedojdou
        b.put(4, TeamSide::HOME, {6, 6}); b.put(5, TeamSide::HOME, {6, 8});
        b.put(13, TeamSide::AWAY, {13, 7}, 4, 4);
        b.s.ball = BallState::carried({12, 7}, 1);
        setCageFeaturesOff(off);
        CageController cc(nullptr, cfg(), 1);
        playAll(cc, b);
        setCageFeaturesOff(0);
        threatAfter = blitzThreat(b.s, b.s.getPlayer(1));
        return b.s.getPlayer(1).position;
    };
    double t = 1.0;
    const Position agile = run(4, 0, t);
    EXPECT_NE(agile, (Position{12, 7})) << "obratný nosič z kontaktu odešel";
    EXPECT_LE(t, kSafeBlitzThreat) << "a po tahu je v bezpečí";
    const Position before = run(4, kFeatEscapeContact, t);
    EXPECT_EQ(before, (Position{12, 7})) << "pozitivní kontrola: bez úpravy řadič nosiče v kontaktu nechal stát";
    EXPECT_GT(t, kSafeBlitzThreat);
    const Position clumsy = run(3, 0, t);
    EXPECT_EQ(clumsy, (Position{12, 7})) << "nosič s úhybem na 3+ neuhýbá";
}
