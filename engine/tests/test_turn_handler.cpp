#include <gtest/gtest.h>
#include "bb/turn_handler.h"
#include "bb/game_state.h"
#include "bb/helpers.h"

using namespace bb;

// ---------------------------------------------------------------------------
// OMRÁČENÍ (F1, 21.08.) — BB2016 ř. 703-708: "All face-down players are turned
// face up at the END of their team's next turn, even if a turnover takes
// place. Note that a player may not turn face up on the turn they are
// Stunned."  Do 21.08. se překlápělo na ZAČÁTKU kola (resetPlayersForNewTurn)
// ⇒ každý omráčený dostal jednu aktivaci navíc. Při 6,2 stunech na zápas.
// ---------------------------------------------------------------------------

TEST(TurnHandler, StunnedFlipsAtEndOfOwnTurnNotStart) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    Player& p = gs.getPlayer(1);
    p.id = 1; p.teamSide = TeamSide::HOME;
    p.state = PlayerState::STUNNED;
    p.stunnedThisTurn = false;          // omráčen v SOUPEŘOVĚ kole

    gs.resetPlayersForNewTurn(TeamSide::HOME);
    EXPECT_EQ(p.state, PlayerState::STUNNED) << "na začátku kola se NEPŘEKLÁPÍ";

    resolveEndTurn(gs, nullptr, /*wasTurnover=*/false);
    EXPECT_EQ(p.state, PlayerState::PRONE) << "překlápí se na KONCI kola";
}

TEST(TurnHandler, StunnedOnOwnTurnStaysDownOneMoreTurn) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    Player& p = gs.getPlayer(1);
    p.id = 1; p.teamSide = TeamSide::HOME;
    p.state = PlayerState::STUNNED;
    p.stunnedThisTurn = true;           // složen ve VLASTNÍM kole

    resolveEndTurn(gs, nullptr, false);
    EXPECT_EQ(p.state, PlayerState::STUNNED) << "ř. 707: ne v kole, kdy byl omráčen";

    gs.activeTeam = TeamSide::HOME;
    gs.resetPlayersForNewTurn(TeamSide::HOME);   // příznak se čistí
    resolveEndTurn(gs, nullptr, false);
    EXPECT_EQ(p.state, PlayerState::PRONE) << "až na konci PŘÍŠTÍHO kola";
}

TEST(TurnHandler, StunnedFlipsEvenOnTurnover) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    Player& p = gs.getPlayer(1);
    p.id = 1; p.teamSide = TeamSide::HOME;
    p.state = PlayerState::STUNNED;
    p.stunnedThisTurn = false;

    resolveEndTurn(gs, nullptr, /*wasTurnover=*/true);
    EXPECT_EQ(p.state, PlayerState::PRONE) << "ř. 705: even if a turnover takes place";
}
