# Tempo doktrína cage advance — report (Fable, 06.08.2026)

Zadání: `evidence/fable_tempo_doctrine_20260810.md` (dispatch předsunut na
06.08.). Stav: **ROZPRACOVÁNO — měření čekají na doběh run_iteration.py**
(verdikt ~13:30 UTC), kód sondy a A/B hotov a commitnut (41fd8c5).

## Závěry napřed (průběžné — finalizace po měřeních řetězu)

1. TEMPO veto je hlavní brzda adoption (68 % relevantních turnů 05.08.;
   61 % na 4 matchupech 04.08.) a grind ho umí otevřít (smoke: 4→1 veto).
2. Trpasličí bezriziková doktrína prokazatelně produkuje 0:0 (zrcadlo
   dw-dw: 57 % her 0:0, 0,48 TD/hru) — potvrzuje „bez rizika není TD".
3. DICEY zeď není primárně o kostkách: 65 % jsou bezkostkové exec-faily
   tras kolem TZ stínů; risk budget a route-fix jsou dvě půlky téhož řešení.
4. Opce (a) a (b) v dnešním kódu splývají; (c) je navazující architektura,
   ne alternativa — TEMPO signál zachovat jako vstup budoucího arbitra.
5. Doporučení (podrobně sekce 5): grind + cage-fill fallback (item13 krok 2
   mašinérie) + risk budget jako jeden modul enginu.
6. Kritéria úspěchu překalibrována vstupem uživatele 06.08. (sekce 2b):
   ne 50% WR, ale dwarf TD/hru ↑, 0:0 ↓, posun distribuce skóre. Dnešní
   cage plán jimi už hýbe správným směrem (dw-sk TD 0,33→0,46, 0:0
   33,5→24,0 %), v zrcadle dw-dw ale nulově — tam míří grind+risk budget.
