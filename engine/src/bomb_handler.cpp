#include "bb/bomb_handler.h"
#include "bb/helpers.h"
#include "bb/injury.h"
#include "bb/ball_handler.h"
#include "bb/coach_choice.h"
#include "bb/pass_handler.h"
#include <algorithm>

namespace bb {

namespace {

// P264: řetěz chycení / zachycení je v praxi krátký (každý pokus může selhat); strop jen pojistka proti smyčce.
constexpr int kMaxBombHops = 8;

// Výbuch (ř. 7968-7974): cílové pole srazí vždy, sousední na 4+, i ležící a omráčené. Turnover, když padl hráč
// týmu na tahu (ř. 7956-7958).
ActionResult explodeBomb(GameState& state, Position at, int throwerId, DiceRollerBase& dice,
                         std::vector<GameEvent>* events) {
    if (!at.isOnPitch()) return ActionResult::ok();   // do davu: bez účinku (ř. 7967-7968)
    bool activeKnockedDown = false;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            const Position checkPos{static_cast<int8_t>(at.x + dx), static_cast<int8_t>(at.y + dy)};
            if (!checkPos.isOnPitch()) continue;
            Player* victim = state.getPlayerAtPosition(checkPos);
            if (!victim) continue;
            if (!isOnPitch(victim->state)) continue;
            const bool sameSquare = (dx == 0 && dy == 0);
            if (!sameSquare && dice.rollD6() < 4) continue;   // sousedi na 4+
            // P73: omraceny zustava omraceny ("treated as Knocked Down even if already Prone or Stunned").
            if (victim->state != PlayerState::STUNNED) victim->setState(PlayerState::PRONE);
            emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, victim->id, throwerId,
                              victim->position, {}, 0, false});
            // OPRAVENO 9.11.2026 (P264) — dřív „tým házeče“; od P264 může bombu hodit i soupeř (chycená bomba),
            //   a turnover dělá jen sražený hráč TÝMU NA TAHU (ř. 7956-7958).
            if (victim->teamSide == state.activeTeam) activeKnockedDown = true;
            InjuryContext ctx;
            resolveArmourAndInjury(state, victim->id, dice, ctx, events);
            handleBallOnPlayerDown(state, victim->id, dice, events);
        }
    }
    return activeKnockedDown ? ActionResult::turnovr() : ActionResult::ok();
}

ActionResult throwBombFrom(GameState& state, int throwerId, Position target, DiceRollerBase& dice,
                           std::vector<GameEvent>* events, int hop);

// Chycenou / zachycenou bombu hráč hned hodí dál (ř. 7961-7963, mimořádná akce mimo pořadí). Cíl volí engine za oba
// kouče (chooseBombTarget, mustThrow). Výbuch v ruce jen jako pojistka: po kMaxBombHops chyceních za sebou (pravidla
// strop nemají; v praxi nenastane) nebo když hráč nemá žádné legální pole (nemožné na hřišti 26×15).
ActionResult rethrowBomb(GameState& state, int holderId, DiceRollerBase& dice, std::vector<GameEvent>* events,
                         int hop) {
    const Player& holder = state.getPlayer(holderId);
    const Position t = hop > kMaxBombHops ? Position{-1, -1} : chooseBombTarget(state, holder, true).pos;
    if (!t.isOnPitch())
        return explodeBomb(state, holder.position, holderId, dice, events);
    return throwBombFrom(state, holderId, t, dice, events, hop);
}

