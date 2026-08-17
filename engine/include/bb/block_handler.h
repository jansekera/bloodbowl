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
                          bool noFollowUp = false);

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

} // namespace bb