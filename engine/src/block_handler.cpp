#include "bb/block_handler.h"
#include "bb/helpers.h"
#include "bb/injury.h"
#include "bb/ball_handler.h"
#include <algorithm>

namespace bb {

// Score a block die face from the attacker's perspective (higher = better for attacker)
static int scoreFace(BlockDiceFace face, bool attHasBlock, bool defHasBlock,
                     bool defHasDodge, bool attHasTackle) {
    switch (face) {
        case BlockDiceFace::DEFENDER_DOWN:
            return 10;
        case BlockDiceFace::DEFENDER_STUMBLES:
            return (defHasDodge && !attHasTackle) ? 5 : 9;
        case BlockDiceFace::PUSHED:
            return 5;
        case BlockDiceFace::BOTH_DOWN:
            if (attHasBlock && !defHasBlock) return 8;  // only def falls
            if (attHasBlock && defHasBlock) return 4;   // nothing happens
            if (!attHasBlock && defHasBlock) return 1;  // only att falls
            return 3;  // both fall
        case BlockDiceFace::ATTACKER_DOWN:
            return 0;
    }
    return 0;
}

thread_local long g_dauntlessRolls = 0;

long takeDauntlessRollEvalsInSearch() {
    long v = g_dauntlessRolls;
    g_dauntlessRolls = 0;
    return v;
}

BlockDiceFace autoChooseBlockDie(const BlockDiceFace* faces, int count,
                                 bool attackerChooses,
                                 const Player& att, const Player& def) {
    bool attBlock = att.hasSkill(SkillName::Block);
    bool defBlock = def.hasSkill(SkillName::Block);
    bool defDodge = def.hasSkill(SkillName::Dodge);
    bool attTackle = att.hasSkill(SkillName::Tackle);

    int bestIdx = 0;
    int bestScore = scoreFace(faces[0], attBlock, defBlock, defDodge, attTackle);

    for (int i = 1; i < count; i++) {
        int s = scoreFace(faces[i], attBlock, defBlock, defDodge, attTackle);
        if (attackerChooses) {
            if (s > bestScore) { bestScore = s; bestIdx = i; }
        } else {
            if (s < bestScore) { bestScore = s; bestIdx = i; }
        }
    }
    return faces[bestIdx];
}

static bool shouldRerollBlock(BlockDiceFace face, const Player& att) {
    if (face == BlockDiceFace::ATTACKER_DOWN) return true;
    if (face == BlockDiceFace::BOTH_DOWN && !att.hasSkill(SkillName::Block)) return true;
    return false;
}

// The square a pushed player leaves the pitch through: one step directly
// away from whoever pushed him. resolvePushback reports a surf as
// pushDest = {-1,-1}, which classifyExit cannot read, so the throw-in
// template needs this reconstructed exit to know which edge it is centred on.
static Position pushOffPitchExit(Position pusher, Position pushed) {
    int dx = pushed.x - pusher.x;
    int dy = pushed.y - pusher.y;
    return Position{static_cast<int8_t>(pushed.x + (dx > 0) - (dx < 0)),
                    static_cast<int8_t>(pushed.y + (dy > 0) - (dy < 0))};
}

static int distanceToEdge(Position p) {
    return std::min({(int)p.x, Position::PITCH_WIDTH - 1 - p.x,
                     (int)p.y, Position::PITCH_HEIGHT - 1 - p.y});
}

// The squares a pushed player may be moved into.
//   normal — the three squares directly away from the pusher (push-back diagram)
//   wide   — any adjacent square.  Side Step: "the coach may choose to move the
//            player to any adjacent square, not just the three squares shown on
//            the Push Back diagram".  Grab: "he may choose any empty square
//            adjacent to his opponent".
// Both skills lapse when nothing adjacent is open ("may not use this skill if
// there are no open squares on the pitch adjacent to this player" / "Grab will
// not work if there are no empty adjacent squares"), so a wide set that turns
// up empty-handed falls back to the standard three and the push chains as usual.
static int pushCandidates(const GameState& state, Position pusherPos,
                          Position pushedPos, bool wide, Position out[8]) {
    if (wide) {
        int n = 0;
        for (Position p : pushedPos.getAdjacent()) {
            if (p.isOnPitch() && !state.getPlayerAtPosition(p)) out[n++] = p;
        }
        if (n > 0) return n;
    }
    return getPushbackSquares(pusherPos, pushedPos, out);
}

// CRP: "The player must be pushed back into an empty square if possible."  That
// holds no matter who is doing the choosing, so empty candidates always win and
// the skill heuristics only rank within them.  Only when every candidate is
// occupied does the push go into an occupied square and chain.
// A push may not hand the man being pushed the touchdown he is carrying the
// ball toward.  CRP scores a carrier who ends up standing in his opponent's end
// zone even during our own turn ("Scoring in the opponent's turn": "a player
// holding the ball could be pushed into the End Zone by a block"), so this is a
// gift we are free to decline by choosing another square.  It went unnoticed
// because choosePushSquare picks on geometry alone -- straight back first -- and
// straight back is sometimes the end zone: measured over 3000 games, eight
// opposing carriers were walked in that way (g0289, pusher on (23,8), carrier on
// (24,7), straight back (25,6)).  Nothing here is a trade-off; the other two
// squares are strictly better, so the only case left alone is the one where
// every square scores and there is no choice to decline.
static bool pushWouldScore(const GameState& state, const Player& pushed,
                           TeamSide blockingSide, Position dest) {
    if (pushed.teamSide == blockingSide) return false;   // our own man scoring is a gift
    if (!state.ball.isHeld || state.ball.carrierId != pushed.id) return false;
    // HOME scores on x = PITCH_WIDTH-1, AWAY on x = 0 (turn_handler.cpp).
    return dest.isInEndZone(pushed.teamSide == TeamSide::AWAY);
}

static int choosePushSquare(const GameState& state, const Position* cand, int count,
                            Position pusherPos, bool defenderChooses, bool towardEdge,
                            const Player& pushed, TeamSide blockingSide) {
    bool anyEmpty = false;
    for (int i = 0; i < count; i++) {
        if (!state.getPlayerAtPosition(cand[i])) { anyEmpty = true; break; }
    }

    // Side Step gives the choice to the pushed player's own coach, who would
    // take the touchdown, so the refusal only applies where the choice is ours.
    bool refuseScoring = false;
    if (!defenderChooses) {
        for (int i = 0; i < count; i++) {
            if (anyEmpty && state.getPlayerAtPosition(cand[i])) continue;
            if (!pushWouldScore(state, pushed, blockingSide, cand[i])) {
                refuseScoring = true;   // a square that does not score exists
                break;
            }
        }
    }

    int best = -1, bestScore = 0;
    for (int i = 0; i < count; i++) {
        if (anyEmpty && state.getPlayerAtPosition(cand[i])) continue;
        if (refuseScoring && pushWouldScore(state, pushed, blockingSide, cand[i])) continue;
        int score;
        if (defenderChooses)   score = cand[i].distanceTo(pusherPos);   // Side Step: get clear
        else if (towardEdge)   score = 100 - distanceToEdge(cand[i]);   // Grab: toward the crowd
        else                   score = count - i;                       // straight back first
        if (best < 0 || score > bestScore) { best = i; bestScore = score; }
    }
    return best < 0 ? 0 : best;
}

// Push one player a square away from `pusherPos`, chaining into whoever is in
// the way.  CRP: "If all such squares are occupied by other players, then the
// player is pushed into an occupied square, and the player that originally
// occupied the square is pushed back in turn.  This secondary push back is
// treated exactly like a normal push back as if the second player had been
// blocked by the first" — hence the recursion rather than a single hop.
// Returns true when the pushed player left the pitch.
//
// resolveSurfHere is false for the player who was actually blocked: resolveBlock
// runs his crowd surf itself, because it has to read his last square and then
// run the attacker's follow-up.  Further down the chain nobody else will, so
// those surfs are resolved here.
// Stand Firm reaches down the chain too: "If a player is pushed back into a
// player using Stand Firm then neither player moves." It is optional ("may
// choose to not be pushed back"), so it is used when it helps its owner --
// a bystander on the blocking team wants the free square a chain hands him,
// an opponent wants to jam the push.
static bool holdsGround(const Player& p, TeamSide blockingSide) {
    return p.hasSkill(SkillName::StandFirm) && p.teamSide != blockingSide;
}

static bool pushOne(GameState& state, Position pusherPos, Player& pushed,
                    bool sideStep, bool grab, bool resolveSurfHere,
                    TeamSide blockingSide, DiceRollerBase& dice, Position& dest,
                    std::vector<GameEvent>* events, int depth) {
    Position cand[8];
    int count = pushCandidates(state, pusherPos, pushed.position,
                               sideStep || grab, cand);

    // "Players must be pushed off the pitch if there are no eligible empty
    // squares on the pitch" — reached when nothing away from the pusher is on
    // the pitch at all.
    if (count == 0) {
        Position last = pushed.position;
        emitEvent(events, {GameEvent::Type::PUSH, pushed.id, -1, last, {-1, -1}, 0, true});
        dest = {-1, -1};
        if (resolveSurfHere) {
            if (state.ball.isHeld && state.ball.carrierId == pushed.id) {
                state.ball = BallState::onGround(last);
                resolveThrowIn(state, last, pushOffPitchExit(pusherPos, last), dice, events);
            } else {
                handleBallOnPlayerDown(state, pushed.id, dice, events);
            }
            pushed.position = {-1, -1};
            resolveCrowdSurf(state, pushed.id, dice, events);
        }
        return true;
    }

    dest = cand[choosePushSquare(state, cand, count, pusherPos,
                                 sideStep && !grab, grab && !sideStep,
                                 pushed, blockingSide)];

    Player* occupant = state.getPlayerAtPosition(dest);
    if (occupant && holdsGround(*occupant, blockingSide)) {
        // The coach picks the direction, so try any other body that will not
        // dig in before giving the push up.  The end-zone refusal holds here
        // too -- dodging a Stand Firm jam is not worth conceding a touchdown.
        for (int i = 0; i < count; i++) {
            Player* other = state.getPlayerAtPosition(cand[i]);
            if (other && !holdsGround(*other, blockingSide) &&
                !pushWouldScore(state, pushed, blockingSide, cand[i])) {
                dest = cand[i];
                occupant = other;
                break;
            }
        }
    }
    if (occupant && holdsGround(*occupant, blockingSide)) {
        dest = pushed.position;   // "neither player moves"
        return false;
    }

    // Chain. Depth is bounded by how many players can stand in a line, and the
    // guard keeps a corrupt board from recursing forever.
    if (occupant && depth < GameState::PLAYERS_TOTAL) {
        // "The coach of the moving team decides all push back directions for
        // secondary push backs unless the pushed player has a skill that
        // overrides this" — so Side Step carries down the chain, Grab does not
        // (it only ever applies to the player its owner blocked).
        Position chainDest;
        pushOne(state, pushed.position, *occupant,
                occupant->hasSkill(SkillName::SideStep), false,
                /*resolveSurfHere=*/true, blockingSide, dice, chainDest, events,
                depth + 1);
    }

    emitEvent(events, {GameEvent::Type::PUSH, pushed.id, -1,
                       pushed.position, dest, 0, true});
    pushed.position = dest;
    if (state.ball.isHeld && state.ball.carrierId == pushed.id) {
        state.ball.position = dest;
    } else if (!state.ball.isHeld && state.ball.position == dest) {
        // Pushed onto a loose ball -- it scatters, no catch attempt/turnover
        resolveBounce(state, dest, dice, 0, events);
    }
    return false;
}

// Resolve pushback: returns true if defender was pushed off pitch (crowd surf)
static bool resolvePushback(GameState& state, Player& attacker, Player& defender,
                            bool isBlitz, DiceRollerBase& dice,
                            Position& pushDest, std::vector<GameEvent>* events) {
    // StandFirm check (Juggernaut ignores on blitz)
    if (defender.hasSkill(SkillName::StandFirm)) {
        bool juggernautOverride = isBlitz && attacker.hasSkill(SkillName::Juggernaut);
        if (!juggernautOverride) {
            pushDest = defender.position;
            return false; // no push
        }
    }

    bool sideStep = defender.hasSkill(SkillName::SideStep);
    // "Grab only works on a Block Action" and "Grab and Side Step will cancel
    // each other out and the standard pushback rules apply".
    bool grab = attacker.hasSkill(SkillName::Grab) && !isBlitz;
    if (sideStep && grab) { sideStep = false; grab = false; }

    return pushOne(state, attacker.position, defender, sideStep, grab,
                   /*resolveSurfHere=*/false, attacker.teamSide, dice, pushDest,
                   events, 0);
}

ActionResult resolveBlock(GameState& state, const BlockParams& params,
                          DiceRollerBase& dice, std::vector<GameEvent>* events,
                          bool frenzySecondBlock, bool noFollowUp) {
    Player& att = state.getPlayer(params.attackerId);
    Player& def = state.getPlayer(params.targetId);
    Position attOldPos = att.position;
    Position defOldPos = def.position;

    // CRP: a block thrown as part of a Blitz costs 1 point of movement.
    // Out of normal movement the blitzer may GFI into the block (2+, 3+
    // in a blizzard, counts against the GFI limit); with no GFI left the
    // block cannot be thrown at all. This is what denies a Frenzy second
    // block after "GFI to arrive + GFI for the first block".
    if (params.isBlitz) {
        int gfiFloor = att.hasSkill(SkillName::Sprint) ? -3 : -2;
        if (att.movementRemaining - 1 < gfiFloor) {
            // No movement and no GFI left: no block is thrown.
            att.hasActed = true;
            return ActionResult::ok();
        }
        att.movementRemaining--;
        if (att.movementRemaining < 0) {
            int gfiTarget = (state.weather == Weather::BLIZZARD) ? 3 : 2;
            bool gfiOk = attemptRoll(state, att.id, dice, gfiTarget,
                                     SkillName::SureFeet, false, true, events);
            emitEvent(events, {GameEvent::Type::GFI, att.id, -1, att.position,
                              att.position, gfiTarget, gfiOk});
            if (!gfiOk) {
                // Falls before the block is thrown
                att.state = PlayerState::PRONE;
                att.hasActed = true;
                InjuryContext ctx;
                resolveArmourAndInjury(state, att.id, dice, ctx, events);
                handleBallOnPlayerDown(state, att.id, dice, events);
                return ActionResult::turnovr();
            }
        }
    }

    // 1. FoulAppearance check
    if (def.hasSkill(SkillName::FoulAppearance)) {
        int faRoll = dice.rollD6();
        if (faRoll == 1) {
            att.hasActed = true;
            emitEvent(events, {GameEvent::Type::SKILL_USED, def.id, att.id, {}, {},
                              static_cast<int>(SkillName::FoulAppearance), true});
            return ActionResult::fail(); // action wasted, not turnover
        }
    }

    // 2. Chainsaw
    if (att.hasSkill(SkillName::Chainsaw)) {
        int chainsawRoll = dice.rollD6();
        emitEvent(events, {GameEvent::Type::BLOCK, att.id, def.id, att.position,
                          def.position, chainsawRoll, chainsawRoll >= 2});

        if (chainsawRoll == 1) {
            // Kickback on attacker
            att.state = PlayerState::PRONE;
            emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, att.id, -1,
                              att.position, {}, 0, false});
            InjuryContext ctx;
            resolveArmourAndInjury(state, att.id, dice, ctx, events);
            handleBallOnPlayerDown(state, att.id, dice, events);
            att.hasActed = true;
            return ActionResult::turnovr();
        } else {
            // Armor roll on defender
            InjuryContext ctx;
            resolveArmourAndInjury(state, def.id, dice, ctx, events);
            handleBallOnPlayerDown(state, def.id, dice, events);
            att.hasActed = true;
            return ActionResult::ok();
        }
    }

    // 3. Stab
    if (att.hasSkill(SkillName::Stab)) {
        emitEvent(events, {GameEvent::Type::BLOCK, att.id, def.id, att.position,
                          def.position, 0, true});
        InjuryContext ctx;
        if (att.hasSkill(SkillName::Stakes)) ctx.hasStakes = true;
        resolveArmourAndInjury(state, def.id, dice, ctx, events);
        handleBallOnPlayerDown(state, def.id, dice, events);
        att.hasActed = true;
        return ActionResult::ok(); // Stab never causes turnover
    }

    // 4. Normal block — calculate strengths
    int attST = att.stats.strength;
    int defST = def.stats.strength;

    if (params.hornsBonus && att.hasSkill(SkillName::Horns)) {
        attST += 1;
    }

    // Dauntless, before assists are counted. CRP: "The skill only works when the
    // player attempts to block an opponent who is stronger than himself ... If
    // the total is greater, then the player with the Dauntless skill counts as
    // having a Strength equal to his opponent's when he makes the block. The
    // strength of both players is calculated before any defensive or offensive
    // assists are added but after all other modifiers."
    //
    // It used to be tested after assists, against effDefST > effAttST, which
    // fired it whenever the opponent merely out-assisted us -- and then it could
    // not fail, since d6 + ST > ST always holds at equal strength. It also
    // equalised onto the opponent's assisted total, handing our player their
    // assists instead of his own. Both are gone: the comparison, the roll and
    // the equalisation now all happen on the modified pre-assist strengths, and
    // assists are added on top afterwards.
    if (att.hasSkill(SkillName::Dauntless) && defST > attST) {
        // ⛔ Tady stálo, že tohle počítá, „kolikrát se OPRAVDU hodil", a že
        // rozdíl proti nabídce odpovídá na „vzal si to search?". Obojí je
        // nepravda: sem se chodí i z každé simulace MCTS. 349/zápas proti
        // 1,88 odehraným (P25, 17.08.) -- 186×. Odehrané se čtou z logu:
        // SKILL_USED s roll == SkillName::Dauntless.
        ++g_dauntlessRolls;
        int dauntlessRoll = dice.rollD6();
        bool psyched = dauntlessRoll + attST > defST;
        if (psyched) {
            attST = defST;
        }
        emitEvent(events, {GameEvent::Type::SKILL_USED, att.id, -1, {}, {},
                          static_cast<int>(SkillName::Dauntless), psyched});
    }

    int attAssists = countAssists(state, def.position, att.teamSide, att.id, def.id, def.id);
    int defAssists = countAssists(state, att.position, def.teamSide, att.id, def.id, att.id);

    int effAttST = attST + attAssists;
    int effDefST = defST + defAssists;

    // Roll block dice
    BlockDiceInfo diceInfo = getBlockDiceInfo(effAttST, effDefST);

    BlockDiceFace faces[3];
    for (int i = 0; i < diceInfo.count; i++) {
        faces[i] = dice.rollBlockDie();
    }

    // Choose best die
    BlockDiceFace chosen = autoChooseBlockDie(faces, diceInfo.count,
                                               diceInfo.attackerChooses, att, def);

    // Juggernaut: BD → PUSHED on blitz
    if (chosen == BlockDiceFace::BOTH_DOWN && params.isBlitz &&
        att.hasSkill(SkillName::Juggernaut)) {
        chosen = BlockDiceFace::PUSHED;
    }

    // Pro/team reroll on bad result
    if (shouldRerollBlock(chosen, att)) {
        bool rerolled = false;

        // Pro reroll
        if (!rerolled && att.hasSkill(SkillName::Pro) && !att.proUsedThisTurn) {
            att.proUsedThisTurn = true;
            int proRoll = dice.rollD6();
            if (proRoll >= 4) {
                for (int i = 0; i < diceInfo.count; i++) faces[i] = dice.rollBlockDie();
                chosen = autoChooseBlockDie(faces, diceInfo.count,
                                             diceInfo.attackerChooses, att, def);
                if (chosen == BlockDiceFace::BOTH_DOWN && params.isBlitz &&
                    att.hasSkill(SkillName::Juggernaut)) {
                    chosen = BlockDiceFace::PUSHED;
                }
                rerolled = true;
            }
        }

        // Team reroll
        if (!rerolled && shouldRerollBlock(chosen, att)) {
            TeamState& team = state.getTeamState(att.teamSide);
            if (team.canUseReroll()) {
                team.rerolls--;
                team.rerollUsedThisTurn = true;

                bool canReroll = true;
                if (att.hasSkill(SkillName::Loner)) {
                    int lonerRoll = dice.rollD6();
                    if (lonerRoll < 4) canReroll = false;
                }

                if (canReroll) {
                    for (int i = 0; i < diceInfo.count; i++) faces[i] = dice.rollBlockDie();
                    chosen = autoChooseBlockDie(faces, diceInfo.count,
                                                 diceInfo.attackerChooses, att, def);
                    if (chosen == BlockDiceFace::BOTH_DOWN && params.isBlitz &&
                        att.hasSkill(SkillName::Juggernaut)) {
                        chosen = BlockDiceFace::PUSHED;
                    }
                }
            }
        }
    }

    emitEvent(events, {GameEvent::Type::BLOCK, att.id, def.id, att.position,
                      def.position, static_cast<int>(chosen), true});

    // 5. Apply block result
    bool defPushed = false;
    bool defKnockedDown = false;
    bool attKnockedDown = false;
    bool turnover = false;
    Position pushDest = def.position;

    switch (chosen) {
        case BlockDiceFace::ATTACKER_DOWN: {
            att.state = PlayerState::PRONE;
            attKnockedDown = true;
            turnover = true;
            emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, att.id, -1,
                              att.position, {}, 0, false});
            break;
        }

        case BlockDiceFace::BOTH_DOWN: {
            // Check Wrestle. Juggernaut cancels the DEFENDER's Wrestle on a
            // Blitz (rules parity, 2026-08-10) -- CRP Juggernaut: "the
            // opposing player may not use his Fend, Stand Firm or Wrestle
            // skills against the Juggernaut player's blocks." Stand Firm was
            // already handled (resolvePushback), Fend and Wrestle were not.
            bool defWrestle = def.hasSkill(SkillName::Wrestle) &&
                              !(params.isBlitz && att.hasSkill(SkillName::Juggernaut));
            bool attWrestle = att.hasSkill(SkillName::Wrestle) &&
                              !att.hasSkill(SkillName::Block);

            if (defWrestle || attWrestle) {
                // Wrestle: both prone, no armor, no turnover
                att.state = PlayerState::PRONE;
                def.state = PlayerState::PRONE;
                emitEvent(events, {GameEvent::Type::SKILL_USED,
                                  defWrestle ? def.id : att.id, -1, {}, {},
                                  static_cast<int>(SkillName::Wrestle), true});
                // Handle ball on either player going down
                handleBallOnPlayerDown(state, att.id, dice, events);
                handleBallOnPlayerDown(state, def.id, dice, events);
                att.hasActed = true;
                return ActionResult::ok(); // Wrestle = no turnover
            }

            // No Wrestle — check Block
            bool attFalls = !att.hasSkill(SkillName::Block);
            bool defFalls = !def.hasSkill(SkillName::Block);

            if (attFalls) {
                att.state = PlayerState::PRONE;
                attKnockedDown = true;
                turnover = true;
                emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, att.id, -1,
                                  att.position, {}, 0, false});
            }
            if (defFalls) {
                defKnockedDown = true;
                defPushed = true; // defender gets pushed then knocked down
            }

            if (!defPushed) {
                // Both have Block or only attacker falls
                if (attKnockedDown) {
                    InjuryContext attCtx;
                    resolveArmourAndInjury(state, att.id, dice, attCtx, events);
                    handleBallOnPlayerDown(state, att.id, dice, events);
                }
                att.hasActed = true;
                return turnover ? ActionResult::turnovr() : ActionResult::ok();
            }
            break;
        }

        case BlockDiceFace::PUSHED: {
            defPushed = true;
            break;
        }

        case BlockDiceFace::DEFENDER_STUMBLES: {
            // Dodge saves: DS → PUSHED (unless Tackle)
            if (def.hasSkill(SkillName::Dodge) && !att.hasSkill(SkillName::Tackle)) {
                defPushed = true; // just a push
            } else {
                defPushed = true;
                defKnockedDown = true;
            }
            break;
        }

        case BlockDiceFace::DEFENDER_DOWN: {
            defPushed = true;
            defKnockedDown = true;
            break;
        }
    }

    // Handle attacker knocked down (from AD)
    if (attKnockedDown && chosen == BlockDiceFace::ATTACKER_DOWN) {
        InjuryContext attCtx;
        resolveArmourAndInjury(state, att.id, dice, attCtx, events);
        handleBallOnPlayerDown(state, att.id, dice, events);
        att.hasActed = true;
        return ActionResult::turnovr();
    }

    // Pushback resolution
    if (defPushed) {
        bool crowdSurf = resolvePushback(state, att, def, params.isBlitz, dice,
                                          pushDest, events);

        if (crowdSurf) {
            // Crowd surf. A surfed BALL CARRIER does not just drop the ball
            // at the touchline (rules parity, 2026-08-10). CRP: "If the
            // player who is holding the ball is pushed out of bounds, then
            // he is beaten up by the fans, who are more than happy to throw
            // the ball back into play! The Throw-in template is centred on
            // the last square the player was in before he was pushed off
            // the pitch." We used to call handleBallOnPlayerDown, leaving
            // the ball a single bounce from the edge -- so a surf won us the
            // removal AND the ball, and was badly overpaid.
            Position lastPos = def.position;
            if (state.ball.isHeld && state.ball.carrierId == def.id) {
                state.ball = BallState::onGround(lastPos);
                resolveThrowIn(state, lastPos, pushOffPitchExit(att.position, lastPos),
                               dice, events);
            } else {
                handleBallOnPlayerDown(state, def.id, dice, events);
            }
            def.position = {-1, -1};
            resolveCrowdSurf(state, def.id, dice, events);

            // StripBall: ball drops at crowd edge (already handled by crowd surf)

            // Follow-up: attacker to old defender position
            if (!noFollowUp) {
                att.position = defOldPos;
                if (state.ball.isHeld && state.ball.carrierId == att.id) {
                    state.ball.position = att.position;
                } else if (!state.ball.isHeld && state.ball.position == att.position) {
                    // Following up onto a loose ball attempts a pickup,
                    // same as deliberately moving onto one.
                    if (!resolvePickup(state, att.id, dice, events)) {
                        turnover = true;
                    }
                }
            }
            att.hasActed = true;

            // Handle BD attacker knockdown
            if (attKnockedDown) {
                InjuryContext attCtx;
                resolveArmourAndInjury(state, att.id, dice, attCtx, events);
                handleBallOnPlayerDown(state, att.id, dice, events);
            }
            return turnover ? ActionResult::turnovr() : ActionResult::ok();
        }

        // StripBall: if defender has ball and not knocked down, and attacker has StripBall.
        if (!defKnockedDown && state.ball.isHeld && state.ball.carrierId == def.id &&
            att.hasSkill(SkillName::StripBall)) {
            if (def.hasSkill(SkillName::SureHands)) {
                // Sure Hands negates Strip Ball (BB2016/LRB6) — ball stays with carrier.
                emitEvent(events, {GameEvent::Type::SKILL_USED, def.id, att.id,
                                  def.position, {},
                                  static_cast<int>(SkillName::SureHands), true});
            } else {
                state.ball = BallState::onGround(def.position);
                resolveBounce(state, def.position, dice, 0, events);
            }
        }

        // Follow-up: attacker moves to old defender position.
        // Fend forbids the follow-up EVEN IF the Fend player goes down
        // (rules parity, 2026-08-10). CRP Fend: "Opposing players may not
        // follow-up blocks made against this player even if the Fend player
        // is Knocked Down." Our `!defKnockedDown` guard switched Fend off in
        // exactly the results where a follow-up is usually at stake, leaving
        // it working only on a clean Pushed. Juggernaut cancels it on a
        // Blitz: "the opposing player may not use his Fend, Stand Firm or
        // Wrestle skills against the Juggernaut player's blocks."
        bool fendPrevents = def.hasSkill(SkillName::Fend) &&
                            !(params.isBlitz && att.hasSkill(SkillName::Juggernaut));
        // A follow-up needs a square that was actually vacated. Stand Firm (and
        // a chain that jams against one) leaves the defender where he stood, and
        // we used to walk the attacker onto him -- two players on one square.
        bool defVacated = def.position != defOldPos;
        if (!noFollowUp && !fendPrevents && defVacated) {
            att.position = defOldPos;
            if (state.ball.isHeld && state.ball.carrierId == att.id) {
                state.ball.position = att.position;
            } else if (!state.ball.isHeld && state.ball.position == att.position) {
                // Following up onto a loose ball attempts a pickup, same
                // as deliberately moving onto one.
                if (!resolvePickup(state, att.id, dice, events)) {
                    turnover = true;
                }
            }
        }

        // Knockdown
        if (defKnockedDown) {
            def.state = PlayerState::PRONE;
            emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, def.id, -1,
                              def.position, {}, 0, false});

            InjuryContext defCtx;
            // One flag, not two modifiers: resolveArmourAndInjury decides which
            // roll it lands on, because CRP spends Mighty Blow on one of them.
            if (att.hasSkill(SkillName::MightyBlow)) defCtx.mightyBlow = true;
            if (att.hasSkill(SkillName::Claw)) defCtx.hasClaw = true;
            if (att.hasSkill(SkillName::Stakes)) defCtx.hasStakes = true;
            if (def.hasSkill(SkillName::Decay)) defCtx.hasDecay = true;

            resolveArmourAndInjury(state, def.id, dice, defCtx, events);
            handleBallOnPlayerDown(state, def.id, dice, events);
        }
    }

    // Handle BD attacker knockdown (if both down and attacker falls)
    if (attKnockedDown && chosen == BlockDiceFace::BOTH_DOWN) {
        InjuryContext attCtx;
        resolveArmourAndInjury(state, att.id, dice, attCtx, events);
        handleBallOnPlayerDown(state, att.id, dice, events);
    }

    att.hasActed = true;

    // Frenzy: if both standing and adjacent after block, mandatory 2nd block
    if (!frenzySecondBlock && att.hasSkill(SkillName::Frenzy) &&
        canAct(att.state) && canAct(def.state) &&
        att.position.distanceTo(def.position) == 1) {
        // CRP: during a Blitz the second block costs movement too — with
        // no movement and no GFI left it is not thrown. Gated here (not
        // only via the charge in the recursive call) so the first block's
        // outcome is preserved unchanged when the second is impossible.
        if (params.isBlitz) {
            int gfiFloor = att.hasSkill(SkillName::Sprint) ? -3 : -2;
            if (att.movementRemaining - 1 < gfiFloor) {
                return turnover ? ActionResult::turnovr() : ActionResult::ok();
            }
        }
        // Mandatory second block
        BlockParams secondParams = params;
        secondParams.hornsBonus = false; // no Horns on 2nd block
        att.hasActed = false; // allow second block
        ActionResult secondResult = resolveBlock(state, secondParams, dice, events, true);
        return secondResult;
    }

    return turnover ? ActionResult::turnovr() : ActionResult::ok();
}

