# Report: OFFLINE FEATUROVÝ A/B (P0-b) — cílený log tří rozbitých míst

**Datum:** 2026-08-10 · **Zadání:** `evidence/fable_offline_feature_ab_20260810.md`
**Navazuje na:** `evidence/fable_learning_mechanism_report_20260811.md` (§1.4, §2/#1, §3)
**Artefakty (harness, korpus, trénink, výsledky, logy) — TRVALÁ kopie:**
`evidence/diag_feature_ab_20260810/` (27 souborů, 15 MB; korpus
`rows_*.jsonl`, kanonické výsledky `results_ab_v3.out/.json`).
Pracovní adresář (živé done-markery, běhové prostředí):
`/tmp/claude-1000/-home-jan-claude/01724f63-791e-456a-936f-c5215d2631bf/scratchpad/feature_ab/`

---

**TL;DR:** Prahy předregistrovány 13:05 UTC před jakýmkoli měřením (§1).
Nový korpus 6 639 rozhodnutí s plnou identitou maker (starý to neumožňoval,
§0). Výsledek na třech rozbitých místech: **A (END_TURN) FAIL** — 0 % → 0 %,
vzácná třída, featury to nespraví; **B (REPOS) FAIL** — spread se poprvé
pohnul od strukturní nuly (~50 % úrovně searche), ale volba KAM zůstala na
náhodě i při 4× tréninku; **C (PICKUP vs AG) PASS** — všechna tři kritéria,
prior poprvé závisí na skutečné šanci hodu (ρ 0,13→0,36, annex 0,67).
Dle předregistrovaného pravidla (§1.5, GO vyžaduje B): **NO-GO** pro
implementaci celé sady do enginu. Doporučený další krok: předregistrovaný
mini-A/B jen pro capability/risk featury + rozseknout REPOS hypotézy
(relační featury vs MCTS-400 targety), §4.

---

## 0. Proč nový sběr dat (a ne jen starý korpus)

Starý korpus (`evidence/diag_learning_mechanism_20260811/decisions.pkl`, 5 866
rozhodnutí) ukládá u každého kandidáta **jen 23 akčních featur + visit
fraction** (`engine/include/bb/policies.h:25-34`, binding
`engine/python/bb_module.cpp:177-224`). **Neukládá identitu makra** — žádné
`playerId`, `targetId`, `targetPos`. Nové featury (identita cíle REPOSITION,
skutečné p(fail) z AG/TZ/kostek, AG+MA aktéra) se z něj proto dopočítat
NEDAJÍ: makro-extractor featury [15-22] nuluje
(`engine/src/macro_actions.cpp:1503-1620`) a z 23 čísel se aktér ani cíl
zrekonstruovat nedá. → Nový sběr standalone diag harness (vzor
`diag_f1_cage_advance_harness.cpp`), který loguje plné makro + rozšířené
featury spočítané engine API (`calculateDodgeTarget`, `calculatePickupTargetAt`,
`countTacklezones`, `getBlockDiceInfo`, `countAssists` — `engine/include/bb/helpers.h`).
Produkční kód nedotčen.

---

## 1. PŘEDREGISTROVANÉ PRAHY — zapsáno 10.08.2026 13:05 UTC, PŘED sběrem dat i tréninkem

Zapsáno před spuštěním sběru; sběrové ani tréninkové výstupy v tuto chvíli
neexistují (ověřitelné z mtime souborů ve scratchpadu vs mtime tohoto souboru).

### 1.0 Protokol A/B (fixní, není metrika, ale předregistrovaný postup)

- **Korpus:** nový sběr, policy OFF (targety = čisté search visity), fairtest
  konfigurace (MCTS-100, explorationC 1.0, dirichlet 0, vf_blend 0.15, policy
  síť nahraná s blend 0 ⇒ heuristické prior floory aktivní — stejně jako
  `diag_f1_cage_advance_harness.cpp` mode 2 / era běhy). 32 her dwarf–skaven
  + 16 her wood-elf–skaven, seedy 92M+ (disjunktní od 30M/31M/34M/37M/51M/63M/91M).
- **Obě ramena trénují IDENTICKY, liší se jen vstupní featury:**
  stará sada = 73 state + 23 akčních (96); nová sada = 73 state + 23 starých
  + 16 nových akčních (112). Architektura zrcadlí produkci: 1 skrytá vrstva
  H=64, ReLU, softmax přes kandidáty, CE na visit fractions
  (`python/blood_bowl/policy_trainer.py`, `weights_best_policy.json`:
  policy_hidden_size=64). SGD per-decision, lr 0,01, 16 epoch, 3 náhodné
  inicializace (seedy 1/2/3) na rameno — reportuje se průměr přes seedy.
- **Vyhodnocení: 4-fold cross-validace po HRÁCH** (žádné rozhodnutí se
  nevyhodnocuje foldem, který ho viděl v tréninku). Všechny metriky níže se
  počítají na out-of-fold predikcích.
- Baseline čísla z reportu 20260811 (nasazená policy, starý korpus) slouží
  jen jako kontext; **rozhoduje srovnání staré vs nové rameno na novém
  korpusu** (spravedlivé: stejný trénink, stejná data).

### 1.1 Metrika A — END_TURN ranking (akční bias)

Množina: out-of-fold rozhodnutí, kde search dává END_TURN ≥50 % visitů a je
≥3 kandidátů. (Baseline kontext: nasazená policy 0/9 top-1 u trpaslíka,
0/11 u elfa, medián rank 4. z 5.)

- **A-PASS ⇔** (podíl případů, kde nové rameno má END_TURN jako top-1) −
  (týž podíl starého ramene) **≥ +30 pp** na sdružené množině všech ras,
  **a zároveň** medián ranku END_TURN v novém rameni **≤ 2**.
- Reportovat i samostatný řez dwarf (primární rasa cíle), ale kvůli malému
  očekávanému N (~15-20 případů) se práh vyhodnocuje na sdružené množině.

### 1.2 Metrika B — REPOSITION spread priorů (největší třída rozhodnutí)

Množina: out-of-fold rozhodnutí s ≥3 REPOSITION kandidáty, primárně dwarf
perspektiva. (Baseline kontext: spread nasazené policy 0,000; search visity
0,109. Staré rameno musí strukturně zůstat ~0 — identické vstupy ⇒ identické
výstupy; slouží jako negativní kontrola.)

- **B-PASS ⇔** průměrný spread (max−min prior mezi REPOS kandidáty téhož
  rozhodnutí) nového ramene **≥ 0,055** (≥50 % search hodnoty 0,109),
  **a zároveň** směrová validita: hit-rate „top-REPOS kandidát nového ramene
  = top-REPOS kandidát searche" **≥ 1,5 × E[1/n_REPOS]** (očekávaná úspěšnost
  uniformní volby na týchž rozhodnutích). Spread bez validity nestačí —
  „sebevědomý nesmysl" neprojde.

