#include "bb/rules_engine.h"
#include "bb/helpers.h"
#include "bb/pathfinder.h"
#include "bb/macro_actions.h"

namespace bb {

void getAvailableActions(const GameState& state, std::vector<Action>& out) {
    out.clear();

    if (state.phase != GamePhase::PLAY) return;

    TeamSide side = state.activeTeam;
    const TeamState& team = state.getTeamState(side);

    // END_TURN is always available
    out.push_back({ActionType::END_TURN, -1, -1, {-1, -1}});

    state.forEachOnPitch(side, [&](const Player& p) {
        // M13 (31.08.2026): DEKLAROVAT smi i lezici (BB2016 r. 669-676).
        // Resolver to uz umel -- `MOVE` i `BLITZ` maji vetev "if prone, stand
        // up first" (action_resolver.cpp:96, :127) -- chybela jen NABIDKA.
        // Tataz trida jako Leap 24.08.: hotovy resolver bez volajiciho.
        if (!p.canDeclareAction()) return;
        const bool prone = (p.state == PlayerState::PRONE);
        // ⭐ M13 NASAZENO 02.09.2026. Noc 01.->02.09. (2 400 paru, dw-dw):
        //   +0,0048 +- 0,0084 (0,57 sigma), jednostranne CI [-0,0059; +0,0107]
        //   ⇒ oprava pravidla NIC MERITELNEHO NESTOJI. Rameno odebrano,
        //   protoze default-OFF vypinac u dolozene neskodne pravidlove opravy
        //   uz nema co hlidat (tyz duvod jako P35 a cena Wrestle u B2).
        //   ⚠️ Ctyri light testy pred noci daly ZAPORNOU deltu
        //   (-0,1875 / 0,0000 / -0,1667 / -0,1250). Byl to sum a bylo to
        //   zapsano PREDEM jako sum. Noc to potvrdila.

        // BallAndChain players can ONLY use the BALL_AND_CHAIN action
        if (p.hasSkill(SkillName::BallAndChain)) {
            if (prone) return;   // krok A meri jen MOVE; B&C z lehu neresime
            out.push_back({ActionType::BALL_AND_CHAIN, p.id, -1, {-1, -1}});
            return; // Skip all other action types
        }

        // MOVE: each adjacent empty square (single-step)
        auto adj = p.position.getAdjacent();
        for (auto& pos : adj) {
            if (!pos.isOnPitch()) continue;
            if (state.getPlayerAtPosition(pos) != nullptr) continue;

            // Check movement remaining (including GFI)
            // Take Root, l. 8577-8578: zakorenený "may not Go For It" -- MA 0
            // by mu jinak porad nechalo dva kroky pres GFI.
            int maxGfi = p.rooted ? 0 : (p.hasSkill(SkillName::Sprint) ? 3 : 2);
            // Lezici plati vstani ze SVEHO pohybu (3 pole, r. 690-695) jeste
            // nez udela prvni krok, takze rozpocet se pocita az PO nem.
            // Pod 3 MA vstani nuluje pohyb => krok je pak nutne GFI, coz je
            // presne to, co pravidlo dovoluje ("unless he Goes For It").
            if (movementAfterStandUp(p) - 1 < -maxGfi) continue;

            out.push_back({ActionType::MOVE, p.id, -1, pos});
        }


        // LEAP -- BB2016 l. 8270-8283: "allowed to jump to any EMPTY square
        // WITHIN 2 SQUARES even if it requires jumping over a player from
        // either team. Making a leap costs the player TWO squares of movement.
        // ... A player may only use the Leap skill ONCE PER TURN."
        // ⛔ Tahle nabidka tu do 24.08.2026 nebyla, takze `resolveLeap` -- hotovy
        // a se tremi zelenymi testy -- nemel volajiciho a nikdo nikdy neskocil.
        // Je to tataz trida jako P45 vstavani: resolver bez nabidky.
        // ⏰ Leap z lehu: rozpocet by musel odecist vstani a resolveLeap
        //    vetev "stand up first" nema. Mimo M13.
        if (p.hasSkill(SkillName::Leap) && !p.leapUsedThisTurn && !p.rooted && !prone) {
            int maxGfi = p.hasSkill(SkillName::Sprint) ? 3 : 2;
            if (p.movementRemaining - 2 >= -maxGfi) {
                for (int dx = -2; dx <= 2; ++dx) {
                    for (int dy = -2; dy <= 2; ++dy) {
                        if (dx == 0 && dy == 0) continue;
                        Position dest{static_cast<int8_t>(p.position.x + dx),
                                      static_cast<int8_t>(p.position.y + dy)};
                        if (!dest.isOnPitch()) continue;
                        if (p.position.distanceTo(dest) > 2) continue;
                        if (state.getPlayerAtPosition(dest) != nullptr) continue;
                        out.push_back({ActionType::LEAP, p.id, -1, dest});
                    }
                }
            }
        }

        // BLOCK: each adjacent standing enemy.
        // Not for a player who already moved this activation: BB2016 l. 675,
        // "you may not move when you take a Block Action" -- move+block is a
        // BLITZ, and that is one per turn (gated below by blitzUsedThisTurn).
        // canAct() cannot carry this: it is also used for targeting checks.
        TeamSide enemySide = opponent(side);
        // ⛔ BLOK Z LEHU NIKDY (r. 674-676, vyslovne). Jedina vyjimka je
        // Jump Up (r. 8200-8204: Block Action z lehu na AG roll +2) -- to je
        // polozka A2 a resolver pro ni neexistuje, takze se nenabizi.
        // ⚠️ Tahle straz musi byt VYSLOVNA: `p.hasMoved` je v okamziku
        // deklarace jeste false, takze zavor nize by blok lezicimu nezabranil.
        for (auto& pos : adj) {
            if (prone) break;
            // ⚠️ VÝJIMKA PRO BLITZ (oprava 21.08.): pohyb + blok JE blitz.
            // `expandBlitzAndScore` provede BLITZ akci, pak si dojde k cíli
            // vlastními kroky MOVE a teprve pak hledá BLOCK -- holé
            // `if (p.hasMoved) break` mu ten blok sebralo a blitz se
            // spotřeboval bez rány. `usedBlitz` se nastavuje na začátku
            // BLITZ akce, takže rozlišuje "deklaroval blitz" od "jen se hnul".
            if (p.hasMoved && !p.usedBlitz) break;
            if (!pos.isOnPitch()) continue;
            const Player* enemy = state.getPlayerAtPosition(pos);
            if (enemy && enemy->teamSide == enemySide && canAct(enemy->state)) {
                out.push_back({ActionType::BLOCK, p.id, enemy->id, enemy->position});
            }
        }

        // BLITZ: if not used this turn, each reachable enemy
        // M13 krok B (31.08.2026): NABIZI SE I Z LEHU. Blitz neni Block Action,
        // takze spada pod r. 676 "may take any Action other than a Block
        // Action"; resolver ho umi (action_resolver.cpp:127 vstane a teprve pak
        // se hybe) a `canReachAdjacentTo` uz dosah o vstani zkracuje.
        // ⭐ Odsud je uzivateluv tah z 27.08. ("zacal bych blitz z (14,2)")
        // vubec zahratelny -- do ted lezici hrac blitz NIKDY nedostal.
        if (!team.blitzUsedThisTurn && !p.usedBlitz) {
            state.forEachOnPitch(enemySide, [&](const Player& enemy) {
                if (!canAct(enemy.state) && !isOnPitch(enemy.state)) return;
                if (enemy.state != PlayerState::STANDING) return;

                // Check if already adjacent
                if (p.position.distanceTo(enemy.position) == 1) {
                    // Uz sousedi: blitz je jen blok s priznakem -- ale i ten
                    // BLOK STOJI JEDNO POLE (r. 549-550), a lezici na nej musi
                    // mit z ceho zaplatit AZ PO vstani. Bez teto podminky by
                    // se lezicimu s MA3 nabidl blitz, ktery po vstani (3-3=0)
                    // hodi GFI, a zakorenenemu blitz, ktery se nehodi vubec --
                    // v obou pripadech se utrati tymovy blitz za nic.
                    // ⭐ P37b (01.09.2026): NA RANU MUSI ZBYT POLE POHYBU.
                    //   r. 549-550: "the block ... costs one square of movement".
                    //   Do dneska tuhle kontrolu meli jen LEZICI (M13, 31.08.);
                    //   stojici ne, takze hrac, ktery uz utratil pohyb i GFI,
                    //   blitz DEKLAROVAL, tym prisel o svuj jediny blitz na
                    //   kolo a rana se nehodila (`block_handler.cpp:480`).
                    //   ZMERENO pred opravou: 142 na 8 paru = ~9 za zapas,
                    //   0,26 % blitzu. Mala, ale CISTA ztrata.
                    // ⛔ ROZSAH JE ZAMERNE UZKY (uzivatel 01.09.: "blitz souseda
                    //   a pak utek je dobry napad"). r. 552-553 dovoluje po rane
                    //   POKRACOVAT v pohybu, takze blitz na souseda je zpusob,
                    //   jak se OSVOBODIT -- srazim ho, zmizi tacklezona, odejdu
                    //   bez dodge. Nabidka se proto NEZUZUJE za to, ze hrac
                    //   sousedi, ani za to, ze mu po rane nezbude krok.
                    //   Odmita se JEDINY pripad: neni z ceho zaplatit ani ranu.
                    //   (Ten hrac se stejne nemuze ani hnout, takze mu deklarace
                    //   nekoupi ani Wild Animal bonus -- viz P37b v knize.)
                    const int gfi = p.rooted ? 0 : maxGfiSquares(p);
                    if (movementAfterStandUp(p) - 1 < -gfi) return;
                    out.push_back({ActionType::BLITZ, p.id, enemy.id, enemy.position});
                    return;
                }

                // Check if we can reach adjacent to enemy with 1 MP still in
                // the budget -- the blitz block itself costs a movement point
                // (CRP), so an approach burning the full MA+GFI range would
                // arrive unable to throw the block at all.
                Position adjPos;
                if (canReachAdjacentTo(state, p, enemy.position, adjPos, 1)) {
                    out.push_back({ActionType::BLITZ, p.id, enemy.id, enemy.position});
                }
            });
        }

        // ⛔⛔ ZAVOR M13 (31.08.2026): lezici dostane MOVE (krok A) a BLITZ
        // (krok B). Vsechno nize je zamerne NEDOSTUPNE, a NENI to opatrnost,
        // je to spravnost:
        //   BLOCK      -- r. 674-676 ho lezicimu VYSLOVNE zakazuje. Vyjimka
        //                 Jump Up (r. 8200-8204, AG roll +2) je polozka A2 a
        //                 resolver pro ni neexistuje.
        //   LEAP/PASS/HAND_OFF/FOUL/GAZE/TTM/BOMB/MULTIPLE_BLOCK
        //              -- vetsina je z lehu podle pravidel legalni (r. 676),
        //                 ale jejich resolvery vetev "if prone, stand up first"
        //                 NEMAJI. Nabidnout je ted znamena zahrat je Z LEHU,
        //                 tedy vyrobit nelegalni tah misto opravy chybejiciho.
        if (prone) return;

        // PASS: if not used this turn, has ball, each standing teammate within range 13
        if (!team.passUsedThisTurn && state.ball.isHeld && state.ball.carrierId == p.id &&
            !p.hasSkill(SkillName::NoHands)) {
            state.forEachOnPitch(side, [&](const Player& teammate) {
                if (teammate.id == p.id) return;
                if (teammate.state != PlayerState::STANDING) return;
                // Reach comes from the ruler grid, not a Chebyshev radius
                // (rules parity, 2026-08-10): "within 13" offered 45 targets
                // the ruler cannot actually reach.
                PassRange range;
                if (!passRangeFromOffset(teammate.position.x - p.position.x,
                                         teammate.position.y - p.position.y,
                                         range)) {
                    return;
                }
                // Blizzard: "only quick or short passes can be attempted".
                if (state.weather == Weather::BLIZZARD &&
                    (range == PassRange::LONG_PASS || range == PassRange::LONG_BOMB)) {
                    return;
                }
                out.push_back({ActionType::PASS, p.id, teammate.id, teammate.position});
            });
        }

        // HAND_OFF: if not used this turn, has ball, each adjacent standing teammate
        // 2026-08-17: gated on its OWN allowance (P4/P26). It used to share
        // passUsedThisTurn, which made a pass and a hand-off mutually exclusive
        // in the same turn -- the rules make them separate declarations.
        if (!team.handOffUsedThisTurn && state.ball.isHeld && state.ball.carrierId == p.id &&
            !p.hasSkill(SkillName::NoHands)) {
            for (auto& pos : adj) {
                if (!pos.isOnPitch()) continue;
                const Player* teammate = state.getPlayerAtPosition(pos);
                if (teammate && teammate->teamSide == side &&
                    teammate->state == PlayerState::STANDING) {
                    out.push_back({ActionType::HAND_OFF, p.id, teammate->id, teammate->position});
                }
            }
        }

        // FOUL: if not used this turn, each adjacent prone/stunned enemy
        if (!team.foulUsedThisTurn) {
            for (auto& pos : adj) {
                if (!pos.isOnPitch()) continue;
                const Player* enemy = state.getPlayerAtPosition(pos);
                if (enemy && enemy->teamSide == enemySide &&
                    (enemy->state == PlayerState::PRONE ||
                     enemy->state == PlayerState::STUNNED)) {
                    out.push_back({ActionType::FOUL, p.id, enemy->id, enemy->position});
                }
            }
        }

        // THROW_TEAM_MATE: player has ThrowTeamMate + adjacent RightStuff teammate
        if (p.hasSkill(SkillName::ThrowTeamMate) && !team.passUsedThisTurn) {
            for (auto& pos : adj) {
                if (!pos.isOnPitch()) continue;
                const Player* teammate = state.getPlayerAtPosition(pos);
                if (teammate && teammate->teamSide == side &&
                    teammate->state == PlayerState::STANDING &&
                    teammate->hasSkill(SkillName::RightStuff)) {
                    // Target positions: any square within pass range
                    // For simplicity, generate targets every 3 squares in each direction
                    for (int tx = 0; tx < 26; tx += 3) {
                        for (int ty = 0; ty < 15; ty += 3) {
                            int dist = p.position.distanceTo({static_cast<int8_t>(tx),
                                                              static_cast<int8_t>(ty)});
                            if (dist > 0 && dist <= 13) {
                                out.push_back({ActionType::THROW_TEAM_MATE, p.id,
                                              teammate->id,
                                              {static_cast<int8_t>(tx), static_cast<int8_t>(ty)}});
                            }
                        }
                    }
                }
            }
        }

        // BOMB_THROW (rules parity, 2026-08-10). CRP Bombardier: the throw
        // "does not use the team's Pass Action for the turn", so it is NOT
        // gated on passUsedThisTurn; but "the player may not move or stand
        // up before throwing it (he needs time to light the fuse!)", so a
        // player who has already moved -- or who is Prone/Stunned -- cannot.
        // Reach comes from the ruler grid, not a Chebyshev radius.
        if (p.hasSkill(SkillName::Bombardier) && !p.hasMoved &&
            p.state == PlayerState::STANDING) {
            state.forEachOnPitch(enemySide, [&](const Player& enemy) {
                if (enemy.state != PlayerState::STANDING) return;
                PassRange range;
                if (!passRangeFromOffset(enemy.position.x - p.position.x,
                                         enemy.position.y - p.position.y,
                                         range)) {
                    return;
                }
                out.push_back({ActionType::BOMB_THROW, p.id, -1, enemy.position});
            });
        }

        // HYPNOTIC_GAZE: each adjacent standing enemy
        if (p.hasSkill(SkillName::HypnoticGaze)) {
            for (auto& pos : adj) {
                if (!pos.isOnPitch()) continue;
                const Player* enemy = state.getPlayerAtPosition(pos);
                if (enemy && enemy->teamSide == enemySide &&
                    enemy->state == PlayerState::STANDING) {
                    out.push_back({ActionType::HYPNOTIC_GAZE, p.id, enemy->id, enemy->position});
                }
            }
        }

        // MULTIPLE_BLOCK: player has MultipleBlock and 2+ adjacent enemies.
        // ⭐ FRENZY UZ TU NENI PODMINKA (oprava 29.08.2026, nalez L6).
        // r. 8302-8305: "...so Multiple Block can be used INSTEAD OF Frenzy,
        // but both skills cannot be used TOGETHER." Pravidlo zakazuje
        // KOMBINACI, ne drzeni obojiho -- `&& !p.hasSkill(Frenzy)` tedy hraci
        // s obema dovednostmi bralo VOLBU, kterou pravidlo predpoklada.
        // Vylouceni se resi na druhe strane: `resolveMultipleBlock` pousti oba
        // bloky s `frenzyDisabled`, takze Frenzy uvnitr nezasahne.
        if (p.hasSkill(SkillName::MultipleBlock)) {
            // Collect adjacent standing enemies
            int adjEnemies[8];
            int nAdj = 0;
            for (auto& pos : adj) {
                if (!pos.isOnPitch()) continue;
                const Player* enemy = state.getPlayerAtPosition(pos);
                if (enemy && enemy->teamSide == enemySide &&
                    enemy->state == PlayerState::STANDING) {
                    if (nAdj < 8) adjEnemies[nAdj++] = enemy->id;
                }
            }
            // Generate all pairs
            for (int i = 0; i < nAdj; i++) {
                for (int j = i + 1; j < nAdj; j++) {
                    // Encode: targetId=first target, target.x=second target ID
                    out.push_back({ActionType::MULTIPLE_BLOCK, p.id, adjEnemies[i],
                                  {static_cast<int8_t>(adjEnemies[j]), 0}});
                }
            }
        }
    });

    // Also allow standing up prone players
    state.forEachOnPitch(side, [&](const Player& p) {
        if (p.state != PlayerState::PRONE) return;
        if (p.hasActed || p.lostTacklezones) return;
        // Stejná výjimka jako v hlavní smyčce (:21) a v makro vrstvě:
        // BallAndChain smí JEN svou akci. Bez toho si dvě vrstvy odporovaly.
        if (p.hasSkill(SkillName::BallAndChain)) return;

        // Anyone prone may ATTEMPT to stand: 3 MA if he has it, otherwise a
        // 4+ roll (BB2016 l. 691-693). The old `movementRemaining >= 3` gate
        // meant a sub-3-MA player was never even offered the action, so a
        // Treeman (MA 2) stayed down for the rest of every drive.
        {
            // After standing up, the player can move — generate a MOVE action
            // to their own position as a "stand up" action
            out.push_back({ActionType::MOVE, p.id, -1, p.position});
        }
    });
}

} // namespace bb
