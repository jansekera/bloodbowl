#include "bb/ttm_handler.h"
#include "bb/helpers.h"
#include "bb/injury.h"
#include "bb/ball_handler.h"
#include <algorithm>

namespace bb {

ActionResult resolveThrowTeamMate(GameState& state, int throwerId, int projectileId,
                                  Position target, DiceRollerBase& dice,
                                  std::vector<GameEvent>* events) {
    Player& thrower = state.getPlayer(throwerId);
    Player& projectile = state.getPlayer(projectileId);

    // OPRAVENO 9.11.2026 (P268) — dosah se kontroloval až po označení házeče za hotového a spotřebování týmové
    //   přihrávky, takže cíl mimo dosah stál hráče akci (a nabídky po mřížce takové cíle dávaly). Ř. 8608-8609:
    //   „Long Pass or Long Bomb range passes are not possible“ ⇒ jen rychlá/krátká.
    PassRange range;
    if (!ttmTargetLegal(thrower, target, &range)) return ActionResult::fail();

    thrower.hasActed = true;
    state.getTeamState(thrower.teamSide).passUsedThisTurn = true;

    // Puvodni pozice -- fumblovany spoluhrac se vraci PRESNE sem (l. 8613-8614).
    const Position origin = projectile.position;
    const bool projectileHadBall =
        state.ball.isHeld && state.ball.carrierId == projectileId;

    // 1. Always Hungry -- BB2016 l. 7784-7795. ⛔ TA3 (24.08.2026): puvodne
    // stacila JEDNA jednicka a spoluhrac byl sezran (a jen "INJURED").
    // Pravidlo: "On a roll of 1 he attempts to eat the unfortunate team-mate!
    // ROLL THE D6 AGAIN, A SECOND 1 means that he successfully scoffs the
    // team-mate down, WHICH KILLS the team-mate without opportunity for
    // recovery. ... If the second roll is 2-6 the team-mate SQUIRMS FREE and
    // the Pass Action is automatically treated as A FUMBLED PASS."
    bool forcedFumble = false;
    if (thrower.hasSkill(SkillName::AlwaysHungry)) {
        // P251 (uživatel 6.11.2026: „always hungry ano — nepřesný hod je vždy — ten nepřehazovat“). Dřív se
        // jednička na Always Hungry v C++ nepřehazovala vůbec, PHP (ThrowTeamMateHandler) týmovým přehozem ano;
        // od 7.11. (P259) obě strany stejně: Pro, pak týmový přehoz (Loner 4+).
        // Ř. 929-931: týmový přehoz smí přehodit kterýkoli hod. Přes attemptRoll (helpers.cpp): kouč-člověk volí
        // sám, engine Pro, pak týmový přehoz (Loner 4+), a kostka jde do záznamu (review P251).
        int hungryRoll = 0;
        attemptRoll(state, throwerId, dice, /*target=*/2, SkillName::SKILL_COUNT, false, /*team=*/true, events,
                    &hungryRoll);
        if (hungryRoll == 1) {
            int second = dice.rollD6();
            emitEvent(events, {GameEvent::Type::SKILL_USED, throwerId, projectileId,
                              thrower.position, {},
                              static_cast<int>(SkillName::AlwaysHungry), second == 1});
            if (second == 1) {
                // Sezran: SMRT bez moznosti zachrany. l. 7791-7792: "If the
                // team-mate had the ball it will SCATTER ONCE from the
                // team-mate's square."
                if (projectileHadBall) {
                    state.ball = BallState::onGround(origin);
                    resolveBounce(state, origin, dice, 0, events);
                }
                projectile.setState(PlayerState::DEAD);
                projectile.position = {-1, -1};
                // P74 (30.09.2026), r. 382-384: sezrany NOSIC MICE je turnover
                // ("including being eaten"); bez mice ne. Vzor PHP ced20b99.
                return projectileHadBall ? ActionResult::turnovr() : ActionResult::ok();
            }
            // Vykroutil se => akce se automaticky bere jako FUMBLE.
            forcedFumble = true;
        }
    }

    // 2. Accuracy roll (dosah ověřen na začátku, ttmTargetLegal — P268).

    // OPRAVENO 7.11.2026 (P256, review dávky) — „worked out exactly the same as … passing a ball, except the player
    // must subtract 1“ (ř. 8605-8607): modifikátory z téhož místa jako přihrávka (helpers.cpp throwRollMods: Strong
    // Arm, Accurate, zóny/NoS, Disturbing Presence, Very Sunny). Dřív tu chyběly DP, Very Sunny a Accurate.
    // ⛔ TA3, ř. 8606-8607: −1 za hod spoluhráčem (dřív chybělo úplně).
    // P76 (30.09.2026): fumble je „1 OR LESS BEFORE OR AFTER MODIFICATION“ (ř. 1742-1744) ⇒ součet před ořezem.
    const int ttmMods = throwRollMods(state, thrower, range) - 1;
    int passTarget = 7 - thrower.stats.agility - ttmMods;

    passTarget = std::clamp(passTarget, 2, 6);

    // Kdyz se spoluhrac vykroutil (l. 7792-7794), je akce "automatically treated
    // as a fumbled pass" -- hod na presnost uz se NEDELA.
    bool fumble = forcedFumble;
    if (!forcedFumble) {
        int roll = dice.rollD6();
        fumble = isThrowFumble(roll, ttmMods);
        // P262: roll = cíl, die1 = kostka (konvence P201/P145)
        emitEvent(events, {GameEvent::Type::PASS, throwerId, projectileId, thrower.position,
                          target, passTarget, !fumble, roll});
    }

    Position landPos;
    Position lastOnPitch = target;
    if (fumble) {
        // l. 8613-8614: "A fumbled team-mate will land in THE SQUARE HE
        // ORIGINALLY OCCUPIED." Puvodne se rozptyloval od HAZECE.
        // l. 8607-8608: "fumbles are NOT automatically turnovers."
        landPos = origin;
    } else {
        // l. 8609-8612: "ACCURATE PASSES ARE TREATED INSTEAD AS INACCURATE
        // PASSES thus scattering the player THREE TIMES as players are heavier
        // and harder to pass than a ball." Nepresny hod scatteruje tri krat
        // stejne (l. 735-737) => obe vetve jsou totozne a "presny" dopad
        // neexistuje. Puvodne: presny = presne na cil, nepresny = JEDEN rozptyl.
        // P76 (PHP 1657a19e): kdo vyleti z hriste, konci v davu (r. 8615-8617)
        // -- dalsi rozptyl ho uz nevrati. `lastOnPitch` je pole, odkud se
        // pripadne vhazuje mic (r. 866-871).
        landPos = target;
        for (int i = 0; i < 3; ++i) {
            lastOnPitch = landPos;
            Position step = scatterDirection(dice.rollD8());
            landPos.x = static_cast<int8_t>(landPos.x + step.x);
            landPos.y = static_cast<int8_t>(landPos.y + step.y);
            if (!landPos.isOnPitch()) break;
        }
    }

    // Hazeny hráč uz neni tam, kde byl.
    projectile.position = {-1, -1};

    auto crowd = [&]() {
        // l. 8615-8617: "If the thrown player scatters off the pitch, he is
        // beaten up by the crowd in the same manner as a player who has been
        // pushed off the pitch."
        // l. 8430-8431: "A failed landing roll or LANDING IN THE CROWD DOES NOT
        // CAUSE A TURNOVER, UNLESS HE WAS HOLDING THE BALL."
        // P76 (PHP ec3feb2c): mic se VHAZUJE z posledniho pole na hristi
        // (r. 866-871). Driv se odrazel z pole (-1,-1), kam uz byl hrac
        // odlozeny.
        if (projectileHadBall) {
            state.ball = BallState::onGround(lastOnPitch);
            resolveThrowIn(state, lastOnPitch, landPos, dice, events);
        }
        resolveCrowdSurf(state, projectileId, lastOnPitch, dice, events);
        return projectileHadBall ? ActionResult::turnovr() : ActionResult::ok();
    };

    if (!landPos.isOnPitch()) return crowd();

    // l. 8617-8622: "If the final square he scatters into is OCCUPIED by another
    // player, treat the player landed on as KNOCKED DOWN and ROLL FOR ARMOUR
    // (EVEN IF ALREADY PRONE OR STUNNED), and then the player being thrown will
    // scatter ONE MORE SQUARE. ... he cannot land on more than one player."
    // Puvodne se jen scatterovalo dal a obsazenemu poli se nestalo nic.
    bool landedOnSomeone = false;
    if (Player* under = state.getPlayerAtPosition(landPos)) {
        landedOnSomeone = true;
        // P76: omraceny zustava omraceny (hod na brneni, ne otoceni) -- jako bomba.
        if (under->state != PlayerState::STUNNED) under->setState(PlayerState::PRONE);
        emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, under->id, projectileId,
                          under->position, {}, 0, false});
        InjuryContext uctx;
        resolveArmourAndInjury(state, under->id, dice, uctx, events);
        handleBallOnPlayerDown(state, under->id, dice, events);

