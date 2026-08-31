#pragma once

#include "bb/game_state.h"
#include "bb/dice.h"
#include "bb/game_event.h"
#include "bb/action_result.h"
#include <vector>

namespace bb {

struct BlockParams {
    int attackerId = -1;
    int targetId = -1;
    bool isBlitz = false;
    bool hornsBonus = false;  // +1 ST on blitz with Horns
};

ActionResult resolveBlock(GameState& state, const BlockParams& params,
                          DiceRollerBase& dice, std::vector<GameEvent>* events,
                          bool frenzySecondBlock = false,
                          bool noFollowUp = false,
                          // L6 (29.08.2026): "both skills cannot be used
                          // together" (r. 8304-8305). Multiple Block vypina
                          // Frenzy na CELOU svou akci -- nestaci spolehnout se
                          // na to, ze po odsunu nebudou sousedit: se Stand Firm
                          // nebo Fend obranci zustanou stat vedle sebe a povinny
                          // druhy blok by se hodil. Vlastni parametr, ne
                          // preteceni pres `noFollowUp` -- ten se nastavuje i
                          // pri zakoreneni (r. 469) a tam Frenzy vypinat nemame
                          // duvod.
                          bool frenzyDisabled = false);

ActionResult resolveMultipleBlock(GameState& state, int attackerId,
                                  int target1Id, int target2Id,
                                  DiceRollerBase& dice, std::vector<GameEvent>* events);

BlockDiceFace autoChooseBlockDie(const BlockDiceFace* faces, int count,
                                 bool attackerChooses,
                                 const Player& att, const Player& def);


// ⛔ TENHLE KOMENTÁŘ TVRDIL „kolikrát se OPRAVDU hodil Dauntless" A BYLA TO
//    NEPRAVDA (P25, 17.08.2026). Blok se resolvuje i uvnitř každé simulace
//    MCTS, takže se to zvyšuje i tam. Naměřeno: **349 na zápas** proti
//    **1,88 skutečně odehraným** hodům v korpusu — přeceněno **186×**.
//
// ⛔ A padá s tím i druhá věta: „rozdíl mezi nabídkou a odebráním říká, jestli
//    si search nabídnutý blok vzal." NEŘÍKÁ. Obě čísla jsou z vnitřku
//    prohledávání, takže se poměřují dvě veličiny téhož druhu; otázku
//    „nabídli jsme a vzal si to?" ani jedna nezodpoví.
//
// ⇒ Skutečně odehrané hody se čtou z logu událostí:
//    SKILL_USED s roll == SkillName::Dauntless, `success` = srovnal.
//    Na korpusu 14.08.: 1,88/hru, srovnáno v 73,8 %, aspoň jeden v 60,3 % her.
//
// K čemu tohle číslo JE: „spustilo se to rameno vůbec?" Nula = obě ramena
// běžela na stejném kódu ⇒ pravý null.
long takeDauntlessRollEvalsInSearch();

// ---------------------------------------------------------------------------
// P9 / P9c ARM (2026-08-18): choose the push DESTINATION, do not just take the
// first one geometry offers.
//
// `choosePushSquare` scores candidates as `count - i` -- "straight back first"
// -- so the destination square is never evaluated for anything except (a) empty
// vs occupied, (b) refusing to walk a carrier into his own end zone, and (c)
// the Side Step / Grab special cases. Measured on the 3000-game corpus
// (evidence/push_choice_ceiling_20260818.md): 21.9 pushes a game, 17.3 of them
// with a REAL choice (>=2 empty candidates), and in 1.04 a game we pushed the
// opponent CLOSER to our own ball carrier when a farther empty square existed
// -- 0.27 a game glued him directly adjacent. Another 0.24 a game left him
// adjacent to a corner of our own cage when another square would have cleared
// him. Pushing a man next to our carrier raises REACH0, which is the SECOND
// strongest predictor of a scoring drive in the 08-18 sigma table (-16.7 sigma,
// and it replicates on both corpus halves).
//
// Per SIDE, because an A/B must be able to run the arm on one team only, and
// thread_local because self-play runs games in parallel. Default OFF on both
// sides = today's behaviour, bit for bit.
void setPushGeometryArm(TeamSide side, bool on);
bool pushGeometryArm(TeamSide side);

// Times the arm actually picked a DIFFERENT square than "straight back first"
// would have, since the last call -- and resets. Same unit warning as the other
// take*EvalsInSearch counters: this counts resolutions inside the search too,
// not just pushes played on the pitch. It answers "did the arm run at all",
// which is what the per-pair null control needs.
long takePushGeometryEvalsInSearch();


// Q3 krok B (31.08.2026): souperova odpoved na vstani vedle nej.
// hit  = na hrace, ktery v predchozim kole vstal vedle soupere, dopadl blok
// blitz= z toho placeno VZACNYM zdrojem (blitz 1/kolo), zbytek je blok ZDARMA
// kd   = z toho skoncilo srazenim
long takeHitOnStoodUpInSearch();
long takeHitOnStoodUpByBlitzInSearch();
long takeKnockedOnStoodUpInSearch();

} // namespace bb