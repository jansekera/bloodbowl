# AUDIT: A/B VERDIKTY PROTI NULOVÉMU RAMENI (P20)
*(Fable, 17.08.2026; zadání: které dosavadní verdikty by při čtení proti
nulovému rameni dopadly jinak. Data: `dauntless_ab_20260814/`,
`gate_measure_20260813/`, `debt_measure_20260812/`, `era_measure_20260810/`,
`ab_measure_20260811/`, `arm_*.json`. Nic se nespouštělo, jen se četlo.)*

## 0. Ověření nálezu z 15.08. — POTVRZEN, včetně příčiny

* `cand_daunt = 0` ve **všech 12 000** hrách `dw-sk` + `orc-sk`; v `dw-orc`
  nenulové ve **všech 1 500/1 500** řádcích na shard. Čítač sedí na jediné
  odlišné větvi (`engine/src/macro_actions.cpp:156`). Kontrolní ramena tedy
  prokazatelně běžela na stejném kódu.
* Seedování (`diag_f1_cage_advance_harness.cpp:302–348`): hra
  `seed*2 + orient`, MCTS home `seed*2654435761u + 11 + orient`, away
  `… + 47 + orient`. **Pár = dvě různé hry na spřízněných seedech**, přesně
  jak tvrdí P20.
* Empiricky: **korelace mezi oběma polovinami páru = −0,017** (n = 6 000).
  SD páru 0,520 vs 0,576, kdyby šlo o dvě nezávislé hry ⇒ párování redukuje
  rozptyl o < 10 %. **Orientační pár je prakticky dvě nezávislá měření**;
  jeho jediná reálná funkce je vyrušení výhody domácí strany/rasy
  *v očekávání* (ne po párech).

---

## 1. Empirická podlaha párové delty (12 000 her, rameno mrtvé)

| běh | n párů | delta (pp) | SD páru | SE (pp) | z |
|---|---|---|---|---|---|
| dw-sk_s0 | 750 | +4,07 | 0,531 | 1,94 | |
| dw-sk_s1 | 750 | +3,13 | 0,541 | 1,98 | |
| dw-sk_s2 | 750 | +0,40 | 0,549 | 2,00 | |
| dw-sk_s3 | 750 | +1,53 | 0,536 | 1,96 | |
| **dw-sk celkem** | **3 000** | **+2,28** | **0,539** | **0,98** | **+2,32** |
| orc-sk_s0 | 750 | −3,80 | 0,488 | 1,78 | |
| orc-sk_s1 | 750 | +2,13 | 0,497 | 1,81 | |
| orc-sk_s2 | 750 | −1,80 | 0,494 | 1,80 | |
| orc-sk_s3 | 750 | −1,73 | 0,518 | 1,89 | |
| **orc-sk celkem** | **3 000** | **−1,30** | **0,499** | **0,91** | **−1,43** |
| **sdruženě** | **6 000** | **+0,49** | **0,520** | **0,67** | |

* **SE, které harness tiskne, je poctivá** — počítá se z týchž delt a
  mezishardový rozptyl je s ní konzistentní (χ² = 7,8 na 6 df, p ≈ 0,25).
  Chyba tedy **není** v podhodnocené SE, ale v tom, (a) že pár nenese žádnou
  redukci rozptylu, a (b) že se delta čte proti nule prahem ~2 SE, který
  mrtvé rameno překročí ve ~2 % běhů na matchup — a přesně to se stalo.
* **Rozdíl dw-sk − orc-sk = +3,58 pp, SE 1,34, z = +2,67 (p ≈ 0,008)** —
  formálně na hraně průkaznosti. Rozšíření o starší mode-2 nulové běhy
  (era/ab/debt; po odstranění bit-identických duplikátů 13 200 párů) dává
  dw-sk **+2,33** (10 běhů, 9 kladných!) vs orc-sk **−1,64** — ale tyto
  starší běhy sdílejí seedy, takže jejich sdružená SE je nadhodnocená
  spolehlivost. **Poctivý závěr: buď smůla 1 : 125, nebo malá systematika
  vázaná na orientaci (parita herního seedu `seed*2+orient` / nízký bit MCTS
  seedu), závislá na matchupu, řádu +2 pp u dw-sk.** Rozhodnout to z existujících
  dat nejde; oprava v §4 ji každopádně odstraní konstrukcí.

