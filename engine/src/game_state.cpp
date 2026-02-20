#include "bb/game_state.h"
#include <stdexcept>

namespace bb {

GameState::GameState() {
    homeTeam.side = TeamSide::HOME;
    awayTeam.side = TeamSide::AWAY;

    // Initialize player IDs and team sides
    for (int i = 0; i < 11; ++i) {
        players[i].id = i + 1;          // IDs 1-11
        players[i].teamSide = TeamSide::HOME;
    }
    for (int i = 0; i < 11; ++i) {
        players[11 + i].id = 12 + i;    // IDs 12-22
        players[11 + i].teamSide = TeamSide::AWAY;
    }
}

Player& GameState::getPlayer(int id) {
    if (id < 1 || id > 22) {
        throw std::out_of_range("Player ID must be 1-22");
    }
    return players[id <= 11 ? id - 1 : id - 12 + 11];
}

const Player& GameState::getPlayer(int id) const {
    if (id < 1 || id > 22) {
        throw std::out_of_range("Player ID must be 1-22");
    }
    return players[id <= 11 ? id - 1 : id - 12 + 11];
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
    forEachPlayer(side, [](Player& p) {
        if (p.state == PlayerState::STUNNED) {
            p.state = PlayerState::PRONE;
        }
        p.hasMoved = false;
        p.hasActed = false;
        p.usedBlitz = false;
        p.lostTacklezones = false;
        p.proUsedThisTurn = false;
        p.movementRemaining = p.stats.movement;
    });
}

} // namespace bb