### 1.3 Metrika C — PICKUP prior masa vs AG

Množina: out-of-fold loose-ball rozhodnutí (state featura [14] loose=1)
s ≥1 PICKUP kandidátem. Definice: delta_rasa = průměr přes rozhodnutí
(prior masa policy na PICKUP − visit masa searche na PICKUP).
(Baseline kontext: dwarf +2,7 pp, wood-elf −10,3 pp, skaven −3,2 pp.)

- **C-PASS ⇔** |delta_wood-elf(nové)| **≤ 0,5 × |delta_wood-elf(staré)|**
  (extrémní rasa se musí srovnat aspoň na polovinu), **a zároveň**
  delta_dwarf(nové) **≤ delta_dwarf(staré) + 1 pp** (trpaslíkovi se nesmí
  našeptávání pickupů zhoršit), **a zároveň** mechanismus: Spearmanova
  korelace přes PICKUP kandidáty mezi priorem nového ramene a (1 −
  p_fail_pickup z featury) **> 0** s p < 0,05 (prior musí ZÁVISET na
  skutečné šanci; to je přesně „začne záviset na AG aktéra" ze zadání §3-C).
- *Upřesnění zapsané 13:20 UTC, stále před existencí jakýchkoli výsledků
  (sběr běží, trénink nezačal):* priory z různých rozhodnutí nejsou přímo
  srovnatelné (závisejí na počtu kandidátů), Spearman se proto počítá na
  **uniform-relativním prioru** = prior × n_kandidátů daného rozhodnutí;
  p-hodnota permutačním testem (10 000 permutací), scipy jen jako křížová
  kontrola.

### 1.4 Agregát — předfiltr BEZ práva veta

Out-of-fold CE a top-1 obou ramen. Jen sanity: pokud se nepohne CE ani
žádná z metrik A/B/C, nemá cenu pokračovat. **Agregát sám nic neschvaluje
ani nevetuje.** Sekundární diagnostika (bez prahu): kontrafaktová citlivost
na capability featury (TV vzdálenost, baseline 0,033).

### 1.5 Rozhodovací pravidlo GO/NO-GO (předregistrováno)

- **GO ⇔ B-PASS a zároveň aspoň jedna z {A-PASS, C-PASS}** a žádná metrika
  se proti starému rameni nezhorší (END_TURN top-1 podíl, REPOS validita,
  |delta| u všech tří ras).
- **NO-GO ⇔** B selže (REPOS je ~45 % rozhodnutí — bez ní featury nedávají
  smysl implementovat), nebo se cokoli zhorší.
- Částečné výsledky (B projde, A i C selžou) = **GO s omezeným scope**
  (implementovat jen REPOS identitu cíle) — explicitně přípustný výstup.

---

## 2. Výsledky — staré vs nové featury na třech metrikách

**Korpus:** 48 her (32 dwarf–skaven, 16 wood-elf–skaven), **6 639 rozhodnutí**
(dwarf 2 396, skaven 2 910, wood-elf 1 333), medián 7 kandidátů/rozhodnutí.
Sběr 13:11–13:19 UTC (`scratchpad/feature_ab/rows_*.jsonl`, logy
`collect_*.log`), trénink dle protokolu §1.0. Kanonická čísla = běh v3
(`results_ab_v3.out/.json`); v1/v2 archivovány tamtéž.

**Oprava měřicí vady (v1 → v2, PŘED verdiktem, protokol nezměněn):**
harness ukládá kandidáty SEŘAZENÉ podle visitů a `np.argmax` láme remízy
k indexu 0 = top searche. Ploché rameno tím dostávalo trefy „zadarmo"
(staré rameno v1: REPOS hit 0,91 při spreadu ~1e-17; agregátní top-1 0,584).
Od v2 se kandidáti při načtení deterministicky zamíchají. V řádku „validita"
navíc od v3 uvádím i tie-robustní variantu (visit fractions při MCTS-100
často remizují): hit_maxset = top volba ramene padne do MNOŽINY
max-visit kandidátů, chance_maxset = E[m/n] (náhodný prediktor).

### 2.1 Metrika A — END_TURN ranking: **A-FAIL**

Jednoznačných END_TURN případů v korpusu: **11** (10 dwarf, 1 wood-elf);
mají 6–16 kandidátů, END_TURN visity 0,50–0,79.

| | staré featury | nové featury | práh |
|---|---|---|---|
| END_TURN top-1 podíl | 0/11 = 0 % | 0/11 = 0 % | rozdíl ≥ +30 pp |
| medián ranku END_TURN | 12,0 | 11,3 (per-seed 12/12/10) | ≤ 2 |

Rozdíl 0 pp, rank hluboko pod prahem. **Ani exploratorní 64-epoch běh
(annex, §2.4) to nezměnil** (oba 0 %, rank ~11). Interpretace: tyto situace
tvoří 0,17 % korpusu — CE ztráta je při imitaci prakticky netrestá; nové
featury dávají síti *možnost* vidět, že alternativy jsou drahé (p_fail),
ale imitační trénink na tak vzácné třídě signál nezvedne. Akční bias tedy
featury samy neopraví; potřebuje buď převážení vzácných „STOP" rozhodnutí
v tréninku, nebo víc takových situací v datech.

### 2.2 Metrika B — REPOSITION spread + validita: **B-FAIL**

946→**1 775** trpasličích rozhodnutí s ≥3 REPOS kandidáty (v tomto korpusu).

| | staré featury | nové featury | search (target) | práh |
|---|---|---|---|---|
| spread priorů (dwarf) | ~0 (1e-17, strukturní nula — negativní kontrola OK) | **0,0110** (per-seed 0,0111/0,0100/0,0120) | 0,0255 | ≥ 0,055 |
| spread priorů (all) | 0,0024 | **0,0144** | 0,0269 | — |
| validita: hit vs chance (dwarf) | 0,43 vs 0,21 (artefakt remíz, viz text) | **0,21 vs 0,21** | — | ≥ 1,5 × chance |
| validita tie-robust: hit_maxset vs chance_maxset (dwarf, v3) | 0,483 vs 0,476 | 0,479 vs 0,476 | — | (pomocná) |

- **Spread**: nové featury poprvé vůbec rozlišují REPOS kandidáty (od
  strukturní nuly na ~43 % spreadu searche u trpaslíka, ~54 % na all).
  Předregistrovaný práh 0,055 ale vycházel ze search spreadu 0,109
  ve starém korpusu (produkční config s dirichletem); v tomto čistším
  korpusu je search spread jen 0,027 — práh 0,055 by nesplnil ANI SEARCH
  SÁM. To je vada mé předregistrace (práh měl být relativní ke korpusu);
  přiznávám ji, ale práh po měření neposouvám.
- **Validita**: nové rameno trefuje top-REPOS searche na úrovni náhody
  (1,0× chance; tie-robustní varianta totéž). Vyšší hit starého ramene
  (0,43) je artefakt remíz ve visit targetech (plochý prediktor + argmax),
  ne signál — viz hit_maxset, kde jsou obě ramena na úrovni náhody.
  **64-epoch annex to nezlepšil** (hit 0,211, spread 0,0125) ⇒ není to
  nedotrénování: buď navržené featury cíle nenesou „KAM přesně" (absolutní
  souřadnice místo relačních vztahů ke klíčovým objektům — nosič, klec,
  screen), nebo to nenesou samotné targety (search na MCTS-100 REPOS
  kandidáty sám skoro nerozlišuje: chance_maxset 0,47 = max-visit množina
  pokrývá skoro polovinu kandidátů). Obojí uvádím jako hypotézy k dalšímu
  kroku; změřený fakt je, že **tato sada featur na těchto targetech
  validitu nezvedla**.