        // "and then the player being thrown will scatter one more square",
        // dokud neskonci v prazdnem poli nebo mimo hriste
        do {
            lastOnPitch = landPos;   // review P76: vhazuje se z pole, odkud vyletel
            Position step = scatterDirection(dice.rollD8());
            landPos.x = static_cast<int8_t>(landPos.x + step.x);
            landPos.y = static_cast<int8_t>(landPos.y + step.y);
            if (!landPos.isOnPitch()) return crowd();
        } while (state.getPlayerAtPosition(landPos) != nullptr);
    }

    projectile.position = landPos;
    if (projectileHadBall) state.ball.position = landPos;

    // Right Stuff, l. 8421-8431: "he must make a LANDING ROLL UNLESS HE LANDED
    // ON ANOTHER PLAYER during the throw. A landing roll is an AGILITY ROLL with
    // a -1 modifier for each opposing player's tackle zone on the square he
    // lands in. ... If the landing roll is failed OR HE LANDED ON ANOTHER PLAYER
    // he is Placed Prone and must pass an Armour roll to avoid injury."
    bool landedOnFeet = false;
    if (!landedOnSomeone) {
        int landTarget = std::clamp(
            7 - projectile.stats.agility + countTacklezones(state, landPos, projectile.teamSide),
            2, 6);
        int landRoll = dice.rollD6();
        landedOnFeet = (landRoll >= landTarget);
        emitEvent(events, {GameEvent::Type::SKILL_USED, projectileId, -1, landPos, {},
                          static_cast<int>(SkillName::RightStuff), landedOnFeet});
    }

    if (landedOnFeet) {
        // OPRAVENO 10.11.2026 (P282, audit goblinů) — míč pod hozeným zůstal ležet. Ř. 440-444: hráč hozený na pole
        //   s míčem ho „cannot pick up … instead it will bounce one square. This does not cause a turnover“ (PHP totéž).
        if (!state.ball.isHeld && state.ball.position == landPos) resolveBounce(state, landPos, dice, 0, events);
        return ActionResult::ok();
    }

    projectile.setState(PlayerState::PRONE);
    emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, projectileId, -1, landPos, {}, 0, false});
    InjuryContext ctx;
    resolveArmourAndInjury(state, projectileId, dice, ctx, events);
    handleBallOnPlayerDown(state, projectileId, dice, events);

    // l. 8430-8431: neuspesny dopad NENI turnover, ledaze nesl mic.
    return projectileHadBall ? ActionResult::turnovr() : ActionResult::ok();
}

} // namespace bb