ActionResult resolveMultipleBlock(GameState& state, int attackerId,
                                  int target1Id, int target2Id,
                                  DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& att = state.getPlayer(attackerId);

    // Resolve first block (defender gets +2 ST, no follow-up)
    {
        Player& def1 = state.getPlayer(target1Id);

        // FoulAppearance check
        if (def1.hasSkill(SkillName::FoulAppearance)) {
            int faRoll = dice.rollD6();
            if (faRoll == 1) {
                // Skip this block (action wasted on this target)
                emitEvent(events, {GameEvent::Type::SKILL_USED, def1.id, att.id, {}, {},
                                  static_cast<int>(SkillName::FoulAppearance), true});
                goto second_block;
            }
        }

        {
            // Temporarily add +2 ST to defender
            int8_t origST = def1.stats.strength;
            def1.stats.strength += 2;

            BlockParams params;
            params.attackerId = attackerId;
            params.targetId = target1Id;
            params.isBlitz = false;
            params.hornsBonus = false;
            ActionResult result = resolveBlock(state, params, dice, events, false, true);

            def1.stats.strength = origST;

            // If attacker went down, turnover — skip 2nd block
            if (result.turnover || att.state != PlayerState::STANDING) {
                return ActionResult::turnovr();
            }
        }
    }

second_block:
    // Reset hasActed so 2nd block can proceed
    att.hasActed = false;

    // Resolve second block (same rules)
    {
        Player& def2 = state.getPlayer(target2Id);

        // FoulAppearance check
        if (def2.hasSkill(SkillName::FoulAppearance)) {
            int faRoll = dice.rollD6();
            if (faRoll == 1) {
                att.hasActed = true;
                emitEvent(events, {GameEvent::Type::SKILL_USED, def2.id, att.id, {}, {},
                                  static_cast<int>(SkillName::FoulAppearance), true});
                return ActionResult::ok();
            }
        }

        // Temporarily add +2 ST to defender
        int8_t origST = def2.stats.strength;
        def2.stats.strength += 2;

        BlockParams params;
        params.attackerId = attackerId;
        params.targetId = target2Id;
        params.isBlitz = false;
        params.hornsBonus = false;
        ActionResult result = resolveBlock(state, params, dice, events, false, true);

        def2.stats.strength = origST;

        return result;
    }
}

} // namespace bb
