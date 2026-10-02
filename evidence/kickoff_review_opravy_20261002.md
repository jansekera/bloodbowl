# Review sjednoceného výkopu — opravy (02.10.2026)

Navazuje na `evidence/kickoff_sjednoceni_20261002.md` (commit 3a1fe879, `resolveKickoff`).
Engine: **C++** (`engine/`). PHP engine se tu nemění ⇒ každá pravidlová oprava = řádek „převést do druhého enginu“.

Pozitivní kontrola = test spuštěn PŘED opravou a spadl (u pokryvných testů: spadl na záměrně
rozbitém kódu, mutace vrácena).

| # | položka | test | pozitivní kontrola | commit |
|---|---|---|---|---|
| 1 | TurnLog: Blitz! po TD se slil do záznamu kola s TD | `KickoffTurnLog.BlitzAfterATouchdownGetsItsOwnTurnLog` | ✅ spadl před opravou (seed 11 a 20: Blitz! po TD bez vlastního záznamu) | 369157cf |
| 2 | Poryv „Nice“ vracel míč zpoza autu / LoS (FAQ ř. 9315-9317) | `KickoffTable.NiceGustDoesNotBringAKickOffThePitchBackIn`, `…BackOverTheLineOfScrimmage` | ✅ oba spadly před opravou (míč na (1,7) nedržen; chytil hráč na LoS) | (tento commit) |

## Nechává se na rozhodnutí uživatele (neimplementováno)

- **#3** AI neví o `kickoffLanding` během Blitz! (bonusové kolo hraje bez znalosti místa dopadu).
- **#6** Návrat z KO / Sweltering Heat se hází i před výkopem, který se nekoná (přijímající nemá kolo).
- **#7** Kick-Off Return se pohybuje přes tacklezóny bez uhýbání.
