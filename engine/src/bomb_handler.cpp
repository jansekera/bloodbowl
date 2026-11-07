#include "bb/bomb_handler.h"
#include "bb/helpers.h"
#include "bb/injury.h"
#include "bb/ball_handler.h"
#include "bb/coach_choice.h"
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
    // OPRAVENO 7.11.2026 (P256, review dávky) — bomba je hod míčem (ř. 7952-7958: „using the rules for throwing
    // the ball (including weather effects …)“, „all skills that may be used when a ball is thrown“). Dřív tu chyběl
    // Accurate a Strong Arm (PHP Accurate bralo) a ve vánici šla i dlouhá. Teď legalita i modifikátory z téhož
    // místa jako přihrávka (helpers.cpp passTargetLegal, throwRollMods). ⏳ Hail Mary u bomby (ř. 7953) zatím ne
    // — mimo pravítko se bomba odmítne jako dřív (P263).
    const PassLegality legality = passTargetLegal(state, thrower, target);
    if (!legality.legal || legality.hailMary) return ActionResult::fail();
    const int bombMods = throwRollMods(state, thrower, legality.band);
    const int passTarget = std::clamp(7 - thrower.stats.agility - bombMods, 2, 6);
    auto isFumble = [&](int r) { return isThrowFumble(r, bombMods); };

    int roll = dice.rollD6();
    // P262 (review dávky 7.11.2026): roll = cílové číslo, die1 = kostka (konvence P201/P145 jako u přihrávky);
    // dřív nesl `roll` kostku a výpis partie ji tiskl jako „potřeba N+“.
    emitEvent(events, {GameEvent::Type::PASS, throwerId, -1, thrower.position, target,
                      passTarget, roll >= passTarget && !isFumble(roll), roll});

    // P251 (6.11.2026, uzivatel: „bombu jen na fumble“) -- driv se fumble bomby neprehazoval vubec.
    // R. 7957-7958: "All skills that may be used when a ball is thrown may be used when a bomb is
    // thrown also" => Pass (zdarma), Pro, tymovy prehoz (zebricek spolecny s prihravkou,
    // rerollThrowByEngine). NEPRESNA bomba se neprehazuje (volba uzivatele). Bomber s Lonerem
    // tymovy prehoz na bombu VUBEC nepouzije, ani na fumble (uzivatel 6.11.: „jestli ma bomber
    // loner - tak se prikloň k neprehozeni ani fumble“); Pass a Pro (zdarma) mu zustavaji.
    // ZMĚNĚNO 7.11.2026 (P251b, uživatel: „přehoz bomby se má člověka ptát“) — dřív se živý kouč na bombu
    // neptal a fumble nechal stát (kromě Pass). Teď jako u přihrávky: Pass zdarma bez otázky, jinak se kouče
    // ptá CoachQuestion::BOMB_REROLL (nechat / tým / Pro); Loner omezení výše je volba AI, člověk volí sám.
    // Parita s PHP: BombThrowHandler.php (webová hra se na přehoz přihrávky ani bomby neptá).
    if (isFumble(roll)) {
        if (!coachDecides(thrower.teamSide) || thrower.hasSkill(SkillName::Pass)) {
            rerollThrowByEngine(state, thrower, dice, events, roll, passTarget, bombMods,
                                /*teamAllowed=*/!thrower.hasSkill(SkillName::Loner));
        } else {
            const int original = roll;
            bool viaPro = false;
            if (coachAskReroll(state, thrower, dice, CoachQuestion::BOMB_REROLL, -1, roll, passTarget,
                               /*teamAllowed=*/true, events, viaPro)) {
                roll = dice.rollD6();
                const bool ok = roll >= passTarget && !isFumble(roll);
                if (viaPro) {
                    emitEvent(events, {GameEvent::Type::SKILL_USED, throwerId, -1, {}, {},
                                      static_cast<int>(SkillName::Pro), ok, roll});
                } else {
                    emitEvent(events, {GameEvent::Type::REROLL, throwerId, -1, {}, {}, 0, ok, original, roll});
                }
            }
        }
    }

    // Determine explosion position
    Position explosionPos = target;

    if (isFumble(roll)) {
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

            // P73: omraceny zustava omraceny ("treated as Knocked Down even
            // if already Prone or Stunned" = hod na brneni, ne otoceni).
            if (victim->state != PlayerState::STUNNED) victim->setState(PlayerState::PRONE);
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
