#include "bb/bomb_handler.h"
#include "bb/helpers.h"
#include "bb/injury.h"
#include "bb/ball_handler.h"
#include <algorithm>

namespace bb {

ActionResult resolveBombThrow(GameState& state, int throwerId, Position target,
                              DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& thrower = state.getPlayer(throwerId);

    // A bomb does NOT consume the team's Pass Action (rules parity,
    // 2026-08-10). CRP Bombardier: "A coach may choose to have a Bombardier
    // (...) throw a bomb instead of taking any other Action with the player.
    // This does not use the team's Pass Action for the turn." We used to
    // spend it, which silently cost the team its throw for the turn.
    thrower.hasActed = true;

    // 1. Accuracy roll (same as pass)
    // Bombs use the ball's throwing rules, so the same ruler grid
    // (rules parity, 2026-08-10).
    PassRange range;
    if (!passRangeFromOffset(target.x - thrower.position.x,
                             target.y - thrower.position.y, range)) {
        return ActionResult::fail();
    }

    int passTarget = 7 - thrower.stats.agility;
    passTarget -= passModifier(range);

    if (!thrower.hasSkill(SkillName::NervesOfSteel)) {
        passTarget += countTacklezones(state, thrower.position, thrower.teamSide);
    }

    passTarget += countDisturbingPresence(state, thrower.position, thrower.teamSide);

    // Bomba se hazi "using the rules for throwing the ball (INCLUDING WEATHER
    // EFFECTS)" (l. 7952-7954), a u mice plati od 10.08.: postih na HOD dava
    // JEN Very Sunny. Pouring Rain ma -1 na chytani/intercept/sber, ne na hod,
    // a Blizzard omezuje DOSAH, ne hod. Tady se poresne dan vsechny tri.
    if (state.weather == Weather::VERY_SUNNY) {
        passTarget += 1;
    }

    passTarget = std::clamp(passTarget, 2, 6);

    int roll = dice.rollD6();
    emitEvent(events, {GameEvent::Type::PASS, throwerId, -1, thrower.position, target,
                      roll, roll >= passTarget && roll != 1});

    // Determine explosion position
    Position explosionPos = target;

    if (roll == 1) {
        // TA4, l. 7967-7968: "If the bomb is FUMBLED it explodes IN THE BOMB
        // THROWER'S SQUARE." Puvodne se rozptylovala D8 od hazece.
        explosionPos = thrower.position;
    } else if (roll < passTarget) {
        // Inaccurate: 3x scatter from target
        for (int i = 0; i < 3; i++) {
            int d8 = dice.rollD8();
            Position scatter = scatterDirection(d8);
            explosionPos = {static_cast<int8_t>(explosionPos.x + scatter.x),
                            static_cast<int8_t>(explosionPos.y + scatter.y)};
        }
    }

    // Off-pitch: fizzle, no effect
    if (!explosionPos.isOnPitch()) {
        return ActionResult::ok(); // Never turnover
    }

    // 2. TA4, l. 7969-7974: "When the bomb finally does explode ANY PLAYER IN
    // THE SAME SQUARE IS KNOCKED DOWN, and players in ADJACENT squares are
    // Knocked Down ON A ROLL OF 4+. Players can be hit by a bomb and treated as
    // Knocked Down EVEN IF THEY ARE ALREADY PRONE OR STUNNED."
    // Tri vady naraz: hazec mel vymyslenou imunitu (pravidlo zadnou nema),
    // sousedni pole se srazela automaticky misto na 4+, a lezici a omraceni se
    // preskakovali uplne.
    bool activeKnockedDown = false;

    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            int px = explosionPos.x + dx;
            int py = explosionPos.y + dy;
            if (px < 0 || px > 25 || py < 0 || py > 14) continue;

            Position checkPos{static_cast<int8_t>(px), static_cast<int8_t>(py)};
            Player* victim = state.getPlayerAtPosition(checkPos);
            if (!victim) continue;
            if (!isOnPitch(victim->state)) continue;

            const bool sameSquare = (dx == 0 && dy == 0);
            if (!sameSquare && dice.rollD6() < 4) continue;   // sousedi na 4+

            victim->state = PlayerState::PRONE;
            emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, victim->id, throwerId,
                              victim->position, {}, 0, false});
            if (victim->teamSide == thrower.teamSide) activeKnockedDown = true;
            InjuryContext ctx;
            resolveArmourAndInjury(state, victim->id, dice, ctx, events);
            handleBallOnPlayerDown(state, victim->id, dice, events);
        }
    }

    // l. 7956-7958: "Fumbles or any bomb explosions that lead to A PLAYER ON THE
    // ACTIVE TEAM BEING KNOCKED OVER ARE TURNOVERS." Test se puvodne jmenoval
    // `NeverTurnover` a certifikoval presny opak. Fumble je pokryty sam od sebe:
    // vybuchne v poli hazece, ktery je tim srazen.
    return activeKnockedDown ? ActionResult::turnovr() : ActionResult::ok();
}

} // namespace bb
