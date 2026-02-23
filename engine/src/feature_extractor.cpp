#include "bb/feature_extractor.h"
#include "bb/helpers.h"
#include <algorithm>
#include <cmath>

namespace bb {

namespace {

inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

// Normalize x so 0 = my endzone, 25 = opponent's endzone
inline float normalizeX(int x, TeamSide perspective) {
    if (perspective == TeamSide::AWAY) return static_cast<float>(25 - x);
    return static_cast<float>(x);
}

// Distance to opponent's endzone (where I score)
inline int distanceToEndzone(int x, TeamSide perspective) {
    if (perspective == TeamSide::HOME) return 25 - x;  // HOME scores at x=25
    return x;                                            // AWAY scores at x=0
}

inline bool isInMyHalf(int x, TeamSide perspective) {
    if (perspective == TeamSide::HOME) return x <= 12;
    return x >= 13;
}

// Check if position is a diagonal neighbor of center
inline bool isDiagonal(Position center, Position pos) {
    int dx = pos.x - center.x;
    int dy = pos.y - center.y;
    return (dx == 1 || dx == -1) && (dy == 1 || dy == -1);
}

// Chebyshev distance between positions
inline int chebyshev(Position a, Position b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

// Forward direction in X: +1 for HOME (scores at x=25), -1 for AWAY (scores at x=0)
inline int forwardDir(TeamSide side) {
    return (side == TeamSide::HOME) ? 1 : -1;
}

// Is position p "between" the ball and my endzone? (closer to my endzone than ball)
inline bool isBetweenBallAndEndzone(Position p, Position ballPos, TeamSide perspective) {
    if (perspective == TeamSide::HOME) {
        // HOME endzone at x=0, opponent scores there. My endzone: x=0
        // "Between ball and my endzone" means p.x < ballPos.x (closer to x=0)
        return p.x < ballPos.x;
    } else {
        // AWAY endzone at x=25
        return p.x > ballPos.x;
    }
}

} // anonymous namespace

void extractFeatures(const GameState& state, TeamSide perspective, float* out) {
    TeamSide opp = opponent(perspective);

    const TeamState& myTeam = state.getTeamState(perspective);
    const TeamState& oppTeam = state.getTeamState(opp);

    // Player counts
    int myStanding = 0, oppStanding = 0;
    int myKO = 0, oppKO = 0;
    int myInjured = 0, oppInjured = 0;
    int myProneStunned = 0, oppProneStunned = 0;

    // Averages
    float myXSum = 0, oppXSum = 0;
    float mySTSum = 0, oppSTSum = 0;
    float myAVSum = 0, oppAVSum = 0;
    float myAGSum = 0, oppAGSum = 0;

    // Sideline counts
    int mySideline = 0, oppSideline = 0;

    // Skill counts
    int myBlock = 0, oppBlock = 0;
    int myDodge = 0, oppDodge = 0;
    int myGuard = 0, myMightyBlow = 0, myClaw = 0;
    int myRegen = 0;
    int myTotal = 0;

    // Engaged counts
    int myEngaged = 0, oppEngaged = 0;

    // Collect standing players for new features
    struct StandingInfo {
        Position pos;
        int id;
        int ma;
        int ag;
        int st;
        bool hasFrenzy;
        bool hasSureHands;
    };
    StandingInfo myStandingPlayers[11];
    int myStandingIdx = 0;
    StandingInfo oppStandingPlayers[11];
    int oppStandingIdx = 0;

    // My players
    state.forEachPlayer(perspective, [&](const Player& p) {
        myTotal++;
        if (p.state == PlayerState::STANDING) {
            myStanding++;
            myXSum += normalizeX(p.position.x, perspective);
            mySTSum += p.stats.strength;
            myAVSum += p.stats.armour;
            myAGSum += p.stats.agility;
            if (p.position.y == 0 || p.position.y == 14) mySideline++;
            if (p.hasSkill(SkillName::Block)) myBlock++;
            if (p.hasSkill(SkillName::Dodge)) myDodge++;
            if (p.hasSkill(SkillName::Guard)) myGuard++;
            if (p.hasSkill(SkillName::MightyBlow)) myMightyBlow++;
            if (p.hasSkill(SkillName::Claw)) myClaw++;
            // Check if engaged (adjacent to enemy standing player)
            if (countTacklezones(state, p.position, perspective) > 0) {
                myEngaged++;
            }
            if (myStandingIdx < 11) {
                myStandingPlayers[myStandingIdx++] = {
                    p.position, p.id, p.stats.movement, p.stats.agility,
                    p.stats.strength, p.hasSkill(SkillName::Frenzy),
                    p.hasSkill(SkillName::SureHands)
                };
            }
        } else if (p.state == PlayerState::KO) {
            myKO++;
        } else if (p.state == PlayerState::INJURED || p.state == PlayerState::DEAD) {
            myInjured++;
        }
        if (p.state == PlayerState::PRONE || p.state == PlayerState::STUNNED) {
            myProneStunned++;
        }
        if (p.hasSkill(SkillName::Regeneration)) myRegen++;
    });

    // Opp players
    state.forEachPlayer(opp, [&](const Player& p) {
        if (p.state == PlayerState::STANDING) {
            oppStanding++;
            oppXSum += normalizeX(p.position.x, perspective);
            oppSTSum += p.stats.strength;
            oppAVSum += p.stats.armour;
            oppAGSum += p.stats.agility;
            if (p.position.y == 0 || p.position.y == 14) oppSideline++;
            if (p.hasSkill(SkillName::Block)) oppBlock++;
            if (p.hasSkill(SkillName::Dodge)) oppDodge++;
            if (countTacklezones(state, p.position, opp) > 0) {
                oppEngaged++;
            }
            if (oppStandingIdx < 11) {
                oppStandingPlayers[oppStandingIdx++] = {
                    p.position, p.id, p.stats.movement, p.stats.agility,
                    p.stats.strength, p.hasSkill(SkillName::Frenzy),
                    p.hasSkill(SkillName::SureHands)
                };
            }
        } else if (p.state == PlayerState::KO) {
            oppKO++;
        } else if (p.state == PlayerState::INJURED || p.state == PlayerState::DEAD) {
            oppInjured++;
        }
        if (p.state == PlayerState::PRONE || p.state == PlayerState::STUNNED) {
            oppProneStunned++;
        }
    });

    // Ball state
    bool iHaveBall = false;
    bool oppHasBall = false;
    bool ballOnGround = false;
    int carrierDistToTD = 13; // default: mid-pitch
    int carrierTZCount = 0;
    bool scoringThreat = false;
    bool oppScoringThreat = false;
    Position carrierPos = {-1, -1};
    Position oppCarrierPos = {-1, -1};
    int carrierMA = 0;

    if (state.ball.isHeld && state.ball.carrierId > 0) {
        const Player& carrier = state.getPlayer(state.ball.carrierId);
        if (carrier.teamSide == perspective) {
            iHaveBall = true;
            if (carrier.state == PlayerState::STANDING) {
                carrierDistToTD = distanceToEndzone(carrier.position.x, perspective);
                carrierTZCount = countTacklezones(state, carrier.position, perspective);
                scoringThreat = (carrier.stats.movement >= carrierDistToTD);
                carrierPos = carrier.position;
                carrierMA = carrier.stats.movement;
            }
        } else {
            oppHasBall = true;
            if (carrier.state == PlayerState::STANDING) {
                int oppDist = distanceToEndzone(carrier.position.x, opp);
                oppScoringThreat = (carrier.stats.movement >= oppDist);
                oppCarrierPos = carrier.position;
            }
        }
    } else {
        ballOnGround = true;
    }

    // Ball in my half
    bool ballInMyHalf = state.ball.isOnPitch() &&
                        isInMyHalf(state.ball.position.x, perspective);

    // Is receiving (not the kicking team)
    bool isReceiving = (state.kickingTeam != perspective);

    // Is my turn
    bool isMyTurn = (state.activeTeam == perspective);

    // Turns remaining
    int turnsRemaining = std::max(0, 9 - myTeam.turnNumber);

    // Score advantage with ball
    int scoreDiff = myTeam.score - oppTeam.score;
    float scoreAdvWithBall = 0.0f;
    if (scoreDiff >= 0 && iHaveBall) {
        scoreAdvWithBall = clampf((scoreDiff + 1) / 4.0f, 0.0f, 1.0f);
    }

    // Carrier near endzone
    bool carrierNearEndzone = iHaveBall && carrierDistToTD <= 3;

    // Stall incentive: reward holding ball when leading/tied with turns remaining
    // Active when: have ball, not trailing, turns remaining > 2
    float stallIncentive = 0.0f;
    if (iHaveBall && scoreAdvWithBall >= 0.0f && turnsRemaining > 2) {
        stallIncentive = (turnsRemaining / 8.0f);  // higher when more turns remain
        if (scoreAdvWithBall > 0.0f) {
            stallIncentive *= 1.5f;  // stronger incentive when leading
        }
    }

    // Cage count (adjacent friendly standing players to ball carrier)
    int cageCount = 0;
    if (iHaveBall && carrierPos.isOnPitch()) {
        auto adj = carrierPos.getAdjacent();
        for (auto& pos : adj) {
            if (!pos.isOnPitch()) continue;
            const Player* p = state.getPlayerAtPosition(pos);
            if (p && p->teamSide == perspective && p->state == PlayerState::STANDING) {
                cageCount++;
            }
        }
    }

    // === NEW FEATURES: strategic patterns ===

    // [56] cage_diagonal_quality: diagonal corners occupied around my carrier (0-4)/4
    int cageDiagonal = 0;
    if (iHaveBall && carrierPos.isOnPitch()) {
        Position diags[4] = {
            {static_cast<int8_t>(carrierPos.x - 1), static_cast<int8_t>(carrierPos.y - 1)},
            {static_cast<int8_t>(carrierPos.x + 1), static_cast<int8_t>(carrierPos.y - 1)},
            {static_cast<int8_t>(carrierPos.x - 1), static_cast<int8_t>(carrierPos.y + 1)},
            {static_cast<int8_t>(carrierPos.x + 1), static_cast<int8_t>(carrierPos.y + 1)},
        };
        for (auto& d : diags) {
            if (!d.isOnPitch()) continue;
            const Player* p = state.getPlayerAtPosition(d);
            if (p && p->teamSide == perspective && p->state == PlayerState::STANDING) {
                cageDiagonal++;
            }
        }
    }

    // [57] cage_overload_risk: >4 adjacent = chain push risk
    // Risk = max(0, adjacentCount - 4) / 4, capped at 1.0
    float cageOverloadRisk = 0.0f;
    if (iHaveBall && carrierPos.isOnPitch()) {
        int adjFriendly = cageCount; // already computed above
        cageOverloadRisk = clampf((adjFriendly - 4) / 4.0f, 0.0f, 1.0f);
    }

    // [58] opp_cage_diagonal_quality: opponent's carrier diagonal cage
    int oppCageDiagonal = 0;
    if (oppHasBall && oppCarrierPos.isOnPitch()) {
        Position diags[4] = {
            {static_cast<int8_t>(oppCarrierPos.x - 1), static_cast<int8_t>(oppCarrierPos.y - 1)},
            {static_cast<int8_t>(oppCarrierPos.x + 1), static_cast<int8_t>(oppCarrierPos.y - 1)},
            {static_cast<int8_t>(oppCarrierPos.x - 1), static_cast<int8_t>(oppCarrierPos.y + 1)},
            {static_cast<int8_t>(oppCarrierPos.x + 1), static_cast<int8_t>(oppCarrierPos.y + 1)},
        };
        for (auto& d : diags) {
            if (!d.isOnPitch()) continue;
            const Player* p = state.getPlayerAtPosition(d);
            if (p && p->teamSide == opp && p->state == PlayerState::STANDING) {
                oppCageDiagonal++;
            }
        }
    }

    // [59] carrier_can_score: MA + 2 GFI >= distance to endzone
    bool carrierCanScore = false;
    if (iHaveBall && carrierPos.isOnPitch()) {
        carrierCanScore = (carrierMA + 2 >= carrierDistToTD);
    }

    // [60] pass_scoring_threat: teammates in pass range who could score after catch
    // Quick pass range ≈ 3, short ≈ 6, long ≈ 10
    // A teammate is a threat if: within 10 squares of carrier AND
    // distToEndzone(teammate) <= teammate.MA + 2
    int passThreats = 0;
    if (iHaveBall && carrierPos.isOnPitch()) {
        for (int i = 0; i < myStandingIdx; i++) {
            auto& mp = myStandingPlayers[i];
            if (mp.pos == carrierPos) continue; // skip carrier
            int distToCarrier = chebyshev(mp.pos, carrierPos);
            if (distToCarrier <= 10) {
                int distTD = distanceToEndzone(mp.pos.x, perspective);
                if (mp.ma + 2 >= distTD) {
                    passThreats++;
                }
            }
        }
    }

    // [61] frenzy_trap_risk: my frenzy players adjacent to 2+ opponents
    // After a block + forced follow-up, the opponent could get 2-dice-against
    int frenzyTraps = 0;
    int myFrenzyCount = 0;
    for (int i = 0; i < myStandingIdx; i++) {
        if (!myStandingPlayers[i].hasFrenzy) continue;
        myFrenzyCount++;
        // Count adjacent opponents
        int adjOpp = 0;
        for (int j = 0; j < oppStandingIdx; j++) {
            if (chebyshev(myStandingPlayers[i].pos, oppStandingPlayers[j].pos) == 1) {
                adjOpp++;
            }
        }
        if (adjOpp >= 2) frenzyTraps++;
    }

    // [62] screen_between_ball: defenders between ball and my endzone
    // When opponent has ball, count my players between their carrier and my endzone
    int screenCount = 0;
    if (oppHasBall && oppCarrierPos.isOnPitch()) {
        for (int i = 0; i < myStandingIdx; i++) {
            if (isBetweenBallAndEndzone(myStandingPlayers[i].pos, oppCarrierPos, perspective)) {
                screenCount++;
            }
        }
    }

    // [63] carrier_blitzable: can any opponent reach my carrier with a blitz?
    // Approximate: any standing opponent where chebyshev(opp, carrier) <= opp.MA
    // and not currently in my TZ (or could dodge out)
    bool carrierBlitzable = false;
    if (iHaveBall && carrierPos.isOnPitch()) {
        for (int j = 0; j < oppStandingIdx; j++) {
            int dist = chebyshev(oppStandingPlayers[j].pos, carrierPos);
            if (dist <= oppStandingPlayers[j].ma) {
                carrierBlitzable = true;
                break;
            }
        }
    }

    // [64] surfable_opponents: opponents on sideline (y=0 or y=14) within blitz range
    int surfableOpps = 0;
    for (int j = 0; j < oppStandingIdx; j++) {
        if (oppStandingPlayers[j].pos.y != 0 && oppStandingPlayers[j].pos.y != 14) continue;
        // Check if any of my standing players could blitz there
        for (int i = 0; i < myStandingIdx; i++) {
            int dist = chebyshev(myStandingPlayers[i].pos, oppStandingPlayers[j].pos);
            if (dist <= myStandingPlayers[i].ma) {
                surfableOpps++;
                break;
            }
        }
    }

    // [65] favorable_blocks: my standing players that could throw 2+ dice blocks
    // Check adjacent opponent, compute effective ST with assists
    int favorableBlocks = 0;
    for (int i = 0; i < myStandingIdx; i++) {
        for (int j = 0; j < oppStandingIdx; j++) {
            if (chebyshev(myStandingPlayers[i].pos, oppStandingPlayers[j].pos) != 1) continue;
            // Compute assists (simplified: count adjacent friendlies - adjacent enemies)
            int myAssists = countAssists(state, oppStandingPlayers[j].pos, perspective,
                                         myStandingPlayers[i].id, oppStandingPlayers[j].id,
                                         oppStandingPlayers[j].id);
            int oppAssists = countAssists(state, myStandingPlayers[i].pos, opp,
                                          oppStandingPlayers[j].id, myStandingPlayers[i].id,
                                          myStandingPlayers[i].id);
            int attST = myStandingPlayers[i].st + myAssists;
            int defST = oppStandingPlayers[j].st + oppAssists;
            auto info = getBlockDiceInfo(attST, defST);
            if (info.count >= 2 && info.attackerChooses) {
                favorableBlocks++;
                break; // count each attacker once
            }
        }
    }

    // [66] one_turn_td_vulnerability: opponent can score in one turn
    // Any opponent standing player where MA+2 >= distToEndzone(opp perspective)
    // and not in my tackle zone
    bool oneTurnTDVuln = false;
    for (int j = 0; j < oppStandingIdx; j++) {
        int dist = distanceToEndzone(oppStandingPlayers[j].pos.x, opp);
        if (oppStandingPlayers[j].ma + 2 >= dist) {
            // Check if in my TZ
            int myTZ = countTacklezones(state, oppStandingPlayers[j].pos, opp);
            if (myTZ == 0) {
                oneTurnTDVuln = true;
                break;
            }
        }
    }

    // [67] loose_ball_proximity: who's closer to loose ball (1.0 = I'm much closer)
    float looseBallProx = 0.5f;
    if (ballOnGround && state.ball.isOnPitch()) {
        Position ballPos = state.ball.position;
        int myClosest = 99, oppClosest = 99;
        for (int i = 0; i < myStandingIdx; i++) {
            int d = chebyshev(myStandingPlayers[i].pos, ballPos);
            if (d < myClosest) myClosest = d;
        }
        for (int j = 0; j < oppStandingIdx; j++) {
            int d = chebyshev(oppStandingPlayers[j].pos, ballPos);
            if (d < oppClosest) oppClosest = d;
        }
        looseBallProx = clampf((oppClosest - myClosest + 5) / 10.0f, 0.0f, 1.0f);
    }

    // [68] deep_safety_count: my players behind the furthest-forward opponent
    // "Behind" = closer to my endzone than the deepest opponent penetration
    int deepSafeties = 0;
    if (oppStandingIdx > 0) {
        // Find the opponent closest to my endzone
        int deepestOppX = -1; // in normalized coords (higher = closer to my endzone... no)
        // Actually: find the opponent with minimum distanceToEndzone(opp.x, perspective)
        // That's the one deepest into my territory
        int minOppDistToMyEZ = 99;
        for (int j = 0; j < oppStandingIdx; j++) {
            // Distance from opponent to MY endzone (i.e. where I defend)
            int dToMyEZ = (perspective == TeamSide::HOME)
                ? oppStandingPlayers[j].pos.x    // HOME endzone at x=0
                : (25 - oppStandingPlayers[j].pos.x);  // AWAY endzone at x=25
            if (dToMyEZ < minOppDistToMyEZ) minOppDistToMyEZ = dToMyEZ;
        }
        // Count my players that are even closer to my endzone (deeper)
        for (int i = 0; i < myStandingIdx; i++) {
            int dToMyEZ = (perspective == TeamSide::HOME)
                ? myStandingPlayers[i].pos.x
                : (25 - myStandingPlayers[i].pos.x);
            if (dToMyEZ < minOppDistToMyEZ) {
                deepSafeties++;
            }
        }
    }

    // [69] isolation_count: my standing players with no friendly within 3 squares
    int isolatedCount = 0;
    for (int i = 0; i < myStandingIdx; i++) {
        bool hasNearby = false;
        for (int k = 0; k < myStandingIdx; k++) {
            if (k == i) continue;
            if (chebyshev(myStandingPlayers[i].pos, myStandingPlayers[k].pos) <= 3) {
                hasNearby = true;
                break;
            }
        }
        if (!hasNearby) isolatedCount++;
    }

    // === Fill features array ===
    // [0] score_diff
    out[0] = clampf(static_cast<float>(myTeam.score - oppTeam.score) / 6.0f, -1.0f, 1.0f);
    // [1] my_score
    out[1] = std::min(myTeam.score / 4.0f, 1.0f);
    // [2] opp_score
    out[2] = std::min(oppTeam.score / 4.0f, 1.0f);
    // [3] turn_progress
    out[3] = std::min((myTeam.turnNumber + (state.half - 1) * 8) / 16.0f, 1.0f);
    // [4-5] standing
    out[4] = myStanding / 11.0f;
    out[5] = oppStanding / 11.0f;
    // [6-7] KO
    out[6] = myKO / 11.0f;
    out[7] = oppKO / 11.0f;
    // [8-9] injured
    out[8] = myInjured / 11.0f;
    out[9] = oppInjured / 11.0f;
    // [10-11] rerolls
    out[10] = std::min(myTeam.rerolls / 4.0f, 1.0f);
    out[11] = std::min(oppTeam.rerolls / 4.0f, 1.0f);
    // [12-14] ball possession
    out[12] = iHaveBall ? 1.0f : 0.0f;
    out[13] = oppHasBall ? 1.0f : 0.0f;
    out[14] = ballOnGround ? 1.0f : 0.0f;
    // [15] carrier dist to TD
    out[15] = iHaveBall ? (carrierDistToTD / 26.0f) : 0.5f;
    // [16] ball in my half
    out[16] = ballInMyHalf ? 1.0f : 0.0f;
    // [17-18] avg x position
    out[17] = myStanding > 0 ? (myXSum / myStanding) / 26.0f : 0.5f;
    out[18] = oppStanding > 0 ? (oppXSum / oppStanding) / 26.0f : 0.5f;
    // [19-20] avg strength
    out[19] = myStanding > 0 ? (mySTSum / myStanding) / 5.0f : 0.0f;
    out[20] = oppStanding > 0 ? (oppSTSum / oppStanding) / 5.0f : 0.0f;
    // [21] cage count
    out[21] = std::min(cageCount / 4.0f, 1.0f);
    // [22] is receiving
    out[22] = isReceiving ? 1.0f : 0.0f;
    // [23] is my turn
    out[23] = isMyTurn ? 1.0f : 0.0f;
    // [24-26] weather
    out[24] = (state.weather == Weather::NICE) ? 1.0f : 0.0f;
    out[25] = (state.weather == Weather::POURING_RAIN) ? 1.0f : 0.0f;
    out[26] = (state.weather == Weather::BLIZZARD) ? 1.0f : 0.0f;
    // [27-28] blitz/pass available
    out[27] = !myTeam.blitzUsedThisTurn ? 1.0f : 0.0f;
    out[28] = !myTeam.passUsedThisTurn ? 1.0f : 0.0f;
    // [29] bias
    out[29] = 1.0f;
    // [30-31] sideline fraction
    out[30] = myStanding > 0 ? static_cast<float>(mySideline) / myStanding : 0.0f;
    out[31] = oppStanding > 0 ? static_cast<float>(oppSideline) / oppStanding : 0.0f;
    // [32] turns remaining
    out[32] = turnsRemaining / 8.0f;
    // [33] score advantage with ball
    out[33] = scoreAdvWithBall;
    // [34] carrier near endzone
    out[34] = carrierNearEndzone ? 1.0f : 0.0f;
    // [35] stall incentive
    out[35] = stallIncentive;
    // [36-37] avg armour
    out[36] = myStanding > 0 ? (myAVSum / myStanding) / 10.0f : 0.0f;
    out[37] = oppStanding > 0 ? (oppAVSum / oppStanding) / 10.0f : 0.0f;
    // [38-39] avg agility
    out[38] = myStanding > 0 ? (myAGSum / myStanding) / 5.0f : 0.0f;
    out[39] = oppStanding > 0 ? (oppAGSum / oppStanding) / 5.0f : 0.0f;
    // [40] carrier TZ count
    out[40] = iHaveBall ? std::min(carrierTZCount / 4.0f, 1.0f) : 0.0f;
    // [41] scoring threat
    out[41] = scoringThreat ? 1.0f : 0.0f;
    // [42] opp scoring threat
    out[42] = oppScoringThreat ? 1.0f : 0.0f;
    // [43-44] engaged fraction
    out[43] = myStanding > 0 ? static_cast<float>(myEngaged) / myStanding : 0.0f;
    out[44] = oppStanding > 0 ? static_cast<float>(oppEngaged) / oppStanding : 0.0f;
    // [45-46] prone/stunned
    out[45] = myProneStunned / 11.0f;
    out[46] = oppProneStunned / 11.0f;
    // [47] free players
    out[47] = (myStanding - myEngaged) / 11.0f;
    // [48-49] block skill fraction
    out[48] = myStanding > 0 ? static_cast<float>(myBlock) / myStanding : 0.0f;
    out[49] = oppStanding > 0 ? static_cast<float>(oppBlock) / oppStanding : 0.0f;
    // [50-51] dodge skill fraction
    out[50] = myStanding > 0 ? static_cast<float>(myDodge) / myStanding : 0.0f;
    out[51] = oppStanding > 0 ? static_cast<float>(oppDodge) / oppStanding : 0.0f;
    // [52] guard fraction
    out[52] = myStanding > 0 ? static_cast<float>(myGuard) / myStanding : 0.0f;
    // [53] mighty blow fraction
    out[53] = myStanding > 0 ? static_cast<float>(myMightyBlow) / myStanding : 0.0f;
    // [54] claw fraction
    out[54] = myStanding > 0 ? static_cast<float>(myClaw) / myStanding : 0.0f;
    // [55] regen fraction
    out[55] = myTotal > 0 ? static_cast<float>(myRegen) / myTotal : 0.0f;

    // === NEW strategic pattern features [56-69] ===
    // [56] cage_diagonal_quality
    out[56] = cageDiagonal / 4.0f;
    // [57] cage_overload_risk
    out[57] = cageOverloadRisk;
    // [58] opp_cage_diagonal_quality
    out[58] = oppCageDiagonal / 4.0f;
    // [59] carrier_can_score (MA+2GFI >= dist)
    out[59] = carrierCanScore ? 1.0f : 0.0f;
    // [60] pass_scoring_threat (normalized by 3 = good count)
    out[60] = std::min(passThreats / 3.0f, 1.0f);
    // [61] frenzy_trap_risk (frenzy players in danger / total frenzy, or 0)
    out[61] = myFrenzyCount > 0 ? static_cast<float>(frenzyTraps) / myFrenzyCount : 0.0f;
    // [62] screen_between_ball (defenders between opp carrier and my endzone /5)
    out[62] = oppHasBall ? std::min(screenCount / 5.0f, 1.0f) : 0.0f;
    // [63] carrier_blitzable (binary — very bad if true)
    out[63] = (iHaveBall && carrierBlitzable) ? 1.0f : 0.0f;
    // [64] surfable_opponents (crowd-surf targets /3)
    out[64] = std::min(surfableOpps / 3.0f, 1.0f);
    // [65] favorable_blocks (/my_standing)
    out[65] = myStanding > 0 ? std::min(static_cast<float>(favorableBlocks) / myStanding, 1.0f) : 0.0f;
    // [66] one_turn_td_vulnerability (binary)
    out[66] = oneTurnTDVuln ? 1.0f : 0.0f;
    // [67] loose_ball_proximity (1.0 = I'm much closer)
    out[67] = looseBallProx;
    // [68] deep_safety_count (/3 = ideal)
    out[68] = std::min(deepSafeties / 3.0f, 1.0f);
    // [69] isolation_count (/my_standing, higher = worse)
    out[69] = myStanding > 0 ? static_cast<float>(isolatedCount) / myStanding : 0.0f;
}

} // namespace bb
