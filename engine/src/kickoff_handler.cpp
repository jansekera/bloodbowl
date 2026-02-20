#include "bb/kickoff_handler.h"
#include "bb/ball_handler.h"
#include "bb/helpers.h"
#include <algorithm>
#include <cmath>

namespace bb {

namespace {

// Find closest standing player on a team to a position
int findClosestPlayer(const GameState& state, TeamSide side, Position target) {
    int bestId = -1;
    int bestDist = 999;
    state.forEachOnPitch(side, [&](const Player& p) {
        if (p.state != PlayerState::STANDING) return;
        int d = p.position.distanceTo(target);
        if (d < bestDist) {
            bestDist = d;
            bestId = p.id;
        }
    });
    return bestId;
}

// Move a player 1 square toward a target (if possible)
void movePlayerToward(GameState& state, int playerId, Position target) {
    Player& p = state.getPlayer(playerId);
    if (p.state != PlayerState::STANDING) return;

    int bestDist = p.position.distanceTo(target);
    Position bestPos = p.position;

    auto adj = p.position.getAdjacent();
    for (auto& pos : adj) {
        if (!pos.isOnPitch()) continue;
        if (state.getPlayerAtPosition(pos) != nullptr) continue;
        int d = pos.distanceTo(target);
        if (d < bestDist) {
            bestDist = d;
            bestPos = pos;
        }
    }

    if (bestPos != p.position) {
        if (state.ball.isHeld && state.ball.carrierId == playerId) {
            state.ball.position = bestPos;
        }
        p.position = bestPos;
    }
}

// Resolve the 2D6 kickoff table event
void resolveKickoffEvent(GameState& state, KickoffEvent event, TeamSide receiving,
                         DiceRollerBase& dice, std::vector<GameEvent>* events) {
    TeamSide kicking = opponent(receiving);

    switch (event) {
        case KickoffEvent::GET_THE_REF:
            // Simplified: no-op (in CRP both teams get a bribe)
            break;

        case KickoffEvent::RIOT: {
            TeamState& recvTeam = state.getTeamState(receiving);
            if (recvTeam.turnNumber <= 1) {
                recvTeam.turnNumber++;  // Receiving team loses a turn
            } else {
                recvTeam.turnNumber--;  // Extra turn
            }
            break;
        }

        case KickoffEvent::PERFECT_DEFENCE:
            // Simplified: no-op (kicking team can rearrange)
            break;

        case KickoffEvent::HIGH_KICK: {
            // Move closest receiving player to ball position
            int closestId = findClosestPlayer(state, receiving, state.ball.position);
            if (closestId >= 0 && !state.ball.isHeld) {
                Player& p = state.getPlayer(closestId);
                // Check no one is at ball position
                if (!state.getPlayerAtPosition(state.ball.position)) {
                    p.position = state.ball.position;
                }
            }
            break;
        }

        case KickoffEvent::CHEERING: {
            // Both teams roll D6, higher gets +1 reroll (ties: no effect)
            int homeRoll = dice.rollD6();
            int awayRoll = dice.rollD6();
            if (homeRoll > awayRoll) {
                state.homeTeam.rerolls++;
            } else if (awayRoll > homeRoll) {
                state.awayTeam.rerolls++;
            }
            break;
        }

        case KickoffEvent::BRILLIANT_COACHING: {
            int homeRoll = dice.rollD6();
            int awayRoll = dice.rollD6();
            if (homeRoll > awayRoll) {
                state.homeTeam.rerolls++;
            } else if (awayRoll > homeRoll) {
                state.awayTeam.rerolls++;
            }
            break;
        }

        case KickoffEvent::CHANGING_WEATHER: {
            int weatherRoll = dice.roll2D6();
            state.weather = weatherFromRoll(weatherRoll);
            emitEvent(events, {GameEvent::Type::WEATHER_CHANGE, -1, -1, {}, {},
                              weatherRoll, true});
            break;
        }

        case KickoffEvent::QUICK_SNAP: {
            // Each receiving standing player moves 1 sq toward LOS
            int losX = (receiving == TeamSide::HOME) ? 12 : 13;
            Position losTarget{static_cast<int8_t>(losX), 7};
            state.forEachOnPitch(receiving, [&](const Player& p) {
                if (p.state != PlayerState::STANDING) return;
                movePlayerToward(state, p.id, losTarget);
            });
            break;
        }

        case KickoffEvent::BLITZ: {
            // Each kicking standing player moves 1 sq toward LOS
            int losX = (kicking == TeamSide::HOME) ? 12 : 13;
            Position losTarget{static_cast<int8_t>(losX), 7};
            state.forEachOnPitch(kicking, [&](const Player& p) {
                if (p.state != PlayerState::STANDING) return;
                movePlayerToward(state, p.id, losTarget);
            });
            break;
        }

        case KickoffEvent::THROW_A_ROCK: {
            // Random standing player from each team gets STUNNED
            for (TeamSide side : {TeamSide::HOME, TeamSide::AWAY}) {
                // Count standing players
                int count = 0;
                state.forEachOnPitch(side, [&](const Player& p) {
                    if (p.state == PlayerState::STANDING) count++;
                });
                if (count == 0) continue;

                int target = dice.rollD6() % count;  // simplified random selection
                int idx = 0;
                state.forEachOnPitch(side, [&](const Player& p) {
                    if (p.state != PlayerState::STANDING) return;
                    if (idx == target) {
                        Player& mp = state.getPlayer(p.id);
                        mp.state = PlayerState::STUNNED;
                        emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, p.id, -1,
                                          p.position, {}, 0, false});
                    }
                    idx++;
                });
            }
            break;
        }

