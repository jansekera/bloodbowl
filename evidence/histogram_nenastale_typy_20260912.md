# 37 TYPŮ UDÁLOSTÍ, KTERÉ NENASTALY — PROJITO JEDEN PO DRUHÉM (12.09.2026)

Korpus: `learning`, 30 her, seed 20260911 — **889 kol, 3 782 rozhodnutí, 26 467 událostí**.
Zdroj: `evidence/php_event_histogram_learning_20260912.txt`.

⚠️ **Dvě měřidla po cestě lhala a obě se chytila křížovou kontrolou:**
1. `GameFlow` události se vůbec nesbíraly ⇒ `touchdown` a `kickoff` vypadaly jako nula *(opraveno, `f2db325f`)*.
2. Kontrola „je ta dovednost v rosterech?" nejdřív porovnávala **hodnoty** enumu proti **jménům** case ⇒ hlásila „ThrowTeamMate v rosterech není", ačkoli ho má goblinní Troll. **Opraveno na `->name`.**

⭐ **Kolik se toho vůbec dá potkat:** korpus losuje 2 rasy z 26 na zápas ⇒ 60 slotů na 30 her,
tedy **jedna konkrétní rasa vyjde průměrně 2,3×**. U dovednosti, kterou má jediná rasa, tedy
nula **není překvapení** — je to slabé měřidlo, ne nález.

## A) VYSVĚTLENO — dovednost není v žádném rosteru korpusu (13)

`hail_mary_pass` · `kick_off_return` · `kick_skill` · `leader` · `multiple_block` ·
`pass_block` · `piling_on` · `pro` · `shadowing` · `stab` · `stakes_block_regen` ·
`strip_ball` · `sneaky_git`

⇒ Kód pro ně existuje *(tovární metoda se volá z konkrétního handleru)*, jen se na hřiště
nemá jak dostat. **Nula je správná.**

## B) ⛔ ENGINE JI NEUMÍ VYROBIT VŮBEC (1)

| typ | nález |
|---|---|
| **`sneaky_git`** | Tovární metoda `GameEvent::sneakyGit()` **se nevolá NIKDE v `src/`**. Událost je definovaná v enumu i v migraci, ale žádná cesta ji nevyrábí. ⇒ **Skutečná díra, ne vzácnost.** |

## C) ⏸ BLOKOVÁNO JINOU, UŽ ZNÁMOU VADOU (3)

`throw_team_mate` · `ttm_landing` · `always_hungry`

⇒ Hod spoluhráče **nemá rameno ANI JEDEN ze tří koučů** *(PHP28)*, takže se nikdy nezvolí —
a s ním nemůže nastat ani přistání, ani Always Hungry. ⚠️ Rostery to přitom **mají**
*(Halfling, Goblin, Ogre, Underworld, Chaos Pact)*, takže vysvětlení „korpus na to nedosáhl"
tady **neplatí** — je to důsledek PHP28, odloženého uživatelem na „až bude tým v TV".

## D) ⚠️ PODEZŘELÉ — dovednost V ROSTERECH JE a cesta v kódu existuje (20)

| typ | rasy, co to mají | kde to engine vyrábí |
|---|---|---|
| `ball_and_chain_move`, `ball_and_chain_block` | Goblin | `BallAndChainHandler` |
| `bomb_landing`, `bomb_explosion`, `bomb_throw` | Goblin *(Bombardier)* | `BombThrowHandler` |
| `chainsaw`, `chainsaw_kickback` | Goblin | `BlockHandler` |
| `no_hands`, `secret_weapon` | Goblin | `BallResolver`, `GameFlowResolver` |
| `take_root` | Halfling | `BigGuyCheckResolver` |
| `hypnotic_gaze` | Vampire | `HypnoticGazeHandler` |
| `wrestle`, `fend` | Bretonnian | `BlockHandler` |
| `juggernaut` | Khorne | `BlockHandler` |
| `nurgles_rot` | Nurgle | `InjuryResolver` |
| `diving_catch` | Slann | `PassResolver` |
| `dump_off` | Dark Elf | `PassResolver` |
| `safe_throw` | High Elf, Pro Elf | `PassResolver` |
| `animosity` | Underworld, Chaos Pact | `PassResolver`, `HandOffHandler` |
| **`touchback`** | ⛔ **žádná dovednost k tomu netřeba** | `KickoffResolver` |

⛔⛔ **`touchback` je z téhle skupiny ten nejpodezřelejší:** nezávisí na rosteru ani na
dovednosti — stačí, aby kop skončil mimo hřiště nebo u nesprávného týmu. Za 30 zápasů
*(desítky výkopů)* by nastat měl.

## ⏰ CO S TÍM — NÁVRH, NE ROZHODNUTÍ

Skupina (D) se nemá probírat po jednom hádáním. **Levný rozhodovací krok: pustit korpus
s VYNUCENÝMI rostery** *(Goblin vs. Halfling, Bretonnian vs. Khorne, Vampire vs. Dark Elf…)*
místo losování z 26 ras. Tím se z „možná je to vzácné" stane **čitelná nula nebo jednička**
u každé položky naráz — a teprve to, co zůstane nulové i tam, je nález.
