// P126 — jedna klec ve třech fázích (uživatel 01.–02.10.2026, paměť
// project_bloodbowl_one_cage_phases_20261002). Testy hlídají POŽADAVKY
// uživatele, ne tvar kódu: kdo se kdy hýbe, kam a co se nesmí stát.
#include <gtest/gtest.h>
#include "bb/one_cage.h"
#include "bb/block_handler.h"
#include "bb/pathfinder.h"
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

// P217 (3) přebíjí staré pořadí rozhodování s míčem (P175/P178 TD hned, P180 TD předávkou, P187 výběh podle
// vzdálenosti, P213 útěk). Testy toho pořadí běží s vypnutým P217 — hlídají dál samotné části, které P217
// používá jako varianty (provedení výběhu, markery, předávka, útěk, připravení hráči).
struct P217Off {
    P217Off() { setExtraFeaturesOff(kP217BestChance); }
    ~P217Off() { setExtraFeaturesOff(0); }
};

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

// PŘEPSÁNO 16.10.2026 — P180 (6), uživatel: „poslední kolo poločasu je poslední šance dát TD — vše pro
// dát TD — vyhledat nejbezpečnější možnost“. Dřív tu stálo, že poslední kolo řadič přenechá hledání;
// teď řadič přikáže TD s největší šancí (tady nosič sám přes GFI — předávku nemá komu dát).
TEST(OneCageScoringRange, LastTurnOfTheHalfOrdersTheBestTouchdown) {
    Board b = scoringRangeBoard(8);          // poslední kolo: skórovat se musí zkusit
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    ASSERT_TRUE(cc.next(b.s, m));
    EXPECT_EQ(m.type, MacroType::SCORE);
    EXPECT_EQ(m.playerId, 1);
    setCageFeaturesOff(kFeatLastTurnBestTd);  // pojistka: bez úpravy staré chování (hledání)
    CageController old(nullptr, cfg(), 1);
    Board b2 = scoringRangeBoard(8);
    EXPECT_FALSE(old.next(b2.s, m));
    setCageFeaturesOff(0);
}