        case KickoffEvent::PITCH_INVASION: {
            // D6 each standing player, 6 → STUNNED
            for (auto& p : state.players) {
                if (p.state != PlayerState::STANDING || !p.isOnPitch()) continue;
                int roll = dice.rollD6();
                if (roll == 6) {
                    p.state = PlayerState::STUNNED;
                    emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, p.id, -1,
                                      p.position, {}, roll, false});
                }
            }
            break;
        }

        default:
            break;
    }
}

} // anonymous namespace

void resolveKickoff(GameState& state, DiceRollerBase& dice, std::vector<GameEvent>* events) {
    TeamSide receiving = opponent(state.kickingTeam);
    state.activeTeam = receiving;

    // Reset turn counters for both teams
    state.getTeamState(TeamSide::HOME).turnNumber = 0;
    state.getTeamState(TeamSide::AWAY).turnNumber = 0;

    // Advance to first turn for receiving team
    TeamState& recvTeam = state.getTeamState(receiving);
    recvTeam.turnNumber = 1;
    recvTeam.resetForNewTurn();
    state.resetPlayersForNewTurn(receiving);

    // Kick target: center of receiving half
    int kickX = (state.kickingTeam == TeamSide::HOME) ? 18 : 7;
    int kickY = 7;

    // Scatter: D6 for distance, D8 for direction
    int dist = dice.rollD6();
    int dir = dice.rollD8();
    Position scatter = scatterDirection(dir);
    int landX = kickX + scatter.x * dist;
    int landY = kickY + scatter.y * dist;

    // Clamp to pitch
    landX = std::clamp(landX, 0, 25);
    landY = std::clamp(landY, 0, 14);

    Position landPos{static_cast<int8_t>(landX), static_cast<int8_t>(landY)};

    // Check touchback: ball must land in receiving half
    bool touchback = false;
    if (receiving == TeamSide::HOME && landPos.x > 12) {
        touchback = true;
    } else if (receiving == TeamSide::AWAY && landPos.x < 13) {
        touchback = true;
    }

    if (touchback) {
        // Touchback: closest receiving player gets ball
        int closestId = findClosestPlayer(state, receiving, landPos);
        if (closestId >= 0) {
            Player& p = state.getPlayer(closestId);
            state.ball = BallState::carried(p.position, closestId);
        } else {
            state.ball = BallState::onGround(landPos);
        }
    } else {
        state.ball = BallState::onGround(landPos);
    }

    // Kickoff event
    emitEvent(events, {GameEvent::Type::KICKOFF, -1, -1, {}, landPos, 0, true});

    // Roll 2D6 for kickoff table
    int kickoffRoll = dice.roll2D6();
    KickoffEvent koEvent = kickoffEventFromRoll(std::clamp(kickoffRoll, 2, 12));
    resolveKickoffEvent(state, koEvent, receiving, dice, events);

    // Roll weather (if not changed by CHANGING_WEATHER)
    if (koEvent != KickoffEvent::CHANGING_WEATHER) {
        state.weather = weatherFromRoll(dice.roll2D6());
    }

    // Kick-Off Return: closest KOR player moves up to 3 sq toward ball
    if (!touchback) {
        state.forEachOnPitch(receiving, [&](const Player& p) {
            if (p.state != PlayerState::STANDING) return;
            if (!p.hasSkill(SkillName::KickOffReturn)) return;

            int closestDist = 999;
            int closestKorId = -1;
            // Just find the closest KOR player
            state.forEachOnPitch(receiving, [&](const Player& kp) {
                if (!kp.hasSkill(SkillName::KickOffReturn)) return;
                if (kp.state != PlayerState::STANDING) return;
                int d = kp.position.distanceTo(state.ball.position);
                if (d < closestDist) {
                    closestDist = d;
                    closestKorId = kp.id;
                }
            });

            if (closestKorId == p.id) {
                // Move up to 3 squares toward ball
                for (int step = 0; step < 3; step++) {
                    movePlayerToward(state, p.id, state.ball.position);
                }
            }
        });
    }

    // Catch/bounce at landing position (if not touchback and ball on ground)
    if (!touchback && !state.ball.isHeld) {
        Player* catcher = state.getPlayerAtPosition(state.ball.position);
        if (catcher && catcher->teamSide == receiving &&
            catcher->state == PlayerState::STANDING) {
            if (!resolveCatch(state, catcher->id, dice, 0, events)) {
                // Failed catch — bounce
                resolveBounce(state, state.ball.position, dice, 0, events);
            }
        }
    }

    state.phase = GamePhase::PLAY;
}

} // namespace bb
