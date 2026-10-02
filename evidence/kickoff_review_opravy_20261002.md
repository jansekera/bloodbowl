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
| 9 | Touchback bez způsobilého hráče: míč na pevné (6,7)/(19,7) i pod ležícího ⇒ teď odraz (`resolveBounce`) | `KickoffLanding.TouchbackFallbackSquareOccupiedByAPronePlayerBounces` | ✅ spadl před opravou (míč pod ležícím na (6,7)) | (tento commit) |

**Pozn. k N1:** TD po turnoveru (např. míč odražený k soupeři v zóně) už dřív dával +1 kolo skórujícím — turnover předá tah a připíše kolo dřív, než se kontroluje TD. Výsledek odpovídá pravidlu; větev `scoringSide != activeTeam` tam nenastane.
PHP engine: **převést do druhého enginu** (#2, #4, N1).

## Nechává se na rozhodnutí uživatele (neimplementováno)

- **#3** AI neví o `kickoffLanding` během Blitz! (bonusové kolo hraje bez znalosti místa dopadu).
- **#6** Návrat z KO / Sweltering Heat se hází i před výkopem, který se nekoná (přijímající nemá kolo).
- **#7** Kick-Off Return se pohybuje přes tacklezóny bez uhýbání.
