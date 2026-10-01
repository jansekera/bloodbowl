# Simplify dávky P68–P97 (rozsah `3d90cc6d..c5c6cdb8`) — nálezy 01.10.2026, ZATÍM NEAPLIKOVÁNO

Čtyři kontroly (reuse, zjednodušení, efektivita, výška opravy). Uživatel 01.10. přednostně P98–P111
(nálezy z turniketu) ⇒ tahle dávka čeká. Nic z toho nemění chování; ověřovat testy + **stejné zápasy se
stejným semínkem** (`mcts_cli --seed=N`) před a po.

## Malé, v rozsahu dávky — udělat
| # | kde | co |
|---|---|---|
| S1 | `bomb_handler.cpp:~49`, `ttm_handler.cpp:~83`, `pass_handler.cpp:~314` | fumble `r==1 \|\| r+mods<=1` 3× ⇒ `isThrowFumble(roll, unclampedMods)` v helpers |
| S2 | `move_handler.cpp:~159`, `pathfinder.cpp:~546`, `macro_actions.cpp:~1062`, `helpers.cpp:~91` | Break Tackle „S>AG a nepoužit“ 4× ⇒ `breakTackleApplies()` + `consumeBreakTackle()`; v odhadech **nekopírovat celý `Player`** (horká cesta MCTS) |
| S3 | ~12 míst C++ (`block_handler:583,1168`, `move_handler:146,271`, `rules_engine:51,152`, `pathfinder:607`, `macro_actions:326,1480,1926,2044,2651`) + PHP `BlockHandler`/`BlitzHandler` | `rooted ? 0 : Sprint ? 3 : 2` ručně, i se znaménkem ⇒ `maxGfiSquares()` (C++), `MatchPlayerDTO::getMaxGfi()` (PHP) — ⚠️ ověřit, že `maxGfiSquares` vrací 0 pro rooted |
| S4 | `action_resolver.cpp:~545` | `wouldScore` → `checkTouchdown` po KAŽDÉ akci ⇒ až za podmínkou `bloodlustHungry` (horká cesta) |
| S5 | `macro_actions.cpp:~1590–1620` (+ `enemiesThatReachNextTurn`) | hledání krytí po blitzu: ~169 polí × ~11 soupeřů ⇒ seznam (pozice, dosah) soupeřů jednou na volání; vytáhnout `offerBlitzCover()` a `offerThrallApproach()` ze `getAvailableMacros` |
| S6 | `pass_handler.cpp:~353` vs `helpers.cpp:330` `attemptRoll`, `block_handler.cpp:757` | Pro přehoz 3. kopie (P70 se opravoval 3×) ⇒ `tryProReroll()` |
| S7 | `foul_handler.cpp:~66–141` | Chainsaw +3 a `dirtyOnInjury` roztroušené ⇒ na jednom místě |
| S8 | `turn_handler.cpp:24` | `p.state = PRONE` záměrně mimo `setState` ⇒ pojmenovaná metoda (dokumentace záměru) |
| S9 | PHP `BlockHandler.php:~194` a `~367` | dvojí obsluha výsledku `payBlitzBlock` ('paid'/'unpayable'/'fell') |

## Střední/velké — zapsat jako samostatné položky, ne v simplify
- `Player::state` veřejné ⇒ zápisy mimo `setState` (P68 invariant jen konvencí).
- Crowd surf 3× (`pushAwayFrom`, B&C, TTM) ⇒ `crowdSurfPlayer()`.
- „Sražení + zranění“ ~8× ⇒ `knockDown()`; navazující „STUNNED→KO mimo hřiště“ ve volajících.
- Chainsaw jako větev v `resolveArmourAndInjury` + 4× `attCtx.chainsaw` ⇒ `ctx.causedBy`, odvodit v resolveru (B&C, TTM, Bomb flag nenastavují — ⚠️ prověřit, zda to není i pravidlová vada).
- Blood Lust 4 háčky v `executeAction` ⇒ jeden `onActivationEnd()`.
- PHP žebříček přehozů (Sure Feet → Pro → tým/Loner) 4× ⇒ služba `RerollLadder`.
- PHP `blitzContinuationPlayerId` odvoditelné z `hasActed && !hasMoved` — ⚠️ jen tvrzení kontroly, neověřeno.
- frontend `BlockDiceModal.test.ts` — DOM mock regexy, kopie z `LevelUpModal.test.ts`.
