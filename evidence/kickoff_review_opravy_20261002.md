# Review sjednoceného výkopu — opravy (02.10.2026)

Navazuje na `evidence/kickoff_sjednoceni_20261002.md` (commit 3a1fe879, `resolveKickoff`).
Engine: **C++** (`engine/`). PHP engine se tu nemění ⇒ každá pravidlová oprava = řádek „převést do druhého enginu“.

Pozitivní kontrola = test spuštěn PŘED opravou a spadl (u pokryvných testů: spadl na záměrně
rozbitém kódu, mutace vrácena).

| # | položka | test | pozitivní kontrola | commit |
|---|---|---|---|---|
| 1 | TurnLog: Blitz! po TD se slil do záznamu kola s TD | `KickoffTurnLog.BlitzAfterATouchdownGetsItsOwnTurnLog` | ✅ spadl před opravou (seed 11 a 20: Blitz! po TD bez vlastního záznamu) | 369157cf |
| 2 | Poryv „Nice“ vracel míč zpoza autu / LoS (FAQ ř. 9315-9317) | `KickoffTable.NiceGustDoesNotBringAKickOffThePitchBackIn`, `…BackOverTheLineOfScrimmage` | ✅ oba spadly před opravou (míč na (1,7) nedržen; chytil hráč na LoS) | 634b5632 |
| 4 | TD kopajících při dopadu po Blitz!: značku posouvá skórující, přijímajícímu kolo nepropadá (ř. 997-1004) | `KickoffTable.KickingTeamScoringOnTheBlitzLandingMovesItsOwnTurnMarker` | ✅ spadl před opravou (kopající 0 místo 1, přijímající 1 místo 0) | 1379a744 |
| N1 | **nová vada (týž princip jako #4)**: TD v soupeřově kole (nosič zatlačený blokem do zóny) neposouval značku skórujících (ř. 997-1004). Značku teď posouvá `executeAction` obecně; výkop jen vrací kolo přijímajícím | `ActionResolver.TouchdownInTheOpponentsTurnMovesTheScorersTurnMarker` | ✅ spadl před opravou (značka 3 místo 4); test #4 dál zelený | 8aa7dcb7 |
| 5 | `setupHalfOrDrive` nečistil `kickoffBallInAir` / `kickoffLanding` | `KickoffHandler.SetupClearsABallStillInTheAirFromBlitz` | ✅ spadl před opravou (setupDrive i setupHalf) | 51e3c133 |
| 8 | Pitch Invasion: `stunnedThisTurn` jako injury.cpp:55. Chování se nemění (kopajícím příznak čistí začátek jejich kola, přijímajícím `finishKickoff`) | `KickoffTable.PitchInvasionStunnedPlayersOfBothTeamsTurnFaceUpAfterTheirOwnNextTurn` | ✅ před opravou spadl jen řádek s příznakem; behaviorální část zelená před i po (= beze změny chování). Mutace: bez resetu přijímajících po dopadu ⇒ spadly ř. 796-797; otáčení všech omráčených na konci každého kola ⇒ spadl ř. 797 | dff3b4d0 |
| 9 | Touchback bez způsobilého hráče: míč na pevné (6,7)/(19,7) i pod ležícího ⇒ teď odraz (`resolveBounce`) | `KickoffLanding.TouchbackFallbackSquareOccupiedByAPronePlayerBounces` | ✅ spadl před opravou (míč pod ležícím na (6,7)) | cf63f57e |
| 10a | diag nástroje volaly odstraněný `simpleKickoff` ⇒ `resolveKickoff(state, dice, nullptr)` | překlad `g++ -std=c++20 -Iengine/include -Iengine/third_party … -lbb_engine` | `diag_blitz_approach_20260807` se přeloží. `diag_f1_adoption_probe` a `diag_f1_cage_advance_harness` dál nejdou přeložit, ale **z jiného důvodu**: `MCTSConfig::cageGrind/cageAdvance`, `takeCageDiceyGfiStats`, `stagedPlansAdopted` odstranil P126 (4ac90bb5, CageController místo plánovače F1) — měří plánovač, který už neexistuje; výkopové chyby v nich nezbyly | (tento commit) |
| 10b | `diag_first_possession.py`: přijímající = aktivní tým prvního záznamu jízdy, i když je to bonusové kolo Blitz! kopajících | `_selftest` (nový případ s Blitz!) | ✅ selftest spadl před opravou (`recv` = away místo home). `get_turn_logs` nově vrací `kickoff_ball_in_air` | (tento commit) |
| 10c | `test_bb_engine.py` počasí: volné `kept >= 40` z 59 ⇒ deterministicky: dvojče `DiceRoller` předpoví hod tabulky; mimo 7 se počasí nesmí změnit nikdy, se 7 aspoň jednou ano | `test_roll_match_weather_is_rolled_once_and_kept_by_kickoff` | ✅ mutace (nový hod počasí po každém výkopu, stará vada P66) ⇒ spadl („seed 4: tabulka 12, počasí se změnilo“); vráceno | (tento commit) |

**Pozn. k N1:** TD po turnoveru (např. míč odražený k soupeři v zóně) už dřív dával +1 kolo skórujícím — turnover předá tah a připíše kolo dřív, než se kontroluje TD. Výsledek odpovídá pravidlu; větev `scoringSide != activeTeam` tam nenastane.
PHP engine: **převést do druhého enginu** (#2, #4, N1).

## Nechává se na rozhodnutí uživatele (neimplementováno)

- **#3** AI neví o `kickoffLanding` během Blitz! (bonusové kolo hraje bez znalosti místa dopadu).
- **#6** Návrat z KO / Sweltering Heat se hází i před výkopem, který se nekoná (přijímající nemá kolo).
- **#7** Kick-Off Return se pohybuje přes tacklezóny bez uhýbání.