// Od 08.10.2026 (P175) platí: dojde bez hodu a hrozí mu rána ⇒ skóruje hned (rozhoduje hledání);
// dojde bez hodu a je v bezpečí ⇒ TD se zdržuje (OneCageStall).
TEST(OneCageScoringRange, CarrierWhoWalksInWithoutDiceIsLeftToTheSearchWhenThreatened) {
    P217Off p217off;
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

// P169 krok 4 + uživatel 27.10.2026 („block na 1 kostku bez block se nemá dít“). Řadič bere rány z nabídky
// hledání (getAvailableMacros), a ta pravidlo hlídá (hitAllowed) — test hlídá požadavek na tom místě. Soupeř ST 5 stojí
// přímo před nosičem (12,7). Bijí na něj dva naši, oba na jednu kostku (ST 3 + 2 asistence):
// hráč 4 z (12,6) vedle nosiče, bez Block (jeho odsun na (14,8) soupeře od nosiče odvede — dřív proto
// vyhrál by, kdyby ho nabídka pustila) a hráč 5 z (14,7) s Block.
// Řadič vezme toho, kdo jednu kostku smí, tedy 5.
TEST(OneCageFreeing, MarkerNextToTheCarrierIsHitOnOneDieOnlyWithBlock) {
    struct Off { Off() { setExtraFeaturesOff(kP213EscapeFirst | kP217BestChance | kP229WallBreak); } ~Off() { setExtraFeaturesOff(0); } } off;
    Board b(2);
    b.put(1, TeamSide::HOME, {12, 7}, 4);
    b.put(2, TeamSide::HOME, {11, 6});
    b.put(3, TeamSide::HOME, {11, 8});
    b.put(4, TeamSide::HOME, {12, 6});
    b.put(5, TeamSide::HOME, {14, 7}, 4, 3, {SkillName::Block});
    b.put(13, TeamSide::AWAY, {13, 7}, 6, 5);
    b.put(14, TeamSide::AWAY, {24, 13}, 6);
    b.s.ball = BallState::carried({12, 7}, 1);
    ASSERT_EQ(blockDiceCount(b.s, b.s.getPlayer(4), b.s.getPlayer(13)), 1);
    ASSERT_EQ(blockDiceCount(b.s, b.s.getPlayer(5), b.s.getPlayer(13)), 1);
    CageController cc(nullptr, cfg(), 1);
    std::vector<Macro> ms;
    playAll(cc, b, &ms);
    for (const Macro& m : ms) EXPECT_FALSE(m.type == MacroType::BLOCK && m.playerId == 4) << "1 kostka bez Block";
    EXPECT_TRUE(std::any_of(ms.begin(), ms.end(), [](const Macro& m) {
        return m.type == MacroType::BLOCK && m.playerId == 5 && m.targetId == 13;
    })) << "soupeře od nosiče shodí hráč s Block";
}

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
    P217Off p217off;
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
    // Hlídá P154 b (nosič bez postupu klece nevybíhá) se starou hrozbou. S P228 jsou rohy u zdi špinavé
    // (hrozba 0,29) a bez prolomení (rohy bez Block na 1 kostku nebijí) řadič nosiče odvede tam, kde je
    // hrozba menší — to je jiné pravidlo (P169 krok 3), ne tohle.
    struct Off { Off() { setExtraFeaturesOff(kP228DirtyCorner | kP229WallBreak); } ~Off() { setExtraFeaturesOff(0); } } off;
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
    // Hlídá zákaz pro HLEDÁNÍ (P154 b) u klece, kterou řadič drží na místě. S P228 + P229 řadič
    // u téhle zdi sám pošle zadní roh na křídlo (prolomení, testy OneCageWallBreak) — klec tu pak
    // „na místě“ nestojí. Proto se starou hrozbou a bez prolomení.
    // P230 záloha vypnutá: jediný zaostalec by zůstal zálohou a hledání by ho dopředu nepustilo (OneCageBackup)
    struct Off { Off() { setExtraFeaturesOff(kP228DirtyCorner | kP229WallBreak | kP230Backup); } ~Off() { setExtraFeaturesOff(0); } } off;
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

// --- P229 (uživatel 26.10.2026): zeď před klecí — pomalejší nebo silnější prolomí, rychlejší obejde ---
// „když jsme silnější — dáme asistenty na 13,5 a 13,9 a přední rohy block dopředu a pak blitz dopředu
// z 13,7 a skrz bariéru — když jsme rychlejší, ostatní kromě předních rohů obíhají“; „rychlejší
// spočítáš z MA — pozor na průměr — počítej jen část týmu — trpaslíci proti orkům … jsou pomalejší —
// tak nemohou obíhat — i když nespočítáš, že jsou silnější“. Zeď je walledCageBoard: klec kolem
// (12,7), soupeři MA 6 na x = 14, řádky 5–9 — přesně příklad uživatele.
namespace {
// Tři volní hráči MA 9 (průměr týmu tím vyskočí nad zeď, obíhat by ale musel nosič a zadní rohy MA 4):
// dva dojdou na křídla, třetí bez hodu na pole před nosičem (13,7).
Board wallWithHelpers(int8_t runnersMa) {
    Board b = walledCageBoard(2);
    for (int id : {1, 2, 3}) {
        b.s.getPlayer(id).stats.movement = runnersMa;
        b.s.getPlayer(id).movementRemaining = runnersMa;
    }
    b.put(6, TeamSide::HOME, {10, 3}, 9);
    b.put(7, TeamSide::HOME, {10, 11}, 9);
    b.put(8, TeamSide::HOME, {9, 7}, 9);
    return b;
}
// Přední rohy a blitzer s Block: rány zdi jsou tu na jednu kostku (asistence ruší křídla i rohy navzájem),
// a ta je při prolamování dovolená jen s Block (uživatel 27.10.: „block na 1 kostku bez block se nemá dít“).
Board wallWithBlockers(int8_t runnersMa) {
    Board b = wallWithHelpers(runnersMa);
    for (int id : {4, 5, 8}) b.s.getPlayer(id).skills.add(SkillName::Block);
    return b;
}
// kostky rány `att` na `def` na desce, kde už stojí asistenti na křídlech (13,5) a (13,9)
int diceWithWings(const Board& b, int att, int def) {
    GameState s = b.s.clone();
    s.getPlayer(6).position = {13, 5};
    s.getPlayer(7).position = {13, 9};
    return blockDiceCount(s, s.getPlayer(att), s.getPlayer(def));
}
struct CageTurn { std::vector<Macro> macros; Position carrier; };
CageTurn cageTurn(Board& b) {
    CageController cc(nullptr, cfg(), 1);
    CageTurn t;
    Macro m;
    while (cc.next(b.s, m)) {             // jen plán řadiče (bez hledání), kostky 6 = vše vyjde
        t.macros.push_back(m);
        play(b.s, m);
    }
    t.carrier = b.s.getPlayer(1).position;
    return t;
}
bool hasMacro(const CageTurn& t, MacroType type, int player, int target = -1, Position sq = {-1, -1}) {
    return std::any_of(t.macros.begin(), t.macros.end(), [&](const Macro& m) {
        return m.type == type && m.playerId == player && (target < 0 || m.targetId == target) &&
               (sq.x < 0 || m.targetPos == sq);
    });
}
}  // namespace

TEST(OneCageWallBreak, SlowerCageBreaksTheWallInTheUsersOrder) {
    Board b = wallWithBlockers(4);
    ASSERT_EQ(diceWithWings(b, 4, 14), 1) << "s asistenty na křídlech jedna kostka";
    ASSERT_EQ(b.s.getPlayer(14).position, (Position{14, 6}));
    ASSERT_EQ(b.s.getPlayer(15).position, (Position{14, 7}));
    ASSERT_EQ(b.s.getPlayer(16).position, (Position{14, 8}));
    const CageTurn t = cageTurn(b);
    EXPECT_TRUE(hasMacro(t, MacroType::REPOSITION, 6, -1, {13, 5})) << "asistent na křídlo 13,5";
    EXPECT_TRUE(hasMacro(t, MacroType::REPOSITION, 7, -1, {13, 9})) << "asistent na křídlo 13,9";
    EXPECT_TRUE(hasMacro(t, MacroType::BLOCK, 4, 14)) << "přední roh (13,6) blokuje dopředu";
    EXPECT_TRUE(hasMacro(t, MacroType::BLOCK, 5, 16)) << "přední roh (13,8) blokuje dopředu";
    EXPECT_TRUE(hasMacro(t, MacroType::BLITZ, 8, 15)) << "blitz na soupeře před nosičem";
    // pořadí: křídla → bloky → blitz
    auto idx = [&](MacroType ty, int pl) {
        for (size_t i = 0; i < t.macros.size(); ++i) if (t.macros[i].type == ty && t.macros[i].playerId == pl) return static_cast<int>(i);
        return -1;
    };
    EXPECT_LT(idx(MacroType::REPOSITION, 7), idx(MacroType::BLOCK, 4));
    EXPECT_LT(idx(MacroType::BLOCK, 5), idx(MacroType::BLITZ, 8));
    EXPECT_EQ(t.carrier, (Position{12, 7})) << "nosič stojí — neobíhá";
}

// Review 27.10.: kostky se počítají znovu těsně před ranou. Křídlo (13,5) po přesunu zmizí (tady ručně —
// ve hře třeba nevyjde přesun) ⇒ soupeř 13 na (14,5) zase asistuje a roh 4 na soupeře 14 by bil na dvě
// kostky, které volí obránce ⇒ rána se nehraje (zbytek tahu přebírá hledání).
TEST(OneCageWallBreak, DiceAreCheckedAgainRightBeforeTheHit) {
    Board b = wallWithBlockers(4);
    CageController cc(nullptr, cfg(), 1);
    std::vector<Macro> played;
    Macro m;
    bool wingGone = false;
    while (cc.next(b.s, m)) {
        played.push_back(m);
        play(b.s, m);
        if (!wingGone && m.type == MacroType::REPOSITION && m.playerId == 6 && b.s.getPlayer(6).position == (Position{13, 5})) {
            b.s.getPlayer(6).position = {10, 3};
            wingGone = true;
            ASSERT_LT(blockDiceCount(b.s, b.s.getPlayer(4), b.s.getPlayer(14)), 1) << "předpoklad: bez křídla volí obránce";
        }
    }
    ASSERT_TRUE(wingGone) << "prolomení začalo křídlem 6";
    // plán se rozešel s deskou ⇒ zbytek tahu dostane hledání (next() vrátí false), řadič už nebije
    for (const Macro& p : played) EXPECT_FALSE(p.type == MacroType::BLOCK && p.playerId == 4) << "roh 4 bez křídla neblokuje";
}

// Uživatel 27.10.2026: „block na 1 kostku bez block se nemá dít“. Táž zeď jako výš, rohy i blitzer bez
// Block: každá rána by byla na jednu kostku (turnover 1/3) ⇒ prolomení nebije vůbec.
TEST(OneCageWallBreak, NoOneDieHitWithoutBlock) {
    Board b = wallWithHelpers(4);
    ASSERT_EQ(diceWithWings(b, 4, 14), 1) << "s asistenty na křídlech jedna kostka";
    ASSERT_EQ(diceWithWings(b, 5, 16), 1);
    {   // blitzer 8 z pole před nosičem (13,7) na soupeře 15: taky jedna kostka
        GameState s = b.s.clone();
        s.getPlayer(6).position = {13, 5};
        s.getPlayer(7).position = {13, 9};
        s.getPlayer(8).position = {13, 7};
        ASSERT_EQ(blockDiceCount(s, s.getPlayer(8), s.getPlayer(15)), 1);
    }
    // požadavek: žádná rána na jednu kostku bez Block — kostky se čtou v okamžiku rány (P233 bije na 2 naše)
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    while (cc.next(b.s, m)) {
        if (m.type == MacroType::BLOCK || m.type == MacroType::BLITZ) {
            GameState s = b.s.clone();
            if (m.type == MacroType::BLITZ) {             // skutečné pole doběhu na desce v okamžiku rány
                Position landing{-1, -1};
                int steps = 0;
                if (blitzLandingDiceFree(s, s.getPlayer(m.playerId), s.getPlayer(m.targetId).position, landing, steps))
                    s.getPlayer(m.playerId).position = landing;
            }
            const int dice = blockDiceCount(s, s.getPlayer(m.playerId), s.getPlayer(m.targetId));
            EXPECT_TRUE(dice >= 2 || b.s.getPlayer(m.playerId).hasSkill(SkillName::Block))
                << "rána hráče " << m.playerId << " na " << m.targetId << " na " << dice << " kostek";
        }
        play(b.s, m);
    }
}

// Review 27.10. (nález 1): nosič stojí vedle soupeře, kterého rány řadiče neodstraní (ST 7, Stand
// Firm) ⇒ zeď nepočká jen na prolamování: napřed úhyb nosiče z kontaktu (P190) a pojistky bezpečí.
// Prolomení by nosiče nechalo stát v kontaktu.
TEST(OneCageWallBreak, MarkedCarrierDoesNotBreakTheWall) {
    struct Off { Off() { setExtraFeaturesOff(kP213EscapeFirst | kP217BestChance); } ~Off() { setExtraFeaturesOff(0); } } off;
    Board b = wallWithHelpers(4);
    b.put(18, TeamSide::AWAY, {12, 6}, 6, 7, {SkillName::StandFirm});
    ASSERT_GT(countTacklezones(b.s, Position{12, 7}, TeamSide::HOME), 0);
    const CageTurn t = cageTurn(b);
    EXPECT_FALSE(hasMacro(t, MacroType::BLOCK, 4, 14)) << "přední roh neprolamuje";
    EXPECT_FALSE(hasMacro(t, MacroType::BLOCK, 5, 16)) << "přední roh neprolamuje";
}

// „Pozor na průměr — počítej jen část týmu“: daleko od klece stojí tři hráči MA 10, průměr týmu
// (77 / 11 = 7) je nad zdí (MA 6). Obíhat by ale musel nosič a zadní rohy MA 4 ⇒ prolamuje.
TEST(OneCageWallBreak, TeamAverageDoesNotMakeTheCageFaster) {
    Board b = wallWithBlockers(4);
    b.put(9, TeamSide::HOME, {2, 1}, 10);
    b.put(10, TeamSide::HOME, {2, 13}, 10);
    b.put(11, TeamSide::HOME, {1, 7}, 10);
    const CageTurn t = cageTurn(b);
    EXPECT_TRUE(hasMacro(t, MacroType::BLOCK, 4, 14)) << "přední roh blokuje dopředu";
    EXPECT_TRUE(hasMacro(t, MacroType::BLOCK, 5, 16));
    EXPECT_EQ(t.carrier, (Position{12, 7})) << "nosič neobíhá";
}

// Rychlejší (nosič i zadní rohy MA 7 > zeď MA 6) a ne silnější (rohy ST 3 na ST 3 = jedna kostka)
// ⇒ neprolamuje, obíhá: nosič se zadními rohy a volnými hráči kolem kraje zdi, přední rohy stojí.
TEST(OneCageWallBreak, FasterCageDoesNotBreakTheWall) {
    Board b = wallWithHelpers(7);
    const CageTurn t = cageTurn(b);
    // co dělat MÁ (review 27.10. nález 13): „ostatní kromě předních rohů obíhají“ — nosič je po tahu za
    // zdí (x > 14) a v bezpečí, přední rohy (13,6)/(13,8) tento tah nejdou nikam
    EXPECT_GT(t.carrier.x, 14) << "klec oběhla zeď";
    EXPECT_LE(blitzThreat(b.s, b.s.getPlayer(1)), kSafeBlitzThreat) << "nosič po tahu v bezpečí";
    for (const Macro& m : t.macros) EXPECT_TRUE(m.playerId != 4 && m.playerId != 5) << "přední roh " << m.playerId << " zůstává";
    EXPECT_FALSE(hasMacro(t, MacroType::BLOCK, 4)) << "přední roh neblokuje";
    EXPECT_FALSE(hasMacro(t, MacroType::BLOCK, 5)) << "přední roh neblokuje";
    EXPECT_FALSE(hasMacro(t, MacroType::BLITZ, 8, 15));
}

// Rychlejší, ale silnější (přední rohy ST 4 proti ST 3; s křídly 2 kostky pro nás) ⇒ prolamuje
// (uživatel 27.10.: „silnější prolamuje, i když je rychlejší“).
TEST(OneCageWallBreak, FasterButStrongerCageBreaksTheWall) {
    Board b = wallWithHelpers(7);
    b.s.getPlayer(4).stats.strength = 4;
    b.s.getPlayer(5).stats.strength = 4;
    ASSERT_EQ(diceWithWings(b, 4, 14), 2);
    const CageTurn t = cageTurn(b);
    EXPECT_TRUE(hasMacro(t, MacroType::BLOCK, 4, 14));
    EXPECT_TRUE(hasMacro(t, MacroType::BLOCK, 5, 16));
    EXPECT_EQ(t.carrier, (Position{12, 7}));
}

// --- P230 (partie 27.10.2026): pomalejší klec u zdi, rána nejde ⇒ celá klec po řadách ve stejném sloupci ---
// Uživatel: „šel bych do cílové situace, kdy je celá klec OK na místě s nižším Y“; „klec má být jen čtyři
// rohy“; „A [prolomit] dělá pomalejší … nejhorší je nedělat nic“. Deska jako NoOneDieHitWithoutBlock:
// přední rohy bez Block mají na zeď jen jednu kostku ⇒ prolomení nebije.
static int cornersAround(const GameState& s, Position c) {
    int n = 0;
    for (int dx : {-1, 1}) for (int dy : {-1, 1}) {
        const Player* q = s.getPlayerAtPosition({static_cast<int8_t>(c.x + dx), static_cast<int8_t>(c.y + dy)});
        if (q && q->teamSide == TeamSide::HOME && q->state == PlayerState::STANDING) ++n;
    }
    return n;
}
TEST(OneCageWallShift, SlowerCageThatMayNotHitShiftsAlongTheWall) {
    // posun po řadách (P230) — bez čištění zdi P233, které má, když jde, přednost (testy OneCageWallClear)
    struct Off { Off() { setExtraFeaturesOff(kP233WallClear); } ~Off() { setExtraFeaturesOff(0); } } off;
    Board b = wallWithHelpers(4);
    const double before = blitzThreat(b.s, b.s.getPlayer(1));
    const CageTurn t = cageTurn(b);
    for (const Macro& m : t.macros) EXPECT_FALSE(m.type == MacroType::BLOCK || m.type == MacroType::BLITZ) << "hráč " << m.playerId;
    EXPECT_EQ(t.carrier.x, 12) << "stejný sloupec — neobíhá, neustupuje";
    EXPECT_NE(t.carrier.y, 7) << "klec se posunula po řadách (nestojí)";
    EXPECT_EQ(cornersAround(b.s, t.carrier), 4) << "celá klec: čtyři rohy";
    for (int dx = -1; dx <= 1; ++dx) for (int dy = -1; dy <= 1; ++dy) {
        if ((dx == 0) == (dy == 0)) continue;                         // jen pole přímo vedle nosiče
        const Player* q = b.s.getPlayerAtPosition({static_cast<int8_t>(t.carrier.x + dx), static_cast<int8_t>(t.carrier.y + dy)});
        EXPECT_FALSE(q && q->teamSide == TeamSide::HOME) << "naši vedle nosiče na (" << t.carrier.x + dx << "," << t.carrier.y + dy << ")";
    }
    EXPECT_EQ(b.s.getPlayer(4).position, (Position{13, 6})) << "přední roh drží zeď";
    EXPECT_EQ(b.s.getPlayer(5).position, (Position{13, 8}));
    EXPECT_LT(blitzThreat(b.s, b.s.getPlayer(1)), before) << "nosič po přestavbě bezpečněji";
}

// „jestli by na roh došel někdo bez GFI … asi by měl přednost“: hráč 9 (MA 1) stojí u rohu blíž, ale dojde
// jen přes GFI; roh dostane hráč, který dojde bez hodu.
TEST(OneCageWallShift, CornerGoesToAPlayerWithoutGfiFirst) {
    Board b = wallWithHelpers(4);
    b.s.getPlayer(6).position = {8, 3};
    b.put(9, TeamSide::HOME, {11, 1}, 1);
    const CageTurn t = cageTurn(b);
    ASSERT_EQ(t.carrier.x, 12);
    ASSERT_EQ(cornersAround(b.s, t.carrier), 4);
    for (const Macro& m : t.macros) EXPECT_FALSE(m.playerId == 9 && m.gfiAllowance > 0) << "roh přes GFI, i když šel bez hodu";
}

// Review 27.10.: nosič dojde jen o 1–2 řady. O řadu by stál přímo vedle vázaného předního rohu (klec = jen
// čtyři rohy ⇒ ne); o dvě řady je vázaný přední roh rohem nové klece (dřív se o dvě řady nedalo nikdy).
TEST(OneCageWallShift, TwoRowsWithTheBoundCornerAsACornerNotOneRowNextToIt) {
    // mechanika posunu po řadách; pořadí do strany / stát / dozadu hlídají testy OneCageShiftCap
    struct Off { Off() { setExtraFeaturesOff(kP230ShiftCap | kP233WallClear); } ~Off() { setExtraFeaturesOff(0); } } off;
    Board b = wallWithHelpers(4);
    b.s.getPlayer(1).movementRemaining = 2;
    const CageTurn t = cageTurn(b);
    EXPECT_EQ(t.carrier.x, 12);
    EXPECT_TRUE(t.carrier.y == 5 || t.carrier.y == 9) << "nosič (" << int(t.carrier.x) << "," << int(t.carrier.y) << ")";
    EXPECT_EQ(cornersAround(b.s, t.carrier), 4);
    EXPECT_TRUE(b.s.getPlayer(4).position == (Position{13, 6}) && b.s.getPlayer(5).position == (Position{13, 8}))
        << "přední rohy zůstaly u zdi";
}
TEST(OneCageWallShift, NeverOneRowNextToTheBoundCorner) {
    // mechanika posunu po řadách; pořadí do strany / stát / dozadu hlídají testy OneCageShiftCap
    struct Off { Off() { setExtraFeaturesOff(kP230ShiftCap); } ~Off() { setExtraFeaturesOff(0); } } off;
    Board b = wallWithHelpers(4);
    b.s.getPlayer(1).movementRemaining = 1;
    const CageTurn t = cageTurn(b);
    EXPECT_NE(t.carrier, (Position{12, 6})) << "vedle vázaného rohu (13,6)";
    EXPECT_NE(t.carrier, (Position{12, 8})) << "vedle vázaného rohu (13,8)";
}

// „Klec má být jen čtyři rohy“ (uživatel 27.10.) platí i pro zaostalce: dojde-li zaostalec nejdál ke sloupci
// nosiče, nestaví se na pole přímo vedle něj. Klec stojí (4 rohy), zaostalec MA4 z (8,7) dojde nejdál do
// sloupce 12.
TEST(OneCageLaggards, LaggardDoesNotStandRightNextToTheCarrier) {
    struct Off { Off() { setExtraFeaturesOff(kP228DirtyCorner | kP229WallBreak | kP230WallShift); } ~Off() { setExtraFeaturesOff(0); } } off;
    Board b = walledCageBoard(2);                                 // klec stojí (OneCagePin)
    b.put(6, TeamSide::HOME, {8, 7}, 4);
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    const Position cp = b.s.getPlayer(1).position;
    ASSERT_EQ(cp, (Position{12, 7})) << "předpoklad: klec stojí";
    const Position p6 = b.s.getPlayer(6).position;
    ASSERT_NE(p6, (Position{8, 7})) << "předpoklad: zaostalec šel dopředu";
    EXPECT_NE(std::abs(p6.x - cp.x) + std::abs(p6.y - cp.y), 1) << "zaostalec (" << int(p6.x) << "," << int(p6.y) << ") vedle nosiče";
}

// Review 27.10. (nález 5): přestavba P230 platí jen celá. První přesun na roh „nedojde“ (tady ho ručně vrátíme
// na start) ⇒ řadič zbytek přestavby nehraje (zbytek tahu dostane hledání) a nosič se k nedostavěné kleci nepřesune.
TEST(OneCageWallShift, AHalfBuiltShiftIsAbandoned) {
    Board b = wallWithHelpers(4);
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    ASSERT_TRUE(cc.next(b.s, m));
    ASSERT_EQ(m.type, MacroType::REPOSITION);
    ASSERT_NE(m.playerId, 1) << "přestavba začíná rohem";
    const Position start = b.s.getPlayer(m.playerId).position;
    play(b.s, m);
    b.s.getPlayer(m.playerId).position = start;                  // nedošel
    std::vector<Macro> rest;
    while (cc.next(b.s, m)) { rest.push_back(m); play(b.s, m); }
    for (const Macro& r : rest) EXPECT_NE(r.playerId, 1) << "nosič k nedostavěné kleci";
    EXPECT_EQ(b.s.getPlayer(1).position, (Position{12, 7}));
}

// Review 27.10.: roh přes GFI je ÚPLNĚ poslední akce tahu (nepovedené GFI = turnover) — až po nosiči,
// vazačích i zaostalcích. Na roh (13,3) dojde jen hráč 9 (MA 1) přes jedno GFI (ostatní mají málo pohybu).
TEST(OneCageWallShift, TheGfiCornerIsTheLastActionOfTheTurn) {
    Board b = wallWithHelpers(4);
    b.s.getPlayer(6).position = {2, 13};                          // daleko, na rohy nedojde
    b.s.getPlayer(8).position = {2, 1};
    b.s.getPlayer(7).position = {9, 3};                           // MA 2: dojde jen na (11,3)
    b.s.getPlayer(7).movementRemaining = 2;
    b.s.getPlayer(2).movementRemaining = 2;                       // zadní roh MA 2: na (13,3) nedojde
    b.put(9, TeamSide::HOME, {11, 1}, 1);
    const CageTurn t = cageTurn(b);
    int gfiAt = -1, carrierAt = -1;
    for (size_t i = 0; i < t.macros.size(); ++i) {
        if (t.macros[i].playerId == 1) carrierAt = static_cast<int>(i);
        if (t.macros[i].gfiAllowance > 0 && t.macros[i].playerId != 1) gfiAt = static_cast<int>(i);
    }
    ASSERT_GE(gfiAt, 0) << "předpoklad: klec se dostaví jen s rohem přes GFI";
    EXPECT_GT(gfiAt, carrierAt) << "roh přes GFI až po nosiči";
    EXPECT_EQ(gfiAt, static_cast<int>(t.macros.size()) - 1) << "a úplně poslední";
}

TEST(OneCageHits, OneDieOnlyWithBlockOrWrestle) {
    Board b(2);
    b.put(1, TeamSide::HOME, {12, 7});
    b.put(2, TeamSide::HOME, {12, 9}, 6, 3, {SkillName::Block});
    b.put(3, TeamSide::HOME, {12, 11}, 6, 3, {SkillName::Wrestle});
    b.put(4, TeamSide::HOME, {12, 13}, 6, 3, {SkillName::Wrestle});
    b.s.ball = BallState::carried({12, 13}, 4);
    EXPECT_TRUE(hitAllowed(b.s, b.s.getPlayer(1), 2));
    EXPECT_TRUE(hitAllowed(b.s, b.s.getPlayer(1), 3));
    EXPECT_FALSE(hitAllowed(b.s, b.s.getPlayer(1), 1)) << "1 kostka bez Block a Wrestle";
    EXPECT_TRUE(hitAllowed(b.s, b.s.getPlayer(2), 1)) << "1 kostka s Block";
    EXPECT_FALSE(hitAllowed(b.s, b.s.getPlayer(2), -2)) << "volí obránce";
    // uživatel 27.10.: „stejně jako bereš 1 kostku s block, tak ber stejně s wrestle — není turnover na both down“
    EXPECT_TRUE(hitAllowed(b.s, b.s.getPlayer(3), 1)) << "1 kostka s Wrestle";
    EXPECT_FALSE(hitAllowed(b.s, b.s.getPlayer(3), -2)) << "Wrestle nepomůže, když volí obránce";
    // ř. 8670–8677: Wrestle u nosiče = turnover (položí i jeho)
    EXPECT_FALSE(hitAllowed(b.s, b.s.getPlayer(4), 1)) << "nosič s Wrestle ne";
}

// Pomalejší klec bez předních rohů (jako CageAdvance.FillsTheCageWhenTheAdvanceCannotRun): plánovač
// by ji s P228 poslal kolem zdi; doktrína: pomalejší neobíhá ⇒ dostaví se na místě.
TEST(OneCageWallBreak, SlowerCageWithoutFrontCornersFillsInsteadOfGoingAround) {
    Board b = walledCageBoard(2);
    b.s.getPlayer(4).position = {10, 5};
    b.s.getPlayer(5).position = {10, 9};
    const CageTurn t = cageTurn(b);
    EXPECT_EQ(t.carrier, (Position{12, 7})) << "nosič neobíhá";
    const Player* f1 = b.s.getPlayerAtPosition({13, 6});
    const Player* f2 = b.s.getPlayerAtPosition({13, 8});
    EXPECT_TRUE(f1 && f1->teamSide == TeamSide::HOME) << "přední roh (13,6) dostavěn";
    EXPECT_TRUE(f2 && f2->teamSide == TeamSide::HOME) << "přední roh (13,8) dostavěn";
}

// (c) „pak co nejdříve dořešit pohyb zaostalců co nejvíce dopředu“
TEST(OneCageLaggards, FreePlayerLeftBehindMovesForwardWithoutContact) {
    struct Off { Off() { setExtraFeaturesOff(kP230Backup); } ~Off() { setExtraFeaturesOff(0); } } off;   // jediný zaostalec by zůstal zálohou
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

// P211b (uživatel 15.10.2026: „Rat Ogre nemá být roh klece — a jako zaostalý se zkusí pohnout až
// poté, co se provede blitz — jen pokud blitz není spuštěn, může pro pohyb využít akci blitz“).
// Pravidla ř. 8666-8669: Wild Animal hází po ohlášení každé akce, +2 jen u Block/Blitz.
namespace {
Board wildAnimalLaggardBoard() {
    Board b(1);
    b.put(1, TeamSide::HOME, {12, 7});
    b.put(2, TeamSide::HOME, {11, 6});
    b.put(3, TeamSide::HOME, {11, 8});
    b.put(4, TeamSide::HOME, {13, 6});
    b.put(5, TeamSide::HOME, {13, 8});
    b.put(6, TeamSide::HOME, {6, 3});                             // zaostalec s Wild Animal
    b.s.getPlayer(6).skills.add(SkillName::WildAnimal);
    b.put(14, TeamSide::AWAY, {24, 13});
    b.s.ball = BallState::carried({12, 7}, 1);
    return b;
}
}  // namespace

TEST(OneCageLaggards, P211bWildAnimalIsNotMovedDuringTheCageTurn) {
    Board b = wildAnimalLaggardBoard();
    CageController cc(nullptr, cfg(), 1);
    std::vector<Macro> ms;
    playAll(cc, b, &ms);
    for (const Macro& m : ms) EXPECT_NE(m.playerId, 6) << "řadič Wild Animal během tahu nepřesouvá";
    EXPECT_EQ(b.s.getPlayer(6).position, (Position{6, 3}));
}

TEST(OneCageLaggards, P211bWildAnimalLaggardDeclaresTheUnusedBlitzBeforeTheTurnEnds) {
    Board b = wildAnimalLaggardBoard();
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    Macro m;
    ASSERT_TRUE(cc.beforeEndTurn(b.s, m)) << "před koncem tahu zaostalec dojde";
    while (m.playerId != 6) ASSERT_TRUE(cc.next(b.s, m)) << "Ogre je ve frontě před koncem tahu";
    EXPECT_EQ(m.type, MacroType::REPOSITION);
    EXPECT_GT(m.targetPos.x, 6) << "dopředu";
    EXPECT_TRUE(m.declareBlitz) << "blitz zůstal nevyužitý ⇒ přesun ho ohlásí (+2)";
}

TEST(OneCageLaggards, P211bWildAnimalLaggardMovesPlainAfterTheBlitzWasUsed) {
    Board b = wildAnimalLaggardBoard();
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    b.s.homeTeam.blitzUsedThisTurn = true;                         // blitz už v tahu proběhl
    Macro m;
    ASSERT_TRUE(cc.beforeEndTurn(b.s, m));
    while (m.playerId != 6) ASSERT_TRUE(cc.next(b.s, m));
    EXPECT_EQ(m.type, MacroType::REPOSITION);
    EXPECT_FALSE(m.declareBlitz) << "blitz je pryč ⇒ obyčejný přesun";
}

// Review 16.10.2026: týmový blitz je jeden — dva zaostalí Wild Animal nesmí ve frontě oba nést ohlášení.
TEST(OneCageLaggards, TwoWildAnimalLaggardsDeclareTheTeamBlitzOnlyOnce) {
    Board b = wildAnimalLaggardBoard();
    b.put(7, TeamSide::HOME, {5, 11});                            // druhý zaostalec s Wild Animal
    b.s.getPlayer(7).skills.add(SkillName::WildAnimal);
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    Macro m;
    int declared = 0, laggards = 0;
    for (bool ok = cc.beforeEndTurn(b.s, m); ok; ok = cc.next(b.s, m)) {
        if (m.playerId == 6 || m.playerId == 7) { laggards++; declared += m.declareBlitz; }
    }
    ASSERT_EQ(laggards, 2) << "oba zaostalci jsou ve frontě před koncem tahu";
    EXPECT_EQ(declared, 1) << "blitz ohlásí jen jeden";
}

TEST(OneCageLaggards, StaleDeclareBlitzMacroDoesNotDeclareASecondBlitz) {
    Board b = wildAnimalLaggardBoard();
    b.s.homeTeam.blitzUsedThisTurn = true;                         // blitz už spotřebovalo jiné makro
    Macro m{MacroType::REPOSITION, 6, -1, {9, 5}};
    m.declareBlitz = true;                                         // zastaralé makro z dřívější nabídky
    DiceRoller dice(7);
    const auto res = greedyExpandMacro(b.s, m, dice);
    for (const Action& a : res.actions) EXPECT_NE(a.type, ActionType::BLITZ) << "druhý blitz v tahu";
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

// Review 16.10.2026: P211b i pro pomocníka u asistence — pomocník se k soupeři PŘESOUVÁ, a Wild Animal
// řadič klece během tahu nepřesouvá. Tatáž pozice jako výše, jen volný pomocník má Wild Animal.
TEST(OneCageFreeing, WildAnimalDoesNotWalkInToAssist) {
    struct OldPath { OldPath() { setCageFeaturesOff(kFeatCleanCageSearch); } ~OldPath() { setCageFeaturesOff(0); } } oldPath;
    Board b = heldTeammateBoard();
    b.put(5, TeamSide::HOME, {16, 4}, 4, 3, {SkillName::Block});
    b.put(14, TeamSide::AWAY, {17, 5}, 6);
    b.put(6, TeamSide::HOME, {13, 2});
    b.s.getPlayer(6).skills.add(SkillName::WildAnimal);
    CageController cc(nullptr, cfg(), 1);
    std::vector<Macro> ms;
    playAll(cc, b, &ms);
    for (const Macro& m : ms) EXPECT_NE(m.playerId, 6) << "Wild Animal nejde pro asistenci";
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
    // P219 (agilní blitz jen při šanci srazit × odskočit ≥ 0,5) by tu blitz nenabídl — test hlídá jiné pravidlo
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
    // Hlídá P169 (Frenzy roh neblokuje) u klece, která stojí. S P228–P230 klec u zdi bez volných hráčů stát
    // nezůstane (nosič odejde na pole s nižší hrozbou) — jiné pravidlo, proto bez nich.
    struct Off { Off() { setExtraFeaturesOff(kP228DirtyCorner | kP229WallBreak | kP230WallShift); } ~Off() { setExtraFeaturesOff(0); } } off;
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
    // P219 (agilní blitz jen při šanci srazit × odskočit ≥ 0,5) by tu blitz nenabídl — test hlídá jiné pravidlo
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
    P217Off p217off;
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
    P217Off p217off;
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

// P217 (1) (uživatel 19.10.: bezpečná klec, ze které nosič dojde do zóny bez hodu — „pokud to jde —
// přibližujeme se k TD zóně“; bezpečná klec obecně „postupujeme dopředu“ = bankování). Nosič MA 6 šest
// polí před zónou, kolem něj klec, soupeř daleko: zdržuje se (TD se nehraje), ale klec postoupí k zóně
// a nosič zůstane před ní. Pozitivní kontrola: s vypnutou úpravou nosič stojí (jako dosud).
TEST(OneCageStall, TheStallingCageStillAdvancesTowardsTheEndZone) {
    auto run = [](bool banks) {
        if (!banks) setExtraFeaturesOff(kP217StallBanks);
        Board b(4);
        b.put(1, TeamSide::HOME, {19, 7}, 6);
        b.put(2, TeamSide::HOME, {18, 6}); b.put(3, TeamSide::HOME, {18, 8});
        b.put(4, TeamSide::HOME, {20, 6}); b.put(5, TeamSide::HOME, {20, 8});
        b.put(13, TeamSide::AWAY, {3, 7}, 6);
        b.s.ball = BallState::carried({19, 7}, 1);
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        Macro m;
        if (cc.beforeEndTurn(b.s, m)) { play(b.s, m); playAll(cc, b, &ms); }
        setExtraFeaturesOff(0);
        for (const Macro& x : ms) EXPECT_NE(x.type, MacroType::SCORE) << "zdržuje se — TD se nehraje";
        EXPECT_TRUE(cc.forbidsCarrierMove(b.s, Macro{MacroType::SCORE, 1, -1, {-1, -1}})) << "předpoklad: zdržovací tah";
        EXPECT_EQ(b.s.homeTeam.score, 0);
        EXPECT_LT(b.s.getPlayer(1).position.x, 25) << "nosič zůstal před zónou";
        EXPECT_LE(blitzThreat(b.s, b.s.getPlayer(1)), kSafeBlitzThreat) << "a v bezpečí";
        return b.s.getPlayer(1).position.x;
    };
    EXPECT_GT(run(true), 19) << "klec postoupila k zóně";
    EXPECT_EQ(run(false), 19) << "pozitivní kontrola: bez úpravy nosič stojí";
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
    P217Off p217off;
    // 16.10. večer: útěk do bezpečí (P213) má přednost — tady by vyhrál; test zkouší samotné P180
    struct NoEscape { NoEscape() { setCageFeaturesOff(kFeatSafetyHandOff); } ~NoEscape() { setCageFeaturesOff(0); } };
    {
        NoEscape off;
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
    {   // pojistka měření: s vypnutou úpravou řadič předávku nepřikazuje (P213 — předávka kvůli bezpečí —
        // by tu jinak předávala taky, správně: nosiči hrozí rána a příjemce uteče; proto vypnutá i ona)
        setCageFeaturesOff(kFeatScoreViaMate | kFeatSafetyHandOff);
        Board b = mateBoard(4, {10, 7}, 5);
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        setCageFeaturesOff(0);
        for (const Macro& m : ms) EXPECT_NE(m.type, MacroType::HAND_OFF_SCORE);
    }
}

// P180 (6), uživatel 16.10.2026: „poslední kolo poločasu je poslední šance dát TD — vše pro dát TD —
// vyhledat nejbezpečnější možnost“. V 8. kole nosič sám nedosáhne, předávkou TD jde ⇒ řadič přikáže
// TD předávkou (dřív v posledním kole rozhodovalo hledání a klec šla dál).
TEST(OneCageMate, InTheLastTurnTheControllerOrdersTheSafestTouchdown) {
    {
        Board b = mateBoard(8, {0, 0});
        ASSERT_GT(handOffTdChance(b.s, b.s.getPlayer(1), b.s.getPlayer(6)), 0.6);
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        ASSERT_FALSE(ms.empty());
        EXPECT_EQ(ms.front().type, MacroType::HAND_OFF_SCORE) << "poslední kolo: TD předávkou";
        EXPECT_EQ(ms.front().targetId, 6);
    }
    {   // pojistka měření: s vypnutou úpravou řadič TD v posledním kole nepřikazuje
        setCageFeaturesOff(kFeatLastTurnBestTd);
        Board b = mateBoard(8, {0, 0});
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        setCageFeaturesOff(0);
        for (const Macro& m : ms) EXPECT_NE(m.type, MacroType::HAND_OFF_SCORE);
    }
}

// P213, uživatel 16.10.2026: „pokud uteče — předat — pokud byl předtím v bezpečí — zdržovat“. Pomalý nosič
// bez klece (MA4), silný soupeř dvě pole od něj; vedle stojí rychlý spoluhráč (MA9, Catch), který do zóny
// nedoběhne (16 polí), ale s míčem uteče z dosahu ⇒ řadič přikáže předávku kvůli bezpečí. Bez soupeře
// (míč v bezpečí) se nepředává.
// Uživatel 16.10. večer: „útěk do bezpečí je lepší než předčasné TD … pozdější TD znamená méně příležitostí
// pro soupeře vyrovnat“; hody jsou stejné ⇒ TD předávkou jen, když bezpečné pole mimo dosah není.
// Tatáž pozice jako u TD předávkou: příjemce by skóroval, ale může i utéct ⇒ útěk.
TEST(OneCageMate, EscapeToSafetyIsPreferredToAnEarlyHandOffTouchdown) {
    P217Off p217off;
    Board b = mateBoard(4, {10, 7}, 5);
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    ASSERT_TRUE(cc.next(b.s, m));
    EXPECT_EQ(m.type, MacroType::HAND_OFF_SCORE);
    EXPECT_NE(m.targetPos.x, 25) << "útěk, ne TD";
    setCageFeaturesOff(kFeatSafetyHandOff);   // pojistka: bez P213 TD předávkou
    CageController cc2(nullptr, cfg(), 1);
    Board b2 = mateBoard(4, {10, 7}, 5);
    ASSERT_TRUE(cc2.next(b2.s, m));
    setCageFeaturesOff(0);
    EXPECT_EQ(m.targetPos.x, 25);
    setExtraFeaturesOff(kP213EscapeFirst | kP217BestChance);        // pojistka měření: staré pořadí ⇒ TD předávkou
    CageController cc3(nullptr, cfg(), 1);
    Board b3 = mateBoard(4, {10, 7}, 5);
    ASSERT_TRUE(cc3.next(b3.s, m));
    setExtraFeaturesOff(kP217BestChance);
    EXPECT_EQ(m.targetPos.x, 25);
}

TEST(OneCageMate, AThreatenedCarrierHandsOffToATeammateWhoRunsOutOfReach) {
    auto board = [](bool threatened) {
        Board b(2);
        b.put(1, TeamSide::HOME, {8, 7}, 4);
        b.put(6, TeamSide::HOME, {9, 7}, 9, 3, {SkillName::Catch}).stats.agility = 4;
        if (threatened) b.put(13, TeamSide::AWAY, {6, 7}, 6, 5);   // silný soupeř dvě pole od nosiče
        else b.put(13, TeamSide::AWAY, {1, 1}, 3, 5);               // pomalý a daleko: na nosiče nedosáhne
        b.s.ball = BallState::carried({8, 7}, 1);
        return b;
    };
    {
        Board b = board(true);
        ASSERT_GT(blitzThreat(b.s, b.s.getPlayer(1)), kSafeBlitzThreat) << "předpoklad: nosiči hrozí rána";
        CageController cc(nullptr, cfg(), 1);
        Macro m;
        ASSERT_TRUE(cc.next(b.s, m));
        EXPECT_EQ(m.type, MacroType::HAND_OFF_SCORE);
        EXPECT_EQ(m.targetId, 6);
        EXPECT_NE(m.targetPos.x, 25) << "ne TD — bezpečné pole";
        GameState proj = b.s.clone();
        proj.getPlayer(6).position = m.targetPos;
        proj.ball = BallState::carried(m.targetPos, 6);
        EXPECT_LE(blitzThreat(proj, proj.getPlayer(6)), kSafeBlitzThreat) << "příjemce je mimo dosah";
    }
    {
        Board b = board(false);
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        for (const Macro& m : ms) EXPECT_NE(m.type, MacroType::HAND_OFF_SCORE) << "míč v bezpečí ⇒ zdržovat";
    }
    {   // pojistka měření
        setCageFeaturesOff(kFeatSafetyHandOff);
        Board b = board(true);
        CageController cc(nullptr, cfg(), 1);
        Macro m;
        const bool any = cc.next(b.s, m);
        setCageFeaturesOff(0);
        if (any) EXPECT_NE(m.type, MacroType::HAND_OFF_SCORE);
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
    P217Off p217off;
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

// P216 (uživatel 18.10.2026: „stejně jako řeší kdo jej blitzne a zkouší utéct z dosahu — tak zkouší utéct i
// z možnosti obklíčit“). Pozice jako výše, ale před zónou čekají tři slabí soupeři (ST2 — rána na připraveného
// nehrozí), kteří ke každému připravenému poli příští tah dojdou a obstoupí ho. Neobratný připravený (AG3, bez
// Nerves of Steel) by míč ve třech zónách chytil na 6+ ⇒ takové pole bezpečné není.
// Záloha (uživatel 18.10.: „blitz je jeden a připravených může být více … když nejde nachystat tak ať jednoho
// blitznou a další obklíčí — tak je stejně chystej“; „když bude více hráčů volných se nachystat — tak může
// být“) ⇒ chystají se všichni volní (tady dva), mezi sebou aspoň dvě pole.
TEST(OneCageMate, WhenNoReadySquareIsSafeTwoPlayersGetReadyAnywayApartFromEachOther) {
    auto run = [](unsigned cageOff, unsigned extraOff) {
        Board b(2);
        b.put(1, TeamSide::HOME, {8, 7}, 4);
        b.put(2, TeamSide::HOME, {7, 6}, 9); b.put(3, TeamSide::HOME, {7, 8}, 9);
        b.put(4, TeamSide::HOME, {9, 6}, 9); b.put(5, TeamSide::HOME, {9, 8}, 9);
        b.put(6, TeamSide::HOME, {8, 3}, 9);
        b.put(7, TeamSide::HOME, {8, 11}, 9);
        for (int i = 0; i < 3; ++i) b.put(13 + i, TeamSide::AWAY, {20, static_cast<int8_t>(3 + 4 * i)}, 6, 2);
        b.s.ball = BallState::carried({8, 7}, 1);
        setCageFeaturesOff(cageOff);
        setExtraFeaturesOff(extraOff);
        CageController cc(nullptr, cfg(), 1);
        playAll(cc, b);
        setCageFeaturesOff(0);
        setExtraFeaturesOff(0);
        const Player& c = b.s.getPlayer(1);
        std::vector<Position> ready;
        for (int id = 2; id <= 7; ++id) {
            const Player& m = b.s.getPlayer(id);
            const bool corner = std::abs(m.position.x - c.position.x) == 1 && std::abs(m.position.y - c.position.y) == 1;
            if (!corner && (25 - m.position.x) <= 9 && m.position.distanceTo(c.position) - 1 <= 4) ready.push_back(m.position);
        }
        return ready;
    };
    const std::vector<Position> two = run(0, 0);
    ASSERT_GE(two.size(), 2u) << "bezpečné pole není ⇒ chystají se oba volní";
    for (size_t i = 0; i < two.size(); ++i)
        for (size_t j = i + 1; j < two.size(); ++j) EXPECT_GE(two[i].distanceTo(two[j]), 3) << "jeden obránce neoznačí dva";
    EXPECT_TRUE(run(0, kP216ReadyAnyway).empty()) << "bez zálohy: obklíčené pole se nepoužije";
    EXPECT_FALSE(run(kFeatReadySurround, kP216ReadyAnyway).empty()) << "pozitivní kontrola: bez P216 se jeden postaví";
}

// P180 (7), uživatel 16.10.2026: „u elfů všichni, kdo netvoří klec a neblitzují a dojdou — postaví se o dvě
// od sebe, ať obránce nemůže markovat dva jedním svým“; „platí pro agilní spíše než rychlé“.
TEST(OneCageMate, AgilePlayersGetReadySeveralAndAtLeastThreeSquaresApart) {
    P217Off p217off;
    auto run = [](int8_t ag, unsigned off) {
        Board b(2);
        b.put(1, TeamSide::HOME, {8, 7}, 4);
        b.put(2, TeamSide::HOME, {7, 6}, 5); b.put(3, TeamSide::HOME, {7, 8}, 5);   // rohy MA5: k zóně nedojdou
        b.put(4, TeamSide::HOME, {9, 6}, 5); b.put(5, TeamSide::HOME, {9, 8}, 5);
        b.put(6, TeamSide::HOME, {7, 1}, 9).stats.agility = ag;                      // volní, od rohů dál než rohy
        b.put(7, TeamSide::HOME, {7, 13}, 9).stats.agility = ag;
        b.put(13, TeamSide::AWAY, {24, 13}, 4);
        b.s.ball = BallState::carried({8, 7}, 1);
        setCageFeaturesOff(off);
        CageController cc(nullptr, cfg(), 1);
        playAll(cc, b);
        setCageFeaturesOff(0);
        std::vector<Position> sq;
        for (int id : cc.readyMates()) sq.push_back(b.s.getPlayer(id).position);
        return sq;
    };
    const auto agile = run(4, 0);
    ASSERT_GE(agile.size(), 2u) << "agilní: víc připravených";
    for (size_t i = 0; i < agile.size(); ++i)
        for (size_t j = i + 1; j < agile.size(); ++j)
            EXPECT_GE(agile[i].distanceTo(agile[j]), 3) << "jeden obránce nesmí markovat dva";
    EXPECT_EQ(run(3, 0).size(), 1u) << "neagilní: jeden připravený";
    EXPECT_EQ(run(4, kFeatReadyMany).size(), 1u) << "pojistka: bez úpravy jeden";
}

// P180 (9), uživatel 16.10.2026: „preferujeme ne, ale zahrnujeme i ano“ — když žádné pole bez hodu
// nejde, připraví se hráč i na pole, odkud do zóny doběhne s GFI. MA8 z x=7: tento tah nejdál x=15,
// bez GFI by potřeboval x ≥ 17, s GFI stačí x ≥ 15.
TEST(OneCageMate, AReadySquareNeedingGfiIsUsedOnlyWhenThereIsNoOther) {
    P217Off p217off;
    auto run = [](unsigned off) {
        Board b(2);
        b.put(1, TeamSide::HOME, {8, 7}, 4);
        b.put(2, TeamSide::HOME, {7, 6}, 5); b.put(3, TeamSide::HOME, {7, 8}, 5);   // rohy MA5: k zóně nedojdou
        b.put(4, TeamSide::HOME, {9, 6}, 5); b.put(5, TeamSide::HOME, {9, 8}, 5);
        b.put(6, TeamSide::HOME, {7, 1}, 8);                                         // od rohů dál než rohy
        b.put(13, TeamSide::AWAY, {24, 13}, 4);
        b.s.ball = BallState::carried({8, 7}, 1);
        setCageFeaturesOff(off);
        CageController cc(nullptr, cfg(), 1);
        playAll(cc, b);
        setCageFeaturesOff(0);
        std::vector<int> r = cc.readyMates();
        return std::make_pair(r, b.s.getPlayer(6).position);
    };
    const auto [withGfi, pos] = run(0);
    ASSERT_EQ(withGfi.size(), 1u) << "připraví se i přes GFI";
    EXPECT_EQ(withGfi.front(), 6);
    EXPECT_GE(pos.x, 15);
    EXPECT_TRUE(run(kFeatReadyMateGfi).first.empty()) << "pojistka: bez úpravy se nechystá nikdo";
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
    // pojistka vypíná i P213: od 17.10. (útěk smí mít úhyb) nosiče z kontaktu dostane i předávka kvůli bezpečí
    const Position before = run(4, kFeatEscapeContact | kFeatSafetyHandOff, t);
    EXPECT_EQ(before, (Position{12, 7})) << "pozitivní kontrola: bez úpravy řadič nosiče v kontaktu nechal stát";
    EXPECT_GT(t, kSafeBlitzThreat);
    const Position clumsy = run(3, 0, t);
    EXPECT_EQ(clumsy, (Position{12, 7})) << "nosič s úhybem na 3+ neuhýbá";
}

// P190: „roh tam dojde“ znamená cestu bez hodu, ne jen vzdálenost. Spoluhráč stojí čtyři pole od
// pole rohu, ale mezi nimi je souvislá zeď soupeřů — nedojde, a odhad bezpečí nosiče s ním nesmí počítat
// (nosič po zvednutí míče odbíhal před tým k „rohům“, které uvízly v půli cesty).
TEST(CarrierThreat, ATeammateBehindAWallIsNotCountedAsAComingCorner) {
    Board b;
    b.put(1, TeamSide::HOME, {12, 7}, 6);
    b.put(2, TeamSide::HOME, {13, 12});
    for (int x = 9; x <= 17; ++x) b.put(4 + x, TeamSide::AWAY, {static_cast<int8_t>(x), 10}, 6);
    const Player& c = b.s.getPlayer(1);
    EXPECT_EQ(cornersWithinReach(b.s, c, {12, 7}), 0) << "přes zeď se na roh (13,8) nedostane";
    setCageFeaturesOff(kFeatCornerPathReach);
    EXPECT_EQ(cornersWithinReach(b.s, c, {12, 7}), 1) << "pozitivní kontrola: podle vzdálenosti by „došel“";
    setCageFeaturesOff(0);
}

// P213, uživatel 17.10.2026: „i útěk může obsahovat GFI“ a „více riskantní pouze když není jiná možnost
// mín riskantní“. Pozice jako výše, ale pomalý příjemce (MA3): bez hodu z dosahu neuteče, s GFI ano ⇒ předá
// a příjemce utíká s GFI (dřív se nepředávalo vůbec). Rychlý příjemce (MA6) bezpečné pole bez hodu má ⇒ GFI
// nehraje.
TEST(OneCageMate, TheEscapeMayUseGoingForItOnlyWhenNoRollFreeSafeSquareExists) {
    auto board = [](int ma) {
        Board b(2);
        b.put(1, TeamSide::HOME, {8, 7}, 4);
        b.put(6, TeamSide::HOME, {9, 7}, ma, 3, {SkillName::Catch}).stats.agility = 4;
        b.put(13, TeamSide::AWAY, {6, 7}, 6, 5);
        b.s.ball = BallState::carried({8, 7}, 1);
        return b;
    };
    {
        Board b = board(3);
        CageController cc(nullptr, cfg(), 1);
        Macro m;
        ASSERT_TRUE(cc.next(b.s, m));
        ASSERT_EQ(m.type, MacroType::HAND_OFF_SCORE) << "pomalý příjemce: útěk s GFI";
        const Player& r = b.s.getPlayer(6);
        EXPECT_GT(r.position.distanceTo(m.targetPos), 3) << "cíl je za pohybem příjemce";
        EXPECT_LE(riskTier(pathFailProb(b.s, r, m.targetPos, 3 + maxGfiSquares(r), Position{-1, -1})), 1);
    }
    {   // pojistka měření: bez části „útěk s hodem“ pomalý příjemce míč nedostane
        setExtraFeaturesOff(kP213EscapeRolls);
        Board b = board(3);
        CageController cc(nullptr, cfg(), 1);
        Macro m;
        const bool any = cc.next(b.s, m);
        setExtraFeaturesOff(0);
        if (any) EXPECT_NE(m.type, MacroType::HAND_OFF_SCORE);
    }
    {
        Board b = board(6);
        CageController cc(nullptr, cfg(), 1);
        Macro m;
        ASSERT_TRUE(cc.next(b.s, m));
        ASSERT_EQ(m.type, MacroType::HAND_OFF_SCORE);
        const Player& r = b.s.getPlayer(6);
        EXPECT_DOUBLE_EQ(pathFailProb(b.s, r, m.targetPos, 6, Position{-1, -1}), 0.0) << "rychlý: bez hodu";
    }
}

// P217 (3) (uživatel 19.10.: „přednost má to, co má nejnižší pravděpodobnost neúspěchu“). Osamělý nosič MA 6
// osm polí před zónou, soupeř v dosahu: TD teď jen přes dvě GFI (0,72). Výběh o 6 polí bez hodu ho dostane
// z dosahu soupeře (ten má MA 6): míč přežije jistě (1,00) ⇒ výběh, ne TD přes hody. Pozitivní
// kontrola: bez P217 přikáže řadič TD hned (P178).
TEST(OneCageBestChance, ARunOutOfReachBeatsATouchdownThroughTwoRushes) {
    auto board = [] {
        Board b(5);
        b.put(1, TeamSide::HOME, {17, 7}, 6);
        b.put(13, TeamSide::AWAY, {13, 7}, 6);
        b.s.ball = BallState::carried({17, 7}, 1);
        return b;
    };
    {
        Board b = board();
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        for (const Macro& m : ms) EXPECT_NE(m.type, MacroType::SCORE) << "TD přes dvě GFI se nehraje";
        EXPECT_GT(b.s.getPlayer(1).position.x, 17) << "nosič vyběhl";
        EXPECT_LT(b.s.getPlayer(1).position.x, 25);
        EXPECT_LE(blitzThreat(b.s, b.s.getPlayer(1)), kSafeBlitzThreat) << "mimo dosah soupeře";
    }
    {
        P217Off off;
        Board b = board();
        CageController cc(nullptr, cfg(), 1);
        std::vector<Macro> ms;
        playAll(cc, b, &ms);
        ASSERT_FALSE(ms.empty());
        EXPECT_EQ(ms.front().type, MacroType::SCORE) << "pozitivní kontrola: bez P217 TD hned";
    }
}

// P217 (3): při stejné šanci vyhrává pozdější TD (uživatel 16.10.: „pozdější TD znamená méně příležitostí pro
// soupeře vyrovnat“). Pozice z P213: TD předávkou teď i útěk s předávkou vycházejí stejně ⇒ útěk.
// Staré pořadí P213 (útěk napřed) je vypnuté, aby rozhodlo jen P217; pozitivní kontrola: bez obou TD teď.
TEST(OneCageBestChance, OnATieTheLaterTouchdownWins) {
    auto first = [](unsigned extraOff) {
        setExtraFeaturesOff(extraOff);
        Board b = mateBoard(4, {10, 7}, 5);
        CageController cc(nullptr, cfg(), 1);
        Macro m;
        const bool any = cc.next(b.s, m);
        setExtraFeaturesOff(0);
        EXPECT_TRUE(any);
        EXPECT_EQ(m.type, MacroType::HAND_OFF_SCORE);
        return m.targetPos.x;
    };
    EXPECT_NE(first(kP213EscapeFirst), 25) << "P217: shoda ⇒ útěk, ne TD teď";
    EXPECT_EQ(first(kP213EscapeFirst | kP217BestChance), 25) << "pozitivní kontrola: bez P217 a starého pořadí TD teď";
}

// ⭐ P230 záloha vzadu (uživatel 27.10.2026: „vzadu nechat zálohu, co může blitznout soupeře, když nám on sebere
// míč — jednoho nebo dva, ať pokryjí šířku plochy“; „první všechny zadní hráče až na jednoho poslat blíž k míči
// dopředu“). Klec stojí v sloupci 12; zaostalec, který jde dopředu, dojde nejméně do sloupce 11, záloha stojí 2+ sloupce za nosičem.
namespace {
// Klec stojí (zeď na x=14, P228–P230 vypnuté jako v OneCageLaggards.LaggardDoesNotStandRightNextToTheCarrier).
struct WallOff { WallOff() { setExtraFeaturesOff(kP228DirtyCorner | kP229WallBreak | kP230WallShift); } ~WallOff() { setExtraFeaturesOff(0); } };
Board backupBoard() { return walledCageBoard(2); }
// Dojde záloha `p` blitzem (pohyb + pole vedle) na řadu `y` ve sloupci nosiče?
bool covers(const Player& p, Position cp, int y) {
    const Position sq{cp.x, static_cast<int8_t>(y)};
    return static_cast<int>(p.position.distanceTo(sq)) <= p.stats.movement + 1;
}
bool behind(const Player& p, Position cp) { return cp.x - p.position.x >= 2; }
}  // namespace

// Rychlý hráč (MA 6) pokryje šířku sám ⇒ vzadu zůstane jen on, ostatní zaostalci jdou dopředu.
TEST(OneCageBackup, OneFastPlayerStaysBackTheOthersGoForward) {
    WallOff off;
    Board b = backupBoard();
    b.put(6, TeamSide::HOME, {8, 3}, 4);
    b.put(7, TeamSide::HOME, {8, 11}, 6);
    b.put(8, TeamSide::HOME, {7, 7}, 4);
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    const Position cp = b.s.getPlayer(1).position;
    int back = 0;
    for (int id : {6, 7, 8}) back += behind(b.s.getPlayer(id), cp);
    EXPECT_EQ(back, 1) << "vzadu jeden, ostatní dopředu";
    const Player& z = b.s.getPlayer(7);
    EXPECT_TRUE(behind(z, cp)) << "záloha je ten, kdo pokryje šířku (MA 6)";
    for (int y = 1; y <= 13; ++y) EXPECT_TRUE(covers(z, cp, y)) << "řada " << y << " bez zálohy";
    EXPECT_EQ(countTacklezones(b.s, z.position, TeamSide::HOME), 0);
    EXPECT_GT(b.s.getPlayer(6).position.x, 8) << "zaostalec šel dopředu";
    EXPECT_GT(b.s.getPlayer(8).position.x, 7) << "zaostalec šel dopředu";
}

// Pomalý tým (vše MA 4): jeden šířku nepokryje ⇒ vzadu dva, každý na jedné polovině, dohromady řady 1–13.
TEST(OneCageBackup, SlowTeamKeepsTwoBackupsCoveringTheWidth) {
    WallOff off;
    Board b = backupBoard();
    b.put(6, TeamSide::HOME, {8, 3}, 4);
    b.put(7, TeamSide::HOME, {8, 11}, 4);
    b.put(8, TeamSide::HOME, {7, 7}, 4);
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    const Position cp = b.s.getPlayer(1).position;
    std::vector<const Player*> zs;
    for (int id : {6, 7, 8}) if (behind(b.s.getPlayer(id), cp)) zs.push_back(&b.s.getPlayer(id));
    ASSERT_EQ(zs.size(), 2u) << "vzadu dva, třetí dopředu";
    for (int y = 1; y <= 13; ++y)
        EXPECT_TRUE(covers(*zs[0], cp, y) || covers(*zs[1], cp, y)) << "řada " << y << " bez zálohy";
}

// Záloha se bere jen ze zaostalců — hráče před klecí zpátky netahá.
// Příprava na přihrávku (P180) vypnutá — jinak by si oba hráče vzala a test by filtr nezkoušel.
TEST(OneCageBackup, NoOneIsPulledBackFromTheFront) {
    WallOff off;
    struct ReadyOff { ReadyOff() { setCageFeaturesOff(kFeatReadyMate); } ~ReadyOff() { setCageFeaturesOff(0); } } readyOff;
    Board b = backupBoard();
    b.put(6, TeamSide::HOME, {13, 2}, 6);
    b.put(7, TeamSide::HOME, {13, 12}, 6);
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    const Position cp = b.s.getPlayer(1).position;
    for (int id : {6, 7}) EXPECT_FALSE(behind(b.s.getPlayer(id), cp)) << id;
}

// Review 27.10. (H1): záloha, která už stojí na svém poli, nedostane od řadiče přesun — hledání ji přesto
// nepošle dopředu k nosiči (méně než 2 sloupce za něj); přesun, po kterém zůstane vzadu, smí.
TEST(OneCageBackup, BackupAlreadyInPlaceIsNotSentForwardBySearch) {
    WallOff off;
    Board b = backupBoard();
    b.put(7, TeamSide::HOME, {10, 7}, 6);                         // už stojí jako záloha
    CageController cc(nullptr, cfg(), 1);
    std::vector<Macro> ms;
    playAll(cc, b, &ms);
    ASSERT_EQ(b.s.getPlayer(1).position, (Position{12, 7})) << "předpoklad: klec stojí";
    ASSERT_EQ(b.s.getPlayer(7).position, (Position{10, 7}));
    EXPECT_TRUE(cc.forbidsCarrierMove(b.s, Macro{MacroType::REPOSITION, 7, -1, {12, 3}})) << "záloha k nosiči";
    EXPECT_TRUE(cc.forbidsCarrierMove(b.s, Macro{MacroType::REPOSITION, 7, -1, {11, 3}})) << "záloha sloupec za nosiče";
    EXPECT_FALSE(cc.forbidsCarrierMove(b.s, Macro{MacroType::REPOSITION, 7, -1, {10, 5}})) << "zůstane vzadu";
    EXPECT_TRUE(cc.forbidsCarrierMove(b.s, Macro{MacroType::BLITZ, 7, 13, {-1, -1}})) << "blitz na zeď u klece";
    EXPECT_TRUE(cc.forbidsCarrierMove(b.s, Macro{MacroType::FOUL, 7, 13, {-1, -1}})) << "faul u klece";
}

// Soupeřova strana (útok doleva): totéž zrcadlově — vzadu jeden rychlý, ostatní dopředu.
TEST(OneCageBackup, AwayTeamKeepsTheBackupBehindToo) {
    WallOff off;
    Board b(2);
    b.s.activeTeam = TeamSide::AWAY;
    b.s.awayTeam.turnNumber = 2;
    b.put(13, TeamSide::AWAY, {13, 7}, 4);
    b.put(14, TeamSide::AWAY, {14, 6});
    b.put(15, TeamSide::AWAY, {14, 8});
    b.put(16, TeamSide::AWAY, {12, 6});
    b.put(17, TeamSide::AWAY, {12, 8});
    for (int i = 0; i < 5; ++i) b.put(1 + i, TeamSide::HOME, {11, static_cast<int8_t>(5 + i)}, 6);
    b.put(18, TeamSide::AWAY, {17, 3}, 4);
    b.put(19, TeamSide::AWAY, {17, 11}, 6);
    b.put(20, TeamSide::AWAY, {18, 7}, 4);
    b.s.ball = BallState::carried({13, 7}, 13);
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    const Position cp = b.s.getPlayer(13).position;
    auto behindAway = [&](int id) { return b.s.getPlayer(id).position.x - cp.x >= 2; };
    int back = 0;
    for (int id : {18, 19, 20}) back += behindAway(id);
    EXPECT_EQ(back, 1) << "vzadu jeden, ostatní dopředu";
    EXPECT_TRUE(behindAway(19)) << "záloha je ten, kdo pokryje šířku (MA 6)";
    for (int y = 1; y <= 13; ++y) EXPECT_TRUE(covers(b.s.getPlayer(19), cp, y)) << "řada " << y;
}

// Review 27.10. (M2): nosič mimo prostřední řadu (3) — dva pomalí v záloze pokryjí přesto řady 1–13.
TEST(OneCageBackup, CarrierOffTheMiddleRowStillTheWholeWidthIsCovered) {
    WallOff off;
    Board b(2);
    b.put(1, TeamSide::HOME, {12, 3}, 4);
    b.put(2, TeamSide::HOME, {11, 2});
    b.put(3, TeamSide::HOME, {11, 4});
    b.put(4, TeamSide::HOME, {13, 2});
    b.put(5, TeamSide::HOME, {13, 4});
    for (int i = 0; i < 5; ++i) b.put(13 + i, TeamSide::AWAY, {14, static_cast<int8_t>(1 + i)}, 6);
    b.s.ball = BallState::carried({12, 3}, 1);
    b.put(6, TeamSide::HOME, {8, 5}, 4);
    b.put(7, TeamSide::HOME, {8, 11}, 4);
    b.put(8, TeamSide::HOME, {7, 8}, 4);
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    const Position cp = b.s.getPlayer(1).position;
    std::vector<const Player*> zs;
    for (int id : {6, 7, 8}) if (behind(b.s.getPlayer(id), cp)) zs.push_back(&b.s.getPlayer(id));
    ASSERT_EQ(zs.size(), 2u);
    for (int y = 1; y <= 13; ++y)
        EXPECT_TRUE(covers(*zs[0], cp, y) || covers(*zs[1], cp, y)) << "řada " << y << " bez zálohy";
}

// Review 27.10. (M3): kdo už vzadu stojí a v tahu hrál (blok z místa), zálohu dělá — zaostalci jdou všichni dopředu.
TEST(OneCageBackup, PlayerWhoAlreadyActedBehindIsTheBackup) {
    WallOff off;
    Board b = backupBoard();
    Player& done = b.put(9, TeamSide::HOME, {10, 7}, 6);
    done.hasActed = true;
    done.movementRemaining = 0;
    b.put(6, TeamSide::HOME, {8, 3}, 4);
    b.put(8, TeamSide::HOME, {7, 7}, 4);
    CageController cc(nullptr, cfg(), 1);
    playAll(cc, b);
    const Position cp = b.s.getPlayer(1).position;
    EXPECT_FALSE(behind(b.s.getPlayer(6), cp)) << "zaostalec dopředu";
    EXPECT_FALSE(behind(b.s.getPlayer(8), cp)) << "zaostalec dopředu";
}

// ⭐ P230 pořadí (uživatel 27.10.2026: „jsem pro posun do boku s rizikem okolo 0,2 — posun kupředu je víc než stát
// nebo dozadu“): do strany do hranice (P232: s časovou rezervou 0,15, bez ní 0,30), jinak stát (na místě do hranice), jinak o sloupec dozadu do čisté klece (do 0,15), jinak nejnižší hrozba, není-li vyšší než stát.
// Nosič MA 2 dojde jen o dvě řady: tam i na místě hrozba 0,29 (zeď) ⇒ ustoupí o sloupec do čisté klece.
TEST(OneCageShiftCap, RiskySidewaysAndRiskyStayRetreatIntoACleanCage) {
    struct Off { Off() { setExtraFeaturesOff(kP233WallClear); } ~Off() { setExtraFeaturesOff(0); } } off;   // jen P230
    Board b = wallWithHelpers(4);
    b.s.getPlayer(1).movementRemaining = 2;
    // ústup (ne posun do strany s 0,29) jen proto, že tým má časovou rezervu ⇒ hranice 0,15 (P232)
    ASSERT_EQ(riskEdge(b.s, b.s.getPlayer(1)), kSafeBlitzThreat);
    const CageTurn t = cageTurn(b);
    EXPECT_EQ(t.carrier.x, 11) << "o sloupec dozadu";
    EXPECT_EQ(cornersAround(b.s, t.carrier), 4) << "čistá klec";
    EXPECT_LE(blitzThreat(b.s, b.s.getPlayer(1)), kSafeBlitzThreat);
}
// Pořadí samo na číslech (pozice „do strany nad hranicí, na místě do ní“ u zdi bez povolené rány se postavit nedá —
// slabší zeď pustí rány a hraje se prolomení P229).
TEST(OneCageShiftCap, OrderSidewaysStayBack) {
    using C = WallShiftChoice;
    const double e = kRiskEdgeNoSlack;   // hranice bez časové rezervy (P232: riskEdge podle času)
    EXPECT_EQ(wallShiftChoice(0.30, 0.05, 0.01, e), C::SIDE) << "do strany do hranice i proti bezpečnějšímu stání";
    EXPECT_EQ(wallShiftChoice(0.31, 0.30, 0.01, e), C::STAY) << "stát (do hranice) je víc než dozadu";
    EXPECT_EQ(wallShiftChoice(0.31, 0.31, 0.15, e), C::BACK) << "dozadu jen do bezpečí";
    EXPECT_EQ(wallShiftChoice(0.35, 0.40, 0.16, e), C::BACK) << "bez bezpečí nejnižší hrozba";
    EXPECT_EQ(wallShiftChoice(0.31, 0.40, 0.35, e), C::SIDE) << "bez bezpečí nejnižší hrozba";
    EXPECT_EQ(wallShiftChoice(std::nullopt, 0.40, 0.35, e), C::BACK) << "jen dozadu — nejhorší je nedělat nic";
    EXPECT_EQ(wallShiftChoice(0.35, 0.40, std::nullopt, e), C::SIDE);
    EXPECT_EQ(wallShiftChoice(std::nullopt, 0.40, std::nullopt, e), C::STAY) << "nic nejde";
    // přesun rizikovější než stát ne (P232)
    EXPECT_EQ(wallShiftChoice(std::nullopt, 0.31, 0.60, e), C::STAY);
    EXPECT_EQ(wallShiftChoice(0.90, 0.31, std::nullopt, e), C::STAY);
    EXPECT_EQ(wallShiftChoice(0.50, 0.31, 0.45, e), C::STAY);
    // hranice se posouvá s časem: s rezervou 0,15, bez ní 0,30 (P232)
    EXPECT_EQ(wallShiftChoice(0.20, 0.25, 0.01, kSafeBlitzThreat), C::BACK) << "s rezervou do strany jen do bezpečí";
    EXPECT_EQ(wallShiftChoice(0.29, 0.29, 0.01, kRiskEdgeNoSlack), C::SIDE) << "bez rezervy do strany i s 0,29";
}

// ⭐ P233 ZEĎ OD KRAJE (uživatel 28.10.2026: „já bych volil blitz na 14,5 — s pomocí, když by nebyly dvě kostky jinak —
//   tím by se pak otevřela možnost mlátit block linemanem na 13,6 na dvě kostky a postupně bych přidával asistence —
//   cíl by byl očistit zeď za cenu, že se toto kolo nepohne dopředu“; „postup od kraje dál“; „nezapomenout, ať si
//   asistencemi nezměníme klec na více hráčů, kteří by v kleci byli navíc a škodili možností chain push“).
//   Zeď pěti hráčů ST 3 na x = 14 před klecí (12,7) pomalých orků bez Block; volní hráči 6, 7, 8 (pohyb 9).
//   Kostky jsou všechny šestky — rány vyjdou, takže je vidět celý sled.
namespace {
struct Hit { Macro m; int dice; int movesBefore = 0; };   // movesBefore: přesuny od předchozí rány
std::vector<Hit> wallClearTurn(Board& b) {
    std::vector<Hit> hits;
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    int moves = 0;
    while (cc.next(b.s, m)) {
        if (m.type == MacroType::REPOSITION) ++moves;
        if (m.type == MacroType::BLOCK || m.type == MacroType::BLITZ) {
            GameState s = b.s.clone();
            if (m.type == MacroType::BLITZ) {             // skutečné pole doběhu na desce v okamžiku rány
                Position landing{-1, -1};
                int steps = 0;
                if (blitzLandingDiceFree(s, s.getPlayer(m.playerId), s.getPlayer(m.targetId).position, landing, steps))
                    s.getPlayer(m.playerId).position = landing;
            }
            hits.push_back({m, blockDiceCount(s, s.getPlayer(m.playerId), s.getPlayer(m.targetId)), moves});
            moves = 0;
        }
        play(b.s, m);
    }
    return hits;
}
}  // namespace

TEST(OneCageWallClear, BlitzOnTheEndThenTheCornerBlocksInwardsAllOnTwoDiceOurs) {
    Board b = wallWithHelpers(4);
    // předpoklad: bez pomoci nejde rána na dvě kostky naše nikde (proto dřív posun po řadách)
    for (int att : {4, 5}) for (int def = 13; def <= 17; ++def) {
        const Player& a = b.s.getPlayer(att);
        if (a.position.distanceTo(b.s.getPlayer(def).position) == 1) ASSERT_LT(blockDiceCount(b.s, a, b.s.getPlayer(def)), 2);
    }
    const std::vector<Hit> hits = wallClearTurn(b);
    ASSERT_GE(hits.size(), 2u);
    EXPECT_EQ(hits[0].m.type, MacroType::BLITZ) << "napřed blitz";
    EXPECT_EQ(hits[0].movesBefore, 1) << "jen tolik asistentů, kolik je na 2 kostky potřeba (tady jeden)";
    const int firstTarget = hits[0].m.targetId;
    EXPECT_TRUE(firstTarget == 13 || firstTarget == 17) << "na krajního hráče zdi";
    EXPECT_EQ(hits[1].m.type, MacroType::BLOCK) << "pak blok";
    EXPECT_TRUE(hits[1].m.playerId == 4 || hits[1].m.playerId == 5) << "blok rohem";
    EXPECT_EQ(hits[1].m.targetId, firstTarget == 13 ? 14 : 16) << "další od kraje dovnitř";
    for (const Hit& h : hits) EXPECT_GE(h.dice, 2) << "rána hráče " << h.m.playerId << " na " << h.m.targetId;
}

TEST(OneCageWallClear, TheCageDoesNotMoveAndNoOneExtraStandsNextToTheCarrier) {
    Board b = wallWithHelpers(4);
    wallClearTurn(b);
    EXPECT_EQ(b.s.getPlayer(1).position, (Position{12, 7})) << "toto kolo se klec nepohne";
    EXPECT_EQ(cornersAround(b.s, {12, 7}), 4);
    int ours = 0;
    for (const Position& sq : Position{12, 7}.getAdjacent()) {
        const Player* q = b.s.getPlayerAtPosition(sq);
        if (q && q->teamSide == TeamSide::HOME) ++ours;
    }
    EXPECT_EQ(ours, 4) << "vedle nosiče jen čtyři rohy (chain push)";
}

// Pozitivní kontrola: bez P233 se tu nebije a klec se posune po řadách (P230).
TEST(OneCageWallClear, WithoutItTheCageShiftsAlongTheWall) {
    struct Off { Off() { setExtraFeaturesOff(kP233WallClear); } ~Off() { setExtraFeaturesOff(0); } } off;
    Board b = wallWithHelpers(4);
    EXPECT_TRUE(wallClearTurn(b).empty());
    EXPECT_NE(b.s.getPlayer(1).position.y, 7);
}

// Bez volných hráčů na asistenci rána na dvě kostky naše nejde ⇒ řadič hraje přesně jako bez P233.
TEST(OneCageWallClear, WithoutHelpersNoHitAndTheOldShift) {
    Board b = wallWithHelpers(4);
    for (int id : {6, 7, 8}) { b.s.getPlayer(id).position = {-1, -1}; b.s.getPlayer(id).state = PlayerState::OFF_PITCH; }
    Board old = b;
    EXPECT_TRUE(wallClearTurn(b).empty());
    {   // totéž, co dělal řadič bez P233
        struct Off { Off() { setExtraFeaturesOff(kP233WallClear); } ~Off() { setExtraFeaturesOff(0); } } off;
        wallClearTurn(old);
    }
    for (int id = 1; id <= 17; ++id) EXPECT_EQ(b.s.getPlayer(id).position, old.s.getPlayer(id).position) << "hráč " << id;
}

// Asistent nedošel (tah mu „vypadne“) ⇒ rána, která by bez něj nebyla na 2 kostky naše, se nehraje (review P233 T2).
TEST(OneCageWallClear, AHitWhoseAssistDidNotArriveIsNotPlayed) {
    Board b = wallWithHelpers(4);
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    bool skipped = false;
    int hits = 0;
    while (cc.next(b.s, m)) {
        if (!skipped && m.type == MacroType::REPOSITION && m.playerId != 1 && b.s.getPlayer(m.playerId).position.x >= 9 &&
            m.targetPos.x >= 13) {
            skipped = true;                           // první asistence u zdi se nezahraje
            continue;
        }
        if (m.type == MacroType::BLOCK || m.type == MacroType::BLITZ) {
            GameState s = b.s.clone();
            if (m.type == MacroType::BLITZ) {
                Position landing{-1, -1};
                int steps = 0;
                ASSERT_TRUE(blitzLandingDiceFree(s, s.getPlayer(m.playerId), s.getPlayer(m.targetId).position, landing, steps));
                s.getPlayer(m.playerId).position = landing;
            }
            EXPECT_GE(blockDiceCount(s, s.getPlayer(m.playerId), s.getPlayer(m.targetId)), 2) << "rána hráče " << m.playerId;
            ++hits;
        }
        play(b.s, m);
    }
    ASSERT_TRUE(skipped) << "předpoklad: P233 poslal asistenta";
    (void)hits;
}

// Totéž s blitzerem s Block: pravidla P229 by mu jednu kostku dovolila, čištění zdi chce 2 kostky naše (review 2).
TEST(OneCageWallClear, AHitWhoseAssistDidNotArriveIsNotPlayedEvenWithBlock) {
    Board b = wallWithHelpers(4);
    b.s.getPlayer(6).skills.add(SkillName::Block);
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    bool skipped = false;
    while (cc.next(b.s, m)) {
        if (!skipped && m.type == MacroType::REPOSITION && m.playerId == 8) {
            ASSERT_EQ(m.targetPos, (Position{14, 4})) << "předpoklad: 8 jde asistovat blitzu hráče 6 na (14,5)";
            skipped = true;
            continue;
        }
        if (skipped && m.type == MacroType::BLITZ) {
            GameState s = b.s.clone();
            s.getPlayer(m.playerId).position = m.viaPos;
            EXPECT_GE(blockDiceCount(s, s.getPlayer(m.playerId), s.getPlayer(m.targetId)), 2) << "blitz hráče " << m.playerId;
        }
        play(b.s, m);
    }
    ASSERT_TRUE(skipped);
}

// Zmařená rána tah neukončí: zbytek týmu hraje dál jako po ranách (review P233 F7).
TEST(OneCageWallClear, AThwartedHitDoesNotEndTheTurn) {
    Board b = wallWithHelpers(4);
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    bool skipped = false;
    int after = 0;
    while (cc.next(b.s, m)) {
        if (!skipped && m.type == MacroType::REPOSITION && m.playerId == 8) {   // asistent nedošel
            ASSERT_EQ(m.targetPos, (Position{14, 4})) << "předpoklad: 8 jde asistovat k cíli (14,5)";
            skipped = true;
            continue;
        }
        if (skipped) {
            EXPECT_FALSE(m.type == MacroType::BLITZ && m.targetId == 13) << "rána bez asistence se nehraje";
            if (m.playerId != 8) ++after;
        }
        play(b.s, m);
    }
    ASSERT_TRUE(skipped) << "předpoklad: první asistentem je 8";
    EXPECT_GT(after, 0) << "po zmařené ráně se ostatní hráči ještě hýbou";
}

// Deska zrcadlově pro stranu AWAY (útok doleva): hráč id ↔ id±11, x ↔ 25−x; na tahu AWAY, míč u zrcadla nosiče 1.
Board mirroredToAway(const Board& h) {
    Board b(2);
    b.s = h.s.clone();
    for (int id = 1; id <= GameState::PLAYERS_TOTAL; ++id) {
        const Player& from = h.s.getPlayer(id);
        const int to = id <= 11 ? id + 11 : (id <= 22 ? id - 11 : id);
        Player& p = b.s.getPlayer(to);
        p = from;
        p.id = to;
        if (id <= 22) p.teamSide = from.teamSide == TeamSide::HOME ? TeamSide::AWAY : TeamSide::HOME;
        if (from.position.isOnPitch()) p.position = {static_cast<int8_t>(25 - from.position.x), from.position.y};
    }
    b.s.homeTeam = h.s.awayTeam; b.s.awayTeam = h.s.homeTeam;
    b.s.homeTeam.side = TeamSide::HOME; b.s.awayTeam.side = TeamSide::AWAY;
    b.s.activeTeam = TeamSide::AWAY;
    const Position cp = h.s.getPlayer(1).position;
    b.s.ball = BallState::carried({static_cast<int8_t>(25 - cp.x), cp.y}, 12);
    return b;
}

// Strana AWAY zrcadlově: táž zeď, táž rána od kraje.
TEST(OneCageWallClear, AwaySideMirrored) {
    Board b = mirroredToAway(wallWithHelpers(4));
    const std::vector<Hit> hits = wallClearTurn(b);
    ASSERT_GE(hits.size(), 2u);
    for (const Hit& h : hits) EXPECT_GE(h.dice, 2) << "rána hráče " << h.m.playerId << " na " << h.m.targetId;
    EXPECT_EQ(b.s.getPlayer(15).position, (Position{12, 6})) << "přední roh (zrcadlo 4) zůstal";
    EXPECT_EQ(b.s.getPlayer(16).position, (Position{12, 8})) << "přední roh (zrcadlo 5) zůstal";
    EXPECT_EQ(hits[0].m.type, MacroType::BLITZ);
    EXPECT_TRUE(hits[0].m.targetId == 2 || hits[0].m.targetId == 6) << "krajní hráč zdi (zrcadlo 13 / 17)";
    EXPECT_EQ(b.s.getPlayer(12).position, (Position{13, 7})) << "klec stojí";
}

// Odtržený soupeř za mezerou zdí není: cílem je konec souvislého úseku před nosičem (review P233 F5).
TEST(OneCageWallClear, ADetachedPlayerBeyondAGapIsNotTheEnd) {
    {   // zeď 14,6–14,9, za mezerou 13 na (14,4): bije se od konce zdi dovnitř, odtržený 13 ne
        Board b = wallWithHelpers(4);
        b.s.getPlayer(13).position = {14, 4};
        const std::vector<Hit> hits = wallClearTurn(b);
        ASSERT_FALSE(hits.empty());
        EXPECT_TRUE(hits[0].m.targetId == 14 || hits[0].m.targetId == 17) << "konec souvislé zdi";
        for (const Hit& h : hits) EXPECT_NE(h.m.targetId, 13) << "odtržený hráč není konec zdi";
    }
    {   // zeď 14,6–14,8, odtržení na (14,4) a (14,10) — oba v řádcích nosiče ±3. Konce 14 a 16 stojí mezi
        //   soupeři (zóny na polích asistence) ⇒ na 2 kostky naše nejdou ⇒ nebije se vůbec, ani do odtržených.
        Board b = wallWithHelpers(4);
        b.s.getPlayer(13).position = {14, 4};
        b.s.getPlayer(17).position = {14, 10};
        EXPECT_TRUE(wallClearTurn(b).empty());
    }
}

// Rána jen na 2 kostky naše: s jediným pomocníkem vyjde nejvýš jedna kostka ⇒ nebije se a řadič hraje přesně
//   jako bez P233 (review P233 W3, W6 — dřív asistent odešel ke zdi a rána pak neprošla).
TEST(OneCageWallClear, WithOneHelperOnlyOneDieNoHitAndTheOldShift) {
    Board b = wallWithHelpers(4);
    for (int id : {7, 8}) { b.s.getPlayer(id).position = {-1, -1}; b.s.getPlayer(id).state = PlayerState::OFF_PITCH; }
    Board old = b;
    EXPECT_TRUE(wallClearTurn(b).empty());
    {
        struct Off { Off() { setExtraFeaturesOff(kP233WallClear); } ~Off() { setExtraFeaturesOff(0); } } off;
        wallClearTurn(old);
    }
    for (int id = 1; id <= 17; ++id) EXPECT_EQ(b.s.getPlayer(id).position, old.s.getPlayer(id).position) << "hráč " << id;
}

// Asistence se přidávají, dokud nejsou 2 kostky, i když jedna sama kostky nezmění (review P233 F3): konce zdi ST 5,
//   na 2 kostky naše proti ST 3 jsou potřeba tři asistence (4 proti 5 je pořád soupeřova volba).
TEST(OneCageWallClear, ThreeAssistsWhenOneAloneDoesNotChangeTheDice) {
    Board b = wallWithHelpers(4);
    for (int id : {13, 17}) b.s.getPlayer(id).stats.strength = 5;
    b.put(9, TeamSide::HOME, {10, 1}, 9);
    b.put(10, TeamSide::HOME, {10, 13}, 9);
    b.put(11, TeamSide::HOME, {9, 5}, 9);
    const std::vector<Hit> hits = wallClearTurn(b);
    ASSERT_FALSE(hits.empty()) << "rána do konce zdi ST 5 se třemi asistencemi";
    EXPECT_TRUE(hits[0].m.targetId == 13 || hits[0].m.targetId == 17);
    for (const Hit& h : hits) EXPECT_GE(h.dice, 2) << "rána hráče " << h.m.playerId << " na " << h.m.targetId;
}

// Řetězové odtlačení rány do zdi nebrání (uživatel 29.10.2026: „nevidím zádrhel — proč by se nemělo dělat řetězové
//   odtlačení“; dřív ji zakazovalo pravidlo z review F6). Za zdí leží soupeři na x = 15, řádky 4–10 ⇒ u rány na kraj
//   zdi jsou všechna tři pole odtlačení obsazená. Rána se hraje a řetěz nejde přes naši klec: pole volí útočník
//   (pravidla ř. 637-644) a bere to nejdál od nosiče.
TEST(OneCageWallClear, AChainPushDoesNotStopTheWallHit) {
    Board b = wallWithHelpers(4);
    const int ids[] = {18, 19, 20, 21, 22, 25, 26};   // soupeřovy sloty (23, 24 = naše lavička)
    for (int i = 0; i < 7; ++i) b.put(ids[i], TeamSide::AWAY, {15, static_cast<int8_t>(4 + i)}, 1).state = PlayerState::PRONE;
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    int hits = 0, chained = 0;
    while (cc.next(b.s, m)) {
        if ((m.type == MacroType::BLOCK || m.type == MacroType::BLITZ) && m.targetId >= 13 && m.targetId <= 17) {
            Position from = b.s.getPlayer(m.playerId).position;
            if (m.type == MacroType::BLITZ) {
                int steps = 0;
                ASSERT_TRUE(blitzLandingDiceFree(b.s, b.s.getPlayer(m.playerId), b.s.getPlayer(m.targetId).position, from, steps));
            }
            const Position d = b.s.getPlayer(m.targetId).position;
            const Position behind{static_cast<int8_t>(2 * d.x - from.x), static_cast<int8_t>(2 * d.y - from.y)};
            int squares = 0, taken = 0;                // pole odtlačení = pole za cílem a jeho dva sousedé od útočníka
            for (const Position& q : d.getAdjacent())
                if (q.distanceTo(from) == 2 && q.distanceTo(behind) <= 1) {
                    ++squares;
                    if (q.isOnPitch() && b.s.getPlayerAtPosition(q)) ++taken;
                }
            ASSERT_EQ(squares, 3);
            if (taken == 3) ++chained;
            ++hits;
        }
        play(b.s, m);
    }
    EXPECT_GT(chained, 0) << "rána do zdi s řetězovým odtlačením se má hrát (ran do zdi " << hits << ")";
    EXPECT_EQ(b.s.getPlayer(1).position, (Position{12, 7})) << "nosič na místě";
    EXPECT_EQ(b.s.getPlayer(4).position, (Position{13, 6})) << "roh nevytlačen řetězem";
    EXPECT_EQ(b.s.getPlayer(5).position, (Position{13, 8})) << "roh nevytlačen řetězem";
}

// Řetěz nejde přes roh klece (předpoklad, na kterém stojí povolení řetězu výš; příklad z review F6): náš hráč na (14,4)
//   bije krajního (14,5) rovně dolů. Pole odtlačení (13,6) náš roh, (14,6) zeď, (15,6) ležící soupeř — všechna
//   obsazená. Útočník volí pole nejdál od nosiče (15,6) ⇒ ležící soupeř jde dál, roh i nosič zůstanou na místě
//   (cíl sám po šestkách na zbroji a zranění může skončit mimo hřiště, proto se jeho pole nekontroluje).
TEST(OneCageWallClear, AChainPushGoesAwayFromTheCage) {
    Board b = wallWithHelpers(4);
    b.s.getPlayer(6).position = {14, 4};
    b.put(18, TeamSide::AWAY, {15, 6}, 1).state = PlayerState::PRONE;
    for (const Position q : {Position{13, 6}, Position{14, 6}, Position{15, 6}})
        ASSERT_NE(b.s.getPlayerAtPosition(q), nullptr) << "předpoklad: pole odtlačení (" << int(q.x) << "," << int(q.y) << ") obsazené";
    play(b.s, Macro{MacroType::BLOCK, 6, 13, {-1, -1}});
    ASSERT_NE(b.s.getPlayer(13).position, (Position{14, 5})) << "předpoklad: cíl odtlačen";
    EXPECT_NE(b.s.getPlayer(18).position, (Position{15, 6})) << "řetěz šel přes (15,6), nejdál od nosiče";
    EXPECT_EQ(b.s.getPlayerAtPosition({14, 6}), &b.s.getPlayer(14)) << "zeď na (14,6) nepostrčena";
    EXPECT_EQ(b.s.getPlayer(4).position, (Position{13, 6})) << "roh nevytlačen řetězem";
    EXPECT_EQ(b.s.getPlayer(1).position, (Position{12, 7})) << "nosič na místě";
}

// Rána do zdi nesmí odtlačením (i řetězem) pohnout nikým z nás vedle nosiče ani dostat soupeře vedle nosiče
//   (uživatel 28.10.: „ať si nezměníme klec“; review 3 k povolení řetězu). Kontrola u každé rány do zdi v tahu řadiče.
namespace {
int expectHitsKeepTheCage(Board& b) {
    int wallHits = 0;
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    const Position cp = b.s.getPlayer(1).position;
    while (cc.next(b.s, m)) {
        const bool wallHit = (m.type == MacroType::BLOCK || m.type == MacroType::BLITZ) && b.s.getPlayer(m.targetId).teamSide == TeamSide::AWAY;
        std::vector<std::pair<int, Position>> near;
        if (wallHit) {
            b.s.forEachOnPitch(TeamSide::HOME, [&](const Player& p) {
                if (p.id != m.playerId && p.position.distanceTo(cp) <= 1) near.push_back({p.id, p.position});
            });
            // odtlačení nanečisto ještě před ranou (výsledek nezávisí na kostkách — se šestkami cíl často skončí mimo hřiště)
            Position from = b.s.getPlayer(m.playerId).position;
            if (m.type == MacroType::BLITZ) {
                int steps = 0;
                EXPECT_TRUE(blitzLandingDiceFree(b.s, b.s.getPlayer(m.playerId), b.s.getPlayer(m.targetId).position, from, steps));
            }
            bool surf = false;
            const GameState sim = previewPushBack(b.s, m.playerId, m.targetId, from, m.type == MacroType::BLITZ, surf);
            for (TeamSide side : {TeamSide::HOME, TeamSide::AWAY})
                sim.forEachOnPitch(side, [&](const Player& o) {
                    EXPECT_FALSE(o.id != m.playerId && o.position.distanceTo(cp) <= 1 && b.s.getPlayer(o.id).position != o.position)
                        << "rána " << m.playerId << "→" << m.targetId << " by dostala hráče " << o.id << " k nosiči";
                });
            for (const auto& [id, pos] : near)
                EXPECT_EQ(sim.getPlayer(id).position, pos) << "rána " << m.playerId << "→" << m.targetId << " by pohnula hráčem " << id;
        }
        play(b.s, m);
        if (!wallHit) continue;
        ++wallHits;
        for (const auto& [id, pos] : near)
            EXPECT_EQ(b.s.getPlayer(id).position, pos) << "rána " << m.playerId << "→" << m.targetId << " pohnula hráčem " << id;
        const Player& t = b.s.getPlayer(m.targetId);
        if (t.isOnPitch()) EXPECT_GT(t.position.distanceTo(cp), 1) << "rána " << m.playerId << "→" << m.targetId << " dostala soupeře k nosiči";
    }
    return wallHits;
}
}  // namespace

// (a) Rána zezadu na řádku nosiče: náš ST 5 na (15,7) za zdí (14,7)–(14,8), před nosičem náš hráč na (13,7) ⇒ všechna
//   tři pole odtlačení (13,6), (13,7), (13,8) jsou naši vedle nosiče. Taková rána se nehraje.
TEST(OneCageWallClear, AHitFromBehindDoesNotPushIntoTheCage) {
    Board b = walledCageBoard(2);
    for (int id : {13, 14, 17}) { b.s.getPlayer(id).position = {-1, -1}; b.s.getPlayer(id).state = PlayerState::OFF_PITCH; }
    b.put(6, TeamSide::HOME, {15, 7}, 4, 5);
    b.put(9, TeamSide::HOME, {13, 7});
    {
        bool surf = false;
        const GameState sim = previewPushBack(b.s, 6, 15, {15, 7}, false, surf);
        const bool moved = sim.getPlayer(9).position != Position{13, 7} || sim.getPlayer(4).position != Position{13, 6} ||
                           sim.getPlayer(5).position != Position{13, 8};
        ASSERT_TRUE(moved)
            << "předpoklad: rána 6→15 by řetězem pohnula naším hráčem vedle nosiče";
    }
    expectHitsKeepTheCage(b);
}

// (c) Rána zezadu bez řetězu: jako (a), ale pole před nosičem (13,7) je volné ⇒ soupeř by tam byl odtlačen (vedle
//   nosiče), aniž by se kdo z nás pohnul. Taková rána se nehraje. Zeď má ST 4, aby rohy nebily (jejich rána by cíl
//   vyřadila dřív, než na ránu zezadu dojde).
TEST(OneCageWallClear, AHitFromBehindDoesNotPushAnOpponentNextToTheCarrier) {
    Board b = walledCageBoard(2);
    for (int id : {13, 14, 17}) { b.s.getPlayer(id).position = {-1, -1}; b.s.getPlayer(id).state = PlayerState::OFF_PITCH; }
    b.put(6, TeamSide::HOME, {15, 7}, 4, 5);
    for (int id : {15, 16}) b.s.getPlayer(id).stats.strength = 4;   // rohy na zeď nemají 2 kostky ⇒ zbývá rána zezadu
    for (int att : {4, 5}) for (int def : {15, 16})
        if (b.s.getPlayer(att).position.distanceTo(b.s.getPlayer(def).position) == 1)
            ASSERT_LT(blockDiceCount(b.s, b.s.getPlayer(att), b.s.getPlayer(def)), 2) << "předpoklad: roh " << att << " nebije";
    {
        bool surf = false;
        const GameState sim = previewPushBack(b.s, 6, 15, {15, 7}, false, surf);
        ASSERT_EQ(sim.getPlayer(15).position, (Position{13, 7})) << "předpoklad: rána 6→15 by soupeře dostala vedle nosiče";
        for (int id : {2, 3, 4, 5}) ASSERT_EQ(sim.getPlayer(id).position, b.s.getPlayer(id).position) << "předpoklad: nikdo z nás se nehne";
    }
    EXPECT_GT(expectHitsKeepTheCage(b), 0) << "jiná rána do zdi (6→16 šikmo, pryč od klece) se hrát má";
}

// (d) Řetěz dotlačí NAŠEHO hráče vedle nosiče (klec by měla hráče navíc — uživatel 28.10.): náš ST 5 na (15,4) bije
//   krajního (14,5) šikmo; pole odtlačení (13,6) roh, (13,5) náš W, (14,6) zeď se Stand Firm ⇒ cíl jde na W, W od (14,5)
//   na (12,4)/(12,5) — tam stojí soupeři se Stand Firm ⇒ W skončí na (12,6) vedle nosiče. Taková rána se nehraje.
TEST(OneCageWallClear, AChainDoesNotPushOurPlayerNextToTheCarrier) {
    Board b = walledCageBoard(2);
    b.put(6, TeamSide::HOME, {15, 4}, 4, 5);
    b.put(9, TeamSide::HOME, {13, 5});
    for (int id : {4, 5}) b.s.getPlayer(id).skills.add(SkillName::Frenzy);   // rohy nebijí ⇒ na krajního zbývá rána zezadu
    b.s.getPlayer(14).skills.add(SkillName::StandFirm);
    b.put(18, TeamSide::AWAY, {12, 4}, 6, 3, {SkillName::StandFirm});
    b.put(19, TeamSide::AWAY, {12, 5}, 6, 3, {SkillName::StandFirm});
    {
        bool surf = false;
        const GameState sim = previewPushBack(b.s, 6, 13, {15, 4}, false, surf);
        ASSERT_EQ(sim.getPlayer(9).position, (Position{12, 6})) << "předpoklad: řetěz by W dotlačil vedle nosiče";
        ASSERT_GT(sim.getPlayer(13).position.distanceTo(b.s.getPlayer(1).position), 1) << "předpoklad: soupeř sám k nosiči nedojde";
    }
    expectHitsKeepTheCage(b);
}

// Side Step volí pole obránce sám (v živé partii člověk) ⇒ nejhorší volba (uživatel 29.10.: „pozor na sidestep“): zeď
//   (14,6)–(14,8), krajní mají Side Step a vedle nich je volné (13,7) u nosiče ⇒ do krajních se nebije. Bez Side Step ano.
TEST(OneCageWallClear, NoHitOnASideStepperWhoCouldStepNextToTheCarrier) {
    auto board = [](bool sideStep) {
        Board b = wallWithHelpers(4);
        for (int id : {13, 17}) { b.s.getPlayer(id).position = {-1, -1}; b.s.getPlayer(id).state = PlayerState::OFF_PITCH; }
        if (sideStep) for (int id : {14, 16}) b.s.getPlayer(id).skills.add(SkillName::SideStep);
        return b;
    };
    Board plain = board(false);
    ASSERT_GT(wallClearTurn(plain).size(), 0u) << "předpoklad: bez Side Step se do krajních bije";
    Board b = board(true);
    for (const Hit& h : wallClearTurn(b))
        EXPECT_TRUE(h.m.targetId != 14 && h.m.targetId != 16) << "rána do Side Steppera " << h.m.targetId;
    // řadič rány do Side Stepperů vůbec nenaplánuje a hraje náhradní posun klece po řadách (P230), ne rozehraný plán
    EXPECT_NE(b.s.getPlayer(1).position, (Position{12, 7})) << "klec se měla posunout po řadách";
}

// Rychlejší tým, jehož rohy na zeď bít nesmějí (Side Step u nosiče), obíhá (P228) — zakázaný blok není „silnější“
//   (review 5). Rohy ST 4 mají 2 kostky; bez Side Step by bily (pozitivní kontrola).
TEST(OneCageWallClear, AFasterTeamRunsAroundWhenItsCornersMayNotHit) {
    auto board = [](bool sideStep) {
        Board b = wallWithHelpers(7);
        for (int id : {13, 17}) { b.s.getPlayer(id).position = {-1, -1}; b.s.getPlayer(id).state = PlayerState::OFF_PITCH; }
        for (int id : {4, 5}) b.s.getPlayer(id).stats.strength = 4;
        if (sideStep) for (int id : {14, 16}) b.s.getPlayer(id).skills.add(SkillName::SideStep);
        return b;
    };
    Board plain = board(false);
    int hits = 0;
    for (const Hit& h : wallClearTurn(plain)) hits += h.m.playerId == 4 || h.m.playerId == 5;
    ASSERT_GT(hits, 0) << "předpoklad: bez Side Step rohy bijí (silnější)";
    Board b = board(true);
    for (const Hit& h : wallClearTurn(b)) EXPECT_TRUE(h.m.targetId != 14 && h.m.targetId != 16) << "rána do Side Steppera";
    EXPECT_GT(b.s.getPlayer(1).position.x, 13) << "rychlejší obíhá: nosič za linií zdi";
}

// Side Step v řetězu (review 5): náš ST 5 na (14,4) bije krajního (14,5) rovně dolů; na (15,6) stojí soupeř se Stand Firm
//   ⇒ řetěz jde na zeď (14,6), a ta má Side Step ⇒ v živé partii si soupeř může vybrat volné (13,7) u nosiče. Rohy mají
//   Frenzy (nebijí). Rána 6→13 se nehraje; bez Side Step u (14,6) ano (řetěz pak vede pryč od klece).
//   Totéž zrcadlově pro AWAY: tam náhled uhne pryč od nosiče, chytí to jen nejhorší případ (bez toho přežila mutace V2).
TEST(OneCageWallClear, ASideStepperInTheChainCountsAsWorstCase) {
    // away: zrcadlo pro stranu AWAY. Tam náhled (automat: pole nejdál od útočníka, při shodě menší x) uhne pryč
    //   od nosiče, takže ránu zastaví jen nejhorší případ; u HOME uhne náhled k nosiči sám (mutace V2 29.10.).
    auto board = [](bool sideStep, bool away) {
        Board b = wallWithHelpers(4);
        for (int id : {7, 8}) { b.s.getPlayer(id).position = {-1, -1}; b.s.getPlayer(id).state = PlayerState::OFF_PITCH; }
        b.put(6, TeamSide::HOME, {14, 4}, 4, 5);
        for (int id : {4, 5}) b.s.getPlayer(id).skills.add(SkillName::Frenzy);
        b.put(18, TeamSide::AWAY, {15, 6}, 6, 3, {SkillName::StandFirm});
        if (sideStep) b.s.getPlayer(14).skills.add(SkillName::SideStep);
        return away ? mirroredToAway(b) : b;
    };
    for (bool away : {false, true}) {
        SCOPED_TRACE(away ? "AWAY" : "HOME");
        const int att = away ? 17 : 6, def = away ? 2 : 13, chain = away ? 3 : 14;
        const Position wallSq{static_cast<int8_t>(away ? 11 : 14), 6}, nearCarrier{static_cast<int8_t>(away ? 12 : 13), 7};
        {
            Board p = board(false, away);
            bool surf = false;
            const GameState sim = previewPushBack(p.s, att, def, p.s.getPlayer(att).position, false, surf);
            ASSERT_NE(sim.getPlayer(chain).position, wallSq) << "předpoklad: řetěz jde přes zeď";
            int hits = 0;
            for (const Hit& h : wallClearTurn(p)) hits += h.m.playerId == att && h.m.targetId == def;
            ASSERT_GT(hits, 0) << "předpoklad: bez Side Step se rána hraje";
        }
        Board b = board(true, away);
        if (away) {
            bool surf = false;
            const GameState sim = previewPushBack(b.s, att, def, b.s.getPlayer(att).position, false, surf);
            ASSERT_EQ(sim.getPlayer(def).position, wallSq) << "předpoklad: obránce zatlačen řetězem";
            ASSERT_GE(sim.getPlayer(chain).position.distanceTo(b.s.getPlayer(12).position), 2) << "předpoklad: náhled uhne pryč";
            ASSERT_EQ(b.s.getPlayerAtPosition(nearCarrier), nullptr) << "předpoklad: pole u nosiče volné";
        }
        for (const Hit& h : wallClearTurn(b)) EXPECT_FALSE(h.m.playerId == att && h.m.targetId == def) << "rána, jejíž řetěz posune Side Steppera";
    }
}

// Grab ruší Side Step (pushSkills) ⇒ rohy s Grab do krajních Side Stepperů bít smějí (review 5: bez výjimky by kontrola
//   ránu zbytečně zakázala).
TEST(OneCageWallClear, AnAttackerWithGrabMayHitASideStepper) {
    Board b = wallWithHelpers(4);
    for (int id : {13, 17}) { b.s.getPlayer(id).position = {-1, -1}; b.s.getPlayer(id).state = PlayerState::OFF_PITCH; }
    for (int id : {14, 16}) b.s.getPlayer(id).skills.add(SkillName::SideStep);
    for (int id : {4, 5}) b.s.getPlayer(id).skills.add(SkillName::Grab);
    int hits = 0;
    for (const Hit& h : wallClearTurn(b)) hits += h.m.targetId == 14 || h.m.targetId == 16;
    EXPECT_GT(hits, 0) << "rohy s Grab do Side Stepperů bít mají";
}

// Kontrola klece i těsně před ranou (review 4): plán se počítal na jiné desce. Po prvním přesunu tahu dostanou krajní
//   zdi Side Step (a vedle nich je volné (13,7) u nosiče) ⇒ naplánovaná rána do nich se už nezahraje.
TEST(OneCageWallClear, TheCageIsCheckedAgainRightBeforeTheHit) {
    Board b = wallWithHelpers(4);
    for (int id : {13, 17}) { b.s.getPlayer(id).position = {-1, -1}; b.s.getPlayer(id).state = PlayerState::OFF_PITCH; }
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    bool changed = false;
    int hitsAfter = 0;
    while (cc.next(b.s, m)) {
        if (changed && (m.type == MacroType::BLOCK || m.type == MacroType::BLITZ) && (m.targetId == 14 || m.targetId == 16)) ++hitsAfter;
        play(b.s, m);
        if (!changed) {
            ASSERT_EQ(m.type, MacroType::REPOSITION) << "předpoklad: tah začíná přesunem (asistent na křídlo)";
            for (int id : {14, 16}) b.s.getPlayer(id).skills.add(SkillName::SideStep);
            ASSERT_EQ(b.s.getPlayerAtPosition({13, 7}), nullptr) << "předpoklad: (13,7) u nosiče volné";
            changed = true;
        }
    }
    EXPECT_EQ(hitsAfter, 0) << "naplánovaná rána do Side Steppera se zahrála";
    // pozitivní kontrola: bez změny se do krajních bije
    Board c = wallWithHelpers(4);
    for (int id : {13, 17}) { c.s.getPlayer(id).position = {-1, -1}; c.s.getPlayer(id).state = PlayerState::OFF_PITCH; }
    int plain = 0;
    for (const Hit& h : wallClearTurn(c)) plain += h.m.targetId == 14 || h.m.targetId == 16;
    ASSERT_GT(plain, 0);
}

// (b) Stand Firm v řetězu: náš ST 5 na (14,4) bije krajního (14,5) rovně dolů; zeď (14,6) i soupeř na (15,6) mají
//   Stand Firm ⇒ řetěz zkusí další pole — náš roh (13,6). Taková rána se nehraje.
TEST(OneCageWallClear, AStandFirmInTheChainDoesNotPushTheCorner) {
    Board b = wallWithHelpers(4);
    for (int id : {7, 8}) { b.s.getPlayer(id).position = {-1, -1}; b.s.getPlayer(id).state = PlayerState::OFF_PITCH; }
    b.put(6, TeamSide::HOME, {14, 4}, 4, 5);
    b.s.getPlayer(14).skills.add(SkillName::StandFirm);
    b.put(18, TeamSide::AWAY, {15, 6}, 6, 3, {SkillName::StandFirm});
    {
        bool surf = false;
        const GameState sim = previewPushBack(b.s, 6, 13, {14, 4}, false, surf);
        ASSERT_NE(sim.getPlayer(4).position, (Position{13, 6})) << "předpoklad: řetěz by šel přes roh";
    }
    expectHitsKeepTheCage(b);
}

// Přesuny rány nesmí zvednout hrozbu na nosiče (P232, review P233 F4): hráč 8 na (10,7) drží zónou cestu silného
//   soupeře zezadu (8,7) ke kleci; jeho odchod k zdi by hrozbu zvedl (0,29 → 0,41).
TEST(OneCageWallClear, AnAssistDoesNotOpenTheWayToTheCarrier) {
    Board b = wallWithHelpers(4);
    b.s.getPlayer(8).position = {10, 7};
    b.put(18, TeamSide::AWAY, {8, 7}, 6, 5).stats.agility = 4;
    const double before = blitzThreat(b.s, b.s.getPlayer(1));
    Board unguarded = b;
    const std::vector<Hit> hits = wallClearTurn(b);
    ASSERT_FALSE(hits.empty()) << "předpoklad: zeď se bije";
    EXPECT_LE(blitzThreat(b.s, b.s.getPlayer(1)), before + 0.01);
    {   // pozitivní kontrola: bez hlídání hrozby (P232) by tah hrozbu zvedl
        struct Off { Off() { setExtraFeaturesOff(kP232KeepThreat); } ~Off() { setExtraFeaturesOff(0); } } off;
        wallClearTurn(unguarded);
        EXPECT_GT(blitzThreat(unguarded.s, unguarded.s.getPlayer(1)), before + 0.01);
    }
}

// Nosič označený uprostřed čištění (třeba soupeř vstal vedle něj): další rány do zdi už ne (review P233 F8).
//   Soupeř stojí ZA nosičem (11,7): mimo zeď, pole asistentů i odtlačení — rány tak nezastaví nic jiného než
//   označení nosiče (review 3: soupeř na (13,7) by asistoval zdi a mohl by srazit kostky pod 2).
//   Platí i bez hlídání hrozby (P232 vypnuté): sousední soupeř hrozbu zvedne, takže se zapnutým P232 by rány zastavilo
//   už hlídání hrozby a samotná kontrola označení by testem neprošla (mutace 29.10.).
TEST(OneCageWallClear, NoMoreWallHitsOnceTheCarrierIsMarked) {
    struct Off { explicit Off(int m) { setExtraFeaturesOff(m); } ~Off() { setExtraFeaturesOff(0); } };
    for (const int off : {0, static_cast<int>(kP232KeepThreat)}) {
        int wallHits = 0;
        bool marked = false;
        {
            Off guard(off);
            Board b = wallWithHelpers(4);
            CageController cc(nullptr, cfg(), 1);
            Macro m;
            while (cc.next(b.s, m)) {
                const bool wallHit = (m.type == MacroType::BLOCK || m.type == MacroType::BLITZ) && m.targetId >= 13 && m.targetId <= 17;
                if (wallHit) ++wallHits;
                if (marked) EXPECT_FALSE(wallHit) << "rána do zdi hráčem " << m.playerId << " na " << m.targetId << " (vypnuto " << off << ")";
                play(b.s, m);
                if (m.type == MacroType::BLITZ && !marked) {
                    EXPECT_EQ(b.s.getPlayerAtPosition({11, 7}), nullptr) << "předpoklad: pole za nosičem volné";
                    b.put(18, TeamSide::AWAY, {11, 7}, 6);     // soupeř vedle nosiče, za ním
                    marked = true;
                }
            }
        }
        ASSERT_TRUE(marked) << "předpoklad: první ranou je blitz (vypnuto " << off << ")";
        // pozitivní kontrola: bez označení by se do zdi bilo dál
        Board c = wallWithHelpers(4);
        int unmarked = 0;
        {
            Off guard(off);
            for (const Hit& h : wallClearTurn(c)) unmarked += h.m.targetId >= 13 && h.m.targetId <= 17;
        }
        ASSERT_GE(unmarked, 2) << "předpoklad: bez označení víc než jedna rána do zdi (vypnuto " << off << ")";
        EXPECT_EQ(wallHits, 1);
    }
}

// Frenzy rohy do zdi nebijí: povinný druhý blok a následování by je vytáhly z klece (review P233 F9).
TEST(OneCageWallClear, FrenzyCornersDoNotHitTheWall) {
    Board b = wallWithHelpers(4);
    for (int id : {4, 5}) b.s.getPlayer(id).skills.add(SkillName::Frenzy);
    const std::vector<Hit> hits = wallClearTurn(b);
    ASSERT_FALSE(hits.empty()) << "předpoklad: zeď se bije (blitz pomocníkem)";
    for (const Hit& h : hits) EXPECT_TRUE(h.m.playerId != 4 && h.m.playerId != 5) << "Frenzy roh " << h.m.playerId;
    EXPECT_EQ(b.s.getPlayer(4).position, (Position{13, 6}));
    EXPECT_EQ(b.s.getPlayer(5).position, (Position{13, 8}));
}

// Blitz se hraje jen z pole, pro které byl spočítán (kostky, odtlačení, hrozba, ne vedle nosiče — review P233 F1):
//   cíl po příchodu asistenta uhne na (15,4) ⇒ doběh by skončil jinde ⇒ blitz se nehraje, tah jde dál.
TEST(OneCageWallClear, TheBlitzIsPlayedOnlyFromThePlannedSquare) {
    Board b = wallWithHelpers(4);
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    bool moved = false;
    while (cc.next(b.s, m)) {
        if (m.type == MacroType::BLITZ) {
            Position landing{-1, -1};
            int steps = 0;
            ASSERT_TRUE(blitzLandingDiceFree(b.s, b.s.getPlayer(m.playerId), b.s.getPlayer(m.targetId).position, landing, steps));
            EXPECT_EQ(landing, m.viaPos) << "blitz hráče " << m.playerId << " na " << m.targetId;
        }
        play(b.s, m);
        if (!moved && m.type == MacroType::REPOSITION && m.playerId == 8) {
            ASSERT_EQ(b.s.getPlayer(8).position, (Position{14, 4})) << "předpoklad: 8 asistuje na cíl (14,5)";
            b.s.getPlayer(13).position = {15, 4};       // cíl uhnul
            moved = true;
        }
    }
    ASSERT_TRUE(moved);
}

// Blitz do zdi bez GFI: nevyjde-li, je to turnover s asistenty u zdi a klecí na místě (review 2). Pomocník 6 má MA 2
//   a ke konci zdi (14,5) je nejblíž — doběhne jen s GFI.
TEST(OneCageWallClear, NoBlitzThatNeedsAGoForIt) {
    Board b = wallWithHelpers(4);
    b.s.getPlayer(6).stats.movement = 2;
    b.s.getPlayer(6).movementRemaining = 2;
    {
        Position landing{-1, -1};
        int steps = 0;
        ASSERT_TRUE(blitzLandingDiceFree(b.s, b.s.getPlayer(6), b.s.getPlayer(13).position, landing, steps));
        ASSERT_GT(steps, 2) << "předpoklad: blitz hráče 6 na konec zdi potřebuje GFI";
    }
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    while (cc.next(b.s, m)) {
        if (m.type == MacroType::BLITZ) {
            Position landing{-1, -1};
            int steps = 0;
            ASSERT_TRUE(blitzLandingDiceFree(b.s, b.s.getPlayer(m.playerId), b.s.getPlayer(m.targetId).position, landing, steps));
            EXPECT_LE(steps, b.s.getPlayer(m.playerId).movementRemaining) << "blitz hráče " << m.playerId << " s GFI";
        }
        play(b.s, m);
    }
}

// Kontrola doběhu blitzu po asistencích v planWallClear (review P233 F1a) zůstala bez zabíjejícího testu: v prohledaných
//   pozicích (asistent na cestě blitzera, tělo na každém poli kolem cíle) se doběh asistencí nezměnil. Nedosažitelnost
//   tím dokázaná není — kontrola zůstává jako pojistka.
// Mutace 29.10. bez zabíjejícího testu, s důvodem:
//   - GFI blitzu hlídají dvě kontroly (doběh před asistencemi a po nich). Shodují se, jen když asistenti doběh nezmění —
//     v prohledaných pozicích vždy (viz F1a výš). Vypnutí obou zachytí NoBlitzThatNeedsAGoForIt; vypnutí jen druhé
//     (asistent doběh prodlouží do GFI) testem kryté není; první navíc odmítne blitz, který by asistent zkrátil.
//   - `ourBall` ve větvi WALL_CLEAR v next(): bez míče je stillValid false; bez té podmínky jde řadič přes
//     AFTER_ADVANCE do planLaggards, který bez míče hned skončí (DONE). Nevyčištěné readyMates_ a deferredGfi_
//     nevadí: readyMates_ plní až planLaggards (po čištění zdi) a forbids bez míče hned končí.

// Hrozba na nosiče se měří proti začátku čištění zdi, ne proti poslední ráně (review 2; tady začíná čištění tah,
//   takže je to i začátek tahu): po blitzu se objeví silný soupeř
//   a hrozba vyskočí z 0,29 na 0,49 ⇒ další rány do zdi už ne. S bází přepočítanou před každou ranou by hrozba
//   „plížila“ nahoru po kouscích a rána by se zahrála.
TEST(OneCageWallClear, TheThreatIsHeldAgainstTheStartOfClearing) {
    Board b = wallWithHelpers(4);
    const double atStart = blitzThreat(b.s, b.s.getPlayer(1));
    CageController cc(nullptr, cfg(), 1);
    Macro m;
    bool placed = false;
    int later = 0;
    while (cc.next(b.s, m)) {
        if (placed && (m.type == MacroType::BLOCK || m.type == MacroType::BLITZ) && m.targetId >= 13 && m.targetId <= 17) ++later;
        play(b.s, m);
        if (m.type == MacroType::BLITZ && !placed) {
            placed = true;
            ASSERT_EQ(b.s.getPlayerAtPosition({18, 7}), nullptr);
            b.put(18, TeamSide::AWAY, {18, 7}, 8, 6, {SkillName::Block, SkillName::Tackle});
            ASSERT_GT(blitzThreat(b.s, b.s.getPlayer(1)), atStart + 0.1) << "předpoklad: soupeř hrozbu zvedne";
        }
    }
    ASSERT_TRUE(placed) << "předpoklad: první ranou je blitz";
    EXPECT_EQ(later, 0) << "rána do zdi po zvednutí hrozby nad začátek čištění";
    // pozitivní kontrola: tentýž soupeř, hlídání hrozby vypnuté ⇒ rány pokračují (zastavuje je jen hrozba)
    {
        struct Off { Off() { setExtraFeaturesOff(kP232KeepThreat); } ~Off() { setExtraFeaturesOff(0); } } off;
        Board c = wallWithHelpers(4);
        CageController cc2(nullptr, cfg(), 1);
        bool placed2 = false;
        int later2 = 0;
        while (cc2.next(c.s, m)) {
            if (placed2 && (m.type == MacroType::BLOCK || m.type == MacroType::BLITZ) && m.targetId >= 13 && m.targetId <= 17) ++later2;
            play(c.s, m);
            if (m.type == MacroType::BLITZ && !placed2) {
                placed2 = true;
                c.put(18, TeamSide::AWAY, {18, 7}, 8, 6, {SkillName::Block, SkillName::Tackle});
            }
        }
        EXPECT_GE(later2, 1) << "bez hlídání hrozby by se po blitzu bilo dál";
    }
    // pozitivní kontrola: bez nového soupeře se po blitzu bije dál
    Board c = wallWithHelpers(4);
    int wall = 0;
    for (const Hit& h : wallClearTurn(c)) wall += h.m.targetId >= 13 && h.m.targetId <= 17;
    ASSERT_GE(wall, 2);
}