7. Poziční riziko přilehlosti má závazné pravidlo (uživatel 06.08. večer,
   sekce 4 „Probed exposure"): **žádný roh klece nesmí mít souseda
   protihráče, bez výjimek podle síly hráčů** — o bezpečí sousedství
   nerozhoduje, kdo je silnější teď, ale co si soupeř ve SVÉM tahu
   doasistuje (a jeho těla u klece jsou zároveň připravené pokrytí
   vypadlého míče). Prakticky: fix nálezu 2 technické cage review
   (přední sloty feasibility) se o toto pravidlo rozšiřuje a jde PŘED
   zapnutí grindu jako defaultu — grind počet tahů klece do kontaktu
   zvyšuje (pořadí v sekci 5).

### Slovníček (lidsky)
- **Adoption** = jak často se doktrinální plán klece skutečně ujme tahu
  (místo obecného prohledávání). Dnes 0,34–0,66 plánu na hru — málo.
- **TEMPO_INSUFFICIENT („tempo veto")** = plánovač spočítá, že klec
  nestihne endzónu do konce poločasu, a celý plán vzdá.
- **DICEY** = některý krok plánu není bezpečný (hrozí kostkový hod nad
  povolený strop, nebo hráč na cíl vůbec bezpečně nedojde) → plán se vzdá.
- **PLAN_READY** = plán prošel a tah se hraje podle doktríny klece.
- **pTO** = pravděpodobnost, že akce skončí turnoverem (ztrátou tahu/míče).
- **chess skóre** = výhra 1, remíza 0,5, prohra 0 — průměr přes hry;
  párová Δchess = totéž měřeno na dvojicích her se stejným seedem a
  prohozenými stranami (odfiltruje šum losu).
- **Attrition** = ubíjení soupeře: KO/zranění/mrtví na konci zápasu.
- **Exec-fail** = krok plánu selhal už v simulaci provedení bez kostek
  (hráč nedošel na cíl), ne na riziku hodu.
- **Probed exposure** = poziční riziko „na zavolání": pozice, která
  vypadá bezpečně teď, ale soupeř si ji svým tahem otevře (doplní si
  asistence, zvolí okamžik) — cenit se musí to, co soupeř MŮŽE příští
  tah udělat, ne statické porovnání statů při stavbě.

## 0. Co je hotovo / infrastruktura (commit 41fd8c5, testy 493/493)

- **Sonda `diag_f1_adoption_probe.cpp` rozšířena** (bez změny chování hry):
  - `[TEMPO]` řádek za každé TEMPO_INSUFFICIENT veto: vzdálenost do endzóny,
    zbývající kola, požadované vs. dosažitelné tempo (req/raw/ach), odpor
    soupeře v koridoru, **větev veta** (u = došla kola, p = tempo nevychází,
    r = tempo vychází, ale finální krok nešel obsadit), kontext drivu
    (driveStart = od kterého kola drive běží).
  - `[DRIVE]` záznam za každý drive: výsledek (TD čí / bez bodu), počty
    TEMPO/DICEY/PLAN_READY vet obou stran, ztráta držení míče po vetu.
  - argv[4]=1: shadow plánovač počítá s grindem (hry stále hraje produkční
    politika — čistě pozorování „co by grind řekl na týchž stavech").
- **`MCTSConfig::cageGrind` (DEFAULT OFF — žádná změna produkčního chování):**
  experimentální brána pro opci (a) grind: při nesplnitelném rozvrhu klec
  tlačí maximálním BEZKOSTKOVÝM krokem místo pádu na search(). Grind nikdy
  neutrácí GFI nositele (rozvrh stejně nevychází, kostka nic nekupuje).
- **Harness `diag_f1_cage_advance_harness.cpp` mode 1 = grind A/B:**
  kandidát = cageAdvance+cageGrind, baseline = cageAdvance bez grindu
  (= dnešní fallback na search). Párované hry se stranovým swapem, seedy 37M
  (disjunktní), řádky do `diag_f1_grind_rows.jsonl`.
- Smoke (1 hra dw-sk, seed 36M): baseline 4×TEMPO (p=2, r=1, u=1) → s grindem
  1×TEMPO(r) + 2×DICEY + 1×PLAN_READY. Grind tedy veta reálně otevírá, ale
  narazí na DICEY koridor (očekáváno dle corner-release 04.08.).
- Kalibrace: ~26 s/hra sondy, ~55 s/pár harnessu (nice -19, 1 jádro).

## Plán měření (session-nezávislý řetěz `run_tempo_measure_20260806.sh`)

Měření sbírá setsid řetěz (PPID 1, přežije session): po doběhu run_iteration
sám postaví binárky a spustí (a) sondu 8 her × matchupy dw-sk/dw-we/orc-sk
baseline + dw-sk/dw-we grind SHADOW → `tempo_measure_20260806/probe_m{0,1,3}_g{0,1}.log`,
marker PROBE_DONE; (b) grind A/B first-read 40 párů × dw-sk a dw-we →
`tempo_measure_20260806/ab_m{0,1}/`, marker ALL_DONE. Pozn.: shadow běhy
hrají TYTÉŽ hry jako baseline (stejné seedy, plánovač je jen pozorovatel)
— konverze verdiktů je tedy párová na úrovni identických stavů.
Vyhodnocovací skripty připraveny: `tempo_measure_20260806/analyze_tempo_probe.py`
a `analyze_grind_ab.py`.

## 1. Datový rozpad TEMPO vet (otázka 2 + šikmá otázka 1)

(čeká na měření)

### Kontext z 05.08. (8 her dw-sk, 78 relevantních turnů)
TEMPO_INSUFFICIENT 53 (68 %), DICEY 17 (22 %), PLAN_READY 8 (10 %).
Corner-release sonda 04.08. (24 her, 4 matchupy): TEMPO 137/225 ADVANCE
turnů (60,9 %) — veto dominuje napříč matchupy.

## 2. Grind A/B (otázka 3)

(čeká na měření)

### Referenční baseline (A/B 04.08., cage vs. off, 400 párů/matchup;
### remízy/0:0 dopočteny z rows 06.08., Wilson 95% CI)
| matchup | párová Δchess | remízy | z toho 0:0 | TD/hru | plánů/hru |
|---|---|---|---|---|---|
| dw-sk | +6,75 pp ± 2,73 SE (2,5 SE) PASS | 39,0 % [35,7–42,4] | 28,7 % | 0,97 | 0,43 |
| dw-we | +2,13 pp ± 2,26 SE (0,9 SE) | 36,6 % [33,4–40,0] | 31,2 % | 0,89 | 0,48 |
| dw-dw | +0,00 pp ± 2,20 SE | **59,2 % [55,8–62,6]** | **57,0 %** | **0,48** | 0,66 |
| orc-sk | +3,87 pp ± 2,81 SE (kontrola OK) | 37,1 % [33,8–40,5] | 24,1 % | 0,99 | 0,34 |

Úspěch doktríny se pozná i na POKLESU remízovosti (uživatel 05.08.:
„bez rizika není TD — a pád k 0:0 zpátky"). Data to tvrdě potvrzují:
v trpasličím zrcadle končí **57 % VŠECH her 0:0** a padá jen 0,48 TD na
hru (obě strany dohromady!) — bezriziková doktrína proti bezrizikové
doktríně skoro nikdy neskóruje. Selekční poznámka: hry, kde se plán aspoň
jednou ujal, mají chess 0,55–0,67 vs 0,44–0,52 bez plánu — deskriptivní
(plán se ujímá v už-dobrých pozicích), ne kauzální.

## 2b. Úspěšnostní kritéria doktríny (ZÁVAZNÝ VSTUP UŽIVATELE, 06.08.)

Kalibrace z lidské hráčské zkušenosti (uživatel, 06.08.): průměrný hráč se
skaveny porazí ne-top hráče s trpaslíky průměrně 2:1 — i když mu přežije
jen málo skavenů. Důsledky:

1. **50 % WR NENÍ cíl ani metrika úspěchu** — matchup dwarf vs rychlé rasy
   je intrinsicky nakloněný. Úspěch = růst dwarf TD/hru, pokles 0:0 remíz,
   posun distribuce skóre k realistickému stropu matchupu („prohra 1:2
   místo 0:1 už je měřitelný pokrok"). Grind A/B proto reportuje
   distribuci skóre, ne jen WR/chess.
2. **Attrition sama nevyhrává** — málo přeživších skavenů dá pořád 2 TD.
   Doktrína musí mít obě nohy: dovézt vlastní TD A bránit rychlému
   skórování soupeře (držení míče klecí jako obrana).
3. **Trpaslíci potřebují top-úroveň hry** — mechanický posun klece
   nestačí; doktrína musí kódovat expertní techniky (uvolňování rohů,
   rozložení rizika, timing). Kde leží největší delta: sekce 3c.

### Jak si podle těchto kritérií vede už dnešní cage plán (data 04.08.,
### rozpad po ramenech „kdo hrál dwarfa", 400 her/rameno)
| matchup | dwarf TD/hru ON→OFF | 0:0 podíl ON vs OFF | soupeř TD/hru |
|---|---|---|---|
| dw-sk | **0,46 vs 0,33** (+39 % rel.) | **24,0 % vs 33,5 %** | 0,56 vs 0,59 (≈beze změny) |
| dw-we | 0,20 vs 0,15 | 28,8 % vs 33,8 % | 0,70 vs 0,72 |
| dw-dw | 0,20 vs 0,20 (nic) | 56,5 % vs 57,5 % (nic) | — |
| orc-sk (orc) | 0,56 vs 0,50 | 21,8 % vs 26,5 % | 0,45 vs 0,47 |

Čtení: cage plán UŽ DNES pohybuje přesně uživatelovými metrikami — dwarf
skóruje víc a 0:0 ubývá, aniž by soupeř skóroval víc (obranná noha drží).
Distribuce dw-sk se posouvá z „0:0 a 0:1" k „1:0 a 1:1" (1:0 109× vs 73×).
V zrcadle dw-dw ale nulový efekt — tam je adoption nejvyšší (0,66/hru),
a přesto se nic nemění: oba týmy narazí na TEMPO/DICEY strop. Přesně tam
míří grind + risk budget.

## 3. Vyhodnocení opcí a/b/c

Slovníček: **grind** = mlet dopředu, posouvat klec tempem, které reálně jde,
i když „rozvrh" (stihnout endzónu do konce poločasu) matematicky nevychází.
**Fallback na search()** = dnešní chování po vetu: tah řeší obecný prohledávač
bez doktríny — právě on produkuje sólo úprky nositele z klece.

### (a) GRIND — posouvat max dosažitelným tempem
- PRO: drží míč v kleci (soupeř nemůže skórovat, dokud meleme — obranná
  hodnota i bez šance na TD); attrition trpaslíků pracuje časem pro nás;
  soupeřovy chyby/úbytek těl mohou rozvrh dodatečně otevřít; nikdy solo útěk.
  Obranná noha má oporu v hráčské zkušenosti uživatele (06.08.): skaven
  vyhrává ~2:1 i s malým počtem přeživších — ubíjení soupeře samo o sobě
  jeho TD nezastaví, ale míč zamčený v mele ANO (soupeř nemůže skórovat
  bez míče). Grind je tedy současně útočný POKUS a obranné DRŽENÍ — proti
  fallbacku, který míč vystavuje sólo úprkem, je lepší v obou rolích;
  data 04.08. to potvrzují (soupeřovo TD/hru se s cage plánem NEZVEDLO,
  dwarf TD/hru ano — sekce 2b).
- PROTI: tam, kde koridor stíní TZ soupeře, grind narazí na DICEY zeď
  (data níže) — grind bez koridorové vrstvy část vet jen přesune z „TEMPO"
  do „DICEY". To ale NENÍ regrese proti dnešku (dnes se nepohne vůbec).
- Měřeno A/B (sekce 2) + shadow konverzí (sekce 1).

### (b) veto jen při achievable = 0
V dnešním kódu (přestavba 04.08.) už „žádný proveditelný krok" vrací
NOT_APPLICABLE (problém formace, ne rozvrhu). Opce (b) se tím v praxi
**redukuje na opci (a)**: jediné, co TEMPO veto dnes navíc filtruje, jsou
případy achievable ≥ 1 s nesplnitelným rozvrhem — přesně ty grind otevírá.
Implementačně jsou (a) a (b) jedna brána (cageGrind); rozdíl zůstal jen
v u-větvi (poslední kolo poločasu, usable<1): grind tam poposune i tak
(držení míče) a čisté (b) by ten krok pustilo také — achievable > 0.
A/B tedy měří společné chování obou opcí.

### (c) eskalace signálu výš
Správná CÍLOVÁ architektura, ale vyšší rozhodovací úroveň dnes neexistuje
a vyžaduje blitz taxonomii (průlom/zranění/míč/poziční, plán 03.08.)
a PASS sérii (seed, nerozpracovávat). Doporučení: (c) NENÍ alternativa
ke grindu, ale jeho pokračování — TEMPO signál (requiredPace,
achievablePace, deficit, turnsLeft) v CageAdvancePlan ZACHOVAT a předávat
jako vstup budoucího arbitra (pass / blitz průlom / committed grind).
Corner-release data 04.08. říkají, kam eskalovat: block+blitz vrstva
odemyká ~56 % DICEY — to je budoucí „blitz průlom" větev arbitra.

### Vztah k závazné hierarchii (posun → doplnit → nikdy solo)
Grind řeší jen první příčku. Když ani grind krok nejde (NOT_APPLICABLE,
DICEY, TEMPO po walk-downu), dnes tah stále padá na search() = riziko sola.
Druhou příčku („DOPLNIT NEÚPLNOU KLEC VŠÍM, CO NA NI DOSÁHNE") pokrývá
mašinérie item13 kroku 2 (evidence/item13_krok2_design_20260806.md):
cage-fill = tryAssign(state, carrier, step=0, reservedPlayerIds) — týž
přiřazovací algoritmus s nulovým posunem + dice-free REPOSITION exekuce.
Nenavrhovat od nuly; jiný trigger (nevyšlý posun místo sebrání). Teprve
tahle dvojice uzavírá „nikdy solo útěk nositele".

## 3b. Interakce s DICEY (šikmá otázka 4)

Rozpad 17 DICEY případů z 05.08. (dw-sk):
- **11/17 (65 %) = exec-fail BEZ kostek**: greedy chůze REPOSITION se stínu
  TZ vyhne, dojde jí pohybový rozpočet a zastaví 1–5 polí před cílem.
  Detail: v 6/11 exec-failů měl hráč rozpočet POHODLNĚ větší než vzdálenost
  (např. dist=2, budget=7, miss=2) — nezastavil ho pohyb, ale to, že
  bezkostková trasa kolem stínů neexistuje nebo ji greedy krokování nenajde.
  Vyšší povolené riziko tady NEPOMŮŽE — je to problém trasy/výběru nohy
  (route-around s vědomým dodge, jiná noha), nález shodný s corner-release
  bodem 5. POZOR ale: „route-fix" tu z velké části znamená POVOLIT dodge
  skrz stín — tedy nakonec taky risk budget, jen na úrovni plánování trasy
  (dnes exekutor REPOSITION kostky odmítá už konstrukcí). Bez budgetu není
  co povolit; bez chytřejší trasy není jak riziko správně umístit — jsou to
  dvě půlky téhož fixu, ne konkurenti.
- 6/17 = skutečný kostkový risk (pTO probe > strop 0,02):
  hodnoty 0,00(no-op)/0,23/0,29/0,31/0,88/0,94. Strop zvednutý na 0,25
  pustí 1, na 0,35 pustí 3; dvě situace ≥0,88 jsou právem vetované.
- Závěr: risk budget otevře jen MENŠINU DICEY (~3/17 zde; přeměří se na
  větším N). Většinu DICEY otevírá (i) route/leg fix bez kostek a
  (ii) block/blitz release vrstva (56 % dle corner-release). Risk budget
  je přesto nutný pro DOKONČENÍ drivu (dodge/GFI/blitz nohy blízko
  endzóny) — viz sekce 4.

## 3c. Kde leží delta mezi průměrnou a top hrou (vstup uživatele 06.08., bod 3)

Uživatelova teze: trpaslíci vyžadují top-úroveň hry — mechanický posun
klece nestačí. Data ji PODPORUJÍ a lokalizují, kde expertní vrstvy leží:

1. **Mechanický posun (grind) je nutný, ale otevře jen část:** smoke +
   shadow konverze (sekce 1 po doplnění) — TEMPO veta se otevřou, ale
   významná část skončí na DICEY zdi. Mechanika = vstupenka, ne výhra.
2. **Největší jednotlivá delta = práce s koridorem a rohy** (expertní
   technika „uvolni si cestu bojem"): block+blitz release odemyká ~56 %
   DICEY (corner-release 04.08.), block-only jen 19 %. To je přesně
   rozdíl mezi hráčem, který klec jen sune, a hráčem, který si tahem
   NAPŘED otevře koridor blokem/blitzem a PAK sune.
3. **Druhá delta = umístění a timing rizika** (load-balancing): risk-last
   řazení (item10), riziko jen na completion nohách, stropy rostoucí s
   fází drivu (sekce 4). Bezriziková hra = 0:0 (dw-dw 57 %); riziko
   rozházené kamkoli = turnovery. Top hra = riziko soustředěné tam, kde
   kupuje TD, a až po postavení bezkostkové části tahu.
4. **Třetí delta = tempo timing** („bank while clear"): už zavedený
   princip banku (jet rychleji, dokud je koridor čistý, rezerva na horší
   kola) je timing technika; grind ji rozšiřuje o „nikdy nestát jen proto,
   že rozvrh nevychází".

Pořadí implementace v sekci 5 z toho vychází: grind (mechanika) →
cage-fill (formace) → risk budget (umístění rizika) → koridorová vrstva
(bojové uvolnění) — každá vrstva přidává jednu „expertní" schopnost.

## 4. Návrh univerzálního RISK BUDGET mechanismu (návrh, BEZ implementace)

Princip (závazný vstup 05.08.): „bez rizika není TD" platí obecně — JEDEN
mechanismus enginu, ze kterého plátky (klec, blitz, pass, pickup) čerpají.
Risk budget = odpověď na otázku „jak velkou šanci na turnover si tento tah
smí koupit a na kterých nohách", spočtená ze situace zápasu, ne konstanta
zadrátovaná v každém plánovači zvlášť.

### Dnešní roztroušené konstanty (inventura kódu, 06.08.)
- `turn_planner.h:78` SAFE_PTO = 0.02 (item13 staged pickup)
- `macro_mcts.cpp:285` RISK_DEFER_SAFE_PTO = 0.02 (item10 risk-deferral)
- `cage_advance.h` SAFE_PTO = 0.02, SAFE_PTO_GFI1 = 0.25, SAFE_PTO_GFI2
  = 0.40 (carrier GFI výjimka — už dnes „risk budget pro jeden typ nohy")
- corner-release návrh: blitz-release práh fail ≤ 0,35 (zatím jen sonda)

### Návrh rozhraní (nový modul `bb/risk_budget.h`)
```cpp
struct RiskContext {                  // vše čitelné z GameState + plánu
    int scoreDiff;                    // moje skóre − soupeřovo
    int turnsLeftHalf;                // kola do konce poločasu (9 − turn)
    int half;
    int distToEndzone;                // fáze drivu (nositel/cíl)
    double requiredPace;              // 0 pokud plán nemá rozvrh
    double achievablePace;
    TurnGoal goal;                    // SCORE/ADVANCE/PICKUP/NONE(=obrana)
};
struct RiskBudget {
    double structuralCeil;            // nohy kostry (rohy, screen)
    double completionCeil;            // nohy dokončení (carrier dodge/GFI,
                                      // pickup hod, blitz-release, pass)
    int    maxRiskyLegs;              // kolik nohou plánu smí být nad
                                      // structuralCeil (řadí se POSLEDNÍ,
                                      // item10 pravidlo risk-last)
};
RiskBudget computeRiskBudget(const RiskContext&);
```

### Doktrína výpočtu (kalibrační východisko; čísla = startovní, ladit A/B)
Klíčová veličina: **rezerva rozvrhu** slack = turnsLeftHalf −
(distToEndzone / achievablePace), tj. o kolik kol dřív než na poslední chvíli
klec dojede. Konec poločasu = konec šance na TD v drivu (závazná definice
05.08.), takže rezerva se počítá VŽDY vůči konci poločasu.

| situace | structuralCeil | completionCeil | maxRiskyLegs |
|---|---|---|---|
| rezerva ≥ 2 kola (pohoda) | 0,02 | 0,02 | 0 (kostky nekupují nic) |
| rezerva 1 kolo | 0,02 | 0,10 | 1 |
| rezerva 0 (přesně na hraně) | 0,02 | 0,25 (≈ dnešní GFI1) | 1 |
| skluz (rozvrh nevychází) | 0,02 | 0,40 (≈ dnešní GFI2) | 2 |
| poslední 2 kola poločasu a TD na dosah (dist ≤ MA+2) | 0,02 | 0,50+ | 2 |
| vedeme a rozvrh nevychází | 0,02 | 0,10 (mel a drž — obranný grind) | 1 |
| prohráváme / remíza pozdě ve hře | +0 | +1 stupeň v tabulce | +0/+1 |

- **Kostra klece nikdy nehazarduje** (structuralCeil zůstává 0,02): rohy
  a screen jsou důvod, proč je nositel v bezpečí; risk se kupuje jen na
  nohách, které posouvají DOKONČENÍ (dodge nositele přes TZ, GFI, pickup,
  block/blitz na uvolnění rohu či koridoru, v budoucnu přihrávka).
- **Risk-last:** riskantní nohy vždy na konci tahu (item10 Q-guard už
  přesně tohle dělá na úrovni search() — mechanismus se sjednotí, ne
  zduplikuje). Když riskantní noha selže, bezkostková část tahu už stojí.
- **DICEY přestane být binární:** verdikt dnes = „noha nad stropem 0,02".
  S budgetem se strop pro completion nohy pohybuje se situací → část DICEY
  se stane sankcionovaným rizikem (kvantifikace: sekce 3b — pouze menšina,
  bez přehnaných očekávání).
- **Zákazníci v pořadí:** 1. klec (GFI nohy už dnes, dodge nositele nově),
  2. blitz-release vrstva (prahy z corner-release 0,35 → completionCeil),
  3. item13 pickup (pickup hod = completion noha), 4. PASS série (po seedu).
  Stávající tři SAFE_PTO konstanty a GFI stropy se stanou řádky
  computeRiskBudget, ne pěti nezávislými čísly.
- Vzor zobecnění = pravidlo blitzových změn 03.08.: každá nová riskantní
  mechanika se vyhodnotí proti TÉTO tabulce, žádné nové lokální konstanty.

### Probed exposure — cena přilehlosti (ZÁVAZNÝ VSTUP UŽIVATELE, 06.08. večer)
Doktrinální pravidlo dne („žádný roh klece nesmí mít souseda protihráče")
platí **stat-agnosticky** — ilustrační scénář uživatele: orc postaví klec
za záda lineelfa tak, že s elfem sousedí DVA orci ST4. Staticky vypadá
sousedství bezpečně (2× ST4 vs ST3), ale soupeř táhne po nás a asistenty
si doplní ve svém kole: pomocníci označkují druhého orka (zruší obranné
asisty) a přidají útočné → elf má výhodný block na roh → sražený roh
otevře lajnu → blitz na ballcarriera → **a titíž pomocníci už stojí tam,
kam po úspěšném blitzu vypadne míč**. Jedna investice soupeře = block +
blitz lane + ball recovery.
Důsledky pro cenovou funkci pozičního rizika (probed exposure):
- Žádná výjimka podle ST — o bezpečí přilehlosti nerozhoduje momentální
  matchup, ale to, že soupeř volí okamžik a asistence.
- Cena přilehlosti se počítá ze **soupeřova asistenčního potenciálu
  příštího tahu** (kolik těl dosáhne na útočné asisty a na zrušení našich
  obranných), ne ze statického porovnání statů při stavbě.
- Cena markovaného rohu zahrnuje i **ball-recovery pokrytí** — soupeřova
  těla u klece jsou zároveň předpřipravené pokrytí dopadu míče.
Zapracovat do fixu nálezu 2 technické cage review (feasibility slotů) a
do budoucí implementace probed exposure pod touto střechou.

### Co risk budget NENÍ
Není to EV kalkulačka (očekávaná hodnota tahu se dál počítá v search/plánu);
je to POVOLENÍ — strop, pod kterým smí plánovač riskantní nohu vůbec
navrhnout. Odděluje „smíme riskovat" (situace zápasu) od „vyplatí se to"
(hodnota konkrétního tahu). Díky tomu je testovatelný samostatně a laditelný
po jednom čísle.

## 5. Podklad rozhodnutí pro uživatele

(čísla v závorkách se doplní z měření; struktura rozhodnutí je nezávislá)

**Kritéria úspěchu (závazný vstup uživatele 06.08., sekce 2b):** růst
dwarf TD/hru, pokles 0:0, posun distribuce skóre — NE 50% WR (matchup je
intrinsicky nakloněný; „prohra 1:2 místo 0:1 už je pokrok").

**Rozhodnutí A — doktrína při nevyšlém rozvrhu.** Doporučení: GRIND
(opce a; opce b je v dnešním kódu totéž). Argumenty: (1) TEMPO veto je
68 % relevantních turnů — plánovač dnes většinou rezignuje dřív, než
začal; (2) držení míče v kleci má obrannou hodnotu (soupeř neskóruje,
dokud meleme; hráčská zkušenost 06.08.: attrition sama 2 TD skavenů
nezastaví, zamčený míč ano) — i grind „bez šance na TD" je lepší než
fallback, který umí ztratit míč sólo úprkem; (3) A/B výsledek dle kritérií
2b (doplnit: dwarf TD/hru, 0:0, distribuce skóre, Δchess).
Rizika: grind část vet jen přesune do DICEY zdi (koridor) — proti dnešku
to nic nezhoršuje, ale plný účinek přijde až s koridorovou vrstvou
(blitz-release / route-fix). Alternativa „nechat dnešní veto" je v přímém
rozporu se závazným rámováním z 05.08.

**Rozhodnutí B — druhá příčka hierarchie (cage-fill).** Když ani grind
krok nejde: doplnit/zpevnit klec bez posunu = tryAssign se step=0 na
mašinérii item13 kroku 2 (implementace 07.08.) s jiným triggerem.
Samostatná malá změna PO kroku 2, ne nová stavba. Teprve tím zmizí
„nikdy solo útěk nositele" i z fallback větve.

**Rozhodnutí C — risk budget.** Zavést `computeRiskBudget` (sekce 4) jako
JEDEN mechanismus; první zákazník klec (GFI nohy už dnes, dodge nositele
nově), pak blitz-release, pickup, pass. Bez něj zůstane doktrína
strukturálně bezriziková → 0:0 (dwarf zrcadlo: 57 % her 0:0). Kalibrační
tabulka v sekci 4 je startovní bod k diskuzi, ne hotová čísla.

**Závazná doktrína přilehlosti (vstup uživatele 06.08. večer — k
zapracování, ne k rozhodnutí).** Žádný roh klece nesmí mít souseda
protihráče, **stat-agnosticky** (scénář orc ST4 vs lineelf v sekci 4:
soupeř táhne po nás, asistence si doplní a jedna jeho investice = block
na roh + blitz lajna + pokrytí vypadlého míče). Dva praktické dopady:
(1) fix nálezu 2 technické cage review (`evidence/
cage_technical_review_20260806.md` — feasibility neváží přední vs. zadní
sloty) se rozšíří o toto pravidlo: soupeř sousedící se slotem rohu =
slot na tomto kroku infeasible, ne jen „open"; fix je PODMÍNKA zapnutí
klece/grindu jako defaultu, protože grind zvyšuje počet tahů klece do
kontaktu. (2) Cenová funkce probed exposure (asistenční potenciál
soupeřova příštího tahu + ball-recovery pokrytí) patří pod střechu risk
budget modulu (sekce 4), ne jako další lokální heuristika klece.

**Navrhované pořadí implementace** (jedna změna najednou, každá s A/B):
1. item13 krok 2 (už naplánováno na 07.08.) → 2. fix nálezu 2 + pravidlo
   přilehlosti rohů (podmínka zapnutí, viz výše) → 3. grind brána (kód
   existuje za cageGrind, „implementace" = učinit z ní default chování
   cage plánu po GO) → 4. cage-fill trigger při nevyšlém posunu →
   5. risk budget modul + přepojení stávajících konstant (včetně cenové
   funkce probed exposure) → 6. koridorová vrstva (route-fix +
   blitz-release dle corner-release opcí R-A/R-B).

---
## Pracovní log
- 08:24 UTC: run_iteration.py běží (PID 115687), verdikt ~13:30 UTC — těžká
  měření odložena; šampion md5 17578260… ověřen, nedotčen.
- 08:30: engine + sonda + harness upraveny, build OK, testy 493/493,
  commit 41fd8c5 pushnut.
- 08:36: smoke výsledky viz sekce 0.
- 08:50: sekce 3/3b/4 nadraftovány.
- 09:05: změna plánu (koordinátor): měření sbírá session-nezávislý řetěz
  `run_tempo_measure_20260806.sh` (PID 119460, čeká na konec tréninku);
  můj launcher smazán. Vyhodnocovací skripty `tempo_measure_20260806/
  analyze_tempo_probe.py` + `analyze_grind_ab.py` napsány a validovány na
  smoke lozích. Doplněna čísla z existujících dat (remízy/0:0 z ab_run
  04.08. rows; exec-fail detail z 05.08. logu), slovníček, sekce 5.
- 09:25: závazný vstup uživatele 06.08. (hráčská kalibrace: skaven ~2:1,
  50 % není cíl) zapracován — nová sekce 2b (kritéria + rozpad 04.08. dat
  po ramenech), 3c (kde leží top-play delta), rozšířeno 3a/5; parser A/B
  doplněn o distribuci skóre a dwarf TD/hru po ramenech.
- ~09:30: původní agent vyčerpal tokeny; navazující agent převzal (10:15).
- 10:20: probed exposure (závazný vstup 06.08. večer, sekce 4) promítnut
  do Závěrů napřed (bod 7), slovníčku a sekce 5 (nový blok „doktrína
  přilehlosti" + pořadí implementace rozšířeno o fix nálezu 2 jako krok 2).
  Ověřeny analyze skripty (py_compile + prázdný běh OK); řetěz
  run_tempo_measure běží (chain.log 08:58 start, čeká na konec tréninku).

### HANDOFF pro pokračovatele (zapsáno 10:35 UTC, aktualizovat u checkpointu)
Session navazujícího agenta končí ~15:45 UTC; měřicí řetěz
`run_tempo_measure_20260806.sh` (PID 119460, setsid) je session-NEZÁVISLÝ
a data doteče sám: `tempo_measure_20260806/PROBE_DONE` ~15:00–15:30 UTC,
`ALL_DONE` ~17:30 UTC. NIC nerestartovat; když chain.log hlásí BUILD FAIL
nebo řetěz zmizel z ps, jen to poznamenat sem — neopravovat.
Zbývající kroky (jen spustit skripty a vložit čísla):
1. Po PROBE_DONE: `python3 tempo_measure_20260806/analyze_tempo_probe.py`
   (čte `probe_m{0,1,3}_g{0,1}.log` tamtéž). Výstup → **sekce 1**:
   rozpad TEMPO vet (větve u/p/r; raw≥2 vs ≤1 = otázka 2 zadání;
   fullHalf vs midHalf = šikmá otázka 1; drive outcomy po vetu; grind
   SHADOW konverze TEMPO→READY/DICEY). Navázat na místa v sekci 3
   („Měřeno … shadow konverzí"), 3b (přeměření DICEY pto histogramu na
   větším N) a 3c bod 1.
2. Po ALL_DONE: `python3 tempo_measure_20260806/analyze_grind_ab.py`
   (čte `ab_m{0,1}/diag_f1_grind_rows.jsonl`). Výstup → **sekce 2**:
   vyhodnotit dle kritérií sekce 2b (dwarf TD/hru, 0:0, distribuce skóre
   po ramenech — NE 50% WR) + vůči referenční tabulce 04.08. v sekci 2;
   pozor: reference je jiné rameno (cage vs off), jen orientačně.
   N=40 párů = first read, Δchess pod ±5 pp bude šum (SE ~±5 pp).
3. Finalizace: doplnit čísla do Rozhodnutí A bod (3) v sekci 5,
   přeformulovat „Závěry napřed" body 1/6 podle výsledků a odstranit
   „(průběžné…)" z nadpisu, aktualizovat hlavičku reportu (Stav:
   FINÁLNÍ) a dopsat log. Commit+push reportu (md) je povolen.