ActionResult throwBombFrom(GameState& state, int throwerId, Position target, DiceRollerBase& dice,
                           std::vector<GameEvent>* events, int hop) {
    Player& thrower = state.getPlayer(throwerId);
    // 1. Accuracy roll (same as pass)
    // OPRAVENO 7.11.2026 (P256, review dávky) — bomba je hod míčem (ř. 7952-7958: „using the rules for throwing
    // the ball (including weather effects …)“, „all skills that may be used when a ball is thrown“). Dřív tu chyběl
    // Accurate a Strong Arm (PHP Accurate bralo) a ve vánici šla i dlouhá. Teď legalita i modifikátory z téhož
    // místa jako přihrávka (helpers.cpp passTargetLegal, throwRollMods).
    // P263 (7.11.2026, uživatel „OK“): Hail Mary u bomby — ř. 7952-7954 „including … use of Hail Mary Pass“, ř. 8164-8169:
    // mimo pravítko (jen s HMP, ne ve vánici) se hází D6: 1 = fumble, 2-6 = hod projde, ale „never accurate“ ⇒ vždy
    // 3 rozptyly od cíle; „may not be intercepted“. Žádné modifikátory ani AG.
    const PassLegality legality = passTargetLegal(state, thrower, target);
    if (!legality.legal) return ActionResult::fail();
    const bool hailMary = legality.hailMary;

    // P264 (uživatel 9.11.2026: „přidej možnost intercept“) — ř. 7959-7963: „A bomb may be intercepted or caught
    //   using the same rules for catching the ball, in which case the player catching it must throw it again
    //   immediately.“ Zachycení se hlásí PŘED hodem na přesnost (ř. 1773-1774); Hail Mary zachytit nejde
    //   (ř. 8167). Zkusí ho jen hráč, jehož týmu výbuch v cíli hrozí (jinak by jen přebíral riziko). Nepovedené
    //   zachycení ⇒ bomba letí dál (uživatel: jako u míče). Zachycená bomba není turnover (ř. 7955).
    if (!hailMary) {
        const int interceptorId = pickInterceptor(state, thrower, target);
        if (interceptorId >= 0) {
            if (shouldInterceptBomb(state, state.getPlayer(interceptorId).teamSide, target) &&
                rollInterception(state, thrower, interceptorId, dice, events))
                return rethrowBomb(state, interceptorId, dice, events, hop + 1);
        }
    }

    const int bombMods = hailMary ? 0 : throwRollMods(state, thrower, legality.band);
    const int passTarget = hailMary ? 2 : std::clamp(7 - thrower.stats.agility - bombMods, 2, 6);
    auto isFumble = [&](int r) { return isThrowFumble(r, bombMods); };

    int roll = dice.rollD6();
    // P262 (review dávky 7.11.2026): roll = cílové číslo, die1 = kostka (konvence P201/P145 jako u přihrávky);
    // dřív nesl `roll` kostku a výpis partie ji tiskl jako „potřeba N+“.
    emitEvent(events, {GameEvent::Type::PASS, throwerId, -1, thrower.position, target,
                      passTarget, roll >= passTarget && !isFumble(roll), roll});

    // P251 (6.11.2026, uzivatel: „bombu jen na fumble“) -- driv se fumble bomby neprehazoval vubec.
    // R. 7957-7958: "All skills that may be used when a ball is thrown may be used when a bomb is
    // thrown also" => Pass (zdarma), Pro, tymovy prehoz (zebricek spolecny s prihravkou,
    // rerollThrow). NEPRESNA bomba se neprehazuje (volba uzivatele). Bomber s Lonerem
    // tymovy prehoz na bombu VUBEC nepouzije, ani na fumble (uzivatel 6.11.: „jestli ma bomber
    // loner - tak se prikloň k neprehozeni ani fumble“); Pass a Pro (zdarma) mu zustavaji.
    // ZMĚNĚNO 7.11.2026 (P251b, uživatel: „přehoz bomby se má člověka ptát“) — dřív se živý kouč na bombu
    // neptal a fumble nechal stát (kromě Pass). Teď jako u přihrávky: Pass zdarma bez otázky, jinak se kouče
    // ptá CoachQuestion::BOMB_REROLL (nechat / tým / Pro); Loner omezení výše je volba AI, člověk volí sám.
    // P264: hod chycené bomby týmem, který není na tahu, týmový přehoz nesmí — hlídá rerollThrow (teamRerollAllowed).
    if (isFumble(roll)) {
        rerollThrow(state, thrower, dice, CoachQuestion::BOMB_REROLL, events, roll, passTarget, bombMods,
                    /*teamAllowedForEngine=*/!thrower.hasSkill(SkillName::Loner));
    }

    // TA4, l. 7967-7968: "If the bomb is FUMBLED it explodes IN THE BOMB THROWER'S SQUARE."
    if (isFumble(roll))
        return explodeBomb(state, thrower.position, throwerId, dice, events);

    Position landPos = target;
    const bool accurate = !hailMary && roll >= passTarget;
    if (!accurate) {
        // Inaccurate (a Hail Mary vždy, P263): 3x scatter from target
        for (int i = 0; i < 3; i++) {
            Position scatter = scatterDirection(dice.rollD8());
            landPos = {static_cast<int8_t>(landPos.x + scatter.x), static_cast<int8_t>(landPos.y + scatter.y)};
        }
    }
    if (!landPos.isOnPitch()) return ActionResult::ok();   // Never turnover

    // P264: hráč v poli dopadu bombu chytá jako míč (catchRoll: přesná +1, Diving Catch jen přesná, Catch, Pro,
    //   týmový přehoz jen tým na tahu). Pokus je vždy aspoň tak dobrý jako odmítnutí: nepovedené chycení i odmítnutí
    //   = výbuch v tom poli (ř. 7964-7966), povedené ⇒ hod dál. Kouč-člověk se zatím neptá (rozhoduje engine, P272).
    if (Player* catcher = state.getPlayerAtPosition(landPos); catcher && canCatchBomb(*catcher)) {
        if (catchRoll(state, catcher->id, dice, accurate ? 1 : 0, events, accurate))
            return rethrowBomb(state, catcher->id, dice, events, hop + 1);
    }
    return explodeBomb(state, landPos, throwerId, dice, events);
}

}  // namespace

ActionResult resolveBombThrow(GameState& state, int throwerId, Position target,
                              DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& thrower = state.getPlayer(throwerId);
    // Nelegální cíl akci nespotřebuje (jako TTM, P268).
    if (!passTargetLegal(state, thrower, target).legal) return ActionResult::fail();
    // A bomb does NOT consume the team's Pass Action (rules parity,
    // 2026-08-10). CRP Bombardier: "A coach may choose to have a Bombardier
    // (...) throw a bomb instead of taking any other Action with the player.
    // This does not use the team's Pass Action for the turn." We used to
    // spend it, which silently cost the team its throw for the turn.
    thrower.hasActed = true;
    return throwBombFrom(state, throwerId, target, dice, events, 0);
}

} // namespace bb
