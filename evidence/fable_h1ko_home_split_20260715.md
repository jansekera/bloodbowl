# H1-KO vs home-slot split: mechanismus opening-drive konverzní mezery

**Datum:** 2026-07-15 (Fable 5) · **Nástroj:** `diag_h1ko_vs_home_split.py` (nový)
· **Data:** `arm_h1ko_split_h1ko_20260715_{fwd,swp}.json` (N=600 seedů × 2 orientace
= 1200 her, base_seed=20260715, mirror `weights_best.json`, MCTS=100, vf_blend=0,
TV=1000, gate config; log `diag_h1ko_home_split_20260715.log`) + offline reanalýza
`arm_first_possession_postfix_20260714_{fwd,swp}.json` (600 her ze 14.07., stejná
binárka — engine .so buildnutá 2026-07-14 13:28, obě sady po ní).

---

## VERDIKT

**Ani (a), ani (b) v původní podobě NEPOTVRZENO. (b) doslovný mechanismus
VYVRÁCEN přímým měřením. (a) NEPOTVRZENO na tomto seed-batchi a KO-stratifikace
nejlépe vysvětluje herní-síla/variance confound, ne engine bug. Navíc:
samostatný a důležitý METODICKÝ nález — home-slot win-rate se NEREPLIKOVALA
mezi seed batchy na TÉŽE binárce (+14.2pp CONFIRMED 14.07. vs −2.2pp
NEVÝZNAMNÉ 15.07., swing 16.4pp) → tato metrika je nespolehlivá, opening-drive
konverzní mezera je robustnější signál (směr replikuje, velikost klesla).**

1. **Hypotéza (b) KO-return: VYVRÁCENA přímým měřením, ne jen čtením kódu.**
   Return audit: 1200/1200 H2 openingů i 949/949 post-TD drivů začíná přesně
   11v11 na obou stranách. Mechanismus „vyřazený hráč se nevrátí do H2" nemá
   na co působit — engine resetuje všech 22 hráčů na STANDING při KAŽDÉM
   výkopu (potvrzuje čtení `setupHalfOrDrive()` ze sekce 2).
2. **Hypotéza (a) home-slot bug: NEPODPOŘENA na tomto běhu.** Pooled home-slot
   win edge na 1200 hrách (07-15 seedy) = **−2.2pp, p=0.6085 NEVÝZNAMNÉ** —
   opačné znaménko než včerejších +14.2pp CONFIRMED (07-14 seedy) na STEJNÉ
   binárce. Code review (formace, MCTS heuristika, prior logika, macro vrstva)
   nenašel žádný home-zvýhodňující absolutní bias; jediný nalezený směrový
   bias (`getAdjacent()` tie-break −x-first) zvýhodňuje AWAY, ne HOME.
3. **KO-stratifikace nejlépe čte jako herní-síla/variance confound, ne
   kauzální mechanismus.** Away-slot H1-removal stratifikace dává zdánlivě
   „potvrzující" vzorec pro (b) (bez KO away konvertuje H2 LÉPE než home
   −9.0pp CONFIRMED; s KO away konvertuje HŮŘ, gap +8.4pp CONFIRMED) — ale
   mechanismus (b) je bodem 1 vyvrácený, takže nejpravděpodobnější čtení je:
   tým, který v H1 nikoho neztratí, je tentýž tým, který tu konkrétní hru
   dominuje (lepší kostky/výběr akcí v tom zápase), a dominance přetrvává do
   H2 bez ohledu na slot. Symetrický (zrcadlový) vzorec u home-slot-removal
   stratifikace (bez KO gap +8.5pp CONFIRMED, s KO gap +2.8pp INCONCLUSIVE)
   podporuje stejné čtení — kdyby šlo o slotový bug, gap by měl být
   konstantní napříč stratifikací, ne se obracet/mizet podle toho, kdo
   dominoval H1.
4. **Nejčistší (nejméně confound) subset ukazuje téměř nulovou mezeru:**
   v `any-slot removal=0` hrách (n=76, žádná KO/casualty v CELÉM H1) je gap
   H1-H2 jen +1.3pp INCONCLUSIVE — nejblíž „čisté" srovnání dostupné v datech.
