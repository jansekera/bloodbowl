#include "bb/block_handler.h"
#include <cmath>
#include "bb/helpers.h"
#include "bb/injury.h"
#include "bb/ball_handler.h"
#include <algorithm>

namespace bb {

// Score a block die face from the attacker's perspective (higher = better for attacker)
//
// ⛔ 24.08.2026: tabulka neznala WRESTLE, a tim si sama zavirala dvere, ktere
// jsme prave otevreli v resolveru. Utocnik s Block+Wrestle proti obranci
// s Blockem: BOTH_DOWN se ocenoval 4 ("nestane se nic"), PUSHED 5 -- takze
// vybirac sahl po odsunu a Wrestle se NIKDY nespustil. Presne ta situace,
// kvuli ktere se o Wrestle u trpaslika uvazuje: PROLOMENI ZDI, kde odsun
// nestaci a je potreba obrance SLOZIT. Tohle je zas jednou "filtr ocenuje
// jinou akci, nez jakou resolver provede".
static int scoreFace(BlockDiceFace face, bool attHasBlock, bool defHasBlock,
                     bool defHasDodge, bool attHasTackle,
                     bool attUsesWrestle, bool defUsesWrestle) {
    switch (face) {
        case BlockDiceFace::DEFENDER_DOWN:
            return 10;
        case BlockDiceFace::DEFENDER_STUMBLES:
            return (defHasDodge && !attHasTackle) ? 5 : 9;
        case BlockDiceFace::PUSHED:
            return 5;
        case BlockDiceFace::BOTH_DOWN:
            // Wrestle prepisuje vsechny ctyri radky pod sebou: oba jdou na zem
            // bez hodu na zbroj, bez ohledu na Block (l. 8674-8676).
            if (attUsesWrestle) {
                // Obrance SLOZEN. Neni to zadarmo -- utocnik lezi taky, takze
                // to je min nez DEFENDER_DOWN a min nez cisty odsun s hodem na
                // zbroj proti nekomu, kdo by stejne spadl. Vic nez PUSHED (5)
                // to je jen tam, kde odsun problem neresi: proti Blocku.
                return defHasBlock ? 7 : 6;
            }
            if (defUsesWrestle) return 1;               // obrance nas polozi taky
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

// P9/P9c arm -- see bb/block_handler.h for the measured motivation.
thread_local bool g_pushGeometry[2] = {false, false};
thread_local long g_pushGeometryPicks = 0;

long takeDauntlessRollEvalsInSearch() {
    long v = g_dauntlessRolls;
    g_dauntlessRolls = 0;
    return v;
}

void setPushGeometryArm(TeamSide side, bool on) {
    g_pushGeometry[side == TeamSide::HOME ? 0 : 1] = on;
}

bool pushGeometryArm(TeamSide side) {
    return g_pushGeometry[side == TeamSide::HOME ? 0 : 1];
}

long takePushGeometryEvalsInSearch() {
    long v = g_pushGeometryPicks;
    g_pushGeometryPicks = 0;
    return v;
}

BlockDiceFace autoChooseBlockDie(const BlockDiceFace* faces, int count,
                                 bool attackerChooses,
                                 const Player& att, const Player& def) {
    bool attBlock = att.hasSkill(SkillName::Block);
    bool defBlock = def.hasSkill(SkillName::Block);
    bool defDodge = def.hasSkill(SkillName::Dodge);
    bool attTackle = att.hasSkill(SkillName::Tackle);
    // Musi to byt TAZ podminka jako v resolveru (case BOTH_DOWN), jinak vybirac
    // ocenuje jinou vec, nez se pak stane. Mic sem nedosahne, takze se pocita
    // konzervativne: bez mice, tj. jak to vypada ve zdi.
    bool attWrestle = att.hasSkill(SkillName::Wrestle) &&
                      (!attBlock || defBlock);
    bool defWrestle = def.hasSkill(SkillName::Wrestle) && !defBlock;

    int bestIdx = 0;
    int bestScore = scoreFace(faces[0], attBlock, defBlock, defDodge, attTackle,
                              attWrestle, defWrestle);

    for (int i = 1; i < count; i++) {
        int s = scoreFace(faces[i], attBlock, defBlock, defDodge, attTackle,
                          attWrestle, defWrestle);
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

// P9/P9c (2026-08-18): score a push DESTINATION instead of taking the first
// square geometry offers. The ordering is the user's, stated 2026-08-14:
//   "priorita u špinavého rohu je odklidit protihráče PRYČ od rohu -- ne jej
//    nechat u rohu a posunout blíž k balonu"
// so it is a sequence, not a trade-off:
//   (1) he stops being adjacent to a corner of OUR cage -- that was the point
//   (2) he does not end up closer to OUR ball carrier  -- REACH0, -16.7 sigma
//   (3) straight back                                   -- today's behaviour
// (3) stays as the tiebreak, so with no carrier on the pitch (i.e. on defence)
// this scores exactly like the old code and the arm is a no-op by construction.
static int pushDestScore(const GameState& state, TeamSide blockingSide,
                         Position dest, int straightBackScore) {
    const Player* carrier = nullptr;
    if (state.ball.isHeld && state.ball.carrierId > 0) {
        const Player& c = state.getPlayer(state.ball.carrierId);
        if (c.teamSide == blockingSide && c.isOnPitch()) carrier = &c;
    }
    if (!carrier) return straightBackScore;

    auto cheb = [](Position a, Position b) {
        return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
    };

    // (1) does he still dirty a STANDING corner of our cage?
    bool dirties = false;
    for (int dx = -1; dx <= 1 && !dirties; dx += 2) {
        for (int dy = -1; dy <= 1 && !dirties; dy += 2) {
            Position corner{static_cast<int8_t>(carrier->position.x + dx),
                            static_cast<int8_t>(carrier->position.y + dy)};
            if (!corner.isOnPitch()) continue;
            const Player* occ = state.getPlayerAtPosition(corner);
            if (!occ || occ->teamSide != blockingSide ||
                occ->state != PlayerState::STANDING) continue;
            if (cheb(dest, corner) <= 1) dirties = true;
        }
    }

    // (2) distance from our carrier, capped -- past 4 squares he is out of
    // reach anyway and further shoving buys nothing worth outranking (1).
    int dist = std::min(cheb(dest, carrier->position), 4);

    return (dirties ? 0 : 10000) + 100 * dist + straightBackScore;
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
        else if (pushGeometryArm(blockingSide))
            score = pushDestScore(state, blockingSide, cand[i], count - i);  // P9/P9c
        else                   score = count - i;                       // straight back first
        if (best < 0 || score > bestScore) { best = i; bestScore = score; }
    }
    if (best < 0) return 0;

    // Count only the pushes the arm actually REDIRECTED. A counter that ticked
    // on every push would say "the arm ran" even where it agreed with straight
    // back, and the per-pair null control needs the number that can be zero.
    if (!defenderChooses && !towardEdge && pushGeometryArm(blockingSide)) {
        int plain = -1, plainScore = 0;
        for (int i = 0; i < count; i++) {
            if (anyEmpty && state.getPlayerAtPosition(cand[i])) continue;
            if (refuseScoring && pushWouldScore(state, pushed, blockingSide, cand[i])) continue;
            int sc = count - i;
            if (plain < 0 || sc > plainScore) { plain = i; plainScore = sc; }
        }
        if (plain >= 0 && plain != best) ++g_pushGeometryPicks;
    }
    return best;
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
    // Take Root (l. 8578-8579) drzi pole bezpodminecne -- i vlastnimu tymu:
    // zakorenený nesmi byt odsunut "for any reason", takze na rozdil od Stand
    // Firm to neni volba jeho trenéra a nezalezi na tom, kdo blokuje.
    if (p.rooted) return true;
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
    // Take Root, l. 8578-8579: zakorenený hráč "may not Go For It, BE PUSHED
    // BACK FOR ANY REASON, or use any skill that would allow him to move out of
    // his current square". Zadny Juggernaut override -- pravidlo vyjimku nema.
    if (defender.rooted) {
        pushDest = defender.position;
        return false; // no push
    }

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

    // Take Root, l. 8580-8582: "The player may block adjacent players WITHOUT
    // FOLLOWING-UP as part of a Block Action." Zakorenený se tedy nikam nehne
    // -- follow-up by ho vytahl z pole, ktere opustit nesmi (l. 8578-8579).
    if (att.rooted) noFollowUp = true;

    // CRP: a block thrown as part of a Blitz costs 1 point of movement.
    // Out of normal movement the blitzer may GFI into the block (2+, 3+
    // in a blizzard, counts against the GFI limit); with no GFI left the
    // block cannot be thrown at all. This is what denies a Frenzy second
    // block after "GFI to arrive + GFI for the first block".
    if (params.isBlitz) {
        // Take Root, l. 8577-8578: zakorenený nesmi GFI.
        int gfiFloor = att.rooted ? 0 : (att.hasSkill(SkillName::Sprint) ? -3 : -2);
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

        // TA7 (24.08.2026) -- BB2016 l. 8001-8006: "Make an Armour roll for the
        // player hit by the chainsaw, ADDING 3 TO THE SCORE. If the roll beats
        // the victim's Armour value then the victim is Knocked Down and injured.
        // If the roll FAILS to beat the victim's Armour value then the attack
        // HAS NO EFFECT."
        // Dve vady: (1) +3 nikde -- pila mela probojnost obycejneho bloku;
        // (2) kickback srazil nositele a vratil turnover BEZ OHLEDU na to, jestli
        // zbroj vubec prorazil, a test `ChainsawKickback` to certifikoval
        // (kostky {1,3,3} => 6 <= AV8 "neproraženo", a presto PRONE + turnover).
        InjuryContext ctx;
        ctx.armourModifier = 3;

        if (chainsawRoll == 1) {
            // Kickback: obeti je NOSITEL
            bool broken = resolveArmourAndInjury(state, att.id, dice, ctx, events);
            att.hasActed = true;
            if (!broken) return ActionResult::ok();   // "no effect"
            emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, att.id, -1,
                              att.position, {}, 0, false});
            handleBallOnPlayerDown(state, att.id, dice, events);
            // Sražený hráč aktivního tymu = turnover (katalog l. 368-369).
            return ActionResult::turnovr();
        } else {
            bool broken = resolveArmourAndInjury(state, def.id, dice, ctx, events);
            att.hasActed = true;
            if (broken) {
                emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, def.id, -1,
                                  def.position, {}, 0, false});
                handleBallOnPlayerDown(state, def.id, dice, events);
            }
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
    // Dauntless, BB2016 l. 8023-8033 (doslova shodne s CRP): "The skill only
    // works when the player attempts to block an opponent who is STRONGER than
    // himself. ... rolls a D6 and adds it to his strength. If the total is EQUAL
    // TO OR LOWER than the opponent's Strength, the player must block using his
    // normal Strength. If the total is GREATER, then the player counts as having
    // a Strength EQUAL TO HIS OPPONENT'S ... calculated BEFORE any defensive or
    // offensive assists are added but AFTER all other modifiers."
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
            // ⛔ VOLBA, NE MECHANIKA (24.08.2026). BB2016 l. 8672-8676: "This
            // player MAY USE Wrestle ... Both players are Placed Prone in their
            // respective squares EVEN IF ONE OR BOTH HAVE THE BLOCK SKILL."
            // Block tedy Wrestle NEPREBIJI -- rozhoduje trenér. Do 24.08. byla
            // volba zadratovana, a to v OPACNYCH smerech: utocnik s Blockem
            // Wrestle nepouzil NIKDY (`&& !att.hasSkill(Block)`), obrance
            // naopak VZDY. Kombinace Block+Wrestle na jednom hráči tim byla
            // v utoku mrtva -- prave ta, kvuli ktere se o Wrestle u trpaslika
            // uvazuje (Longbeard).
            const bool attHasBall = state.ball.isHeld && state.ball.carrierId == att.id;
            const bool defHasBall = state.ball.isHeld && state.ball.carrierId == def.id;

            // Utocnik: bez Blocku ho Both Down slozi tak jako tak, takze Wrestle
            // je lepsi vzdy (slozi i souperě a nikdo nehazi na zbroj). S Blockem
            // by zustal stat, takze Wrestle stoji za to jen kdyz je co ziskat:
            // souper ma Block (jinak by se NESTALO NIC), nebo drzi mic.
            const bool attWantsWrestle =
                att.hasSkill(SkillName::Wrestle) &&
                (!att.hasSkill(SkillName::Block) ||
                 (!attHasBall && (def.hasSkill(SkillName::Block) || defHasBall)));

            // Obrance: kdyz ma Block, Both Down ho necha stat a slozi utocnika
            // -- to je lepsi nez Wrestle, ledaze utocnik drzi mic (pak je jeho
            // polozeni na zem turnover, l. 368-372). Bez Blocku pada tak jako
            // tak, takze Wrestle bere jako lepsi variantu (bez hodu na zbroj).
            const bool defWantsWrestle =
                def.hasSkill(SkillName::Wrestle) &&
                (!def.hasSkill(SkillName::Block) || attHasBall);

            // Juggernaut na blitzu rusi Wrestle OBRANCE (rules parity 10.08.):
            // "the opposing player may not use his Fend, Stand Firm or Wrestle
            // skills against the Juggernaut player's blocks."
            bool defWrestle = defWantsWrestle &&
                              !(params.isBlitz && att.hasSkill(SkillName::Juggernaut));
            bool attWrestle = attWantsWrestle;

            if (defWrestle || attWrestle) {
                // F11, 24.08.2026: Wrestle NENI bezpodminecne bez turnoveru.
                // BB2016 l. 8677-8678: "Use of this skill does not cause
                // a turnover UNLESS THE ACTIVE PLAYER WAS HOLDING THE BALL",
                // a katalog turnoveru l. 368-372 to rika stejne: "being Placed
                // Prone is not a turnover unless it is a player from the active
                // team holding the ball ... e.g. skills like Diving Tackle,
                // Piling On and Wrestle count as being Placed Prone".
                // Cte se PRED padem, protoze handleBallOnPlayerDown mic upusti.
                const bool activeHeldBall =
                    state.ball.isHeld && state.ball.carrierId == att.id;

                // Wrestle: both prone, no armor
                att.state = PlayerState::PRONE;
                def.state = PlayerState::PRONE;
                emitEvent(events, {GameEvent::Type::SKILL_USED,
                                  defWrestle ? def.id : att.id, -1, {}, {},
                                  static_cast<int>(SkillName::Wrestle), true});
                // Handle ball on either player going down
                handleBallOnPlayerDown(state, att.id, dice, events);
                handleBallOnPlayerDown(state, def.id, dice, events);
                att.hasActed = true;
                return activeHeldBall ? ActionResult::turnovr() : ActionResult::ok();
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
            // ⛔⛔ N8 (24.08.2026): tady stalo `defPushed = true;` s komentarem
            // "defender gets pushed then knocked down" -- to je ale chovani
            // DEFENDER DOWN, ne BOTH DOWN.
            // BB2016 l. 514-519: "BOTH DOWN: **Both players are Knocked Down**,
            // unless one or both of the players involved has the Block skill."
            // Zadny odsun, zadny follow-up. Srovnej l. 530-533 (Defender Down):
            // "The defending player is **pushed back and then Knocked Down in
            // the square they are moved to**. The attacking player may follow up."
            // ⇒ Kazdy Both Down proti obranci bez Blocku posouval DVA hráče
            // o pole, kam pravidla nedovoluji -- a `scoreFace` tuhle tvar ceni
            // 8/10 pro utocnika s Blockem, takze se VOLI casto. Deformovalo to
            // klece, screeny a vsechna dosavadni pozicni mereni.
            if (defFalls) {
                def.state = PlayerState::PRONE;
                emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, def.id, -1,
                                  def.position, {}, 0, false});
                InjuryContext defCtx;
                if (att.hasSkill(SkillName::MightyBlow)) defCtx.mightyBlow = true;
                if (att.hasSkill(SkillName::Claw)) defCtx.hasClaw = true;
                resolveArmourAndInjury(state, def.id, dice, defCtx, events);
                handleBallOnPlayerDown(state, def.id, dice, events);
            }
            if (attKnockedDown) {
                InjuryContext attCtx;
                resolveArmourAndInjury(state, att.id, dice, attCtx, events);
                handleBallOnPlayerDown(state, att.id, dice, events);
            }
            att.hasActed = true;
            return turnover ? ActionResult::turnovr() : ActionResult::ok();
        }

