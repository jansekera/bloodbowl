#include "bb/game_state.h"
#include <stdexcept>

namespace bb {

GameState::GameState() {
    homeTeam.side = TeamSide::HOME;
    awayTeam.side = TeamSide::AWAY;

    // Initialize player IDs and team sides
    for (TeamSide side : {TeamSide::HOME, TeamSide::AWAY}) {
        for (int n = 0; n < SQUAD_SIZE; ++n) {
            int id = squadId(side, n);
            players[id - 1].id = id;
            players[id - 1].teamSide = side;
        }
    }
}

Player& GameState::getPlayer(int id) {
    if (id < 1 || id > PLAYERS_TOTAL) {
        throw std::out_of_range("Player ID out of range");
    }
    return players[id - 1];   // IDs are contiguous across both squads
}

const Player& GameState::getPlayer(int id) const {
    if (id < 1 || id > PLAYERS_TOTAL) {
        throw std::out_of_range("Player ID out of range");
    }
    return players[id - 1];   // IDs are contiguous across both squads
}

Player* GameState::getPlayerAtPosition(Position pos) {
    for (auto& p : players) {
        if (p.isOnPitch() && p.position == pos) return &p;
    }
    return nullptr;
}

const Player* GameState::getPlayerAtPosition(Position pos) const {
    for (const auto& p : players) {
        if (p.isOnPitch() && p.position == pos) return &p;
    }
    return nullptr;
}

TeamState& GameState::getTeamState(TeamSide side) {
    return side == TeamSide::HOME ? homeTeam : awayTeam;
}

const TeamState& GameState::getTeamState(TeamSide side) const {
    return side == TeamSide::HOME ? homeTeam : awayTeam;
}

void GameState::resetPlayersForNewTurn(TeamSide side) {
    // New turn (or new drive via kickoff): no activation is open.
    currentActivationId = -1;
    forEachPlayer(side, [](Player& p) {
        // ⛔ STUNNED se tu UŽ NEPŘEKLÁPÍ (oprava 21.08.). BB2016 l. 703-708:
        // face-down se otáčí na KONCI příštího kola svého týmu, ne na začátku
        // -- překlopení tady dávalo každému omráčenému jednu aktivaci navíc.
        // Flip je teď v resolveEndTurn(); tady se jen ČISTÍ příznak, aby hráč
        // omráčený v SOUPEŘOVĚ kole na konci toho svého už vstal, kdežto ten,
        // koho složili ve VLASTNÍM kole, ležel ještě jedno.
        p.stunnedThisTurn = false;
        p.hasMoved = false;
        p.hasActed = false;
        p.usedBlitz = false;
        // M3/N12: gaze konci pristim kolem obeti, big-guy stav az uspesnym
        // hodem nebo koncem drivu -- viz komentar u `bigGuyStupefied`.
        p.lostTacklezones = p.bigGuyStupefied;
        p.proUsedThisTurn = false;
        p.bigGuyCheckedThisTurn = false;
        p.leapUsedThisTurn = false;
        // Q3: priznak zil pres souperovo kolo prave proto, aby se dala
        // zmerit JEHO odpoved; ted uz je vycerpany.
        p.stoodUpNextToEnemy = false;
        p.dodgeRerollUsedThisTurn = false;
        p.sureFeetRerollUsedThisTurn = false;
        // Take Root (l. 8573-8576): zakorenení konci, kdyz je hráč sražen
        // nebo polozen na zem -- do dalsiho vlastniho kola uz s nim nepocitame.
        if (p.state != PlayerState::STANDING) p.rooted = false;
        p.movementRemaining = p.rooted ? 0 : p.stats.movement;
    });
}

} // namespace bb
