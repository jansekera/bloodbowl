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
        int hungryRoll = dice.rollD6();
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
                projectile.state = PlayerState::DEAD;
                projectile.position = {-1, -1};
                return ActionResult::ok();
            }
            // Vykroutil se => akce se automaticky bere jako FUMBLE.
            forcedFumble = true;
        }
    }

    // 2. Accuracy roll. Range comes from the ruler grid (rules parity,
    // 2026-08-10), and CRP Throw Team-Mate caps it: "Long Pass or Long Bomb
    // range passes are not possible" -- Short Pass is the ceiling, which is
    // what the printed grid marks as "Max. TTM".
    PassRange range;
    if (!passRangeFromOffset(target.x - thrower.position.x,
                             target.y - thrower.position.y, range) ||
        range == PassRange::LONG_PASS || range == PassRange::LONG_BOMB) {
        return ActionResult::fail();
    }

    if (thrower.hasSkill(SkillName::StrongArm) && range != PassRange::QUICK_PASS) {
        range = static_cast<PassRange>(static_cast<int>(range) - 1);
    }

    int passTarget = 7 - thrower.stats.agility;
    passTarget -= passModifier(range);

    if (!thrower.hasSkill(SkillName::NervesOfSteel)) {
        passTarget += countTacklezones(state, thrower.position, thrower.teamSide);
    }

    // ⛔ TA3, l. 8606-8607: "the player must SUBTRACT 1 FROM THE D6 ROLL when he
    // passes the player". Ten postih tu nebyl vubec.
    passTarget += 1;

    passTarget = std::clamp(passTarget, 2, 6);

    // Kdyz se spoluhrac vykroutil (l. 7792-7794), je akce "automatically treated
    // as a fumbled pass" -- hod na presnost uz se NEDELA.
    bool fumble = forcedFumble;
    if (!forcedFumble) {
        int roll = dice.rollD6();
        fumble = (roll == 1);
        emitEvent(events, {GameEvent::Type::PASS, throwerId, projectileId, thrower.position,
                          target, roll, !fumble});
    }

    Position landPos;
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
        landPos = target;
        for (int i = 0; i < 3; ++i) {
            Position step = scatterDirection(dice.rollD8());
            landPos.x = static_cast<int8_t>(landPos.x + step.x);
            landPos.y = static_cast<int8_t>(landPos.y + step.y);
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
        if (projectileHadBall) handleBallOnPlayerDown(state, projectileId, dice, events);
        resolveCrowdSurf(state, projectileId, dice, events);
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
        under->state = PlayerState::PRONE;
        emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, under->id, projectileId,
                          under->position, {}, 0, false});
        InjuryContext uctx;
        resolveArmourAndInjury(state, under->id, dice, uctx, events);
        handleBallOnPlayerDown(state, under->id, dice, events);

        // "and then the player being thrown will scatter one more square",
        // dokud neskonci v prazdnem poli nebo mimo hriste
        do {
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

    if (landedOnFeet) return ActionResult::ok();

    projectile.state = PlayerState::PRONE;
    emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, projectileId, -1, landPos, {}, 0, false});
    InjuryContext ctx;
    resolveArmourAndInjury(state, projectileId, dice, ctx, events);
    handleBallOnPlayerDown(state, projectileId, dice, events);

    // l. 8430-8431: neuspesny dopad NENI turnover, ledaze nesl mic.
    return projectileHadBall ? ActionResult::turnovr() : ActionResult::ok();
}

} // namespace bb