        case BlockDiceFace::PUSHED: {
            defPushed = true;
            break;
        }

        case BlockDiceFace::DEFENDER_STUMBLES: {
            // Dodge saves: DS → PUSHED (unless Tackle)
            // Tackle, l. 8569-8571: "...nor may they use their Dodge skill if
            // the player throws a block at them and uses the Tackle skill."
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

        // Strip Ball, l. 8517-8521: "When a player with this skill blocks an
        // opponent with the ball, applying a 'Pushed' or 'Defender Stumbles'
        // result will cause the opposing player to DROP THE BALL IN THE SQUARE
        // THAT THEY ARE PUSHED TO, EVEN IF the opposing player is NOT KNOCKED
        // DOWN." Sražený nosič mic pusti i bez Strip Ballu, takze zdejsi vetev
        // resi prave to "i kdyz nespadl". Sure Hands ho rusi (l. 8546-8548).
        // Mic padá z def.position, coz uz JE cilove pole odsunu (odsun probehl
        // vys v resolvePushback).
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

    // M1/N10 (25.08.2026): BB2016 l. 347-350 -- "He may make one block during
    // the move. The block may be made AT ANY POINT during the move." A Block
    // Action IS the activation and ends here; a Blitz is a move with a block
    // inside it, so it stays open while the blitzer is still on his feet.
    // Every other `hasActed = true` above is a path where the attacker went
    // down or the action was wasted (Foul Appearance, chainsaw kickback, stab,
    // failed GFI) -- those close the activation in a Blitz too, which is why
    // this is the only site that changes.
    // ⚠️ The offer side is a SEPARATE half: macro_actions.cpp:584 does not emit
    // REPOSITION for a player adjacent to a standing opponent, i.e. exactly the
    // player who just blocked. Reopening the activation without that gives him
    // permission to move and nowhere to go.
    att.hasActed = !(params.isBlitz && canAct(att.state));

    // Frenzy: if both standing and adjacent after block, mandatory 2nd block.
    // ⛔ JEN PO PUSHED / DEFENDER STUMBLES (oprava 21.08.). BB2016 l. 8139-8141:
    // "If a 'Pushed' or 'Defender Stumbles' result WAS CHOSEN, the player must
    // immediately throw a second block." Dosud se druhý blok házel po každém
    // výsledku, kde oba zůstali stát a sousedili -- tedy i po BOTH_DOWN, kdy
    // oba mají Block a nikdo nepadne. To je blok navíc zadarmo, a v TV1200 to
    // má jen trpaslík (2 Troll Slayeři), takže to hrálo PRO nás.
    const bool frenzyTrigger = (chosen == BlockDiceFace::PUSHED ||
                                chosen == BlockDiceFace::DEFENDER_STUMBLES);
    if (!frenzySecondBlock && frenzyTrigger && att.hasSkill(SkillName::Frenzy) &&
        canAct(att.state) && canAct(def.state) &&
        att.position.distanceTo(def.position) == 1) {
        // CRP: during a Blitz the second block costs movement too — with
        // no movement and no GFI left it is not thrown. Gated here (not
        // only via the charge in the recursive call) so the first block's
        // outcome is preserved unchanged when the second is impossible.
        if (params.isBlitz) {
            // Take Root, l. 8577-8578: zakorenený nesmi GFI.
            int gfiFloor = att.rooted ? 0 : (att.hasSkill(SkillName::Sprint) ? -3 : -2);
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