### 2.3 Metrika C — PICKUP vs AG: **C-PASS**

Loose-ball rozhodnutí s PICKUP kandidátem: dwarf n=162, skaven n=222,
wood-elf n=100; 882 PICKUP kandidátů. Delta = prior masa − visit masa
na PICKUP (out-of-fold, průměr přes seedy).

| | staré featury | nové featury | práh |
|---|---|---|---|
| delta wood-elf | −2,81 pp | **−1,21 pp** | \|new\| ≤ 0,5 × \|old\| = 1,40 pp ✓ |
| delta dwarf | −10,12 pp | **−9,41 pp** | ≤ old + 1 pp = −9,12 pp ✓ |
| delta skaven (kontext) | −16,36 pp | −14,59 pp | — |
| Spearman ρ(prior_unif-rel, 1−p_fail) | 0,132 | **0,362** (per-seed 0,31/0,36/0,41) | > 0, p < 0,05; p = 1e-4 ✓ |

Všechna tři předregistrovaná kritéria splněna. Mechanismus je přesně ten
zamýšlený: prior na PICKUP začal záviset na skutečné šanci hodu (AG +
tackle zóny + Sure Hands přes `calculatePickupTargetAt`). V 64-epoch
annexu se závislost dále prohlubuje (ρ 0,42 → **0,67**; delta dwarf
+1,2 pp → −0,3 pp). Pozn.: znaménka delt se v tomto korpusu liší od
baseline nasazené policy (ta byla +2,7 dwarf / −10,3 we) — retrénovaná
hlava na fairtest targetech PICKUP obecně podshoots; kritérium bylo
předregistrováno jako old-vs-new na stejném korpusu, takže srovnání platí.

