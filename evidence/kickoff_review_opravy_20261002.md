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
| 10a | diag nástroje volaly odstraněný `simpleKickoff` ⇒ `resolveKickoff(state, dice, nullptr)` | překlad `g++ -std=c++20 -Iengine/include -Iengine/third_party … -lbb_engine` | `diag_blitz_approach_20260807` se přeloží. `diag_f1_adoption_probe` a `diag_f1_cage_advance_harness` dál nejdou přeložit, ale **z jiného důvodu**: `MCTSConfig::cageGrind/cageAdvance`, `takeCageDiceyGfiStats`, `stagedPlansAdopted` odstranil P126 (4ac90bb5, CageController místo plánovače F1) — měří plánovač, který už neexistuje; výkopové chyby v nich nezbyly | 6ba3ed4b |
| 10b | `diag_first_possession.py`: přijímající = aktivní tým prvního záznamu jízdy, i když je to bonusové kolo Blitz! kopajících | `_selftest` (nový případ s Blitz!) | ✅ selftest spadl před opravou (`recv` = away místo home). `get_turn_logs` nově vrací `kickoff_ball_in_air` | 6ba3ed4b |
| 10c | `test_bb_engine.py` počasí: volné `kept >= 40` z 59 ⇒ deterministicky: dvojče `DiceRoller` předpoví hod tabulky; mimo 7 se počasí nesmí změnit nikdy, se 7 aspoň jednou ano | `test_roll_match_weather_is_rolled_once_and_kept_by_kickoff` | ✅ mutace (nový hod počasí po každém výkopu, stará vada P66) ⇒ spadl („seed 4: tabulka 12, počasí se změnilo“); vráceno | 6ba3ed4b |

## Pokrytí dosud netestovaných pravidel (vše prošlo hned — žádná nová vada; pozitivní kontrola mutací)

| pravidlo | test | mutace, na které test spadl |
|---|---|---|
| touchback při odrazu z hřiště (ř. 280-283) | `KickoffLanding.BounceOffThePitchIsATouchback` | M1: touchback jen u prvního dopadu, ne u odrazu |
| nechycený míč odražený přes LoS (FAQ ř. 9312-9314) | `KickoffLanding.FailedCatchBouncingOverTheLineOfScrimmageIsATouchback` | M1 |
| turnover ukončí Blitz! a míč dopadne (ř. 1339-1341) | `KickoffTable.TurnoverEndsTheBlitzBonusTurnAndTheBallLands` | M2: dopad jen po END_TURN, ne po turnoveru |
| týmový přehoz v Blitz! (ř. 1338-1339) | `KickoffTable.KickingTeamMayUseATeamRerollDuringTheBlitz` | M3: přehoz zakázán, je-li míč ve vzduchu |
| kopající omráčený v Blitz! vstane po svém příštím řádném kole (ř. 703-708) | `KickoffTable.KickingPlayerStunnedInTheBlitzTurnsFaceUpAfterHisNextNormalTurn` | M4: hod na zranění bez `stunnedThisTurn` |
| Riot D6 4-6 zpět (ř. 1291-1296) | `KickoffTable.RiotMidHalfOnFourToSixMovesBothMarkersBack` | M5: D6 vždy vpřed |
| Throw a Rock remíza ⇒ oba (ř. 1345-1347) | `KickoffTable.ThrowARockTieHitsBothTeams` | M6: remíza jen jeden tým |
| High Kick: obsazené pole / touchback (ř. 1302-1308) | `KickoffTable.HighKickDoesNothingWhenTheLandingSquareIsOccupied`, `…WhenTheKickIsATouchback` | M7: bez podmínky pole/polovina |
| „žádné kolo“ v `simulateGame` (ř. 1033-1035) | `KickoffHandler.NoKickoffWithoutATurnLeftInARealGame` | M8: bez strážce `turnNumber > 8` ⇒ kostky výkopu (seed 6, 34) |
| **Quick Snap do soupeřovy poloviny** (ř. 1331-1333) | — **netestovatelné bez změny AI** | Do soupeřovy poloviny se dá vkročit jen z LoS (x=12/13), a hráče na LoS volba AI drží na místě (`resolveQuickSnap`). Kód polovinu nefiltruje, ale volba AI ji nikdy nevyužije ⇒ **rozhodnutí uživatele** (volba AI, ne vada pravidla) |

