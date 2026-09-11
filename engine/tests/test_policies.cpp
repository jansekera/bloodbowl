#include <gtest/gtest.h>
#include "bb/policies.h"
#include "bb/rules_engine.h"
#include "bb/game_state.h"
#include "bb/dice.h"

using namespace bb;

namespace {

void addPlayer(GameState& s, int id, TeamSide side, int x, int y,
               PlayerState st = PlayerState::STANDING) {
    Player& p = s.getPlayer(id);
    p.id = id; p.teamSide = side; p.state = st;
    p.position = {static_cast<int8_t>(x), static_cast<int8_t>(y)};
    p.stats = {6, 3, 3, 8};
    p.movementRemaining = 6; p.hasMoved = false; p.hasActed = false;
}

// Nosič zazděný osmi LEŽÍCÍMI soupeři.
//   žádné volné pole  ⇒ nabídka nemá MOVE   (ramena 1, 2 a 5 neseponou)
//   žádný STOJÍCÍ soupeř ⇒ nabídka nemá BLOCK (rameno 3)
//   blitz vyčerpán    ⇒ nabídka nemá BLITZ  (rameno 4)
//   ...a přesto je legální FOUL (BB2016: ležící/omráčený soused).
// `foulAvailable=false` je druhá polovina páru: nabídka je pak OPRAVDU
// jen END_TURN a ukončit kolo je správně.
GameState makeWalledCarrierWithFoul(bool foulAvailable) {
    GameState s;
    s.phase = GamePhase::PLAY;
    s.activeTeam = TeamSide::HOME;
    s.half = 1;
    s.homeTeam.turnNumber = 3;
    s.homeTeam.rerolls = 3;
    s.awayTeam.rerolls = 3;
    s.homeTeam.blitzUsedThisTurn = true;
    s.homeTeam.passUsedThisTurn = true;
    s.homeTeam.handOffUsedThisTurn = true;
    s.homeTeam.foulUsedThisTurn = !foulAvailable;
    s.weather = Weather::NICE;

    addPlayer(s, 1, TeamSide::HOME, 13, 7);
    s.ball = BallState::carried({13, 7}, 1);

    // ⚠️ AWAY starteri maji id 12..22 (`GameState::squadId`), ne 2..9.
    //     S id 2..9 by ta tela sedela v HOME slotech, `forEachOnPitch(HOME)`
    //     by je iteroval jako nase a nabidka by vysla uplne jinak.
    int id = GameState::awayBaseId();
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (!dx && !dy) continue;
            addPlayer(s, id++, TeamSide::AWAY, 13 + dx, 7 + dy,
                      PlayerState::PRONE);
        }
    }
    return s;
}

int countOfType(const std::vector<Action>& actions, ActionType t) {
    int n = 0;
    for (auto& a : actions) if (a.type == t) ++n;
    return n;
}

}  // namespace

// ⛔ VADA (10.09.2026): `greedyPolicy` končila fallbackem `return actions[0]`,
//    jenže `rules_engine.cpp:17` vkládá END_TURN jako PRVNÍ prvek nabídky
//    ("END_TURN is always available"). Fallback tedy neukončoval kolo někdy,
//    ale KDYKOLI nesepnulo rameno MOVE/BLOCK/BLITZ -- i když bylo co hrát.
//    Pravidlová kotva: uzavřený sedmičlenný seznam turnoverů (BB2016
//    r. 368-384) "politiku nenašla rameno" NEOBSAHUJE.
TEST(GreedyPolicy, FallbackDoesNotEndTheTurnWhenSomethingIsStillPlayable) {
    GameState state = makeWalledCarrierWithFoul(/*foulAvailable=*/true);

    // --- SEBEKONTROLA FIXTURY: postavil jsem opravdu ten stav? ---
    std::vector<Action> offer;
    getAvailableActions(state, offer);

    // (a) žádné z ramen 1-5 sepnout nemůže -- jinak se do fallbacku vůbec
    //     nedojde a test neměří nic.
    ASSERT_EQ(countOfType(offer, ActionType::MOVE), 0)
        << "fixtura je vadná: nosič má kam šlápnout, rameno MOVE sepne";
    ASSERT_EQ(countOfType(offer, ActionType::BLOCK), 0)
        << "fixtura je vadná: je koho blokovat, rameno BLOCK sepne";
    ASSERT_EQ(countOfType(offer, ActionType::BLITZ), 0)
        << "fixtura je vadná: blitz je v nabídce, rameno BLITZ sepne";

    // (b) PRVNÍ prvek nabídky je END_TURN -- přesně to je mechanismus vady.
    //     Bez tohoto tvrzení by test prošel i v repozitáři, kde se nabídka
    //     řadí jinak, a nedokazoval by nic.
    ASSERT_FALSE(offer.empty());
    ASSERT_EQ(offer[0].type, ActionType::END_TURN)
        << "fixtura je vadná: END_TURN už není první, vada má jiný tvar";

    // (c) a je co hrát -- jinak by ukončení kola nebyla škoda.
    ASSERT_GE(countOfType(offer, ActionType::FOUL), 1)
        << "fixtura je vadná: hrát opravdu není co, END_TURN by byl správně";

    DiceRoller dice(4321);
    Action action = greedyPolicy(state, dice);

    EXPECT_NE(action.type, ActionType::END_TURN)
        << "fallback zahodil kolo, přestože FOUL byl legální -- to je ta vada";
    EXPECT_EQ(action.type, ActionType::FOUL)
        << "jediná hratelná akce v nabídce je FOUL, jinou vrátit nelze";
}

TEST(GreedyPolicy, FallbackStillEndsTheTurnWhenNothingElseIsOffered) {
    // ⛔ Druhá polovina páru: oprava NESMÍ vyrábět akci tam, kde žádná není.
    GameState state = makeWalledCarrierWithFoul(/*foulAvailable=*/false);

    std::vector<Action> offer;
    getAvailableActions(state, offer);

    // SEBEKONTROLA: nabídka je OPRAVDU jen END_TURN. Tohle je ta přesná
    // podmínka, za které END_TURN zůstává správná odpověď.
    ASSERT_EQ(offer.size(), 1u)
        << "fixtura je vadná: v nabídce je ještě něco jiného než END_TURN";
    ASSERT_EQ(offer[0].type, ActionType::END_TURN);

    DiceRoller dice(4321);
    Action action = greedyPolicy(state, dice);

    EXPECT_EQ(action.type, ActionType::END_TURN)
        << "když opravdu nic nejde, kolo se ukončit MÁ -- jinak je to přeoprava";
}
