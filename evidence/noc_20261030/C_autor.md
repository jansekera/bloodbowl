# Noc 30.10.2026 — úkol C (autor): Ball & Chain Grab jen proti soupeři · Diving Catch jako otázka kouči

## Průběh

### (1) B&C Grab jen proti soupeři — C++
- Chokepoint: `pushSkills` (engine/src/block_handler.cpp) — tudy jde Grab Block Action i bloku B&C (`pushAwayFrom`).
  Oprava: `grab = hasGrab && grabMayApply && defender.teamSide != attacker.teamSide` (ř. 8146-8150 „his opponent“).
- Testy (engine/tests/test_ball_and_chain_handler.cpp): `GrabIsNotUsedAgainstATeammate`, `LiveGameDoesNotAskAboutGrabAgainstATeammate`.
  Na starém kódu SPADLY oba (spoluhráč odtlačen Grabem na (10,x); přišla otázka USE_GRAB = 15). Po opravě zelené.
  Pozn.: první verze fixture byla vadná (natočení B&C se spoluhráčům vyhýbá ⇒ B&C šel jinam, živý test „prošel“ i bez opravy);
  fixture opravena (šablona +x musí vyhrát), test teď kontroluje i to, že B&C opravdu šel na spoluhráče.
- Strážný test „ležící ⇒ bez Grabu“ už existuje: `P222PushingAProneOccupantIsNotABlockSoNoGrab` — zelený.
- Otevřená otázka (neměněno): zrušení Grab × Side Step (`hasGrab`) dál platí i proti spoluhráči se Side Step
  (ř. 8151-8153 ruší i v blitzu, kde Grab sám nepůsobí). Zda má Grab, který proti spoluhráči nepůsobí, rušit jeho Side Step, nechávám na uživateli.

