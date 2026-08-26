#include "bb/move_handler.h"
#include "bb/helpers.h"
#include "bb/injury.h"
#include "bb/ball_handler.h"
#include <algorithm>

namespace bb {

namespace {

// Check Tentacles: adjacent enemies with Tentacles contest the move
// Returns true if player is caught (movement ends)
bool checkTentacles(GameState& state, int playerId, Position from,
                    DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& mover = state.getPlayer(playerId);
    auto adj = from.getAdjacent();

    for (auto& pos : adj) {
        if (!pos.isOnPitch()) continue;
        const Player* opp = state.getPlayerAtPosition(pos);
        if (!opp || opp->teamSide == mover.teamSide) continue;
        if (!canAct(opp->state) || opp->lostTacklezones) continue;
        if (!opp->hasSkill(SkillName::Tentacles)) continue;

        // TA8 (24.08.2026) -- BB2016 l. 8588-8591: "The opposing player rolls
        // 2D6 ADDING THEIR OWN player's ST and SUBTRACTING the Tentacles
        // player's ST from the score. If the final result is 5 OR LESS, then
        // the moving player is held firm." Do 24.08. se tu hral protihod
        // D6 vs D6, tedy uplne jine rozdeleni: symetricke kolem nuly misto
        // 2D6 s prahem 5, a s jinou citlivosti na rozdil sil.
        int roll = dice.roll2D6();
        int total = roll + mover.stats.strength - opp->stats.strength;
        bool escaped = (total >= 6);

        emitEvent(events, {GameEvent::Type::SKILL_USED, opp->id, playerId, {}, {},
                          static_cast<int>(SkillName::Tentacles), !escaped});

        if (!escaped) {
            // Caught: movement ends, player stays at from
            mover.hasMoved = true;
            return true;
        }
        // Only one Tentacles check per dodge step
        break;
    }
    return false;
}

// Check Shadowing: after successful dodge, adjacent enemy with Shadowing may follow
void checkShadowing(GameState& state, int playerId, Position from,
                    DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& mover = state.getPlayer(playerId);
    auto adj = from.getAdjacent();

    for (auto& pos : adj) {
        if (!pos.isOnPitch()) continue;
        Player* opp = state.getPlayerAtPosition(pos);
        if (!opp || opp->teamSide == mover.teamSide) continue;
        if (!canAct(opp->state) || opp->lostTacklezones) continue;
        if (!opp->hasSkill(SkillName::Shadowing)) continue;

        // TA9 (24.08.2026) -- BB2016 l. 8458-8464: "The opposing player rolls
        // 2D6 ADDING THEIR OWN player's movement allowance and SUBTRACTING the
        // Shadowing player's movement allowance. If the final result is 7 OR
        // LESS, the player with Shadowing MAY move into the square vacated."
        // Do 24.08. tu byl JEDEN D6 a znamenka OBRACENE (+ stinujici MA
        // - utikajici MA >= 6), takze rychly hráč se stinovani hur ubranil.
        int roll = dice.roll2D6();
        int total = roll + mover.stats.movement - opp->stats.movement;
        bool follows = (total <= 7);

        emitEvent(events, {GameEvent::Type::SKILL_USED, opp->id, playerId, opp->position, from,
                          static_cast<int>(SkillName::Shadowing), follows});

        if (follows) {
            // Check that vacated square is empty (should be, since we just left)
            if (!state.getPlayerAtPosition(from)) {
                opp->position = from;
            }
        }
        // Only one Shadowing attempt per dodge step
        break;
    }
}

} // anonymous namespace

ActionResult resolveMoveStep(GameState& state, int playerId, Position to,
                             DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& player = state.getPlayer(playerId);
    Position from = player.position;

    // Validate: adjacent and on-pitch
    if (from.distanceTo(to) != 1 || !to.isOnPitch()) {
        return ActionResult::fail();
    }

    // Validate: destination not occupied
    if (state.getPlayerAtPosition(to) != nullptr) {
        return ActionResult::fail();
    }

    // Check if leaving a tackle zone → dodge required
    bool needsDodge = countTacklezones(state, from, player.teamSide) > 0;

    // Check Tentacles before dodge (if leaving TZ)
    if (needsDodge) {
        if (checkTentacles(state, playerId, from, dice, events)) {
            return ActionResult::ok();  // Caught, movement ends, not turnover
        }
    }

    // Decrement movement
    player.movementRemaining--;
    player.hasMoved = true;

    // Check GFI
    bool needsGfi = false;
    if (player.movementRemaining < 0) {
        // Allow up to -2 GFI squares (or -3 with Sprint)
        // N11: zakorenený nesmi GFI (l. 8577-8578) -- tady to chybelo, takze
        // ho blitz smycka pres GFI protlacila i pres zakaz v nabidce.
        int maxGfi = player.rooted ? 0 : (player.hasSkill(SkillName::Sprint) ? -3 : -2);
        if (player.movementRemaining < maxGfi) {
            player.movementRemaining++; // undo
            return ActionResult::fail();
        }
        needsGfi = true;
    }

    // Perform dodge roll if needed
    if (needsDodge) {
        int target = calculateDodgeTarget(state, player, to, from);

        // Check if Tackle negates Dodge reroll
        // Tackle, BB2016 l. 8566-8571: "Opposing players who are standing in
        // any of this player's tackle zones are NOT ALLOWED TO USE THEIR DODGE
        // SKILL if they attempt to dodge out of any of the player's tackle
        // zones." Rusi se tim REROLL, ne modifikator -- Dodge zadny nema.
        bool tackleNegates = false;
        auto srcAdj = from.getAdjacent();
        for (auto& apos : srcAdj) {
            if (!apos.isOnPitch()) continue;
            const Player* opp = state.getPlayerAtPosition(apos);
            if (opp && opp->teamSide != player.teamSide &&
                exertsTacklezone(opp->state) && !opp->lostTacklezones &&
                opp->hasSkill(SkillName::Tackle)) {
                tackleNegates = true;
                break;
            }
        }

        bool dodgeOk = attemptRoll(state, playerId, dice, target,
                                    SkillName::Dodge, tackleNegates, true, events);

        emitEvent(events, {GameEvent::Type::DODGE, playerId, -1, from, to,
                          target, dodgeOk});

        if (!dodgeOk) {
            // Failed dodge: player falls at destination
            player.position = to;
            player.state = PlayerState::PRONE;
            player.hasActed = true;

            InjuryContext ctx;
            resolveArmourAndInjury(state, playerId, dice, ctx, events);
            handleBallOnPlayerDown(state, playerId, dice, events);

            return ActionResult::turnovr();
        }
    }

    // Perform GFI roll if needed
    if (needsGfi) {
        int gfiTarget = (state.weather == Weather::BLIZZARD) ? 3 : 2;

        bool gfiOk = attemptRoll(state, playerId, dice, gfiTarget,
                                  SkillName::SureFeet, false, true, events);

        emitEvent(events, {GameEvent::Type::GFI, playerId, -1, from, to,
                          gfiTarget, gfiOk});

        if (!gfiOk) {
            // Failed GFI: player falls at destination
            player.position = to;
            player.state = PlayerState::PRONE;
            player.hasActed = true;

            InjuryContext ctx;
            resolveArmourAndInjury(state, playerId, dice, ctx, events);
            handleBallOnPlayerDown(state, playerId, dice, events);

            return ActionResult::turnovr();
        }
    }

    // Move player
    player.position = to;

    // Update ball position if carrier
    if (state.ball.isHeld && state.ball.carrierId == playerId) {
        state.ball.position = to;
    }

    emitEvent(events, {GameEvent::Type::PLAYER_MOVE, playerId, -1, from, to, 0, true});

    // Shadowing: after successful dodge, enemy may follow
    if (needsDodge) {
        checkShadowing(state, playerId, from, dice, events);
    }

    // Pickup ball if on ground at destination
    if (!state.ball.isHeld && state.ball.position == to) {
        bool pickupOk = resolvePickup(state, playerId, dice, events);
        if (!pickupOk) {
            // Failed pickup — turnover
            player.hasActed = true;
            return ActionResult::turnovr();
        }
    }

    return ActionResult::ok();
}

ActionResult resolveLeap(GameState& state, int playerId, Position to,
                         DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& player = state.getPlayer(playerId);
    Position from = player.position;

    // Validate: within distance 2, on pitch, unoccupied
    int dist = from.distanceTo(to);
    if (dist < 1 || dist > 2 || !to.isOnPitch()) {
        return ActionResult::fail();
    }
    if (state.getPlayerAtPosition(to) != nullptr) {
        return ActionResult::fail();
    }

    // Leap costs 2 MA
    player.movementRemaining -= 2;
    player.hasMoved = true;

    // ⛔ N7 (24.08.2026): skok o dve pole muze stat DVA GFI hody, ne jeden.
    // BB2016 l. 1701: "Roll a D6 for the player after they have moved EACH
    // EXTRA SQUARE." Pri deficitu 2 se tedy hazi dvakrat. Dokud byl Leap mrtvy
    // kod, nevadilo to; dnesnim zapojenim nabidky se to PROBUDILO -- tataz
    // trida jako 21.08. "oprava pravidla umi probudit spici vadu".
    int gfiSquares = 0;
    if (player.movementRemaining < 0) {
        // N11: zakorenený nesmi GFI (l. 8577-8578) -- tady to chybelo, takze
        // ho blitz smycka pres GFI protlacila i pres zakaz v nabidce.
        int maxGfi = player.rooted ? 0 : (player.hasSkill(SkillName::Sprint) ? -3 : -2);
        if (player.movementRemaining < maxGfi) {
            player.movementRemaining += 2; // undo
            return ActionResult::fail();
        }
        gfiSquares = -player.movementRemaining;   // 1 nebo 2
    }

    // ⛔ N6 (24.08.2026): Tentacles a Shadowing platí i na SKOK, a chybely tu.
    // BB2016 l. 8586-8587: Tentacles se pouziva, kdyz souper "attempts to DODGE
    // OR LEAP out of any of his tackle zones" -- leap je jmenovan vyslovne.
    // l. 8456-8458: Shadowing plati pri opusteni TZ "FOR ANY REASON".
    // Latentni to bylo jen proto, ze Leap nemel volajiciho; dnesnim zapojenim
    // nabidky se to probudilo.
    if (checkTentacles(state, playerId, from, dice, events)) {
        player.movementRemaining += 2;   // skok se nekonal
        return ActionResult::ok();       // "held firm, his action ends" -- ne turnover
    }

    // Leap agility check: plain Agility roll, NO modifiers except Very Long
    // Legs (rules parity, 2026-08-10). CRP Leap: "make an Agility roll for
    // the player. No modifiers apply to this D6 roll unless he has Very
    // Long Legs." We used to add the destination's tackle zones, which made
    // leaping into a cage far dearer than the rules intend.
    // T5.32 (26.08.): TEDDA se skok pocita za pouzity -- po VSECH validacich
    // a po Tentacles. Drive to delal action_resolver jeste pred validaci,
    // takze neplatny pokus sebral skok za cele kolo. Tentacles ("held firm")
    // skok take nespotrebuje: hrac se o nej nedostal pokusit.
    player.leapUsedThisTurn = true;

    int target = 7 - player.stats.agility;
    if (player.hasSkill(SkillName::VeryLongLegs)) target -= 1;
    target = std::clamp(target, 2, 6);

    bool leapOk = attemptRoll(state, playerId, dice, target,
                               SkillName::SKILL_COUNT, false, true, events);

    // T5.31 (26.08.): bylo DODGE -- skok se v korpusu nedal odlisit od dodge.
    emitEvent(events, {GameEvent::Type::LEAP, playerId, -1, from, to,
                      target, leapOk});

    if (!leapOk) {
        // Failed leap: player prone at destination, armor+injury, turnover
        player.position = to;
        player.state = PlayerState::PRONE;
        player.hasActed = true;

        InjuryContext ctx;
        resolveArmourAndInjury(state, playerId, dice, ctx, events);
        handleBallOnPlayerDown(state, playerId, dice, events);

        return ActionResult::turnovr();
    }

    // GFI -- jeden hod za KAZDE pole nad ramec MA (l. 1701)
    for (int g = 0; g < gfiSquares; ++g) {
        int gfiTarget = (state.weather == Weather::BLIZZARD) ? 3 : 2;
        bool gfiOk = attemptRoll(state, playerId, dice, gfiTarget,
                                  SkillName::SureFeet, false, true, events);
        if (!gfiOk) {
            player.position = to;
            player.state = PlayerState::PRONE;
            player.hasActed = true;

            InjuryContext ctx;
            resolveArmourAndInjury(state, playerId, dice, ctx, events);
            handleBallOnPlayerDown(state, playerId, dice, events);

            return ActionResult::turnovr();
        }
    }

    // Move player
    player.position = to;
    // Shadowing, l. 8456-8458: plati pri opusteni TZ "FOR ANY REASON",
    // tedy i po skoku (N6).
    checkShadowing(state, playerId, from, dice, events);

    if (state.ball.isHeld && state.ball.carrierId == playerId) {
        state.ball.position = to;
    }

    emitEvent(events, {GameEvent::Type::PLAYER_MOVE, playerId, -1, from, to, 0, true});

    // Pickup ball if on ground at destination
    if (!state.ball.isHeld && state.ball.position == to) {
        bool pickupOk = resolvePickup(state, playerId, dice, events);
        if (!pickupOk) {
            player.hasActed = true;
            return ActionResult::turnovr();
        }
    }

    return ActionResult::ok();
}

ActionResult resolveStandUp(GameState& state, int playerId, DiceRollerBase& dice,
                            std::vector<GameEvent>* events) {
    Player& player = state.getPlayer(playerId);

    if (player.state != PlayerState::PRONE) {
        return ActionResult::fail();
    }

    if (player.hasSkill(SkillName::JumpUp)) {
        // Free stand up
        player.state = PlayerState::STANDING;
        player.hasMoved = true;   // standing up IS movement (see below)
        // ⚠️ hasMoved je tu SPRÁVNĚ, ne regrese: BB2016 l. 8198 dává volné
        // vstání jen tomu, kdo deklaroval JINOU akci než blok, a pak platí
        // l. 674 ("a player who stands up may not take a Block Action").
        // Blok z lehu je samostatná cesta s hodem AG+2 (l. 8200), kterou
        // engine neumí -- chybí NABÍDKA blokové akce ležícímu, ne tenhle
        // řádek. Před 21.08. Jump Up vstal zdarma a blokoval NELEGÁLNĚ.
        emitEvent(events, {GameEvent::Type::STAND_UP, playerId, -1,
                          player.position, player.position, 0, true});
        return ActionResult::ok();
    }

    // BB2016 (rules_bb2016.txt, l. 690-695): "The only time a player can stand
    // up is at the beginning of an Action at a cost of three squares from his
    // movement. If the player has less than three squares of movement, he must
    // roll 4+ to stand up - if he stands up successfully, he may not move
    // further squares unless he Goes For It. Failure to stand successfully for
    // any reason is not a turnover."
    if (player.movementRemaining < 3) {
        // Přes attemptRoll, ne holým rollD6 (oprava 21.08.): každý jiný hod
        // v enginu (dodge :137, GFI :161, pickup) jde přes něj a vrstvi
        // skill reroll -> Pro -> týmový reroll s Loner bránou. Holý hod
        // znamenal, že Treemanovi nešel použít týmový reroll -- tedy přesně
        // to selhání, které měl P45 odstranit.
        bool ok = attemptRoll(state, playerId, dice, /*target=*/4,
                              SkillName::SKILL_COUNT, /*negated=*/false,
                              /*canUseTeamReroll=*/true, events);
        emitEvent(events, {GameEvent::Type::STAND_UP, playerId, -1,
                          player.position, player.position, 4, ok});
        if (!ok) {
            // Action is used up, but this is explicitly NOT a turnover.
            player.hasActed = true;
            return ActionResult::fail();
        }
        player.movementRemaining = 0;   // any further step must be a GFI
        player.state = PlayerState::STANDING;
        player.hasMoved = true;
        return ActionResult::ok();
    }

    player.movementRemaining -= 3;
    player.state = PlayerState::STANDING;
    // Standing up is movement, so it closes the activation the same way a step
    // does: the actor-switch close-out in executeAction() keys off hasMoved,
    // and l. 674-676 forbids a Block Action afterwards ("you may not move when
    // you take a Block Action"). Without this a prone player stood up for 3 MA
    // and then blocked as a fresh, unacted activation -- strictly better than
    // Jump Up, which at least costs an AG roll.
    player.hasMoved = true;
    emitEvent(events, {GameEvent::Type::STAND_UP, playerId, -1,
                      player.position, player.position, 0, true});
    return ActionResult::ok();
}

} // namespace bb