Mutace M1–M8: `/tmp/…/scratchpad/mut.py` (mimo repo), každá po běhu vrácena.

**Pozn. k N1:** TD po turnoveru (např. míč odražený k soupeři v zóně) už dřív dával +1 kolo skórujícím — turnover předá tah a připíše kolo dřív, než se kontroluje TD. Výsledek odpovídá pravidlu; větev `scoringSide != activeTeam` tam nenastane.
PHP engine: **převést do druhého enginu** (#2, #4, N1).

## Nechává se na rozhodnutí uživatele (neimplementováno)

- **#3** AI neví o `kickoffLanding` během Blitz! (bonusové kolo hraje bez znalosti místa dopadu).
- **#6** Návrat z KO / Sweltering Heat se hází i před výkopem, který se nekoná (přijímající nemá kolo).
- **#7** Kick-Off Return se pohybuje přes tacklezóny bez uhýbání.

## Ověření (po posledním commitu)

- `bb_tests`: **839 passed** (před review 821; +18 testů).
- `engine/python/test_bb_engine.py` (python3.8): 17 passed · `python/tests` (venv): 202 passed · `diag_first_possession.py selftest`: ALL PASS.
- Kouř `mcts_cli --home=greedy --away=greedy --games=6 --home-roster=wood-elf --away-roster=orc`: 6 her doběhlo (4:0:2, ⌀ 2,17 : 1,33).
- Nemergováno, nepushnuto (worktree `worktree-agent-ac466507256f9769d`).

## Rozhodnutí uživatele 02.10.: #3, #6, #7

| # | rozhodnutí | test | pozitivní kontrola | commit |
|---|---|---|---|---|
| 6 | „KO mezi poločasy má házet jednou“: po TD, kdy přijímající nemá kolo, se `setupDrive` nevolá (výkop se nekoná ⇒ žádný návrat z KO ani Sweltering Heat); poločas/hra končí rovnou, házet se bude až v `setupHalf` (ř. 1007-1012). Sdílený predikát `receivingTeamHasATurnLeft` (kickoff_handler.h) používá i `resolveKickoff` | `KickoffHandler.KoPlayerRollsOnceToRecoverBetweenHalvesAfterALastTurnTouchdown` (simulateGame, greedy, human × orc; kostky mimo výkop od TD do výkopu 2. poločasu = počet KO hráčů; hry s horkem vynechány) | ✅ spadl před opravou: seed 6 — 3 KO, 6 hodů; seed 34 — 2 KO, 3 hody (pozn.: první verze testu prošla, protože počítala jen `half == 1`, a `simulateGame` přepne `half = 2` před `setupHalf` ⇒ hody v `setupHalf` neviděla; opraveno před commitem) | f5458ed7 |
| 7 | Kick-Off Return bez uhýbání: z pole v soupeřově tacklezóně hráč neodchází (vstoupit smí, pak stojí); cíl = nejblíž míči, při shodě pole mimo TZ, pak méně kroků (prohledání do šířky, nejvýš 3 kroky, rozšiřuje se jen z polí mimo TZ). Žádné hody na uhýbání | `KickOffReturn.StopsOnTheFirstSquareInAnOpposingTacklezone` (dopad (13,7) na LoS přijímajících; jediná cesta přes (13,5) v TZ), `KickOffReturn.PrefersAnEquallyGoodSquareOutsideTacklezones` ((13,6) v TZ × (14,6) mimo, obě vzdálenost 1) | ✅ oba spadly před opravou: skončil na (13,7) místo (13,5) (prošel TZ (13,5)→(13,6)); skončil na (13,6) v TZ. Pozn.: první fixtura testu 1 měla obchvat (15,4)-(16,5)-(15,6) na stejnou vzdálenost mimo TZ — nová logika ho správně vzala; fixtura opravena (zatarasena i (16,5)) a pozitivní kontrola zopakována na starém kódu | def3e701 |
| 3 | „Při Blitz! ví, kam dopadne míč“ (ř. 1242-1248: rozptyl před tabulkou). Nový `looseBallSquare(state)` (helpers.h, SSOT): míč na zemi, nebo při Blitz! místo dopadu (jen na hřišti v přijímající polovině; touchback ⇒ žádné pole). Používají ho: **greedy** (priorita 2 „k míči“ — kopající táhnou k místu dopadu, smí i na něj: míč nezvedne, při dopadu chytá), **makra** (`getAvailableMacros`: REPOSITION „obklíčit volný míč“ kolem místa dopadu místo obrany u vlastní zóny; PICKUP dál jen pro míč na zemi), **listové hodnocení MacroMCTS** (bonus za blízkost k místu dopadu, bez postihu −0,1 „volný míč“). Python: `GameState.kickoff_ball_in_air`, `kickoff_landing` | `KickoffBlitzLanding.LooseBallSquareIsTheLandingWhileTheBallIsInTheAir`, `…GreedyBonusTurnMovesTowardTheLandingSquare` (záměr: ≥ 90 % kroků greedy během letu míče přibližuje hráče k dopadu, 10 semínek), `…MacrosOfferSquaresAroundTheLandingButNoPickup`; Python `test_blitz_exposes_the_kickoff_landing` | ✅ všechny 3 C++ testy spadly na staré sémantice (helper jen „míč na zemi“, greedy a makra beze změny): helper {-1,-1}; greedy 50 ze 109 kroků blíž (nový 45 ze 46); žádné makro k dopadu. Python test spadl na `AttributeError` před rozšířením vazby | viz git log |

**K #3 — co zůstává (vědomě nezměněno):**
- Atomické MCTS (`mcts_cli --home=mcts`, `MCTSSearch`): rollouty jsou náhodné akce a hodnota jde z hodnotové funkce přes `extractFeatures` — rysy volného míče (67, 70-72) berou jen míč na zemi. Měnit vstupy natrénované sítě kvůli bonusovému kolu jsem nechal být (jiné rozložení vstupů ⇒ nová báze). Rozhodnutí uživatele, jestli rysy rozšířit.
- `classifyTurnGoal` (turn_planner) dál dává při Blitz! `NONE`, ne `PICKUP_BALL` — míč ve vzduchu sebrat nelze, plán zvednutí by byl nelegální.
- Greedy jde k dopadu i přes tacklezóny (uhýbání ⇒ turnover může bonusové kolo ukončit) — stejné chování jako u míče na zemi; měřeno: nejbližší kopající se z 10 polí dostal na 1–9 (starý kód 8–10).
- PHP engine: Blitz! jako bonusové kolo nemá (most není) — převést do druhého enginu spolu s #6 a #7.

## Ověření po #3, #6, #7

- `bb_tests`: **845 passed** (839 → +1 #6, +2 #7, +3 #3).
- `engine/python/test_bb_engine.py` (python3.8): **18 passed** · `python/tests` (venv): **202 passed**.
- Kouř `mcts_cli --home=greedy --away=greedy --games=6 --home-roster=wood-elf --away-roster=orc`: 6 her doběhlo (3:0:3, ⌀ 2,00 : 1,17). Navíc 3 hry `macro_mcts` (30 iterací) × greedy přes Python doběhly (2:0, 1:0, 1:0).
- Nemergováno, nepushnuto.
