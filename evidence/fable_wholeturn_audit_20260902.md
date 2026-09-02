# AUDIT „CELOTAH" — jak engine skládá CELÝ TAH (02.09.2026)

Zadání: sekce `CELOTAH` z `evidence/task_queue.md` (řádek 265).
Spouštěč (uživatel 01.09.): *„blitz na souseda a pak útěk je dobrý nápad"* — BB2016
**ř. 550-552**: *„The player may carry on moving after the effects of the block have
been worked out if he has any squares of movement left."*
Hodnota té akce se **neměří ranou, ale zbytkem tahu** — a plánovač oceňuje jednotlivě.

⚠️ **Tenhle soubor je PODKLAD, ne verdikt.** Je psaný tak, aby se do něj dalo
přidávat: nálezy jsou číslované `W1…`, měřicí zadání `WM1…`, a poslední sekce říká,
kam se má sáhnout příště. **Nic se neopravovalo, jen četlo.**

⛔ Metodická poznámka k celému souboru: **kde není citace `soubor:řádek`, tvrzení není.**
Kde kód sám v komentáři říká, že je něco záměr nebo zjednodušení, je komentář citován
doslova a nálezový sloupec „vysvětleno?" to nese.

---

## 0. Slovník (aby se čtyři vrstvy nepletly)

| vrstva | co je jednotka | kde |
|---|---|---|
| **atomická akce** `Action` | jeden krok / jeden blok / jeden hod | `rules_engine.cpp:8` nabízí, `action_resolver.cpp:483` provádí |
| **makro** `Macro` | „záměr" — SCORE, ADVANCE, BLITZ, CAGE… | `macro_actions.h:30-41`, nabídka `macro_actions.cpp:783` |
| **expandér** | makro → **seznam atomických akcí**, provedený na KLONU | `macro_actions.cpp:2706` `greedyExpandMacro` |
| **hledání** | vybere JEDNO makro | `macro_mcts.cpp:162` `search()` |
| **plán tahu** | ⛔ **neexistuje jako objekt**, kromě dvou default-OFF plánovačů | `turn_planner.cpp`, `cage_advance.cpp` |

---

## 1. MAPA — od „je můj tah" k „provedla se akce"

### 1.1 Vnější smyčka: engine se ptá po JEDNÉ AKCI, nikdy po tahu

`game_simulator.cpp:677-738` (a totéž v logující variantě `:855-930`):

```
while (phase != GAME_OVER && totalActions < MAX_ACTIONS) {     // MAX_ACTIONS = 5000, :654
    getAvailableActions(state, actions);                        // :719
    if (actions.empty()) { END_TURN; continue; }                // :721-728
    Action chosen = policy(state);                              // :733
    executeAction(state, chosen, dice, nullptr);                // :736
}
```

⭐ **Tady je první a nejdůležitější fakt celé sekce: smyčka nemá pojem „tah".**
Kolo skončí jedině tak, že (a) politika vrátí `END_TURN`, (b) `getAvailableActions`
vrátí prázdno, nebo (c) padne turnover (`action_resolver.cpp:503-507`).
BB2016 ř. 363-364 přitom říká: *„Normally, a turn only ends when all of the players
in the team have performed an Action."* — konec kola je tedy podle pravidel
**vyčerpání těl**, u nás je to **rozhodnutí politiky, nebo prázdná expanze** (viz W7).

### 1.2 Politika: `MacroMCTSPolicy::operator()` — `macro_mcts.cpp:1077-1203`

```
1079-1095  je ještě nedohraný plán z minula? ověř akci proti getAvailableActions,
           když sedí, vrať ji a INKREMENTUJ planIndex_  ← žádné nové hledání
1102-1117  zapiš TurnPlanRecord (goal), i když žádný plánovač neběží
1122-1126  fromStagedPlan = (stagedPlanner_ || cagePlanner_) && nextStagedMacro(...)
           ⇒ oba jsou v produkci VYPNUTÉ (mcts.h:29-30) ⇒ vždy search_.search(state)
1161-1162  planState = state.clone();
           expansion = greedyExpandMacro(planState, bestMacro, expansionDice_)
1177-1178  currentPlan_ = expansion.actions;  planIndex_ = 0
1180-1183  if (currentPlan_.empty()) return END_TURN;      ← ⛔ VIZ W7
1186-1195  ověř PRVNÍ akci proti getAvailableActions, vrať ji
1198-1202  jinak zahoď plán a vrať greedyPolicy(state, expansionDice_)
```

⭐ **Plán tahu tedy JE, ale je krátký a je to plán JEDNOHO MAKRA.** `currentPlan_` je
seznam atomických akcí jednoho makra, spočítaný na **klonu** a **jinými kostkami**
(`expansionDice_`, `macro_mcts.h:118`) než jakými se pak hraje (`dice` v simulátoru).

### 1.3 Hledání: `MacroMCTSSearch::search` — `macro_mcts.cpp:162-294`

