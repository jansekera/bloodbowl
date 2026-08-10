# ZADÁNÍ pro Fable: MÁ UČITEL VŮBEC CO UČIT? (10.08.2026, večer)

Navazuje na **tvůj vlastní** `fable_offline_feature_ab_report_20260810.md`
(verdikt NO-GO) a jeho §2.2, kde jsi našel, že **search na MCTS-100
u REPOSITION sám skoro nerozlišuje** — max-visit množina pokrývá 48 %
kandidátů, `chance_maxset` 0,476.

Uživatel to označil za **vleklejší problém**, a má pravdu: policy se učí
IMITOVAT search, imitace nemůže překonat učitele, a na největší třídě
rozhodnutí učitel nemá názor. To není featurový problém, to je **strop
celého imitačního přístupu**.

## Tři úkoly, v tomhle pořadí

### 1. ⭐ NEJDŘÍV: JE SLEPÝ SEARCH, NEBO VALUE FUNKCE? (nejlevnější, rozsekne to)
Tenhle test v tvém reportu není a je **levnější než sběr s MCTS-400**.

Tři možnosti, které se vylučují:
* **(a) málo iterací** — 100 návštěv na ~15 kandidátů = 2-5 na kandidáta,
  rozdíly v šumu vzorkování;
* **(b) value funkce ta pole nerozliší** — pak budou návštěvy ploché
  **bez ohledu na rozpočet**, protože víc rolloutů nerozlišitelných pozic
  dá pořád nerozlišitelné hodnoty;
* **(c) ty pozice se opravdu neliší** — pak „nemá co učit" není vada.

**Test:** vzít REPOSITION kandidáty z korpusu, **ohodnotit výsledné pozice
value funkcí NAPŘÍMO** (`evaluateLeaf` / `ValueFunction::evaluate`) a
změřit rozptyl hodnot mezi kandidáty téhož rozhodnutí. Porovnat
s rozptylem návštěv.
* rozptyl hodnot ≈ 0 ⇒ **(b)**, a **MCTS-400 nepomůže** — bottleneck je
  value funkce, ne rozpočet;
* rozptyl hodnot slušný, ale návštěvy ploché ⇒ **(a)**, tedy rozpočet;
* rozptyl hodnot ≈ 0 **a zároveň** doktrinálně by na tom mělo záležet
  ⇒ silný argument, že value funkce je slepá na postavení.
**Předregistruj, co bude znamenat „slušný rozptyl", než se podíváš.**

⚠️ **(c) prosím neber jako pravděpodobné bez důkazu:** u pomalého týmu
postavení rozhoduje (trpaslík ve špatném poli se z toho nedostane, elf
ano). Když vyjde, že se pozice neliší, je to nález o **našem modelu**,
ne o hře.

### 2. Sběr s MCTS-400 a přeměření search spreadu
Tvůj vlastní návrh z §4/2(b). **Pusť ho odpojeně** a nech běžet; pokud
test 1 ukáže (b), bude to jen potvrzení, ne rozhodnutí.
Kritérium máš vlastní: *„když search_spread zůstane ~0,03, je problém
ve featurách"*.

### 3. Mini-A/B jen pro capability/risk featury (mechanismus C)
Tvůj návrh z §4/1. **Nově předregistrované prahy, relativní k TOMUTO
korpusu** (nepřenášet 0,055 z jiného, to byla přiznaná vada minule).
Jen featury capability+risk, **bez identity cíle** — tedy bez toho, co
u B selhalo. Offline, ~1 h.
⚑ Důvod, proč to nejde vyříznout post-hoc z minulé sady: řezat podle
výsledků je přesně to, co sis sám zakázal — proto nová registrace.

## Provozní pravidla (stejná jako minule)
* **Vše dlouhé ODPOJENĚ** (`setsid nohup … & disown`), log do souboru.
  Kdyby to nestihlo, **logy čteme zítra**.
* **CPU JE OBSAZENÉ:** do ~19:00 UTC běží 6 procesů srovnání ér
  (`era_measure_20260810/`), pak na ně naváže M1 přes noc
  (`m1_measure_20260810/`). **Nezabíjej je, nesoutěž s nimi** — sběr
  s MCTS-400 je 4× dražší než minule, tak `nice -19` a ať si to jede.
* **NEMĚŇ produkční kód.** Skripty do `diag_*` nebo `scratchpad/`.
* **Launchery IDEMPOTENTNÍ** (marker + vlastní běžící instance).
* **Nevymýšlej si čísla**; co nejde ověřit, označ jako hypotézu.
* Anglické názvy skillů a herních pojmů, nepřekládat.

## Deliverable
`evidence/fable_teacher_signal_report_20260810.md`:
1. **předregistrované prahy** (zapsat před výsledky);
2. **verdikt (a) / (b) / (c)** s čísly — tohle je hlavní výstup;
3. výsledek MCTS-400, pokud doběhne;
4. výsledek mini-A/B pro C + GO/NO-GO na implementaci capability featur;
5. **co z toho plyne pro imitační přístup jako takový** — když je učitel
   slepý, je otázka, jestli se má policy učit imitací, nebo něčím jiným.
   Tohle je ta „vleklá" část a zajímá nás nejvíc.
