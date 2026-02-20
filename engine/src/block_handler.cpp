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

    Position pushSquares[3];
    int pushCount = getPushbackSquares(attacker.position, defender.position, pushSquares);

    if (pushCount == 0) {
        // Off pitch — crowd surf
        emitEvent(events, {GameEvent::Type::PUSH, defender.id, -1,
                          defender.position, {-1, -1}, 0, true});
        pushDest = {-1, -1};
        return true;
    }

    // SideStep: defender picks best square (furthest from attacker)
    // Grab: attacker picks worst square for defender
    // Default: pick first empty, then first available
    int chosenIdx = 0;

    if (defender.hasSkill(SkillName::SideStep) &&
        !(attacker.hasSkill(SkillName::Grab) && !isBlitz)) {
        // Defender picks: choose square furthest from attacker
        int bestDist = -1;
        for (int i = 0; i < pushCount; i++) {
            int dist = pushSquares[i].distanceTo(attacker.position);
            if (dist > bestDist) { bestDist = dist; chosenIdx = i; }
        }
    } else if (attacker.hasSkill(SkillName::Grab) && !isBlitz) {
        // Grab only works on non-blitz blocks
        // Attacker picks: choose square closest to sideline (worst for defender)
        int bestWide = -1;
        for (int i = 0; i < pushCount; i++) {
            // Prefer edges
            int edgeDist = std::min({
                (int)pushSquares[i].x,
                Position::PITCH_WIDTH - 1 - pushSquares[i].x,
                (int)pushSquares[i].y,
                Position::PITCH_HEIGHT - 1 - pushSquares[i].y
            });
            int score = 100 - edgeDist; // closer to edge = higher score
            if (score > bestWide) { bestWide = score; chosenIdx = i; }
        }
    } else {
        // Default: prefer empty square, then center push direction
        for (int i = 0; i < pushCount; i++) {
            if (!state.getPlayerAtPosition(pushSquares[i])) {
                chosenIdx = i;
                break;
            }
        }
    }

    pushDest = pushSquares[chosenIdx];

    // Check if destination is occupied → chain push
    Player* occupant = state.getPlayerAtPosition(pushDest);
    if (occupant) {
        // Chain push: push occupant in the same direction
        Position chainPushDest;
        Position chainSquares[3];
        int chainCount = getPushbackSquares(defender.position, occupant->position, chainSquares);

        if (chainCount == 0) {
            // Chain push off pitch
            Position occupantOldPos = occupant->position;
            emitEvent(events, {GameEvent::Type::PUSH, occupant->id, -1,
                              occupant->position, {-1, -1}, 0, true});
            handleBallOnPlayerDown(state, occupant->id, dice, events);
            resolveCrowdSurf(state, occupant->id, dice, events);
        } else {
            // Find empty chain destination
            int chainIdx = 0;
            for (int i = 0; i < chainCount; i++) {
                if (!state.getPlayerAtPosition(chainSquares[i])) {
                    chainIdx = i;
                    break;
                }
            }
            Position chainDest = chainSquares[chainIdx];

            emitEvent(events, {GameEvent::Type::PUSH, occupant->id, -1,
                              occupant->position, chainDest, 0, true});

            // Move chain-pushed player
            if (state.ball.isHeld && state.ball.carrierId == occupant->id) {
                state.ball.position = chainDest;
            }
            occupant->position = chainDest;
        }
    }

    // Check if push goes off pitch
    if (!pushDest.isOnPitch()) {
        return true; // crowd surf
    }

    emitEvent(events, {GameEvent::Type::PUSH, defender.id, -1,
                      defender.position, pushDest, 0, true});

    // Move defender
    Position defOldPos = defender.position;
    defender.position = pushDest;
    if (state.ball.isHeld && state.ball.carrierId == defender.id) {
        state.ball.position = pushDest;
    }

    return false;
}

ActionResult resolveBlock(GameState& state, const BlockParams& params,
                          DiceRollerBase& dice, std::vector<GameEvent>* events,
                          bool frenzySecondBlock, bool noFollowUp) {
    Player& att = state.getPlayer(params.attackerId);
    Player& def = state.getPlayer(params.targetId);
    Position attOldPos = att.position;
    Position defOldPos = def.position;

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

    int attAssists = countAssists(state, def.position, att.teamSide, att.id, def.id, def.id);
    int defAssists = countAssists(state, att.position, def.teamSide, att.id, def.id, att.id);

    int effAttST = attST + attAssists;
    int effDefST = defST + defAssists;

    // Dauntless
    if (att.hasSkill(SkillName::Dauntless) && effDefST > effAttST) {
        int dauntlessRoll = dice.rollD6();
        if (dauntlessRoll + att.stats.strength > def.stats.strength) {
            effAttST = effDefST; // treat as equal
            emitEvent(events, {GameEvent::Type::SKILL_USED, att.id, -1, {}, {},
                              static_cast<int>(SkillName::Dauntless), true});
        } else {
            emitEvent(events, {GameEvent::Type::SKILL_USED, att.id, -1, {}, {},
                              static_cast<int>(SkillName::Dauntless), false});
        }
    }

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
            // Check Wrestle
            bool defWrestle = def.hasSkill(SkillName::Wrestle);
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
            // Crowd surf
            handleBallOnPlayerDown(state, def.id, dice, events);
            Position lastPos = def.position;
            def.position = {-1, -1};
            resolveCrowdSurf(state, def.id, dice, events);

            // StripBall: ball drops at crowd edge (already handled by crowd surf)

            // Follow-up: attacker to old defender position
            if (!noFollowUp) {
                att.position = defOldPos;
                if (state.ball.isHeld && state.ball.carrierId == att.id) {
                    state.ball.position = att.position;
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

        // StripBall: if defender has ball and not knocked down, and attacker has StripBall
        if (!defKnockedDown && state.ball.isHeld && state.ball.carrierId == def.id &&
            att.hasSkill(SkillName::StripBall)) {
            state.ball = BallState::onGround(def.position);
            resolveBounce(state, def.position, dice, 0, events);
        }

        // Follow-up: attacker moves to old defender position
        bool fendPrevents = def.hasSkill(SkillName::Fend) && !defKnockedDown;
        if (!noFollowUp && !fendPrevents) {
            att.position = defOldPos;
            if (state.ball.isHeld && state.ball.carrierId == att.id) {
                state.ball.position = att.position;
            }
        }

        // Knockdown
        if (defKnockedDown) {
            def.state = PlayerState::PRONE;
            emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, def.id, -1,
                              def.position, {}, 0, false});

            InjuryContext defCtx;
            if (att.hasSkill(SkillName::MightyBlow)) {
                defCtx.armourModifier += 1;
                defCtx.injuryModifier += 1;
            }
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