```
163-164  getAvailableMacros(state, macros, dauntlessInOffer)
177      MacroMCTSNode root;                       ← ⛔ LOKÁLNÍ, žádné znovupoužití stromu
182      expand(&root, state)                      ← priory, :387-661
215-270  iterace: select → replayToNode → expand → simulate → backpropagate
282      best = root.mostVisitedChild()            ← vrací se JEDNO makro
287-289  if (config_.riskDeferral) pick = applyRiskDeferral(...)   ← default OFF
```

⭐⭐ **Strom UMÍ sekvenci maker.** `expand()` (`:387-402`) generuje děti z **stavu po
předchozím makru**, `select()` (`:376-385`) sestupuje, `replayToNode()` (`:949-978`)
celou cestu přehraje. Uzel navíc ví, čí je to rozhodnutí (`actingTeam`, `:392`), takže
strom střídá i soupeře. **Tohle je jediné místo v enginu, kde se pořadí aktivací
vůbec zvažuje.**

⛔⛔ **A pak se to zahodí.** `root` je lokální proměnná (`:177`) — po každé aktivaci
se strom staví od nuly. Vrátí se `mostVisitedChild` (`:282`), tedy **první makro**;
zbytek nalezené sekvence — ta „představa, jak dopadne celý tah" — se **nikam nezapíše**.
Není tu `TurnPlan`, do kterého by se uložil, ani reuse podstromu.

### 1.4 Provedení: `executeAction` — `action_resolver.cpp:483-528`

```
492-499  uzávěrka AKTIVACE: když začne jednat JINÝ hráč a ten předchozí měl hasMoved,
         dostane hasActed = true   (oprava free-blitz bugu 15.07.)
501      resolveAction(...)  → resolveActionInner (:224)
504-507  turnover ⇒ turnoverPending + resolveEndTurn(wasTurnover=true)
510-516  touchdown
519-525  konec půle
```

⭐ **`state.currentActivationId` (`:493-498`) je JEDINÝ pojem „aktivace" v enginu.**
Není to plán, je to uzávěrka: „přišel jiný hráč ⇒ ten předchozí už dohrál."

### 1.5 Shrnutí mapy — jedna cesta, jedno místo, kde se dívá dopředu

```
game_simulator  ──jednu akci──▶  MacroMCTSPolicy::operator()
                                   │
                   ┌───────────────┴───────────────┐
                   │ currentPlan_ nedohraný?       │ ano ⇒ vrať další akci (bez hledání)
                   └───────────────┬───────────────┘
                                   │ ne
                          search_.search(state)   ← strom sekvencí maker, JEDINÝ výhled
                                   │  vrátí 1 makro, strom zahodí
                          greedyExpandMacro(KLON, expansionDice_)
                                   │  seznam atomických akcí
                          vrať první; ostatní čekají v currentPlan_
                                   ▼
                          executeAction(REÁLNÝ stav, REÁLNÉ kostky)
```

---

## 2. NÁLEZY

