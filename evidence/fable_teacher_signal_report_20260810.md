# Report: MÁ UČITEL VŮBEC CO UČIT? (teacher signal na REPOSITION)

**Datum:** 2026-08-10 · **Zadání:** `evidence/fable_teacher_signal_20260810.md`
**Navazuje na:** `evidence/fable_offline_feature_ab_report_20260810.md` (NO-GO;
§2.2: max-visit množina u REPOS pokrývá 48 % kandidátů, chance_maxset 0,476)
**Artefakty:** `evidence/diag_teacher_signal_20260810/` (skripty, výsledky, logy)
Pracovní adresář (živé markery): `/tmp/claude-1000/-home-jan-claude/01724f63-791e-456a-936f-c5215d2631bf/scratchpad/teacher_signal/`

---

**TL;DR:** Prahy předregistrovány 14:25–14:40 UTC před výsledky (§1).
**Test 1 — verdikt (b):** teacherova leaf hodnota je na REPOS kandidátech
plochá (medián spread 0,0058 < práh 0,010; (c) vyloučeno — doktrinální
páry pokrývají 28 % rozhodnutí a kde se pole liší, hodnota rozdíl vidí,
Δ 0,065). Post-hoc dekompozice: 60 % rozhodnutí je „KDO jde" (eval slepý,
spread 0,003 — nález o modelu), zbytek „KAM" (eval vidí, ale MCTS-100
visity to nepropíší — UCB člen ~0,8 » Δ 0,065; visity ≈ šum i tam, kde
učitel ví). **Test 3 NO-GO:** capability-only featury udrží mechanismus C
jen zčásti (ρ 0,13→0,20, práh chtěl +0,10 a polovinu miskalibrace we).
**Test 2 (MCTS-400)** běží přes noc, kotva zapsána předem. **Důsledek pro
imitaci (§5):** na největší třídě rozhodnutí nemá učitel ve visit
distribuci co učit — měnit se musí TARGETY (value-destilace pro „KAM",
vacate/screen termy v leaf evalu pro „KDO"), ne vstupní featury.

---

## 1. PŘEDREGISTRACE — zapsáno 10.08.2026 14:25–14:40 UTC, PŘED existencí jakýchkoli výsledků

Žádný sběr ani trénink v tuto chvíli neběží a žádné výstupní soubory tohoto
zadání neexistují (ověřitelné z mtime souborů ve scratchpadu/evidence vs mtime
tohoto souboru). Prahy se po měření NEposouvají.

### 1.1 Test 1 — přímá value evaluace REPOSITION kandidátů (hlavní výstup)

**Postup (fixní):** replay téhož korpusového protokolu (fairtest config:
MCTS-100, C=1,0, dirichlet 0, vfBlend 0,15, policy nahraná s blend 0; stejné
seedy 92 000 000+ / 92 000 100+ / 92 000 200+ jako korpus A/B ⇒ identické
trajektorie her). U každého ROZHODNUTÍ s ≥3 REPOSITION kandidáty (top-20 dle
visitů) se pro každý REPOS kandidát sestaví **výsledná pozice**: klon stavu,
aktér teleportován na targetPos (míč následuje, nese-li ho),
movementRemaining −= Chebyshev vzdálenost (min 0), hasMoved/hasActed = true.
Ta se ohodnotí NAPŘÍMO třemi způsoby:
- **V015** = `MacroMCTSSearch::evaluateLeaf(výsledný stav, activeTeam)`
  s vfBlend 0,15 — **přesně signál, kterým učitel hodnotí listy** (heuristika
  0,85 + NN value 0,15 + scoringBonus; leafLookahead default off ⇒
  deterministické);
- **Vheur** = totéž s vfBlend 0 (čistá heuristika + scoringBonus);
- **Vnn** = surový výstup NN value funkce (`extractFeatures` + `evaluate`).

**Statistiky (fixní):** S_val(d) = max−min V015 přes REPOS kandidáty
rozhodnutí d; primárně **medián S_val přes trpasličí rozhodnutí** (uvádí se
i mean a řez all-races). Dekompozice S_heur, S_nn (bez prahu, diagnostika).
**Doktrinální páry**: dvojice kandidátů téhož rozhodnutí s rozdílem ≥2
v počtu enemy TZ na cílovém poli (marked vs volné pole — u pomalého týmu
doktrinálně rozhodující); D_val = medián |ΔV015| přes tyto páry; coverage =
podíl rozhodnutí s ≥1 takovým párem.

**Kalibrace prahů (odvození, fixní):** jednotky V jsou [−1,1], kde ±1 =
výhra/prohra a jeden TD = 0,5. Nejmenší doktrinální term, který leaf
heuristika vůbec obsahuje, je 0,03 (výhoda jednoho hráče); marking term u
AG4 handlera je 0,06. „Slušný rozptyl" tedy = aspoň jeden plný nejmenší
term; „plochý" = pod třetinou nejmenšího termu.

**Verdikt (fixní pravidlo):**
- **(b) slepá value funkce** ⇔ medián S_val (dwarf) **< 0,010**;
- **(a) málo iterací** ⇔ medián S_val (dwarf) **≥ 0,030** (hodnoty se liší
  o ≥1 doktrinální term, ploché visity jdou za rozpočtem; MCTS-400 to má
  potvrdit);
- šedá zóna 0,010–0,030 ⇒ rozhodují doktrinální páry: D_val < 0,010 ⇒ (b)
  (ani povinný marking rozdíl se nepropíše); D_val ≥ 0,010 ⇒ slabé (a);
- **(c) „pozice se opravdu neliší"** smí být závěr JEN když platí všechno:
  medián S_val < 0,010 **a** coverage doktrinálních párů < 5 % rozhodnutí
  **a** kandidáti jsou pozorovatelně homogenní (medián rozsahu TZ_tgt ≤ 1
  a medián rozsahu progress cíle ≤ 0,08 ≈ 2 pole). Jinak ploché hodnoty =
  nález o NAŠEM modelu ⇒ (b).

**Odchylka protokolu zjištěná PŘED spuštěním sběru (zapsáno 14:35 UTC, stále
před jakýmikoli výsledky):** engine byl dnes ve 14:00 UTC přebuildován
s commity `6beee84..4bd66a4` (G-fixy: substitutes/13hráčová soupiska,
Casualty table, apothecary, Sweltering Heat) — tedy PO sběru A/B korpusu
(13:11–13:19). Stejné seedy proto už NEreprodukují identické trajektorie
(ověřeno: hra 1 seed 92000000 dřív 118 rozhodnutí/377 akcí, teď 147/444;
stará binárka na novém .so padá — ABI změna soupisky). Test 1 tedy měří
čerstvý vzorek REPOS rozhodnutí ze STEJNÉHO protokolu na AKTUÁLNÍM enginu;
prahy §1.1 na identitě trajektorií nezávisejí a nemění se.

### 1.2 Test 2 — MCTS-400 sběr a search spread

Stejný kolektor jako korpus A/B, jediná změna maxIterations 100→400; stejné
rasy/počty her, **nové seedy 93 000 000+** (disjunktní; 4× dražší běh nesmí
záviset na dojetí — čte se ráno). Metriky identické s §2.2 minulého reportu
(mean search_spread = max−min visit fraction přes REPOS kandidáty, dwarf
≥3 REPOS; chance_maxset = E[m/n]).

**Předregistrace (fixní; kotva upřesněna 14:35 UTC kvůli změně enginu, stále
před výsledky):** kvůli přebuildu enginu (viz odchylka výše) se MCTS-100
baseline NEBERE ze starého korpusu (0,0255 / 0,476, jiná éra pravidel), ale
spočítá se z výstupu testu 1 (stejný engine, stejné seedy, MCTS-100):
**baseline_100 = mean search_spread (dwarf, ≥3 REPOS) z testu 1** a
**chance_maxset_100 z testu 1**. Prahová PRAVIDLA zůstávají:
- **„search se rozpočtem probudí"** ⇔ search_spread(400) ≥ 2× baseline_100
  NEBO chance_maxset(400) ≤ 0,30;
- **„search nerozliší ani s 4× rozpočtem"** ⇔ search_spread(400) ≤ 1,5×
  baseline_100 A chance_maxset(400) ≥ 0,40;
- mezi tím = neprůkazné. Konzistence: při verdiktu (b) z testu 1 čekáme
  druhou větev. Vyhodnocení proběhne až po doběhu MCTS-400 (ráno); baseline
  z testu 1 se do reportu zapíše dřív, než budou výsledky 400 k dispozici.

### 1.3 Test 3 — mini-A/B jen capability/risk featury (mechanismus C)

Ramena: **old** = 73 state + 23 akčních; **cap** = 73 + 23 + **8** nových:
nf[0] AG/6, nf[1] MA/9, nf[2] movementRemaining/9, nf[11] identita aktéra,
nf[12] p(fail), nf[13] TZ na zdrojovém poli, nf[14] TZ na cílovém poli,
nf[15] normalizovaný roll target — tj. featury [23-25]+[34-38] z reportu
A/B §4/1, **bez identity cíle** (nf[3..10] vyřazeny). Stejný korpus
(rows_*.jsonl), stejný protokol (16 epoch, seedy 1/2/3, 4-fold CV po hrách,
shuffle kandidátů, tie-robustní metriky).

**Prahy — relativní k TOMUTO korpusu** (hodnoty old ramene a searche na
tomto korpusu jsou z reportu A/B v3, tj. známé před tímto během; cap rameno
neexistuje). **GO ⇔ všechna tři:**
1. **mechanismus:** Spearman ρ(uniform-relativní prior, 1−p_fail) cap ramene
   **≥ 0,20** (≈55 % z 0,362, které na tomto korpusu dala plná 16-featurová
   sada — capability sada nese p_fail přímo, má udržet většinu efektu)
   s permutačním p < 0,05, **a** ρ_cap ≥ ρ_old + 0,10;
2. **kalibrace:** |delta_wood-elf(cap)| ≤ 0,5 × |delta_wood-elf(old)|
   (na tomto korpusu old = −2,81 pp ⇒ práh 1,41 pp), **a** delta_dwarf(cap)
   ≥ delta_dwarf(old) − 1 pp (žádné zhoršení podshootu přes 1 pp);
3. **non-worsening:** out-of-fold CE(cap) ≤ CE(old) + 0,002; REPOS
   hit_maxset(cap) ≥ hit_maxset(old) − 0,02; END_TURN top-1 share(cap) ≥
   top1 share(old).
NO-GO jinak. (Registrováno před spuštěním; řez sady je převzat doslova
z doporučení §4/1 minulého reportu, ne vybírán podle nových výsledků.)

---

## 2. Výsledky — test 1 (value vs search): **VERDIKT (b), s podstatnou (a) složkou**

Sběr 14:41–15:05 UTC, 48 her, **2 211 rozhodnutí s ≥3 REPOS kandidáty**
(dwarf 846, wood-elf 382, skaven 983). Determinismus leaf evalu ověřen za
běhu (dvojí evaluace téhož stavu, žádný NONDETERMINISTIC výskyt v logách).
Kanonická čísla: `evidence/diag_teacher_signal_20260810/teacher_value_results.out/.json`.

### 2.1 Předregistrované metriky (dwarf)

| metrika | hodnota | práh | čtení |
|---|---|---|---|
| **medián S_val** (max−min V015 přes REPOS kandidáty) | **0,0058** | (b) < 0,010 | **(b) slepá value funkce** |
| mean S_val / p90 | 0,0256 / 0,0758 | — | rozdělení silně šikmé |
| D_val (doktrinální páry, TZ-diff ≥2) | **0,0654** | — | kde se pole liší, hodnota to VIDÍ |
| coverage doktrinálních párů | 28,1 % | (c) chce < 5 % | **(c) vyloučeno** |
| tz_range / prog_range medián | 0 / 0 | (c) chce ≤1 / ≤0,08 | typická sada je homogenní |
| search_spread (MCTS-100, tento engine) | 0,0241 | — | baseline pro test 2 |
| chance_maxset (MCTS-100) | **0,518** | — | max-visit množina = 52 % kandidátů |
| corr(V015, visit fraction) uvnitř rozhodnutí | 0,115 | — | visity skoro nesledují ani vlastní hodnoty |

Wood-elf: medián S_val 0,0068 ⇒ (b); skaven: 0,0118 (šedá zóna) a D_val
0,069 ≥ 0,010 ⇒ slabé (a). Dekompozice složek (diagnostika bez prahu,
medián spread): **heuristika (váha 0,85) 0,0027; NN value (váha 0,15)
0,0227** — plochá je především ručně psaná heuristika, NN složka rozlišuje
~8× víc, ale dostává jen 15 % váhy.

### 2.2 Post-hoc dekompozice (exploratorní, NENÍ součást brány; skript
`diag_explore_decomposition.py`)