### 2.4 Exploratorní annex (64 epoch, seed 1 — NENÍ součást brány)

Označeno předem jako exploratorní (`results_ab_v2_explor64.out`): 4× delší
trénink nechává A i B beze změny (A: 0 %, rank 11; B: spread 0,0125,
hit = chance) a C dále zlepšuje (ρ 0,67). Závěr „B není nedotrénování"
stojí na tomto běhu.

## 3. Agregát (doplněk bez práva veta)

| | staré featury | nové featury |
|---|---|---|
| out-of-fold CE | 1,8961 (per-seed 1,8960/1,8955/1,8969) | **1,8953** (1,8959/1,8953/1,8948) |
| out-of-fold top-1 | 0,416 (0,418/0,416/0,414) | 0,382 (0,375/0,385/0,387) |
| kontrafakt TV (capability swap) | 0,0116 | 0,0116 |

CE nepatrně lepší (−0,0008), top-1 o 3,4 pp horší — nové rameno rozbíjí
plochý tie-break artefakt (dřív „trefa" = shoda indexu 0), reálné pořadí
se za 16 epoch nesrovná. Předfiltr „nepohnulo se vůbec nic" nenastal
(C i spread se pohnuly), takže agregát nic neblokuje — a dle
předregistrace ani nesmí. Kontrafaktová citlivost na capability state
featury se nezvedla (očekávatelné: nové featury jsou AKČNÍ, kontrafakt
přepisuje STATE featury; capability-aware chování se projevilo v C přes
p_fail, ne přes state vstupy). Pozn.: staré rameno top-1 0,416 je blízko
produkčnímu plateau 42 % — konzistence s produkcí (i když korpusy nejsou
identické, viz §5), žádný náznak, že by offline protokol měřil jiný svět.

## 4. Doporučení GO/NO-GO + odhad práce

### Verdikt podle předregistrovaného pravidla (§1.5): **NO-GO**

- A-FAIL, B-FAIL, C-PASS. Pravidlo §1.5 vyžaduje pro GO **B-PASS** —
  B selhala, tedy NO-GO pro implementaci celé 16-featurové sady do enginu.
  Prahy po měření neposouvám, i když §2.2 ukazuje, že práh spreadu byl
  zkalibrován na nepřenositelné číslo (přiznaná vada předregistrace,
  ne důvod verdikt obcházet).
- Non-worsening klauzule: na cílených metrikách se nic podstatného
  nezhoršilo (A 0 %→0 %; B tie-robust validita 0,483→0,479 ≈ šum; |delta|
  u všech tří ras KLESLA). Agregátní top-1 kleslo o 3,4 pp, ale agregát je
  dle zadání předfiltr bez práva veta a pokles je z většiny rozpad
  tie-break artefaktu (§3).

### Co z toho plyne (doporučení, nevynucené branou)

1. **p(fail) + AG/TZ mechanismus prokazatelně funguje** (C-PASS; ρ 0,13 →
   0,36, v annexu 0,67; miskalibrace PICKUP klesla u všech tří ras).
   Je to první změřený případ, kdy policy prior závisí na schopnostech
   a situaci místo konstanty — přesně směr „generic over skills".
   Navrhuji ale NEIMPLEMENTOVAT izolovaně hned: samostatná brána pro
   „jen p_fail featury" nebyla předregistrována a řezat sadu post-hoc
   podle výsledků je přesně to, co si zadání zakázalo. Správný postup =
   nový, předem registrovaný mini-A/B „jen featury [23-25]+[34-38]
   (capability+risk), bez identity cíle" — offline, 1 h práce, prahy
   podle TOHOTO korpusu (relativní), pak teprve engine.
2. **REPOS identita cíle v absolutních souřadnicích nestačí** (B): spread
   se objevil (0→~50 % search hodnoty), ale volba KAM zůstala na náhodě
   i při 4× tréninku. Dvě hypotézy k rozseknutí příštím krokem:
   (a) featury cíle musí být RELAČNÍ (vzdálenost k nosiči/kleci/screen
   lajně, marking hodnota cílového pole), ne absolutní souřadnice;
   (b) targety jsou vadné — search na MCTS-100 REPOS kandidáty sám
   nerozlišuje (max-visit množina pokrývá 48 % kandidátů; chance_maxset
   0,476). Test (b) je levný: přesbírat korpus s MCTS-400 (páka 5 už to
   stejně navrhla) a přeměřit search_spread; když zůstane ~0,03, je
   problém ve featurách (a).
3. **END_TURN bias featury nevyřeší** (A): 11 případů v 6 639 rozhodnutích
   CE-trénink nezvedne. Kandidátní cesty: převážit vzácné jednoznačné
   „STOP" situace v imitaci (loss weighting), nebo cílený sběr takových
   situací. Patří k páce 5 (rozvrh učení), ne k páce 2 (featury).

### Odhad práce při případném GO (sepsán před výsledky, platí beze změny)

**Odhad práce při GO (implementace do enginu):**
1. `engine/include/bb/action_features.h`: NUM_ACTION_FEATURES 23 → 39;
   `POLICY_INPUT_SIZE` se zvedne automaticky (96 → 112).
2. `engine/src/macro_actions.cpp` `extractMacroFeatures`: doplnit výpočet
   16 nových featur — **kód už existuje a je ověřený v harnessu**
   (`scratchpad/feature_ab/diag_feature_ab_collect.cpp`,
   `computeNewFeatures` + `effectiveTarget`), jde o přenos ~150 řádků
   volajících výhradně existující API (`countTacklezones`,
   `calculateDodgeTarget`, `calculatePickupTargetAt`, `pickApproachStep`,
   `countAssists`, `getBlockDiceInfo`). Mikro-extractor
   (`action_features.cpp`) nechat být, nebo dorovnat později.
3. `python/blood_bowl/policy_trainer.py`: NUM_ACTION_FEATURES konstanta
   (23 → 39) — trenér je na dimenzi jinak agnostický.
4. Změna vstupní dimenze resetuje akumulovanou policy hlavu — dle reportu
   20260811 §2/#1 o nic nepřicházíme (policy je na REPOS plochá); retrain =
   16 epoch imitace v rámci běžné iterace.
5. Gtest na nové featury (REPOS kandidáti musejí dostat odlišné vektory;
   PICKUP p_fail musí klesat s AG) + jedna ostrá iterace s gate +
   race_guard + M1.

Celkem: **~1 den práce + 1 noční okno na ostrou iteraci** (shodné s odhadem
v reportu 20260811). Žádné rasové labely — vše ze schopností a situace.

## 5. Co se nepodařilo a proč / limity

- **Nešlo použít starý korpus** (plán §3 starého reportu) — neobsahuje
  identitu maker (§0). Stálo to nový sběr (~8 min CPU × 3 procesy,
  nice -19, era běhy nedotčeny — ověřeno před i po).
- **v1 měření mělo vadu** (tie-break leak přes pořadí kandidátů, §2) —
  opraveno PŘED vyhodnocením brány; v1 výstupy archivovány
  (`results_ab.json/.out` bez suffixu), verdikt stojí na v3.
- **Vady mé předregistrace, přiznané, prahy neposunuty:** (i) B-práh
  spreadu 0,055 absolutně přenesen z korpusu s jinou search konfigurací —
  v tomto korpusu ho nesplní ani samotný search (0,027); (ii) A-metrika
  je podvzorkovaná (11 případů; práh na podílu je při tomto N hrubý);
  (iii) chance model B-validity nepočítal s remízami visit targetů —
  doplněna tie-robustní varianta, obě čísla v tabulce.
- **Dvojí spuštění waiteru** launch_train.sh (vadný pgrep vzor) — chyceno
  do ~10 s, oba zabity, opraveno + přidán marker `train_started`; žádný
  výsledek tím nevznikl dvakrát. (Stejný vzor chyby jako páteční launcher.)
- Analýza běžela třikrát (v1 → v2 oprava shuffle, v2 → v3 tie-robustní
  metrika); všechny verze archivovány. v3_explor64 doběhl a čísla se
  s v2_explor64 shodují (ověřeno; navíc hit_maxset: old 0,489 / new 0,472
  vs chance 0,476 — i při 64 epochách validita na náhodě, konzistentní
  se závěrem §2.2).
- **Éra pravidel:** sběr běžel už na enginu PO opravách D-vlny 1 (dodge +1,
  leap, pass range, interception koridor, throw-in, Thick Skull) i po fixu
  balíku G (918fc58). Baseline čísla §1.4 starého reportu (0/9, spread
  0,000/0,109, ±pp na PICKUP) pocházejí z PŘEDCHOZÍ éry pravidel a z
  NASAZENÉ policy — s tabulkami §2 se nedají srovnávat 1:1; platné je jen
  old-vs-new rameno uvnitř tohoto reportu (stejná éra, stejné targety).
- Balík G je v enginu, ale self-play trénink na post-fix datech ještě
  neproběhl; tento A/B měří jen reprezentační stránku (páka 2), ne datovou
  (páka 4 — attrition signál ve featurách/targetech stále není).
- p(fail) featura je odhad: u pohybových maker počítá jen PRVNÍ dodge krok
  (`pickApproachStep` + `calculateDodgeTarget`), ne celou trasu; PASS jen
  hrubě z AG házejícího; CAGE/FOUL konstanty. Pro účel A/B (unese síť
  rasově správnou opatrnost?) to stačí — monotónie vůči AG/TZ/kostkám je
  zachována engine výpočty.
- Targety v novém korpusu jsou z fairtest searche (dirichlet 0) — ČISTŠÍ
  než produkční tréninkový korpus (dirichlet 0,3 šumí do targetů,
  `run_iteration.py:137`). Absolutní čísla top-1/CE proto nejsou přímo
  srovnatelná s produkčním plateau 42 %; A/B srovnání ramen to nijak
  nekřiví (obě ramena mají stejné targety).
- Trénink obou ramen je offline nápodoba produkčního traineru
  (`NeuralPolicyTrainer`: H=64, ReLU, minibatch 32, lr 0,01, 16 epoch);
  přenositelnost do C++ inference je triviální (stejná architektura jako
  `weights_best_policy.json`).
