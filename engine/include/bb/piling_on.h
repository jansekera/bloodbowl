#pragma once

#include "bb/game_state.h"

namespace bb {

// P310 (uživatel 24.11.2026): Piling On, `rules_bb2016.txt` ř. 8361-8371 (a ř. 3050-3060):
// "The player may use this skill after they have made a block as part of a Block or Blitz action, but only if
// they are currently standing adjacent to the victim and the victim was Knocked Down. You can use a team re-roll
// to re-roll the Armour roll or Injury roll. Then the Piling On player is Placed Prone in their own square - no
// Armour roll is made for them, and this does not cause a turnover unless the Piling On player is carrying the
// ball. Piling On cannot be used with the Stab or Chainsaw skills. If a player with the Loner skill wishes to use
// Piling On, roll a D6. On a 1-3, the team re-roll is spent, but they remain standing and cannot re-roll the
// Armour or Injury roll."
// Parita s PHP `App\Engine\PilingOn`.

// Šance, že hod 2k6 + `mod` na zranění vyřadí hráče z hřiště (KO nebo horší); Stunty (7 = KO) a Thick Skull (8 = Stunned).
double injuryOutChance(const Player& victim, int mod);

// Šance, že hod na brnění 2k6 + `mod` (s Mighty Blow podle `resolveArmourAndInjury`) prolomí brnění a zranění vyřadí.
double armourThenOutChance(const Player& victim, int armourMod, bool mightyBlow, bool claw);

// Smí útočník Piling On teď použít (pravidla): skill, stojí, sousedí se sraženým, tým má v tahu přehoz.
bool pilingOnAllowed(const GameState& state, const Player& att, const Player& def);

// Rozhodnutí AI (uživatel 24.11.: „1 vše, 2 ano“ + „piling on dává našeho hráče na zem - cíl pro faul“):
//   ne s míčem (turnover), ne uvnitř Multiple Block; šetří přehoz, dokud tým hraje s míčem (použije, jen když míč
//   drží soupeř nebo náš nosič už v tahu dohrál); a jen když zisk (soupeř vyřazen po přehozu) > ztráta (náš ležící
//   útočník vyřazen faulem: všichni stojící soupeři vedle něj asistují, faulující přijde zvenku, Dirty Player +1).
//   Uživatel 24.11. (2): po použití musí zbýt aspoň jeden přehoz (na tah s možností TD; kromě našeho posledního
//   tahu poločasu) a útočník nesmí stát vedle nosiče (ležící neotevře cestu k míči — roh klece / krytí nosiče).
// `armourStage` = přehazuje se brnění (vydrželo), jinak zranění (padlo Stunned) s modifikátorem `injuryMod`.
bool aiPilingOn(const GameState& state, const Player& att, const Player& def, bool armourStage,
                int armourMod, bool mightyBlow, bool claw, int injuryMod, bool multipleBlock);

// Šance, že faul na ležícího `p` ho vyřadí (asistence = stojící soupeři vedle, +1 Dirty Player na hřišti).
double foulOutChance(const GameState& state, const Player& p, int excludeId);

} // namespace bb