Rozhodnutí se rozpadají na dvě strukturně různé situace:
1. **„KDO tam jde"** — 60 % trpasličích rozhodnutí má JEDINÉ cílové pole
   a kandidáti se liší jen aktérem. Value spread uvnitř téhož pole:
   **medián 0,0029** (u všech ras ~0,002–0,003). Leaf eval je prakticky
   slepý k tomu, který hráč pole obsadí a KTERÉ pole tím uvolní — a to
   doktrinálně jedno NENÍ (vacate-first, integrita screenu; vzor „vlastní
   těla si překážejí" 07.08.).
2. **„KAM se jde"** — když se cílová pole liší (40 % dwarf, 69 % skaven),
   rozdíly mezi poli hodnota vidí jasně: **S_across medián 0,064–0,069**
   (≈2× nejmenší doktrinální term).

**A přesto: na podmnožině, kde teacherova VLASTNÍ leaf hodnota rozlišuje
(S_val ≥ 0,03; 27 % dwarf rozhodnutí), visity zůstávají ploché:**
search_spread 0,0241 (= celkový průměr), chance_maxset 0,525 a
P(value-argmax ∈ max-visit množina) = 0,520 ≈ přesně náhoda. Vysvětlení
je aritmetické: při C=1,0, N=100 a ~10–15 kandidátech je UCB explorační
člen ~0,8, tj. o řád víc než value rozdíl 0,065 — visit distribuce je
v tomto režimu ≈ prior + explorační šum, ne ranking hodnot. Open-loop
replay s čerstvými kostkami rozptyl dál zvyšuje.

**Souhrnný verdikt testu 1:** podle předregistrovaného pravidla **(b)** —
medián S_val 0,0058 < 0,010; (c) je vyloučeno (coverage 28 % ≥ 5 %).
Poctivé úplné čtení: porucha učitele je DVOUVRSTVÁ — (b)-vrstva: eval
nerozlišuje „kdo jde" (většina rozhodnutí); (a)-vrstva: i kde eval
rozlišuje „kam", MCTS-100 to do visitů nepropíše. Víc iterací tedy může
pomoci jen na ~27–40 % rozhodnutí, a jen pokud překoná explorační člen
(predikce pro test 2 na základě aritmetiky výše, zapsaná před jeho
doběhem: chance_maxset zůstane ≥ 0,40 ⇒ větev „nerozliší").

## 3. Výsledky — test 2 (MCTS-400)

Běží odpojeně přes noc (sekvenční runner, nice -19, seedy 93M+); čte se
ráno. **Kotva z testu 1 (zapsáno PŘED výsledky 400):** baseline_100 dwarf
search_spread = 0,0241, chance_maxset = 0,518 ⇒ prahy: „probudí se" ⇔
spread ≥ 0,0481 nebo chance_maxset ≤ 0,30; „nerozliší" ⇔ spread ≤ 0,0361
a chance_maxset ≥ 0,40. Vyhodnocení: `diag_analyze_m400.py` (spustí ho
watcher automaticky, výsledek `m400_spread_results.out`).

## 4. Výsledky — test 3 (mini-A/B capability): **NO-GO**

Běh 14:41–14:55 UTC (`evidence/diag_teacher_signal_20260810/results_ab_cap.out/.json`,
log `train_cap.log`). Korpus = zmražený A/B korpus (6 639 rozhodnutí, stejná
éra pravidel uvnitř srovnání). Old rameno reprodukuje čísla v3 z minulého
reportu 1:1 (CE 1,8961, ρ 0,132, delta_we −2,81 pp — sanity ✓).

| kritérium (předregistrace §1.3) | old | cap (23+8) | práh | výsledek |
|---|---|---|---|---|
| 1a. Spearman ρ(prior, 1−p_fail) | 0,132 | **0,202** | ≥ 0,20 | ✓ |
| 1b. permutační p | 0,0014 | 0,0013 | < 0,05 | ✓ |
| 1c. ρ_cap − ρ_old | — | **+0,069** | ≥ +0,10 | **✗** |
| 2a. \|delta_wood-elf\| | 2,81 pp | **2,08 pp** | ≤ 1,41 pp | **✗** |
| 2b. delta_dwarf | −10,12 pp | −10,22 pp | ≥ −11,12 pp | ✓ |
| 3. non-worsening (CE, hit_maxset, END_TURN) | | 1,8957 / 0,481 / 0 % | CE ≤ 1,8981; hm ≥ 0,463; ≥ 0 % | ✓ |

**GO vyžadoval všechna tři kritéria ⇒ NO-GO.** Mechanismus p(fail) se
v capability-only sadě udrží jen zčásti (ρ 0,13 → 0,20), zatímco plná
16-featurová sada dala na stejném korpusu 0,362 — tj. NEplatí, že sílu
mechanismu C nesly capability featury samotné; poziční/identitní kontext
(který u B selhal) k C-PASS přispíval podstatně. Kalibrace wood-elf se
zlepšila (2,81→2,08 pp), ale ne na polovinu. Izolovaná implementace
capability featur do enginu se tímto NEdoporučuje; případný další pokus
by měl testovat p_fail featury SPOLU s relačním kontextem cíle, ne místo něj.

## 5. Co z toho plyne pro imitační přístup

1. **Strop imitace je potvrzen číslem.** Na největší třídě rozhodnutí
   (REPOSITION, ~45 % korpusu) je učitelova visit distribuce ≈ uniformní
   šum: max-visit množina pokrývá 52 % kandidátů a visity korelují
   s učitelovými VLASTNÍMI hodnotami jen 0,12. Policy, která se učí CE na
   tyto targety, se nemůže naučit víc než uniformitu — B-FAIL minulého A/B
   nebyl selháním featur, ale nevyhnutelný: **žádné vstupní featury
   nenaučí síť targety, které signál nenesou.** Plateau policy (~25 %
   signálu, top-1 42 %) je s tím konzistentní.

2. **Porucha má dvě vrstvy a každá chce jiný lék:**
   - **„KAM" vrstva (řešitelná bez nové sítě):** u ~27 % dwarf (39 % we,
     46 % sk) rozhodnutí učitel v leaf hodnotách VÍ, které pole je lepší
     (Δ ~0,065), jen to visit distribuce nepřenáší, protože UCB explorační
     člen (~0,8 při N=100) je o řád větší. Víc iterací to řeší brutálně
     neefektivně (potřeba ~100× rozpočet, viz aritmetika §2.2; test 2 to
     ráno změří). Přímější cesta: **destilovat hodnoty místo visitů** —
     targety pro pohybová makra stavět z přímé evaluace výsledných pozic
     (softmax přes evaluateLeaf výsledných stavů, rizikově korigovaný
     přes p_fail — přesně mechanismus C, který v A/B prošel), případně
     Gumbel/Q-style „completed values" místo visit counts. Výroba těchto
     targetů je levná: 2 211 rozhodnutí × ~10 evaluací stálo minuty CPU.
   - **„KDO" vrstva (nález o našem modelu):** u 60 % dwarf REPOS
     rozhodnutí je jediné cílové pole a volí se jen aktér — a tam je leaf
     eval slepý (spread 0,003), heuristika i NN. Doktrinálně přitom volba
     uvolňovaného pole rozhoduje (screen, vacate-first, „vlastní těla si
     překážejí"). Tady nepomůže ani víc searche, ani destilace — **není
     co destilovat, dokud eval nevidí hodnotu zdrojového pole.** Lék =
     doplnit leaf evalu/featurám člen za uvolnění/obsazení klíčových polí
     (screen lajna, roh klece, marking). To je práce na učiteli, ne na
     žákovi.

3. **Pořadí kroků, které z toho plyne:** (i) nejdřív value-destilační
   targety pro „KAM" (levné, bez zásahu do enginu — offline A/B stejným
   harnessem); (ii) pak vacate/screen termy do leaf evalu („KDO");
   (iii) do té doby NEinvestovat do dalších vstupních featur pro imitaci
   (potvrzuje NO-GO z A/B). Hypotéza k levnému ověření (neověřeno): NN
   složka evalu rozlišuje ~8× víc než heuristika, ale má váhu jen 0,15 —
   zvednout vfBlend pro ranking listů může „KAM" signál zesílit; může ale
   jít i o šumovou citlivost NN, proto nejdřív malý A/B.

4. **Kalibrace očekávání:** dokud se nezmění TARGETY (ne featury), není
   důvod čekat, že další imitační iterace zlepší trpasličí poziční hru.

## 6. Limity a co se nepovedlo

- **Éra enginu:** engine přebuildován 14:00 UTC (commity G-fixů) — testy
  1–3 běžely celé na NOVÉM enginu a jsou vnitřně konzistentní; srovnání
  s absolutními čísly z A/B reportu (starý engine) je jen orientační.
  Test 3 běžel na zmraženém starém korpusu — old-vs-cap srovnání je
  uvnitř korpusu validní, ale absolutní hodnoty patří staré éře.
- **Teleport aproximace:** výsledná pozice REPOS kandidáta se staví
  deterministicky (úspěšné provedení, bez pádů po cestě); riziko cesty
  v hodnotě záměrně není — test měří rozlišitelnost POZIC, ne EV makra.
- **„KDO" vrstva bez zdrojových featur:** log neukládá TZ/roli zdrojového
  pole, takže homogenitu volby aktéra nejde rozložit dál; závěr 2.2/1 se
  opírá o changed-nothing spread 0,003 + doktrínu, ne o přímé měření
  hodnoty screenu.
- **Watcher infra:** první instance watcheru tiše zemřela (pravděpodobně
  úklid procesní skupiny sandboxu); druhá instance s debug logem běží a
  analýzu testu 1 provedla; m400 větev hlídá marker `done_m400_all`.
  Kdyby watcher nepřežil, ráno stačí ručně:
  `python3 diag_analyze_m400.py` ve scratchpadu `teacher_signal/`.
- Predikce výsledku testu 2 v §2.2 je označená hypotéza zapsaná před
  doběhem; verdikt testu 2 se přečte ráno z `m400_spread_results.out`.