---

## 2. Přepočet historických verdiktů

Metr: efekt musí přežít srovnání s tím, co produkují prokazatelně mrtvá
ramena téhož aparátu (realizace ±2,3 pp na n = 3 000, ±5,3 pp na n = 400).

| verdikt (datum, data) | původní čtení | přepočet | výrok |
|---|---|---|---|
| **Brána klece ZAMÍTNUTA** (13.08., `gate_measure_20260813`, 1 500 párů/matchup) | dw-we −0,0297 = −2,0σ ⇒ „NEPROŠLO, škodí" | dw-we **−2,97 ± 1,45 (z = −2,05)**; dw-sk +0,83 ± 1,39; orc-sk +1,00 ± 1,30. Mrtvé rameno na dw-sk udělalo **+2,28 (z = +2,3)** a jeden 400párový null −5,25 (z = −2,07). −2,05σ je uvnitř toho, co nuly reálně produkují. Navíc vlastní historie brány kolísá o ±3 pp mezi běhy (10.–11.08. „vnitřní A/B" dw-sk +3,0 → +3,3 pp; 13.08. +0,83). | **Část „škodí" PADÁ** — škodlivost není doložena. **Část „nezapínat" PLATÍ** — požadovaný přínos ≥ +3 pp se neukázal (dw-sk +0,83, 95% CI −1,9…+3,6). Správné čtení: **NEROZHODNUTO, břemeno důkazu zůstává na bráně.** |
| **P13 Dauntless PROŠLO** (14.08., `dauntless_ab_20260814`, 3 000 párů) | dw-orc +4,08 ⇒ PROŠLO | proti sdružené nule: **+4,08 − 0,49 = +3,59 pp, SE 1,04 ⇒ 3,4σ**; SD páru dw-orc jen 0,438 ⇒ z proti nule +5,1 | **PLATÍ** (jediný verdikt, který proti podlaze bez debat přežije; velikost efektu ale je 2–4 pp, ne nutně 4). Formální práh „≥ +2 pp" ovšem splnilo i mrtvé rameno ⇒ práh sám je vadný. |
| **Šumové dno ±5,3 pp / 400 párů** (13.08., `debt_measure`) | „12 nezávislých null-testů" | SD páru potvrzena na 6 000 nezávislých párech: **0,50–0,54**. ALE z 12 běhů bylo jen **8 unikátních** (dw-sk B≡C; orc-sk B≡C≡D; A ≡ `ab_*_post` — bit-identické) a všechny sdílejí seedy ⇒ „12 nezávislých" neplatilo | **ČÍSLO PLATÍ, zdůvodnění ne.** Dovětek „harness je deterministický ⇒ párová srovnání platí" byl non-sequitur: determinismus hry ≠ síla páru. |
| **Balík G „pojistka OK"** (11.08., chess +1,2…+2,6 pp na ~400 párech) | pojistka prošla | vše hluboko pod podlahou ±5,3 pp | **PADÁ jako důkaz** (už přiznáno 13.08.; potvrzuji). Atriční část (DEAD/hru 6×, dno ±0,01) **drží**. |
| **ERA D-vlna 1 „prošla pojistkou, ale dw-sk těsně"** (11.08., `era_measure`, 400 seedů) | z = −1,2778 proti prahu −1,28, „o dvě tisíciny" | dw-sk −4,75 pp na škále páru, SD **0,74** (mezistavbové srovnání je ještě hlučnější než pár!); kontrola orc-sk −6,38. Obě uvnitř podlahy | **PLATÍ, a silněji než tehdy**: ani to „těsně" nebyl signál. Mechanistická část (attrition KO 0,60→0,50, TD hbitých +0,05–0,09) stojí na počítaných veličinách s malým šumem — drží. |
| **Oprava rozestavení / O8** (11.08., `ab_measure` post vs pre) | trpaslík výrazně lepší | dw-sk **+33,8 pp (z = +9,1)**, orc-sk +22,3 (z = +6,2), dw-we +6,9 (z = +1,9) | **PLATÍ** — o řád nad podlahou. (dw-we samo o sobě by neprošlo.) |
| **ADVANCE floor „draws 63→47 %"** (16.07., `arm_advfloor_*`, 300 seedů) | shipnuto | draws **−16,3 pp, SE 4,2, z = −3,9**; chess +2,5 (z = +0,96, nikdy netvrzeno) | **PLATÍ.** |
| **hasacted fix „bez regrese"** (15.07., 300 seedů) | shipnuto (parita) | chess −1,8 ± 2,6; draws +5,0 ± 3,9 | **rozhodnutí PLATÍ** (parita se shipuje pro správnost), ale měření nic nedoložilo — dno ±5,2 pp. |
| **Páky policy: leverA2 / leverC / REPOSITION / pb** (16.–17.07., 50–800 seedů) | „bez efektu", NO-GO | všechna \|z\| ≤ 1,7; detekovatelné bylo až ~4–8 pp | **rozhodnutí NO-GO PLATÍ** (zapnutí vyžaduje pozitivní důkaz), ale výrok „efekt není" je **NEROZHODNUTELNÝ** — nikdy nešlo odlišit 0 od ±4 pp. |
| **SCORE-availability patch** (20.07., 150 seedů) | shipnuto | chess **−3,3 ± 2,2 (z = −1,55)** | **NEROZHODNUTELNÉ** — jediný starý A/B, kde bodový odhad míří PROTI shipnuté změně a n na rozhodnutí nestačí. Kandidát na re-test, pokud je změna stále v produkci. |
| **item7 top-2, item10 prior-floor** (20.07., 150 seedů) | shipnuto dle mechanismu | z ≈ 0; dno ±8 pp | rozhodnutí **PLATÍ** (mechanistické), měření bez výpovědi. |
| **Chain push T4.4 ZAMÍTNUTO** (12.08.) | zamítnuto | stálo na počítaných tempech (0,345 úniku/10 kol vs potřeba ~1), ne na chess; chess delty ±2,4 pp jsou šum | **PLATÍ** — párováním nedotčeno. |
| **Fair-gate 4/4 REJECTED** (22.07.) | — | jiný aparát (training gate, `training_gatefix_20260722.log`); párová ramena neexistují | **CHYBÍ DATA — nehodnotím.** |
| **Featurový A/B NO-GO** (10.08.) | NO-GO | offline klasifikace, ne herní A/B | mimo rozsah, párováním nedotčeno. |

