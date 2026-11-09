#include "bb/coach_choice.h"
#include "bb/pass_handler.h"
#include "bb/ball_handler.h"
#include "bb/helpers.h"
#include <algorithm>
#include <cmath>

namespace bb {

namespace {

// Squared distance from a square centre to the segment thrower->target,
// scaled by 4 so it stays in integers (half-square precision is enough).
static int distSqToSegment4(Position p, Position a, Position b) {
    int apx = 2 * (p.x - a.x), apy = 2 * (p.y - a.y);
    int abx = 2 * (b.x - a.x), aby = 2 * (b.y - a.y);
    int denom = abx * abx + aby * aby;
    if (denom == 0) return apx * apx + apy * apy;
    int t = apx * abx + apy * aby;          // projection numerator
    if (t <= 0) return apx * apx + apy * apy;
    if (t >= denom) {
        int bpx = apx - abx, bpy = apy - aby;
        return bpx * bpx + bpy * bpy;
    }
    // Perpendicular component, kept exact: |ap|^2 - t^2/denom, times denom.
    long long num = static_cast<long long>(apx * apx + apy * apy) * denom
                    - static_cast<long long>(t) * t;
    return static_cast<int>(num / denom);
}

// Check for interception. The RANGE RULER HAS WIDTH (rules parity,
// 2026-08-10): eligibility is "have the plastic Range Ruler pass over at
// least part of the square the intercepting player is standing in", i.e. a
// CORRIDOR of about one square either side of the thrower->target line, not
// the mathematical line itself. The BB2025 wording is blunter and agrees:
// "if the Range Ruler overlaps any squares containing a Standing opposition
// player". Walking a Bresenham line demanded distance ~0 and so allowed only
// a fraction of the players the rules let try -- which quietly favoured the
// throwing side. CRP also requires the interceptor to have a tackle zone and
// to be closer to each end than the ends are to each other, and lets only
// ONE player attempt; where several qualify we take the likeliest, since a
// free choice with no downside is always used (user's rule, 2026-08-10).
int checkInterception(GameState& state, int passerId, Position target,
                      DiceRollerBase& dice, std::vector<GameEvent>* events) {
    // P264 (9.11.2026): výběr a hod rozděleny (pickInterceptor / rollInterception), ať je sdílí bomba.
    const Player& passer = state.getPlayer(passerId);
    const int interceptorId = pickInterceptor(state, passer, target);
    if (interceptorId < 0 || !rollInterception(state, passer, interceptorId, dice, events)) return -1;
    const Player& interceptor = state.getPlayer(interceptorId);
    state.ball = BallState::carried(interceptor.position, interceptor.id);
    return interceptor.id;
}

} // anonymous namespace

// Cíl hodu na zachycení (ř. 1736-1737, 1800-1802): 7 − AG + 2, Very Long Legs a Extra Arms −1, zóny (ne s Nerves of
// Steel), Disturbing Presence, liják; ořez 2..6. Jedno místo pro výběr i hod (L2: „musí být na OBOU místech“).
int interceptionTarget(const GameState& state, const Player& p) {
    int t = 7 - p.stats.agility + 2;
    if (p.hasSkill(SkillName::VeryLongLegs)) t -= 1;
    if (p.hasSkill(SkillName::ExtraArms)) t -= 1;
    if (!p.hasSkill(SkillName::NervesOfSteel)) t += countTacklezones(state, p.position, p.teamSide);
    t += countDisturbingPresence(state, p.position, p.teamSide);
    if (state.weather == Weather::POURING_RAIN) t += 1;
    return std::clamp(t, 2, 6);
}

int pickInterceptor(const GameState& state, const Player& passer, Position target) {
    TeamSide enemySide = opponent(passer.teamSide);
    const Player* interceptor = nullptr;
    int bestTarget = 99;
    int throwLen = passer.position.distanceTo(target);
    state.forEachOnPitch(enemySide, [&](const Player& p) {
        if (!canAct(p.state) || p.lostTacklezones) return;   // must have a TZ
        if (p.hasSkill(SkillName::NoHands)) return;
        if (p.position == passer.position || p.position == target) return;
        // Closer to each end than the ends are to each other (CRP).
        if (p.position.distanceTo(passer.position) >= throwLen) return;
        if (p.position.distanceTo(target) >= throwLen) return;
        // Ruler overlaps the square: centre within ~1 square of the line.
        if (distSqToSegment4(p.position, passer.position, target) > 4) return;
        const int t = interceptionTarget(state, p);
        // shoda: přednost Catch, pak nižší id (review 9.11.: výslovně, ať nerozhoduje pořadí procházení — parita s PHP)
        const bool catchBetter = interceptor && p.hasSkill(SkillName::Catch) && !interceptor->hasSkill(SkillName::Catch);
        const bool sameCatch = interceptor && p.hasSkill(SkillName::Catch) == interceptor->hasSkill(SkillName::Catch);
        if (t < bestTarget || (t == bestTarget && (catchBetter || (sameCatch && p.id < interceptor->id)))) {
            bestTarget = t;
            interceptor = &p;
        }
    });
    return interceptor ? interceptor->id : -1;
}

bool rollInterception(GameState& state, const Player& passer, int interceptorId, DiceRollerBase& dice,
                      std::vector<GameEvent>* events) {
    const Player& interceptor = state.getPlayer(interceptorId);
    const int intTarget = interceptionTarget(state, interceptor);
    // Interception attempt
    int roll = dice.rollD6();
    bool success = (roll >= intTarget);

    // Catch re-rolls a FAILED interception (rules parity, 2026-08-10).
    // CRP Catch: "allowed to re-roll the D6 if he fails a catch roll.
    // It also allows the player to re-roll the D6 if he drops a hand-off
    // or fails to make an interception." A skill re-roll is free and
    // never competes with the scarce team re-roll, so it is always taken
    // (user's rule, 2026-08-10: optional skills default ON when they
    // cost nothing).
    if (!success && interceptor.hasSkill(SkillName::Catch)) {
        emitEvent(events, {GameEvent::Type::SKILL_USED, interceptor.id, -1, {}, {},
                          static_cast<int>(SkillName::Catch), true});
        roll = dice.rollD6();
        success = (roll >= intTarget);
    }

    // SafeThrow: if intercepted, passer can force a reroll -- but Very
    // Long Legs blocks it (rules parity, 2026-08-10). CRP Very Long
    // Legs: "the Safe Throw skill may not be used to affect any
    // Interception rolls made by this player."
    // P78 (30.09.2026), port PHP faef31f3: Safe Throw je NEMODIFIKOVANY hod
    // na Obratnost HAZECE ("may make an unmodified Agility roll"), ne
    // prehoz kostky zachycujiciho proti jeho cili.
    if (success && passer.hasSkill(SkillName::SafeThrow) &&
        !interceptor.hasSkill(SkillName::VeryLongLegs)) {
        const int safeTarget = std::clamp(7 - passer.stats.agility, 2, 6);
        const int safeRoll = dice.rollD6();
        if (safeRoll != 1 && (safeRoll == 6 || safeRoll >= safeTarget)) {
            success = false;
            emitEvent(events, {GameEvent::Type::SKILL_USED, passer.id, -1, {}, {},
                              static_cast<int>(SkillName::SafeThrow), true});
        }
    }
    // Review 9.11. (L11): výsledek se zapisuje vždy, i nepovedený (dřív jen úspěch, a to u volajícího) — jako PHP
    //   hodZachyceni (událost interception).
    emitEvent(events, {GameEvent::Type::CATCH, interceptor.id, passer.id, interceptor.position, {}, intTarget, success});
    return success;
}


// BB2016 l. 735-737: "Roll for scatter THREE TIMES, ONE AFTER THE OTHER, to see
// where the ball ends up. Note that each of the Scatter rolls is made
// separately, so it is possible for the ball to end up back in the target
// square." Tri nezavisle kroky o JEDNO pole, ne jeden dlouhy skok.
//
// P67 (port PHP cd2f72fe, 29.09.2026): jakmile míč opustí hřiště, DÁL SE
// NEROZPTYLUJE a hned se vhazuje z posledního pole na hřišti (l. 866-869).
// Hozený míč došel do klidu: turnover není jen tehdy, když ho drží hráč týmu, který házel
// (ř. 881-889).
static ActionResult thrownBallTurnover(const GameState& state, TeamSide throwingSide) {
    const bool ours = state.ball.isHeld && state.ball.carrierId > 0 &&
                      state.getPlayer(state.ball.carrierId).teamSide == throwingSide;
    return ours ? ActionResult::ok() : ActionResult::turnovr();
}

// Vrací true, když míč vyletěl a byl vhozen; jinak je `land` místo dopadu.
static bool scatterOrThrowIn(GameState& state, Position target, DiceRollerBase& dice,
                             std::vector<GameEvent>* events, Position& land) {
    land = target;
    for (int i = 0; i < 3; i++) {
        const Position prev = land;
        Position step = scatterDirection(dice.rollD8());
        land = {static_cast<int8_t>(land.x + step.x),
                static_cast<int8_t>(land.y + step.y)};
        if (!land.isOnPitch()) {
            resolveThrowIn(state, prev, land, dice, events);
            return true;
        }
    }
    return false;
}

ActionResult resolvePass(GameState& state, int passerId, Position target,
                         DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& passer = state.getPlayer(passerId);

    // ZMĚNĚNO 6.11.2026 (P254 + review) — legalita hodu PŘED puštěním míče (dřív míč puštěn, zachycení, pak fail).
    // ZMĚNĚNO 7.11.2026 (P255/P257) — jedno místo `passTargetLegal` (helpers.cpp); Hail Mary jen MIMO pravítko
    // (dřív u každé přihrávky hráče s HMP, i rychlé — PHP to má jen mimo dosah). Ř. 8164-8171, CRP vánice.
    const PassLegality legality = passTargetLegal(state, passer, target);
    if (!legality.legal) return ActionResult::fail();
    const bool isHailMary = legality.hailMary;
    PassRange range = legality.band;

    // Mark pass used
    state.getTeamState(passer.teamSide).passUsedThisTurn = true;
    passer.hasActed = true;

    // Ball must be held by passer
    if (!state.ball.isHeld || state.ball.carrierId != passerId) {
        return ActionResult::turnovr();
    }

    // Release the ball from passer
    state.ball = BallState::onGround(passer.position);

    if (isHailMary) {
        // Hail Mary: no interception, D6: 1=fumble, 2+=inaccurate
        int hmpRoll = dice.rollD6();
        // P201 (9): roll = cílové číslo, die1 = kostka (jako ostatní hody, P145)
        emitEvent(events, {GameEvent::Type::PASS, passerId, -1, passer.position, target,
                          2, hmpRoll >= 2, hmpRoll});

        if (hmpRoll == 1) {
            // Fumble: ball bounces from thrower
            resolveBounce(state, passer.position, dice, 0, events);
            return ActionResult::turnovr();
        }

        // Inaccurate: 3 single scatters from target (l. 735-737)
        Position landPos;
        if (scatterOrThrowIn(state, target, dice, events, landPos)) {
            // OPRAVENO 08.10.2026 (P89) — tady byl turnover VŽDY. Pravidla ř. 881-889: turnover
            // nastává, až když míč „finally comes to rest“, a nenastane, když ho po vhození
            // chytí hráč týmu na tahu.
            return thrownBallTurnover(state, passer.teamSide);
        }

        resolveLanding(state, landPos, dice, events);
        return thrownBallTurnover(state, passer.teamSide);
    }

    // Normal pass

    // Check interception
    int interceptorId = checkInterception(state, passerId, target, dice, events);
    if (interceptorId >= 0) {
        return ActionResult::turnovr();
    }

    // P256 (7.11.2026): modifikátory hodu (Strong Arm, Accurate, zóny/NoS, DP, Very Sunny) na jednom místě —
    // helpers.cpp throwRollMods; dřív tu byly rozepsané (a v bombě a TTM jinak). F7 (24.08.): modifikátory jdou
    // do HODU, ne jen do cíle, protože fumble je „1 or less before or after modification“ (ř. 1742-1744).
    const int passMods = throwRollMods(state, passer, range);
    int passTarget = std::clamp(7 - passer.stats.agility - passMods, 2, 6);

    // BB2016 l. 1742-1744 (F7): "if the D6 roll for a pass is 1 OR LESS BEFORE
    // OR AFTER MODIFICATION, then the thrower has fumbled". Do 24.08.2026 se
    // fumblovalo jen na PRIROZENOU 1, takze jsme fumblovali MIN, nez mame:
    // hazec se dvema tackle zonami fumbluje uz na 3, u nas to byla jen neprecna
    // prihravka.
    auto isFumble = [&](int r) { return isThrowFumble(r, passMods); };

    int roll = dice.rollD6();

    // P201 (9), 14.10.2026: roll = cílové číslo, die1 = kostka (jako ostatní hody, P145). Dřív
    // nesl `roll` kostku a výpis partie ji tiskl jako „potřeba N+“.
    emitEvent(events, {GameEvent::Type::PASS, passerId, -1, passer.position, target,
                      passTarget, roll >= passTarget && !isFumble(roll), roll});

    // P78 (PHP e81bb57d): Safe Throw -- fumble jinak nez prirozenou 1 hazec
    // "manages to keep hold of the ball ... and the team does not suffer a
    // turnover". Takovy fumble nehrozi turnoverem, takze se za nej neplati
    // prehozem (fumble zachraneny Safe Throw se neprehazuje).
    auto safeThrowKeeps = [&](int r) {
        return passer.hasSkill(SkillName::SafeThrow) && r != 1 && isFumble(r);
    };
    // P260 (úklid 7.11.): od P247 se přehazuje fumble i nepřesná stejně ⇒ jedna podmínka. Fumble, který Safe Throw
    // podrží (review P78), se nepřehazuje — nový hod by mohl být skutečný fumble.
    const bool needsReroll = !(roll >= passTarget && !isFumble(roll)) && !safeThrowKeeps(roll);

    if (needsReroll) {
        // F6, l. 8336-8337: "A player with the Pass skill is allowed to re-roll
        // the D6 if he throws AN INACCURATE PASS OR FUMBLES." Do 24.08. se
        // rerollovalo jen na fumble, tj. na polovinu pripadu, ktere pravidlo
        // jmenuje.
        // ⚠️ TA1 (l. 925-927): jeden hod = nejvýš jeden přehoz — Pass, jinak kouč | Pro | týmový (dřív se stupně
        // řetězily). Žebříček je společný s bombou (rerollThrow, helpers.cpp). P130/P247: kouč se ptá u fumble
        // I u nepřesné.
        rerollThrow(state, passer, dice, CoachQuestion::PASS_REROLL, events, roll, passTarget, passMods,
                    /*teamAllowedForEngine=*/true);
    }

    if (safeThrowKeeps(roll)) {
        // P78: mic zustava hazeci, prihravka se neuskutecni, turnover neni.
        // (Mic uz byl "uvolnen" na zacatku resolvePass -- vraci se do ruky.)
        state.ball = BallState::carried(passer.position, passerId);
        emitEvent(events, {GameEvent::Type::SKILL_USED, passerId, -1, {}, {},
                          static_cast<int>(SkillName::SafeThrow), true});
        return ActionResult::ok();
    }

    if (isFumble(roll)) {
        // Fumble: ball bounces from passer, turnover (l. 1744-1746)
        resolveBounce(state, passer.position, dice, 0, events);
        return ActionResult::turnovr();
    }

    const bool accurate = (roll >= passTarget);
    Position landPos = target;
    // F8, l. 735-737: neprecna prihravka = tri samostatne rozptyly po JEDNOM poli. Do 24.08.2026
    // se sem recyklovala VYKOPOVA sablona (jeden smer D8 x vzdalenost D6).
    // OPRAVENO 08.10.2026 (P89) — po vhozeni byl turnover VŽDY. Pravidla ř. 881-889: turnover
    // nastává, až když míč „finally comes to rest“, a nenastane, když ho chytí hráč týmu na tahu.
    if (!accurate && scatterOrThrowIn(state, target, dice, events, landPos)) {
        return thrownBallTurnover(state, passer.teamSide);
    }
    // P223: dopad (chytání, Diving Catch, odraz) na jednom místě — `resolveLanding`.
    resolveLanding(state, landPos, dice, events, accurate);
    return thrownBallTurnover(state, passer.teamSide);
}

ActionResult resolveHandOff(GameState& state, int giverId, int receiverId,
                            DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& giver = state.getPlayer(giverId);
    Player& receiver = state.getPlayer(receiverId);

    // Mark the HAND-OFF allowance used, not the pass one (P4/P26, 2026-08-17).
    // CRP gives the hand-off its own once-per-turn declaration.
    state.getTeamState(giver.teamSide).handOffUsedThisTurn = true;
    giver.hasActed = true;

    // Must be adjacent
    if (giver.position.distanceTo(receiver.position) != 1) {
        return ActionResult::fail();
    }

    // Ball must be held by giver
    if (!state.ball.isHeld || state.ball.carrierId != giverId) {
        return ActionResult::turnovr();
    }

    // A hand-off left no trace of its own in the log: this function calls
    // resolveCatch and what came out was a bare CATCH, which reads exactly like
    // catching a bounce or a kick-off. So the corpus could not answer "did we
    // hand off at all" -- the check written for the pricing fix counted a
    // HAND_OFF string the engine never emitted and duly reported zero across
    // 3000 games. Declare the action here; the CATCH that follows carries the
    // roll, the same shape as PASS.
    emitEvent(events, {GameEvent::Type::HAND_OFF, giverId, receiverId,
                       giver.position, receiver.position, 0, true});

    // Transfer ball to ground at receiver position
    state.ball = BallState::onGround(receiver.position);

    // Catch with +1 modifier
    if (resolveCatch(state, receiverId, dice, 1, events)) {
        return ActionResult::ok();
    }

    // Failed catch — ball bounces from receiver's position
    if (!state.ball.isHeld) {
        resolveBounce(state, receiver.position, dice, 0, events);
    }

    // If own team caught on bounce, not a turnover
    if (state.ball.isHeld && state.getPlayer(state.ball.carrierId).teamSide == giver.teamSide) {
        return ActionResult::ok();
    }

    return ActionResult::turnovr();
}

} // namespace bb