5. **Metodický nález (samostatný, viz feedback_draw_rate_noise_floor):**
   home_win pooled edge na N=600×2 orientací se mezi dvěma nezávislými seed
   batchi na téže binárce liší o 16.4pp a mění znaménko. To je na hraně/za
   dosavadním prahem pro mezi-běhové delty (±11pp) a ukazuje, že samotné
   „CONFIRMED" z jednoho batche (i při N=600, exact p<0.05) u této konkrétní
   odvozené metriky (decisive-games win share) nestačí jako rozhodovací
   důkaz — potřebuje cross-batch replikaci. Opening-drive-conversion gap je
   stabilnější: SMĚR (H1 opening > H2 opening) replikoval na obou batchích
   (25.3 %/17.3 % → 24.1 %/19.8 %), i když velikost klesla ze ~8pp na ~4.3pp.

---

## 1. Otázka

Postfix report 2026-07-14 potvrdil home-slot výhodu +14.2pp na rozhodnutých hrách
(p=0.0133) a konverzní mezeru opening drivů: H1 opening (post-H2-fix vždy přijímá
HOME) 25.3 % vs H2 opening (vždy AWAY) 17.3 %. „Home" a „H1" jsou rozvrhem výkopů
dokonale konfundované. Dvě hypotézy:

- **(a) home-slot bug** — něco strukturně zvýhodňuje stranu kódovanou „home"
  (formace / featury / pathfinding), nezávisle na poločase;
- **(b) H2-return efekt** — hráči vyřazení (KO/casualty) během H1 se na začátek
  H2 nevracejí správně, což degraduje tým přijímající v H2 (= vždy away).

Kvantitativní kontext: mezera openingů (+0.080 TD/hru) vysvětluje **~85 %**
celkové TD asymetrie home−away (0.426 vs 0.332 TD/hru) — tj. skoro celý
home-slot edge JE tato mezera.

## 2. Nález z kódu (před měřením; e2e ověřeno běhy níže)

`game_simulator.cpp: setupHalfOrDrive()` na KAŽDÉM výkopu (po TD i na hranici
poločasu) resetuje všech 22 hráčů na OFF_PITCH a `buildTeam()` je znovu postaví
se `state = STANDING` — `resetHalfState` gate-uje jen rerolly/turn clock/apothecary,
**ne stavy hráčů**. Důsledek: KO/INJURED/DEAD **nepřežívá konec drivu**. Vyřazení
trvá jen do konce aktuálního drivu; na začátku H2 (i každého post-TD drivu) hrají
vždy obě strany 11v11.

Mechanismus hypotézy (b) tedy podle kódu **nemá na co působit**. Per konvence
projektu (třída bugů H2-kickoff) to ale nesmí zůstat jen čtením kódu — audit
návratů je součást měření (sekce 4.1): on-pitch počty v `get_turn_logs()`
snapshotech (`forEachOnPitch` KO/INJURED/DEAD vynechává), očekávání 11v11 na
100 % začátků drivů.

**Vedlejší nález (samostatný, k zařazení do fronty):** resurrect-all je odchylka
od pravidel Blood Bowl (KO se má vracet jen hodem při výkopu, casualty vůbec).
V tomto enginu má bash/attrition strategie nulovou trvalou hodnotu — relevantní
pro trénink i pro věrnost pravidel. NEOPRAVOVAT v rámci tohoto measurement tasku;
kandidát na master-list položku (engine-fix třída, resetuje baseline).

## 3. Offline reanalýza postfix dat (600 her ze 14.07., zdarma)

Skript: scratchpad `reanalyze_postfix.py` (jednorázový; čísla níže reprodukovatelná
z uložených ramen).

**3a. Score-state H2 openingu — hlavní alternativní H2 kanál — VYLOUČEN jako
hlavní driver.** H2-opening konverze away podle stavu skóre v poločase:

| stav v poločase | konverze H2 openingu |
|---|---|
| 0-0 | 69/370 = **18.6 %** [15.0, 22.9] |
| home vede | 24/146 = 16.4 % [11.3, 23.3] |
| away vede | 10/76 = 13.2 % [7.3, 22.6] |