**Souhrn: jediný verdikt, který se PŘEVRACÍ, je „brána klece škodí" (dw-we
−2,0σ).** Zbytek buď přežívá (Dauntless, O8, advfloor, éra, chain), nebo už
byl degradován 13.08. a degradace se potvrzuje (balík G, všechna měření na
≤ 400 párech), plus jeden nový kandidát na re-test (SCORE-availability).

---

## 3. Kolik párů je doopravdy potřeba (SD páru 0,52; dw-orc 0,44)

| kritérium | 1 pp | 2 pp | 3 pp |
|---|---|---|---|
| „efekt = 2 SE" (kritérium předregistrace; **síla jen 50 %**, falešné PROŠLO mrtvým ramenem ~2,3 %/matchup) | 10 800 | 2 700 | 1 200 |
| α = 5 %, síla 80 % | 21 200 | 5 300 | 2 360 |
| totéž, čteno jako rozdíl dvou matchupů (efekt − null) | ×2 | ×2 | ×2 |

**Předregistrace 14.08. („SD 0,54–0,56 ⇒ ~3 000 párů na 2 pp") aritmeticky
sedí** — SD odhadla správně (empiricky 0,50–0,54). Nesedí kritérium: 3 000
párů dává na 2pp efekt jen ~50% šanci detekce a mrtvému rameni ~2% šanci
„PROŠLO" na každém matchupu. Bez zásahu do harnessu by poctivé A/B na 2 pp
stálo **~5 300 párů + nulový matchup**, tj. cca dvojnásobek noci.

---

## 4. ⭐ Jde pár utáhnout? ANO — a je to změna o tři `+ orient`

**Diagnóza:** současné párování je fakticky **dvě nezávislá měření** (korelace
−0,017). Přitom červencové A/B (`arm_*.json`) **stejnou hru mezi rameny
sdílely** a ukazují, co to dělá: podíl bit-identických her 22–91 % a u
`chain_h2` (91 % identických) **SE 0,6 pp na pouhých 400 párech** — 4,5×
lepší než dnešní pár při stejném n.

**Co změnit** (párovací smyčka `diag_f1_cage_advance_harness.cpp:302–348`):
odstranit `+ orient` ze všech tří seedů —

```
hra:  seed * 2                      (místo seed*2 + orient)
home: seed * 2654435761u + 11u      (místo … + 11u + orient)
away: seed * 2654435761u + 47u      (místo … + 47u + orient)
```

MCTS seed se váže na **stranu hřiště** (home/away), ne na rameno — config se
dál prohazuje přes `candHome`. Obě orientace pak hrají **tutéž hru bit po
bitu** až do prvního místa, kde se cand a base rozhodnou jinak.

**Co měnit nelze / netřeba:** rasy se u orientace neprohazují už dnes (home
roster zůstává home; prohazuje se jen, které RAMENO hraje kterou rasu) —
sdílení seedu tomu nijak nebrání. Determinismus hry při daném seedu je
doložený (bit-identické duplikáty v §2 vznikly právě z něj).

**Co to dá:**
* **Mrtvé rameno ⇒ delta ≡ 0 exaktně.** Nulové matchupy přestanou stát
  6 000 her na statistiku — stačí ~100 párů jako kontrola implementace
  („jsou řádky bit-identické?"). Zároveň zmizí konstrukcí i podezřelá
  orientační systematika z §1.
* **Redukce SE na měřeném matchupu** škáluje ~√(podíl divergentních her):
  z červencových CRN dat SD 0,12–0,51 podle toho, jak často změna zasáhne
  (proti dnešním 0,50–0,56). Vzácně střílející ramena (hand-off P5, Wrestle
  P17): identických her ~70–90 % ⇒ **3–4× menší SE ⇒ ~10× méně párů**.
  Všudypřítomná ramena (Dauntless na dw-orc, ~1 700 nabídek/hru): úspora
  malá (~15–25 % párů), ale nic se nezhorší.
* Zadarmo navíc: přesná lokalizace efektu (delta ≠ 0 ⇔ rameno v té hře
  změnilo rozhodnutí) — `n_nonzero` je nový povinný řádek souhrnu.

**Past, na kterou se zadání ptá:** zavádí exaktní nula závislost mezi páry?
**Ne** — seedy párů jsou disjunktní, páry zůstávají i.i.d. a výběrový SD delt
zůstává nestranný. Dvě skutečné výhrady: (a) při málo nenulových deltách
neplatí normální aproximace na celkovém n — číst z proti SE počítané z dat
(ta se zmenší sama) a hlásit `n_nonzero`; pod ~30 nenulovými páry
nerozhodovat; (b) průměr se dál počítá **přes všechny páry včetně nul** —
dělit jen nenulovými by odhad vychýlilo. CRN teoreticky může rozptyl i
zvětšit při záporné korelaci ramen — u nás nehrozí (hry sdílejí trajektorii,
korelace je z konstrukce ≥ 0).

---

## 5. DOPORUČENÍ (jedno)

**Před dalším nočním během odstranit `+ orient` ze všech tří seedů
v `diag_f1_cage_advance_harness.cpp` (řádky ~340, ~343, ~348), ověřit
smoke-runem mode 2 na ~50 párech, že všechny delty vyjdou exaktně 0, a do
SUMMARY přidat `n_nonzero` párů.** Tím se nulový matchup zlevní z 6 000 her
na kontrolu implementace, vzácná ramena (P5, P17) se stanou vůbec poprvé
měřitelnými (~10× méně párů) a zmizí i podezření na orientační systematiku
+2 pp z §1. Prahy pak číst takto: efekt ≥ 2 pp žádat při α 5 % / síle 80 %
(~5 300 párů dnešního typu, ale s CRN reálně výrazně méně — kolik přesně,
řekne první běh sám přes `n_nonzero`).