| # | co | doklad (`soubor:řádek`) | v komentáři vysvětlené? | odhad dopadu |
|---|---|---|---|---|
| **W1** | **Strom sekvencí se po každé aktivaci zahazuje.** `root` je lokální, žádný reuse podstromu, žádné uložení nalezené sekvence. Engine tedy „plán celého tahu" v každé aktivaci **postaví a zahodí**, a příští aktivace ho staví znovu z jiných kostek. | `macro_mcts.cpp:177` (lokální `root`), `:282` (vrací se jen `mostVisitedChild`), `:162-164` (nová `getAvailableMacros` při každém volání) | **NE** — nikde není napsáno, že je to záměr ani že je to zjednodušení | **VELKÝ pro celotah.** Je to přesně ta chybějící vrstva: výhled existuje, ale nemá kam se uložit |
| **W2** | **Rozpočet hledání je 100 iterací.** Při ~6-25 kandidátech v kořeni (engine sám odhaduje `n<=12` / `n>=13` jako „malý/velký obranný uzel") většina dětí nedostane ani jednu návštěvu, takže strom je prakticky **jednoplýtvý** a „sekvence" z W1 je z velké části teorie. | `run_iteration.py:31` (`MCTS_ITERATIONS = 100`, přepínatelné `BB_MCTS` na `:67`), **`diag_utils.py:123`** (`mcts_iterations: int = 100` — default `run_arm`, tedy rozpočet, na kterém běží noční A/B), `macro_mcts.cpp:594-595` a `:604-608` (odhad `n`), `mcts.h:16` (default 100000 — v produkci se nepoužije) | částečně: komentáře u priorů mluví o „starvation of visits", tedy o tom, že rozpočet je úzký; NIKDE není, že by hloubka byla záměrně 1 | **VELKÝ** — kdyby to platilo, `W1` je akademická vada a skutečná vada je hloubka. **Rozhoduje měření `WM1`** |
| **W3** | **BLITZ makro NENESE, kdo blitzuje.** Nabídka emituje `{BLITZ, playerId = -1, targetId}` — vybírá se jen CÍL. Blitzující se volí až v expandéru. | `macro_actions.cpp:1245` (`out.push_back({MacroType::BLITZ, -1, candidates[i].targetId, {-1,-1}})`), výběr v `:2329-2342` | **ANO, částečně**: `:1160-1164` komentář „Score each target (best blitzer for each)" — ale že se identita nepředává, nikde | **STŘEDNÍ-VELKÝ**: uživatelovo *„cíl blitzu se rozhoduje PRVNÍ"* je v enginu **napůl splněné** (cíl je opravdu první a samostatný), ale druhá půlka — KDO a ODKUD — se rozhoduje mimo ocenění |
| **W4** | **Nabídka a expandér blitzu měří DVĚ RŮZNÉ VĚCI.** Nabídka: `score = dice*2 + sideline/carrier/threat bonusy`, **maximum přes blitzující**, a pak se **ořízne na top-1 (útok) / top-2 (obrana)**. Expandér: **minimum** `estimateBlitzFailChance` = blok × doběh. Cíl, který nabídka zahodila, expandér nikdy neuvidí. | nabídka `macro_actions.cpp:1190-1227` + ořez `:1241-1246`; expandér `:2329-2342`; `estimateBlitzFailChance` `:714-761` | **NE** pro rozpor; `:2332-2335` vysvětluje jen VOLBU uvnitř expandéru (item 14) | **STŘEDNÍ**: nesouhlas se projeví v ADMISI (co se nenabídne, se neocení), ne v Q — Q se počítá z reálné expanze |
| **W5** | **Blitz se týmu odečte PŘED chůzí.** `blitzUsedThisTurn = true` na `:332` je nad smyčkou doběhu; každá z pěti cest `noteBlitzWasted(0..5)` pak vrátí `fail()` — **tým už blitz nemá**. | `action_resolver.cpp:332-333` vs. `:373,379,384,391,392,395,403,409` | **ANO, a bohatě**: `:89-102` vyjmenovává rozpad příčin a `:104-111` říká, že „P37b jsem ráno opravil v NABÍDCE a nezabralo to" ⇒ je to **známý, měřený, neuzavřený** stav | **VELKÝ pro vzácný zdroj**: engine sám změřil 9,3 % blitzů ze stoje bez rány (`:90-91`) |
| **W6** | **Neúspěšný blitz NEZAVŘE aktivaci.** Cesty `fail()` z doběhu nikde nestaví `hasActed`. `usedBlitz` ale zůstává `true`, takže výjimka v nabídce BLOCKu (`if (p.hasMoved && !p.usedBlitz) break;`) hráče **pustí k obyčejnému bloku** — tedy k druhé deklarované akci v téže aktivaci. | `action_resolver.cpp:373-411` (žádné `hasActed`), `rules_engine.cpp:99-113` (zejména `:107`), makro nabídka `macro_actions.cpp:1310-1311` | **ANO pro záměr, NE pro důsledek**: `rules_engine.cpp:101-107` výslovně vysvětluje, proč výjimka existuje (`expandBlitzAndScore` musí dojít a teprve pak najít BLOCK); že táž díra pustí i **neúspěšný** blitz, tam není | **STŘEDNÍ, nutno změřit** (`WM4`). Kontrast: cesta „ránu není z čeho zaplatit" `hasActed` **staví** (`block_handler.cpp:538`) |
| **W7** | ⛔⛔ **PRÁZDNÁ EXPANZE ZAHODÍ CELÝ ZBYTEK KOLA.** Když vybrané makro nevyrobí ani jednu akci, politika vrátí `END_TURN` — a to je konec kola celého týmu, ne přeskočení makra. Záchranná větev existuje **jen pro staged plán** (`:1164-1175`), tedy pro default-OFF plánovače. | `macro_mcts.cpp:1180-1183`; fallback jen pro staged `:1164-1175` | **ANO, doslova, a na jiném místě**: `macro_actions.cpp:2546-2550` — *„expanze by vrátila prázdno a MacroMCTSPolicy z toho udělá END_TURN — tedy zahodí zbytek kola CELÉHO týmu"* (napsáno 21.08. u vstávacího makra) | **VELKÝ**, viz W8: cest k prázdné expanzi je jedenáct |
| **W8** | **Jedenáct cest k prázdné expanzi.** `expandAdvance` rezignace · `expandBlitz` nenašel · `expandBlock` neshoda · `expandFoul` neshoda · `expandPass` neshoda · `expandScore` nosič mimo hřiště · `expandCage` nikdo volný · `expandReposition` první krok selhal · `expandHandOffScore` / `expandPassScore` / `expandChainScore` časná návratnost | `:2244-2264`, `:2344`, `:2451`, `:2521`, `:2504`, `:1940`, `:2298-2299`, `:2554` (přes `movePlayerToward` `:1874,1879,1885`), `:2572-2586`, `:2621-2636`, `:2655-2669` | jednotlivé návraty ano, **jejich SPOLEČNÝ důsledek nikde** | **VELKÝ**. `expandAdvance` má vlastní čítač rezignací (`g_advanceResigned`, `:345,2249`) a engine u něj sám zapsal *„z 9202 rezignací jich 8294 = 90,1 % mělo volné pole"* (`:2210-2211`) |
| **W9** | ⭐ **Hledání tuhle cenu NEVIDÍ.** Uvnitř `replayToNode` se prázdná expanze projeví jako „stav se nezměnil" a list se ocení, jako by se nic nestalo (`complete` zůstane `true`). Tedy: **v hledání je no-op zdarma, v produkci stojí zbytek kola.** | `macro_mcts.cpp:964-977` (`replayToNode` nekontroluje `result.actions.empty()`), `:257` (`simulate(sim, …)` na nezměněném stavu) vs. `:1180-1183` | **NE** | **VELKÝ** — je to učebnicový rozchod ocenění a provedení, a jde ho odstranit bez změny doktríny |
| **W10** | **Vzácnost zdrojů umíme spočítat — ale jen SOUPEŘOVU.** `worstReplyCost` má explicitní váhy `blok 1,00 · blitz 0,45 · faul 0,30` a komentář, proč: *„blok muze souper hodit KAZDYM sousedem a nic ho to nestoji; blitz jen JEDNOU za kolo."* **Pro vlastní zdroje takový výpočet neexistuje nikde.** | `macro_actions.cpp:621-626` (váhy), `:629-662` (funkce), volání jen z Q3 vstávacího ramene `:878, :930` | **ANO pro to, co dělá** (`:580-596` celý blok doktríny); **NE pro to, že se nepoužívá na nás | **VELKÝ pro celotah**: nástroj je hotový, chybí druhá strana rovnice |
| **W11** | **Vlastní rozpočty jsou jen ZÁVORY, ne ocenění.** `blitzUsedThisTurn` / `passUsedThisTurn` / `handOffUsedThisTurn` / `foulUsedThisTurn` se čtou výhradně jako „nabídnout / nenabídnout". Žádné „tenhle blitz si schovám". | `macro_actions.cpp:1015, 1041, 1081-1082, 1155, 1270, 1456, 1483, 1563`; nastavení `action_resolver.cpp:27-37, :332`, `pass_handler.cpp:158,395`, `foul_handler.cpp:129`, `ttm_handler.cpp:16` | **NE** | **VELKÝ** |
| **W12** | **Listová heuristika nemá ani jeden člen o nespotřebovaných zdrojích.** Celých 210 řádků `simulate()` — skóre, míč, vzdálenost k endzóně, tempo, klec, marking, sideline, počet těl — a **ani jeden term o tom, že máme/nemáme blitz, pass, hand-off nebo faul**. | `macro_mcts.cpp:729-939` (celá funkce) | **NE** | **VELKÝ.** Doplněk: síťové featury `f27` (blitz volný) a `f28` (pass volný) existují (`feature_extractor.cpp:544-545`), takže **VF to vidět může, ruční heuristika ne** — a produkce jede s `vfBlend` z konfigurace. **Pro faul a hand-off featura neexistuje vůbec** |
| **W13** | **FOUL má strop prioru jen v obraně.** `if (onDef) maxPrior = 0.08f;` — v útoku propadne do `default:` a drží plný uniformní `1/n`, tedy až ~0,17 při řídkém uzlu. A faul je přitom **1×/kolo** (BB2016 ř. 359-361). | `macro_mcts.cpp:598-609` | **ANO a přiznaně**: *„Deliberately onDef-gated: loose-ball FOUL overuse is item 7 territory, not rebalanced here"* | **STŘEDNÍ** — je to napsané jako vědomý odklad, ne jako správný stav |
| **W14** | **Pořadí aktivací nikdo neřídí; role se přerozdělují při KAŽDÉ nabídce.** Obranné a útočné role (`hunterPlaced`, `receiverPlaced`, `safetyPlaced`, `markerPlaced`, `cageTagPlaced`, `interceptPlaced`, `endzoneGuardCount`, `screenSlot`) jsou **lokální proměnné jednoho volání** `getAvailableMacros`. Přiděluje je pořadí `forEachOnPitch`, tedy **pořadí ID hráčů** (`game_state.h:93-104` → `forEachPlayer` `:79-91`). Při další aktivaci se počítají od nuly. | `macro_actions.cpp:1585-1593` (deklarace), `:1666-1680` hunter, `:1683-1690` receiver, `:1706-1733` cage tag, `:1745-1765` intercept, `:1768-1794` safety/marker/guard/screen | **NE** — komentáře popisují, co role dělá, ale ne že je jednorázová a závislá na ID | **VELKÝ pro celotah**: „safety" může být každou aktivaci jiné tělo, a nikdo nedrží, že už jedno safety stojí |
| **W15** | **Explicitní pojem „nejdřív tenhle, protože pak potřebuju tamtoho" v enginu JE — třikrát, a všechny tři jsou default OFF.** (a) `applyRiskDeferral` = risk-LAST; (b) `CageAdvancePlanner` = závislostní setřídění „kdo stojí na cizím cílovém poli, jde první" + „nosič poslední"; (c) `StagedTurnPlanner` = bezpečné zálohy → pickup → doplnění klece, s **kontrolou koridoru sběrače** (záloha nesmí zazdít sběrači cestu k míči). | (a) `macro_mcts.cpp:287-289, 340-374`, vypnuto `mcts.h:28`; (b) `cage_advance.cpp:695-742`, vypnuto `mcts.h:30`; (c) `turn_planner.cpp:233-333` + koridor `:305-326` + doplnění klece `:367-426`, vypnuto `mcts.h:29` | **ANO, všechny tři velmi podrobně** (např. `cage_advance.cpp:695-700`: *„Base order: front movers, back movers, carrier last (risk-last…). Execution order is then SITUATIONAL: whoever stands on another mover's target square goes first"*) | **VELKÝ pro plánování dalšího kroku**: **doktrína pořadí je napsaná a otestovaná, jen se v produkci nikdy nezapne.** To je jiná situace než „chybí to" |
| **W16** | **Pět maker je fakticky VÍCEAKTIVAČNÍCH — a hodnotí se jako JEDEN uzel.** `CAGE` pohne až 4 různými hráči; `BLITZ_AND_SCORE` blitzujícím + nosičem; `HAND_OFF_SCORE` a `PASS_SCORE` nosičem + příjemcem; `CHAIN_SCORE` třemi. Vnitřní pořadí je pevně zadrátované. | `:2270-2306` (CAGE, `findNearestFreePlayer` pro každý roh), `:2350-2435`, `:2567-2614`, `:2616-2647`, `:2649-2704` | **NE jako třída**; jednotlivá makra ano | **STŘEDNÍ**: existující „mini-celotahy" jsou vzor i past — jejich pořadí není odvozené, je napsané |
| **W17** | **Klecové kritérium UMÍ počítat s cizími aktivacemi — ale nekontroluje, jestli ta těla ještě mohou jednat.** `cageScoreForSquare` hledá čtyři těla, která dosáhnou na rohy (`d > b.movementRemaining` je jediná zábrana), **bez testu `hasActed` / `hasMoved`**. Hráč, který už v tomhle kole blokoval (a `movementRemaining` mu blok nesnížil), se počítá jako volný roh. | `macro_actions.cpp:2094-2113`, zejména `:2101-2107` | **ANO pro zjednodušení** (*„Greedy is enough for 4x N and stays cheap"*), **NE pro chybějící test aktivace** | **STŘEDNÍ**, a je to živé jen pod ramenem P38/P40 (`:2141`) |
| **W18** | **Blitzová kontinuace (uživatelův spouštěč) v enginu JE — ale jen jako ÚSTUP, jen pod ramenem, a nosič je z ní vyloučený.** Rameno drží aktivaci otevřenou (`endBlockActivation`), umí odmítnout follow-up, když by vedl do víc tacklezón (`wantsFollowUp`), a nabídne jediné makro: `REPOSITION` na **nejbližší pole bez cizí tacklezóny**. | rameno `macro_actions.cpp:444-461`; nabídka `:971-1002`; `block_handler.cpp:404-413` a `:415-456`; vypínač je per-strana, produkce ho nestaví | ⭐ **ANO, a přesně o tom, co uživatel včera řekl**: `macro_actions.cpp:965-968` — *„SCOPE — this is the RETREAT only. The user's other case, 'the carrier opens his own lane with a blitz and runs through it', needs SCORE/ADVANCE to accept a mid-activation player and is a bigger change; the carrier is skipped here"*; a `:969-970` — *„No GFI: a retreat bought with a Go For It is a gamble, not hygiene"* | **VELKÝ**: chybí právě ta polovina, kterou uživatel jmenoval. Navíc cíl je **geometrický** („nejblíž a bez TZ"), ne **účelový** („tam, kde jsem potřeba") ⇒ i zapnuté rameno by tu hodnotu neumělo ocenit |
| **W19** | **Ocenění blitzu končí ranou.** `estimateBlitzFailChance` = 1 − (1−`blockFail`)(1−`approachFail`). **Žádný člen za to, co se stane po ráně** — ani za zmizelou tacklezónu, ani za zbytek pohybu, ani za follow-up. Přitom `estimateApproachFailChance` si už **rezervuje pole na ránu** (`moveLeft = movementAfterStandUp(mover) - 1`), takže „kolik zbude" je na dosah ruky. | `:714-761` (zejména `:758-760`), rezerva `:681` | **NE** | ⭐⭐ **Přesně spouštěč tohohle auditu.** Hodnota „srazím souseda ⇒ odejdu bez dodge" **nemá v žádném vzorci člen** |
| **W20** | **Nosičovo tempo se počítá na CELÝ TAH, ale bez ohledu na to, kdo z týmu už hrál.** `carrierStallAwareSteps` si nechává polovinu MA v rezervě, *„so teammates still have a decision window to form a cage around them"* — jenže **nezjišťuje, kolik spoluhráčů ještě aktivaci má**. Když nosič jde poslední, rezerva nekoupí nic. | `:2025-2050`, zejména `:2039-2042` | **ANO pro záměr** (`:2025-2029`), **NE pro to, že se počet volných těl nečte** | **STŘEDNÍ-VELKÝ**: je to jediné místo, kde engine explicitně **šetří zdroj na pozdější aktivace**, a šetří ho naslepo |
| **W21** | **Ocenění a provedení běží na JINÝCH KOSTKÁCH.** Hledání expanduje makra `dice_` (`macro_mcts.cpp:160`), politika pak totéž makro expanduje znovu `expansionDice_` na klonu (`:983, :1162`), a **výsledné akce se hrají třetími kostkami** — simulátorovými (`game_simulator.cpp:736`). Sonda plánovačů má čtvrtý proud (`turn_planner.cpp:86`). | `macro_mcts.cpp:160, 983, 1161-1162`; `game_simulator.cpp:733-736`; `turn_planner.cpp:86` | **ANO pro hledání** (*„open-loop: fresh dice each replay"*, `:219, :959`), **NE pro dvojí expanzi téhož makra** | **STŘEDNÍ**: u dice-free maker (REPOSITION) neškodí; u CAGE/BLITZ_AND_SCORE znamená, že **konkrétní posloupnost kroků je jeden vzorek, ne plán** |
| **W22** | **Ověřování plánu je EXAKTNÍ SHODA, takže drobná odchylka zahodí celý zbytek makra.** `currentPlan_[planIndex_]` se hledá v `getAvailableActions` na rovnost typu, hráče, cíle i pozice. Neshoda ⇒ `currentPlan_.clear()` a nové hledání. | `macro_mcts.cpp:1082-1095` (a totéž pro první akci `:1186-1195`) | částečně: `:1092` *„Plan invalidated — search again"* | **MALÝ-STŘEDNÍ**: chová se bezpečně (přeplánuje), ale u víceaktivačních maker (W16) zahodí i tu část, která ještě platí |
| **W23** | **`TurnPlanRecord` už existuje a zapisuje se i tam, kde žádný plánovač neběží.** Nese `goal`, `turnsLeft`, `distToEndzone`, `adopted`. Je to **jediná stopa po „záměru kola"** v celém enginu. | `macro_mcts.cpp:1016-1021` a `:1102-1117`, `turn_plan_record.cpp` | **ANO a s důvodem**: *„'search took the whole turn' is itself the answer we keep failing to have"* (`:1016-1017`) | **užitečné** — je to hotový háček, na který se dá plán tahu pověsit |
| **W24** | **Cíl kola zná jen tři hodnoty a obranu neřeší vůbec.** `classifyTurnGoal` vrací `PICKUP_BALL` / `SCORE_BALL` / `ADVANCE_BALL` / `NONE`, a když má míč soupeř, vrátí `NONE`. | `turn_planner.cpp:18-43`, zejména `:25-27` | ⭐ **ANO doslova**: *„return TurnGoal::NONE; // opponent's ball — defensive goals are out of MVP scope"* | **VELKÝ pro celotah**: celotah v obraně nemá ani JMÉNO cíle, natož plán |

---

## 3. SEZNAM: kde se OCENĚNÍ a PROVEDENÍ rozcházejí

⚠️ Nejdřív rozlišení, bez kterého je celý seznam k ničemu:
**Q hodnota v MCTS se počítá z REÁLNÉ expanze** (`macro_mcts.cpp:250, 970`), takže
u většiny maker se ocenění a provedení **rozejít nemůže** — hodnotí se to, co se stalo.
Rozchod proto bije jen na čtyřech přesně určených místech.

### R1 — ADMISE: co se nenabídne, se nikdy neocení
* **BLITZ: cíle se ořežou na top-1 (útok) / top-2 (obrana)** metrikou, kterou expandér
  nepoužívá — `macro_actions.cpp:1190-1227` (skóre) a `:1241-1246` (ořez) proti
  `:2336` (`estimateBlitzFailChance`).
* **BLOCK: jednokostkový blok se nabídne jen nositeli `Block`** — `:1337-1341`.
* **PASS/HAND_OFF: prahy `complete >= 0.5` a „swap gate"** — `:1484-1555`.
* **BLITZ_AND_SCORE: dosah bez rezervy** (T5.35a) — `:1252-1279`.
* **PICKUP: nejvýš dva sběrači, druhý jen do rozdílu 25 bodů** — `:1360-1363, :1444-1447`.

### R2 — PRIORY: rozdělují návštěvy podle TYPU makra, ne podle jeho následku
`macro_mcts.cpp:471-627`. Priorita se počítá z `macros[i].type` (+ jedna výjimka:
`targetId == oppCarrierId`, `:541, :554`). **Nic v prioru neví, co makro se stavem udělá.**
Při rozpočtu 100 iterací (W2) prior o výsledku fakticky rozhoduje — a to je vada,
kterou komentáře u `ADVANCE` (`:505-521`), `BLITZ` (`:523-537`) a `CAGE` (`:559-572`)
samy popisují jako **starvation**: makro s nejvyšším jednoplýtvým Q může být nevybráno.

### R3 — PRÁZDNÁ EXPANZE: v hledání zdarma, v produkci konec kola
`macro_mcts.cpp:964-977` (replay neřeší prázdno) proti `:1180-1183` (`END_TURN`).
**Toto je jediný rozchod v seznamu, kde se ocenění a provedení liší v ZNAMÉNKU**, ne
v přesnosti. Viz W7, W8, W9.

### R4 — DVOJÍ EXPANZE TÉHOŽ MAKRA JINÝMI KOSTKAMI
Makro se ocení expanzí uvnitř hledání (`dice_`), a pak se **znovu expanduje**
(`expansionDice_`, `:1162`) a teprve ta druhá posloupnost se hraje. U víceaktivačních
maker (W16) tedy platí: **ocenil se jeden průběh, hraje se jiný.**

### R5 — dědictví, doložené jinde a stále platné
* **Dosah `MA + 2` napevno, tedy bez `Sprint` a bez `rooted`** — zbytek nálezu N15
  z `fable_movement_parity_20260824.md`. ⭐ **V `macro_actions.cpp` je to už OPRAVENÉ**
  (`:1007, 1017, 1032, 1043, 1060, 1084, 1134, 1274` volají `maxGfiSquares`,
  `helpers.cpp:60-63`), ale **v ostatních vrstvách přežívá**:
  `turn_planner.cpp:34, 39, 77` (klasifikace cíle kola a platnost staged pickupu),
  `macro_mcts.cpp:812, 820` (hand-off bonus v listové heuristice),
  `macro_actions.cpp:1208, 1607` (odhad SOUPEŘOVY skórující hrozby).
  ⚠️ Tři z těch pěti míst počítají **dosah soupeře**, kde `+2` naopak SOUPEŘE
  podceňuje — a to je jiný nález než původní N15.
* **`cageScoreForSquare` nekontroluje `hasActed`** (W17).
* **`carrierStallAwareSteps` nečte počet volných těl** (W20).

---

## 4. CO NEJDE ROZHODNOUT ČTENÍM (zadání na měření, ne závěry)

⛔ U každého je napsané, **co by výsledek rozhodl** — brzda bez čísla úkolu je slepá kolej
(`feedback_brake_needs_a_measurement_id`).

| # | co změřit | jak | co to rozhodne |
|---|---|---|---|
| **WM1** | ⭐⭐⭐ **Jak hluboký je strom při `BB_MCTS=100`?** Rozdělení hloubek navštívených uzlů a **počet kandidátů `n` v kořeni** — zvlášť útok / obrana / volný míč. | čítač v `macro_mcts.cpp:217-270` (hloubka po `select`) + histogram `macros.size()` z `:164`; produkční config, dw-dw | **Pořadí celé sekce.** Je-li medián hloubky 1, `W1` (zahozený strom) je bezpředmětné a prioritou je hloubka. Je-li 2-3, `W1` je největší nález a plán tahu se má **uložit**, ne hledat znovu |
| **WM2** | ⭐⭐⭐ **Jak často padne `END_TURN` z prázdné expanze — a kolik těl v tu chvíli ještě mohlo hrát?** | čítač na `macro_mcts.cpp:1180`, a k němu počet hráčů s `canAct() && !hasActed`; **jmenovatel = počet ukončení kola celkem** (past ze `feedback_wrong_result_looks_normal`) | Jestli je `W7` velký nález, nebo vzácnost. **Ztracené aktivace jsou přímá měna celotahu** |
| **WM3** | ⭐⭐ **Rozpad těch prázdných expanzí podle makra** (11 cest z `W8`). | typ makra u čítače z WM2 | Kde opravovat. `expandAdvance` má už dnes `g_advanceResigned` (`:2249`) a 90,1 % z nich mělo kam jít (`:2210-2211`) — ale **ten čítač tiká i uvnitř hledání**, takže sám nestačí |
| **WM4** | ⭐⭐ **Kolikrát hráč po NEÚSPĚŠNÉM blitzu ještě zahraje BLOCK?** (`W6`) | v `expandBlock` (`:2444`) test `att.usedBlitz && blockThrowSeq() nezvýšen tímto hráčem` | Jestli je `W6` živá vada nebo mrtvá cesta. **Odlišit od legálního `expandBlitzAndScore`**, který tutéž cestu potřebuje |
| **WM5** | ⭐⭐⭐ **Jaká je horní mez blitzové kontinuace „na účel", ne „na ústup"?** Kolik blitzů skončí tak, že blitzující má zbylý pohyb A existuje pole, které je **užitečné** (roh vlastní klece / koridor nosiče / marking soupeřova nosiče), ne jen bez TZ? | rozšířit sondu M9 (18 000 her, 24.08.: 4,09 blitzů/zápas se zbylým pohybem a volným polem) o test **užitečnosti** cíle | Jestli má smysl doplňovat `W18`/`W19` — ⭐ a měří to **to, co změna dělá**, ne to, co se zrovna měří (`feedback_measure_what_the_change_does`) |
| **WM6** | ⭐⭐ **Kolikrát nosič jde jako POSLEDNÍ aktivace kola?** (`W20`) | pořadí aktivací v kole z `state.currentActivationId` (`action_resolver.cpp:498`) | Jestli je rezerva `mvRemaining/2` **placená naslepo**. ⚠️ Naráží na `T5.33` — `turn_logs` dnes `hasActed` **nenesou**, takže se to z korpusu zpětně nedopočítá a musí se to logovat nově |
| **WM7** | ⭐ **Jak stabilní jsou role v `expandReposition`?** (`W14`) Kolikrát za jedno kolo dostane roli „safety" jiné tělo? | log `(kolo, role, playerId)` u `:1768-1794` | Jestli je přerozdělování rolí jen kosmetika, nebo **rozpouští obranu**. Souvisí s `M11` (149/149 „zavřeno vlastními") |
| **WM8** | ⭐ **Jaký podíl kol vůbec DOJDE k vyčerpání těl** (BB2016 ř. 363-364) proti tomu, kolik skončí `END_TURN`em nebo turnoverem? | tři čítače v `resolveEndTurn` (`turn_handler.cpp:6`) rozlišené podle důvodu | **Referenční číslo pro celý okruh.** Bez něj nemá „ztracená aktivace" měřítko — táž past s jmenovatelem jako u vstávání (`action_resolver.cpp:77-85`) |

⚠️ Ke všem: **měřidla z hledání nejsou měřidla ze hřiště.** `macro_actions.h:60-75` na to
varuje číslem 186× (349 vyhodnocení proti 1,88 skutečným hodům). Každý čítač výše musí
mít jasně napsané, jestli tiká v produkci, nebo uvnitř MCTS.

---

## 5. CO DOPLNIT PŘÍŠTĚ

Tenhle soubor se má rozšiřovat. Co v něm **schválně ještě není**:

1. **Obrana.** `classifyTurnGoal` obranný cíl nemá (`W24`), takže celá obranná polovina
   celotahu je nezmapovaná. Vstup: řádky `Q16`, `T1.11`, doktrína „2 sloupce → L" z fronty.
2. **Přihrávkový kanál.** `T5.19` a měření *„v 29 % kol vede k TD cesta, kterou umí JEN
   přihrávka"* (`task_queue.md:101`) — pass je vzácný zdroj úplně stejně jako blitz, a
   v tomhle auditu má jen `W11`/`W13`.
3. **Interakce s klecí.** `W17` a `W20` jsou obě klecové a obě jsem sem dal jen jako
   vedlejší nález; okruh `KLEC` je před `CELOTAH`em a jeho výstup sem patří celý.
4. **Soupeřův celotah.** `P62` (soupeř neumí jednokolovou hrozbu) a `P59` (dopočet dosahu)
   — celotah potřebuje **soupeřův** plán jako vstup, ne jen svůj.
5. **Číselné podklady WM1-WM8.** Až budou, nálezy `W1`, `W2`, `W7` dostanou dopad z měření
   místo odhadu, a tabulka se má přepsat, ne doplnit.
6. **Rozhodnutí, jestli se zapnou hotová ramena** (`W15`). Tři plánovače s napsanou
   doktrínou pořadí leží vypnuté; než se bude psát nová vrstva, patří se změřit tyhle.
7. ⏰ **Fable audit nad tímhle souborem** — `project_bloodbowl_fable_audit_after_cage_20260820`
   je vázán na klec, ale tenhle podklad je jeho přirozený druhý vstup.

---

## 6. Kotvy pro příští doplnění

Aby se dalo přidávat bez přečíslování: nálezy `W1-W24`, měření `WM1-WM8`, rozchody
`R1-R5`. **Nové položky se přidávají na konec řady, nikdy se nepřečíslovávají** —
ve frontě i v paměti se na ně bude odkazovat číslem.