I při identickém score-state jako H1 opening (0-0) zůstává konverze 18.6 % —
hluboko pod H1 25.3 % [22.0, 29.0]. Skóre vysvětluje nejvýš ~1-2pp z ~8pp mezery.
(Pozn.: away vedoucí konvertuje NEJHŮŘ — konzistentní se stall chováním při vedení,
ne s „dohánějící tým to vzdá".)

**3b. Post-TD drivy: žádná stranová asymetrie.** home 6/199 = 3.0 % vs away
8/256 = 3.1 %; per-poločas rozpady bez vzoru (away H1 3.7 % > home H1 2.3 %;
home H2 3.5 % > away H2 2.1 %; vše malá n, floor efekt). Post-TD drivy používají
TENTÝŽ setup kód (formace, kickoff) jako openingy — plošný slot bug by se sem
propsat měl; nepropisuje se (s výhradou floor efektu ~3 % a malých n).

**3c. Mezera je celá v td_recv, ne v turnoverech.** Zakončení openingů:
H1: td_recv 25.3 %, td_kick (counter-TD) 13.0 %, vyšumění do konce poločasu 61.7 %;
H2: td_recv 17.3 %, td_kick 14.8 %, vyšumění 67.8 %. Counter-TD prakticky stejné —
H2 receiver se nehroutí do turnoverů, jen častěji NEDOJDE do endzóny
(non-converted drivy: recv_turns 7.47 vs 7.70 — časový budget srovnatelný).

## 4. Výsledky nového běhu (N=1200 her, `h1ko_20260715`, base_seed=20260715)

Kompletní log: `diag_h1ko_home_split_20260715.log`. Ramena: `arm_h1ko_split_
h1ko_20260715_{fwd,swp}.json`. Běh doběhl ~09:02 UTC 2026-07-15 (start 06:56,
~2h6m, 1200 her, žádné watchdog-skipy, 0 segmentačních anomálií).

**4.1 Return audit** — přímé měření nahrazující čtení kódu ze sekce 2:

| kategorie drivu | on-pitch počet (home,away) | n |
|---|---|---|
| H1 opening | (11,11) | 1200/1200 |
| H2 opening | (11,11) | 1200/1200 |
| post-TD | (11,11) | 949/949 |

100 % — žádný drive nikdy nezačíná s méně než 11v11. **Vyvrací doslovnou
verzi (b).**

**4.2 KO stratifikace opening-drive konverze (H1 home vs H2 away)**

| stratifikátor | podmínka | n her | H1 opening conv. | H2 opening conv. | gap (H1−H2) | verdikt |
|---|---|---|---|---|---|---|
| any-slot H1 removal | bez KO | 76 | 18.4 % [11.3,28.6] | 17.1 % [10.3,27.1] | +1.3pp [−10.8,+13.5] | INCONCLUSIVE |
| any-slot H1 removal | s KO | 1124 | 24.5 % [22.0,27.1] | 19.9 % [17.7,22.4] | +4.5pp [+1.1,+8.0] | CONFIRMED |
| away-slot H1 removal | bez KO | 278 | 15.8 % [12.0,20.6] | 24.8 % [20.1,30.2] | **−9.0pp** [−15.6,−2.3] | CONFIRMED (obráceně!) |
| away-slot H1 removal | s KO | 922 | 26.6 % [23.8,29.5] | 18.2 % [15.9,20.8] | +8.4pp [+4.6,+12.1] | CONFIRMED |
| home-slot H1 removal | bez KO | 319 | 30.1 % [25.3,35.3] | 21.6 % [17.5,26.5] | +8.5pp [+1.7,+15.2] | CONFIRMED |
| home-slot H1 removal | s KO | 881 | 21.9 % [19.3,24.8] | 19.1 % [16.6,21.8] | +2.8pp [−0.9,+6.6] | INCONCLUSIVE |

Vzorec je symetrický/zrcadlový mezi away-slot a home-slot stratifikací a
znaménko gapu se u away-slot řezu OBRACÍ podle KO stavu — přesně opačně, než
by predikoval jednoduchý „away je v H2 slabší" příběh. Nejpravděpodobnější
čtení: KO status v H1 je proxy pro to, který tým tu konkrétní hru dominuje
(lepší kostky/výběr akcí), a dominance přetrvává do H2 nezávisle na slotu —
ne kauzální řetězec KO→nesprávný návrat (ten je bodem 4.1 vyvrácený).

**4.3 Halftime score strata (replikace offline reanalýzy z 3a)**

| stav v poločase | H2 opening (away) konverze |
|---|---|
| 0-0 | 143/705 = 20.3 % [17.5,23.4] |
| home vede | 62/276 = 22.5 % [17.9,27.7] |
| away vede | 27/199 = 13.6 % [9.5,19.0] |
| tied>0 | 5/20 = 25.0 % [11.2,46.9] |

Konzistentní s 3a (offline 07-14 data): skóre v poločase nevysvětluje mezeru
(0-0 subset pořád ~20 %, ne 24 %).

**4.4 Pooled companion čísla**

- H1 opening: 289/1200 = **24.1 %** [21.7,26.6] (07-14: 25.3 % [22.0,29.0] — replikuje)
- H2 opening: 237/1200 = **19.8 %** [17.6,22.1] (07-14: 17.3 % [14.5,20.6] — replikuje směrem, o něco výš)
- gap: ~4.3pp (07-14: ~8.0pp) — **směr stejný, velikost o polovinu menší**
- home-slot decisive win edge: 315W 329L = **−2.2pp, exact p=0.6085 NEVÝZNAMNÉ**
  (07-14: +14.2pp, p=0.0133 CONFIRMED) — **NEREPLIKOVALO, obrácené znaménko**

## 5. Inspekce kódu — kandidáti na (a) prověřeni, žádný home-favoring nenalezen

- **Formace** (`game_simulator.cpp:20-109`): všech 5 párů HOME_*/AWAY_* je
  přesné zrcadlo (dx negované kolem LOS 12/13, y identické). Kick target
  (kickX 3/7 vs 22/18, kickY=7) zrcadlově symetrický, Kick skill sweeper slotu 10
  obou stran stejně.
- **simulate() heuristika** (`macro_mcts.cpp:483+`): plně perspective-relativní
  (ezX podle strany carriera, turnsLeft per-half — v obou poločasech stejné).
  Žádná závislost na `state.half` ani na absolutní straně.
- **Prior heuristiky** (`macro_mcts.cpp:292+`): turnsRemaining/scoreDiff/isFirstTurn
  — vše per-half a per-perspective, symetrické.
- **Macro vrstva** (`macro_actions.cpp`): všechna absolutní čísla (endzóna 25/0,
  směr +1/−1) podmíněná stranou — symetrické.
- **Jediný nalezený absolutní směrový bias:** `Position::getAdjacent()` skenuje
  −x před +x a BFS v `pathfinder.cpp` bere first-found při shodě ceny → tie-break
  preferuje −x. To ale zvýhodňuje AWAY (útočí na x=0), tedy **proti** pozorovanému
  směru — home edge to nevysvětluje, spíš ho mírně podceňuje.
- Featury `feature_extractor.cpp` obsahují game-progress `out[3]` (zná poločas),
  ale při vf_blend=0 a policy_blend=0 do searche nevstupují (null-weights finding);
  na diag bězích se uplatnit nemohou.

## 6. Interpretační limity

- Post-fix rozvrh dělá „home" a „přijímá dřív (H1)" observačně neoddělitelné
  na opening drivech; post-TD drivy konfound lámou, ale jsou floor-limitované
  (~3 % konverze, zbytkový clock).
- Šumové dno: jednotlivé stratum CIs jsou ±4-6pp; verdikty jen přes CI/exact testy,
  ne bodové odhady.
- Drivy v téže hře nejsou nezávislé; párové (within-game) diskordantní testy
  H1-vs-H2 openingu tím netrpí (jednotka = hra).

## 7. Doporučené další kroky

1. **Uzavřít hypotézu (b) jako vyvrácenou.** Return audit (4.1) je přímý,
   nekonfundovaný důkaz — nepokračovat v hledání KO-return bugu pro tuto
   konkrétní otázku. (Samostatná, nesouvisející položka: resurrect-all na
   každém výkopu je odchylka od pravidel Blood Bowl — sekce 2 — kandidát na
   master-list, ale mimo scope téhle investigace a neopravovat teď.)
2. **NEŠÍPOVAT side-swap gating patch (`proposals_gating_sideswap_20260714.md`)
   na základě +14.2pp čísla — to selhalo v cross-batch replikaci.** Patch
   samotný zůstává levný a measurement-only (nulová cena, žádný rebuild), ale
   framing „opravuje potvrzený +7-14pp home-slot bug" už neplatí — snížit na
   „defensive hygiene proti nejistému, možná menšímu efektu", ne urgentní fix.
3. **Před jakýmkoli dalším rozhodnutím o home-slot efektu spustit potvrzující
   běh, který POOLUJE oba batche (07-14 + 07-15, N=2400 her) nebo přidá třetí
   nezávislý seed batch.** Samotné N=600×2 orientací se u metriky home_win
   (decisive-only, tedy efektivní N ~317-644) ukázalo nedostatečné —
   rozšířit `feedback_draw_rate_noise_floor` o poznámku, že i N=600 může být
   pod šumovým dnem pro tuto konkrétní odvozenou metriku.
4. **Opening-drive conversion gap (H1 vs H2) brát jako primární/robustnější
   signál** pro budoucí monitoring — směr replikoval na dvou nezávislých
   batchích (i když velikost klesla ~8pp→~4.3pp). Pokračovat v jeho měření
   při každém budoucím `diag_first_possession`-stylu běhu, ne home_win.
5. **Nevyšetřený, ale konkrétní follow-up lead pro (a)**, pokud by se k němu
   chtělo vrátit: audit ID-based tie-breaků (hráči home mají ID 1-11, away
   12-22) v `getAvailableActions`/expand()/pathfinder — pokud kdekoli platí
   „při shodě kvality vyhrává nižší ID", je to strukturní home-favoring bias
   nezávislý na geometrii, který code review v sekci 5 explicitně
   nekontroloval. Čistě čtecí úkol, žádný běh potřeba předem.
6. Skript `diag_h1ko_vs_home_split.py` ponechat v repu — je opakovaně
   spustitelný (`run <label> [N]`) pro krok 3 nebo pro budoucí re-audit KO
   mechanismu po jakékoli změně `setupHalfOrDrive()`.

## 8. Pooled home_win reanalysis (N=2400, both batches)

Offline pooling obou existujících datasetů na identickém binary
(rebuild 14.07. 13:28, žádný engine commit mezi batchi) a identických
vahách (`weights_best.json` mtime 30.06., `weights_policy.json` 13.07.
— obě starší než první batch) → pooling je validní. Pozn.: skutečný
pooled počet her je **1800** (batch 14.07. měl 300 her/orientaci,
ne 600 — per-batch čísla 317 dec / 181W-136L a 644 dec / 315W-329L
reprodukují přesně, jde tedy o tytéž datasety, jen popisek N byl výš
nadsazený).

| batch | her | decisive | home W/L | edge | exact p |
|---|---|---|---|---|---|
| 20260714 (first_possession_postfix) | 600 | 317 | 181/136 | +14.2pp | 0.0133 |
| 20260715 (h1ko_split) | 1200 | 644 | 315/329 | -2.2pp | 0.6085 |
| **POOLED** | **1800** | **961** | **496/465** | **+3.2pp** | **0.3332** |

- pooled home_win podíl decisive: **51.6 %**, Wilson 95% CI [48.5, 54.8] %
  → edge **+3.2pp**, CI **[-3.1, +9.5] pp**, exact two-sided p = **0.33**.
- Mezi-batchová heterogenita (57.1 % vs 48.9 %, z≈2.4, p≈0.017) je přesně
  to, co čekáme, když jeden N=317 vzorek náhodou padl do ocasu — ne důkaz,
  že se efekt „změnil".

**Verdikt:** pooled CI pohodlně obsahuje nulu; „CONFIRMED +14.2pp"
ze 14.07. byl s vysokou pravděpodobností šum (jeden p=0.013 mezi mnoha
screenovanými metrikami). Home-slot decisive win-rate edge **nepovažovat
za reálný potvrzený efekt** — pokud existuje, je nejspíš ≤ ~5pp. Metriku
vyřadit z primárního sledování (drop as metric of interest); primární
signál zůstává opening-drive conversion gap dle sekce 7 bodu 4.
