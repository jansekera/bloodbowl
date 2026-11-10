#include "bb/ball_and_chain_handler.h"
#include "bb/helpers.h"
#include "bb/injury.h"
#include "bb/ball_handler.h"
#include "bb/block_handler.h"
#include <algorithm>

namespace bb {

namespace {

// Sablona vhazovani natocena `dir` (jednotkovy smer), D6: 1-2 vlevo od smeru,
// 3-4 rovne, 5-6 vpravo. Vzor PHP ScatterCalculator::templateOffset.
Position templateStep(Position from, Position dir, int d6) {
    const int side = d6 <= 2 ? -1 : (d6 <= 4 ? 0 : 1);
    const int dx = dir.x != 0 ? dir.x : side;
    const int dy = dir.x != 0 ? side : dir.y;
    return {static_cast<int8_t>(from.x + dx), static_cast<int8_t>(from.y + dy)};
}

// Natoceni sablony -- rozhodnuti uzivatele 25.09. (znovu 30.09.): "vzdy k
// souperi a pozor na svoje i lezici". Kazde natoceni ma tri pole po 1/3;
// skore = stojici soupere - 2 x NASI (i lezici a omraceni) - dav; shodu
// rozhodne blizkost rovneho pole k nejblizsimu STOJICIMU souperi.
Position chooseOrientation(const GameState& state, const Player& bcp) {
    static const Position dirs[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    Position best = dirs[0];
    double bestScore = -1e9;
    for (const Position& dir : dirs) {
        double score = 0.0;
        for (int d6 : {1, 3, 5}) {
            const Position sq = templateStep(bcp.position, dir, d6);
            if (!sq.isOnPitch()) { score -= 1.0; continue; }
            const Player* who = state.getPlayerAtPosition(sq);
            if (who && who->teamSide == bcp.teamSide) score -= 2.0;
            else if (who && who->state == PlayerState::STANDING) score += 1.0;
        }
        const Position straight{static_cast<int8_t>(bcp.position.x + dir.x),
                                static_cast<int8_t>(bcp.position.y + dir.y)};
        int nearest = 99;
        state.forEachOnPitch(opponent(bcp.teamSide), [&](const Player& e) {
            if (e.state == PlayerState::STANDING)
                nearest = std::min(nearest, straight.distanceTo(e.position));
        });
        if (nearest < 99) score -= nearest / 100.0;
        if (score > bestScore) { bestScore = score; best = dir; }
    }
    return best;
}

// Sraženy (nebo vyleteny) B&C: rovnou hod na ZRANENI bez brneni; Stunned se
// pocita jako KO (r. 7848-7850).
void injureBnC(GameState& state, int id, DiceRollerBase& dice,
               std::vector<GameEvent>* events) {
    Player& bcp = state.getPlayer(id);
    handleBallOnPlayerDown(state, id, dice, events);
    bcp.setState(PlayerState::PRONE);
    InjuryContext ctx{};
    resolveInjuryRoll(state, id, dice, ctx, events);   // Stunned = KO řeší resolveInjuryRoll (P276)
}

// P284 (10.11.2026): `mightyBlow` = blokující B&C má Mighty Blow a cíl je soupeř (ř. 8293-8295 „when an opponent is
//   Knocked Down by this player during a block“); dřív se házelo vždy bez něj.
void knockDownTarget(GameState& state, Player& target, DiceRollerBase& dice,
                     std::vector<GameEvent>* events, bool mightyBlow) {
    if (!target.isOnPitch()) return;
    target.setState(PlayerState::PRONE);
    emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, target.id, -1,
                      target.position, {}, 0, false});
    InjuryContext ctx;
    ctx.mightyBlow = mightyBlow;
    resolveArmourAndInjury(state, target.id, dice, ctx, events);
    handleBallOnPlayerDown(state, target.id, dice, events);
}

// Strana kostky: proti VLASTNIMU voli nas kouc a bere, co nejmene ublizi
// (rozhodnuti uzivatele 25.09.); proti souperi nejlepsi pro B&C, nebo
// nejhorsi, kdyz voli souper.
BlockDiceFace chooseFace(const BlockDiceFace* faces, int n, bool ownTarget, bool attackerChooses) {
    static const BlockDiceFace own[] = {BlockDiceFace::PUSHED, BlockDiceFace::DEFENDER_STUMBLES,
        BlockDiceFace::BOTH_DOWN, BlockDiceFace::ATTACKER_DOWN, BlockDiceFace::DEFENDER_DOWN};
    static const BlockDiceFace good[] = {BlockDiceFace::DEFENDER_DOWN, BlockDiceFace::DEFENDER_STUMBLES,
        BlockDiceFace::PUSHED, BlockDiceFace::BOTH_DOWN, BlockDiceFace::ATTACKER_DOWN};
    BlockDiceFace order[5];
    for (int i = 0; i < 5; ++i)
        order[i] = ownTarget ? own[i] : (attackerChooses ? good[i] : good[4 - i]);
    for (BlockDiceFace f : order)
        for (int i = 0; i < n; ++i)
            if (faces[i] == f) return f;
    return faces[0];
}

// Blok podle beznych pravidel (r. 7840-7842): kostky podle sily vc. asistenci.
// Vraci true = turnover: sražený B&C, nebo sražený spoluhráč (P281, ř. 366-367 „A player on the moving team is
// Knocked Down“; B&C blokuje „friend or foe“, ř. 7820-7822). Vytlačený do davu sražen není.
bool bncBlock(GameState& state, Player& bcp, Player& target, DiceRollerBase& dice,
              std::vector<GameEvent>* events) {
    const int attAssists = countAssists(state, target.position, bcp.teamSide, bcp.id, target.id, target.id);
    const int defAssists = countAssists(state, bcp.position, target.teamSide, bcp.id, target.id, bcp.id);
    const BlockDiceInfo info = getBlockDiceInfo(bcp.stats.strength + attAssists,
                                                target.stats.strength + defAssists);
    BlockDiceFace faces[3];
    for (int i = 0; i < info.count; ++i) faces[i] = dice.rollBlockDie();
    const bool ownTarget = target.teamSide == bcp.teamSide;
    const bool mb = !ownTarget && bcp.hasSkill(SkillName::MightyBlow);   // P284
    const BlockDiceFace face = chooseFace(faces, info.count, ownTarget,
                                          info.attackerChooses);
    emitEvent(events, {GameEvent::Type::BLOCK, bcp.id, target.id, bcp.position,
                      target.position, static_cast<int>(face), true});

    switch (face) {
        case BlockDiceFace::ATTACKER_DOWN:
            injureBnC(state, bcp.id, dice, events);
            return true;
        case BlockDiceFace::BOTH_DOWN: {
            const bool bcFalls = !bcp.hasSkill(SkillName::Block);
            const bool targetFalls = !target.hasSkill(SkillName::Block);
            if (targetFalls) knockDownTarget(state, target, dice, events, mb);
            if (bcFalls) injureBnC(state, bcp.id, dice, events);
            return bcFalls || (targetFalls && ownTarget);
        }
        case BlockDiceFace::PUSHED:
            pushAwayFrom(state, bcp, target, /*isBlock=*/true, dice, events);
            return false;
        case BlockDiceFace::DEFENDER_STUMBLES: {
            const bool down = !target.hasSkill(SkillName::Dodge) || bcp.hasSkill(SkillName::Tackle);
            const bool surfed = pushAwayFrom(state, bcp, target, /*isBlock=*/true, dice, events);
            if (down && !surfed) knockDownTarget(state, target, dice, events, mb);
            return down && !surfed && ownTarget;
        }
        default: {   // DEFENDER_DOWN
            const bool surfed = pushAwayFrom(state, bcp, target, /*isBlock=*/true, dice, events);
            if (!surfed) knockDownTarget(state, target, dice, events, mb);
            return !surfed && ownTarget;
        }
    }
}

} // anonymous namespace