### (1) B&C Grab jen proti soupeři — PHP
- Vendor: `App\` → `src/` tohoto worktree (ověřeno: `ReflectionClass(BlockHandler)` ukazuje do worktree).
- Chokepoint: `BlockHandler::grabSquares` (jediný predikát Grabu — skutečné odtlačení `odtlacit` i otázka `grabQuestionSquares`).
  Oprava: spoluhráč ⇒ `[]`.
- Testy (tests/Engine/BallAndChainGrabTest.php): `testGrabSeNaSpoluhraceNepouzije`, `testKoucSeNaGrabProtiSpoluhraciNepta`.
  Na starém kódu SPADLY oba (spoluhráč Grabem na x=5; PendingGrabDTO s poli (5,6),(5,8)). Po opravě zelené.
- Starý test `GrabKoucTest::testStandFirmVlastnihoTymuBncProtiVlastnimuGrabOtazku` kódoval Grab proti spoluhráči
  (odtlačení četl z otázky na Grab) ⇒ po opravě spadl. Jeho požadavek (Stand Firm proti vlastnímu B&C nedrží,
  ř. 1824-1825) jsem přepsal: žádná otázka Grab/Stand Firm a B&C následoval. Mutace (Stand Firm drží i proti
  vlastnímu) ⇒ přepsaný test spadne. ✔
- Strážný test na ležícího: `testOdtlaceniLezicihoNeniBlokTakzeBezGrabu` existuje, zelený.

### (2) Diving Catch jako otázka kouči — PHP (zápis průběžně)
- Problém: otázka přijde UPROSTŘED akce, až po hodech (přihrávka, rozptyl, vhazování z libovolné akce — blok,
  odraz za lajnu, výkop). PHP kostky nejdou vrátit (`random_int`), a vhazování se volá hluboko z mnoha míst ⇒
  pokračovací DTO pro každé místo by bylo křehké. Řešení = princip C++ `CoachChoiceScope`: akce se přeruší,
  vrátí se stav PŘED akcí s `PendingDivingCatchDTO` (akce, parametry, zapsané hody, dosavadní odpovědi) a po
  odpovědi `RESOLVE_DIVING_CATCH` se akce **přehraje** se zapsanými hody (`ReplayDiceRoller`) a frontou odpovědí;
  další hody jsou nové. Jedno místo (`ActionResolver::resolve`) pro všechny akce.
- Nové: `src/Engine/ReplayDiceRoller.php`, `src/Engine/DivingCatchQuestion.php` (výjimka), `src/DTO/PendingDivingCatchDTO.php`,
  `ActionType::RESOLVE_DIVING_CATCH`. Rozhodnutí na jednom místě: `BallResolver::divingCatchJumps($diver, $landing, $aiJumps)`
  — volá ho `resolveLanding` (přihrávka, Hail Mary, Dump-Off, vhazování) i `KickoffResolver` (AI: kopající na lajně neskočí).
- Strany kouče-člověka: `GameOrchestrator` a `AITurnService` předají `$lide` (jako Stand Firm P226) ⇒ ptá se i v tahu AI,
  tah AI se zastaví; `RulesEngine`: s otázkou jen `RESOLVE_DIVING_CATCH` (`use` bool), přednost před vším
  (stav před akcí může nést jiné čekající volby, ze kterých akce vyšla).
- Testy `tests/Engine/DivingCatchKoucTest.php` (14): otázka přijde (+ stav beze změny, jen odpověď povolena), ano = chytá,
  ne = bez pokusu a odraz, tah soupeře, druhá strana (AI) se neptá a skočí, 2 kandidáti ⇒ nic, odrážející se míč ⇒ nic,
  vhazování ptá/ne/ano, výkop kopajícího člověka ptá/ano=touchback/ne+AI beze změny, orchestrátor zastaví tah AI a po odpovědi
  ho dohraje, otázka přežije uložení stavu (toArray/fromArray).
  - Na starém kódu: 13 chyb + 1 selhání (třídy/akce neexistují).
  - Mutace `divingCatchJumps` = vždy AI (stará mechanika, nová infrastruktura): **10 z 14 spadne**; projdou 4 strážné
    (AI beze změny, 2 kandidáti, odraz, výkop „ne“/AI) — ty mají projít i na staré mechanice.
  - Mutace přehrání (zapsané hody se nevrátí): spadnou 4 (ano/ne/uložení/orchestrátor).
- Commit části (1): `590893aa` (make check zelená: PHPUnit 1564, C++ 1297, kouř 6 her), pushnuto.
- Frontend: `frontend/src/ui/DivingCatchModal.ts` (+ `DivingCatchModal.test.ts`, 3 testy vitest), typ `PendingDivingCatch`,
  `match.ts`: dialog má přednost před ostatními (stav před akcí může nést jiné čekající volby), i po obnovení stránky;
  odpověď `resolve_diving_catch` `{use}`.

### (2) Diving Catch jako otázka kouči — C++
- `CoachQuestion::USE_DIVING_CATCH = 16` (na konec výčtu; playerId = hráč, target = pole dopadu x*100+y, odpovědi ne/ano).
- Rozhodnutí na jednom místě: `divingCatchChosen(state, diver, at, aiJumps)` (ball_handler.cpp) — v živé partii
  `askCoach` pro kouče hráče (i v tahu soupeře), jinak `aiJumps`. Volá ho `resolveLanding` (přihrávka, Hail Mary,
  vhazování; AI skočí) a `landBall` u výkopu (AI: kopající v kopající polovině neskočí — beze změny).
- Testy: `DivingCatchCoach.*` (8, test_diving_catch.cpp), `KickoffLanding.LiveGameAsksTheKickingDivingCatchersCoach`,
  `KickingDiverAnsweringYesDivesIntoATouchback`, `KickingDiverAnsweringNoLetsTheBallLand`.
  Na starém kódu (jen s hodnotou výčtu) SPADLO 6: otázka přijde (přihrávka, tah soupeře, vhazování, výkop), ne = bez pokusu,
  výkop ano = touchback. Projde 5 strážných (ano = chytá, druhá strana se neptá a skočí, 2 kandidáti, odraz, výkop ne).
  Mutace `divingCatchChosen` = vždy AI ⇒ týchž 6 spadne. ✔
- Python: jméno 16 ve vazbě (`bb_module.cpp`), `bb_play_session_20261012.py` — vlastní řádek otázky + `odp ano|ne`.
  Testy `test_diving_catch_otazka_a_odpoved`, `test_diving_catch_otazka_prijde_z_vazby_jako_use_diving_catch`
  (Wood Elf TV1500 Catcher, přihrávka soupeře — otázka i v cizím tahu). Mutace: jméno 16 ve vazbě chybí ⇒ test spadne. ✔
  `python/tests/test_coach_choice.py`: 18 passed.
- Ověření výpisu na skutečné otázce z enginu (tytéž funkce jako server sezení; celý server jsem nespouštěl — žádná
  předvolba partie nemá hráče s Diving Catch):
  ```
     (zatím) PASS hráč 1: potřeba 4+, padlo 4 -> OK
  ❓ DIVING CATCH hráč 20: míč dopadá na (10,7) vedle něj — skočit (chytá ze svého pole, neúspěch odskočí od něj)?  ->  odp ano / ne (ne = míč dopadne a odrazí se)
  odp ano -> … SKILL DivingCatch hráč 20 … CATCH hráč 20 … | míč u 20
  odp ne  -> … ODRAZ míče (10,7) -> (10,6) … | míč u (10, 6)
  ```
- PHPStan napoprvé: 6 chybějících typů v novém testu (`@param list<int>` ap.) — doplněno.

## Otevřené body / návrhy pro uživatele (NEPROGRAMOVÁNO)
1. **Grab × Side Step proti spoluhráči** — `pushSkills` / `grabSquares` dál ruší Side Step cíle, má-li útočník Grab
   (`hasGrab`), i když Grab proti spoluhráči nepůsobí. Ř. 8151-8153 ruší i v blitzu, kde Grab sám nepůsobí, takže
   doslovné čtení „ruší se vždy“ obstojí; jen upozorňuji, rozhodnutí je na uživateli.
2. **PHP přehrání akce po otázce Diving Catch** spoléhá na to, že `ActionResolver` sám staví `BallResolver`/`KickoffResolver`
   (kostky jdou přes `ReplayDiceRoller`). Varianta z kontejneru (`ServiceProvider`, předané resolvery s vlastními
   kostkami) by přehrála špatně — dnes ji nikdo nepoužívá (grep `get(ActionResolver` prázdný), komentář v konstruktoru.
3. **Události před otázkou ve webu**: PHP vrací stav PŘED akcí bez událostí (akce se po odpovědi přehraje a události
   přijdou celé). Dialog proto říká jen „míč dopadá na (x,y)“; hod přihrávky kouč uvidí až po odpovědi. C++ partie
   události „(zatím)“ ukazuje. Pokud to vadí, šlo by je vracet z přerušeného pokusu — návrh, nedělal jsem.
4. Celý server živé partie (`bb_play_session`) jsem nespouštěl — žádná předvolba partie nemá hráče s Diving Catch;
   výpis a `odp` ověřeny na skutečné otázce z enginu přes tytéž funkce (viz výš). e2e v prohlížeči (`make check-all`) nespuštěno.
5. Prostředí worktree: `make check` padal na PostgreSQL (port) — chyběl `.env`; zkopírován z main (`.env` je v `.gitignore`).

## Návrh řádku do knihy (P235)
| P235 | Noc 30.10. C: (1) Ball & Chain Grab jen proti soupeři (ř. 8146-8150 „his opponent“; uživatel 30.10.) — C++ `pushSkills`, PHP `grabSquares`, oba enginy; (2) Diving Catch jako otázka kouči (ř. 8065 „can attempt“) — C++ `USE_DIVING_CATCH = 16` + vazba + živá partie (`odp ano/ne`), PHP `PendingDivingCatchDTO` s přehráním akce (`ReplayDiceRoller`) + dialog ve webu; AI beze změny. Oba enginy hotové ⇒ převod netřeba. Otevřené: Grab×Side Step proti spoluhráči (bod 1), události před otázkou ve webu (bod 3). | hotovo (větev oprava-grab-dc), čeká recenze |

## Kontrola a commity
- `make check` (části 2) ⇒ **„== check: vše zelené“** (PHPUnit 1578 OK, vitest 98, C++ 1308 PASSED, kouř 6 her; log mimo git `/tmp/c_scratch/check2.log`).
- Python `python/tests/test_coach_choice.py`: 18 passed (`check-py` celé nespuštěno).
- Commity: `590893aa` (část 1), další commit části 2 + tento zápis; větev pushnuta na origin/oprava-grab-dc.

HOTOVO