// P72 (30.09.2026) -- port PHP e373bf23 + 04e4946c, BB2016 r. 7820-7850.
// Viz komentar u testu `P72*`. C++ dosud: D8 misto sablony, jedna kostka bloku
// bez sily, auto-KO v davu, sraženy B&C s hodem na brneni, "nikdy turnover",
// lezici v ceste se preskocil.
ActionResult resolveBallAndChain(GameState& state, int playerId,
                                 DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& bcp = state.getPlayer(playerId);
    bcp.hasActed = true;
    bool knockedInBlock = false;

    for (int step = 0; step < bcp.stats.movement; ++step) {
        if (!bcp.isOnPitch() || bcp.state != PlayerState::STANDING) break;
        const Position dir = chooseOrientation(state, bcp);
        const Position target = templateStep(bcp.position, dir, dice.rollD6());

        // Mimo hriste: dav ho zbije jako vytlaceneho (r. 7835-7837) -- hod na
        // zraneni, ne automaticke KO; turnover to neni (r. 369-370).
        if (!target.isOnPitch()) {
            handleBallOnPlayerDown(state, playerId, dice, events);
            const Position last = bcp.position;
            bcp.position = {-1, -1};
            resolveCrowdSurf(state, playerId, last, dice, events);
            if (bcp.state == PlayerState::STUNNED || bcp.state == PlayerState::OFF_PITCH) {
                bcp.setState(PlayerState::KO);
            }
            break;
        }

        Player* occupant = state.getPlayerAtPosition(target);
        if (occupant) {
            if (occupant->state == PlayerState::STANDING) {
                if (bncBlock(state, bcp, *occupant, dice, events)) { knockedInBlock = true; break; }
            } else {
                // Lezici nebo omraceny: odtlacit a hod na brneni misto bloku
                // (r. 7843-7845). Omraceny zustava omraceny.
                const bool surfed = pushAwayFrom(state, bcp, *occupant, /*isBlock=*/false, dice, events);
                if (!surfed && occupant->isOnPitch()) {
                    InjuryContext ctx;
                    resolveArmourAndInjury(state, occupant->id, dice, ctx, events);
                }
            }
            // Povinny follow-up (r. 7845-7847): uvolnilo-li se pole, B&C postoupi.
            if (state.getPlayerAtPosition(target) != nullptr) continue;
        }

        const Position oldPos = bcp.position;
        bcp.position = target;
        emitEvent(events, {GameEvent::Type::PLAYER_MOVE, playerId, -1, oldPos, target, 0, true});
        // OPRAVENO 10.11.2026 (P280) — míč jen odskočil a B&C šel dál. Vstup na pole s míčem = pokus o zvednutí
        //   (ř. 436-438); No Hands ⇒ „will bounce, and will cause a turnover if it is his team's turn“ (ř. 8320-8322).
        if (!state.ball.isHeld && state.ball.position == target) {
            resolveBounce(state, target, dice, 0, events);
            return ActionResult::turnovr();
        }
    }

    // Sraženy pri bloku = hrac tymu na tahu Knocked Down = turnover (r. 368);
    // dav turnover neni (r. 369-370).
    return knockedInBlock ? ActionResult::turnovr() : ActionResult::ok();
}

} // namespace bb
