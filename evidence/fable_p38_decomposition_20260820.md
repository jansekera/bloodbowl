# P38 DEKOMPOZICE 20.08.2026 (Fable) — kolik je klec a kolik „nosič smí uhnout do strany"

Zadání: `fable_brief_p38_decomposition_20260820.md`. Noc P38 dala **+0,0827 ± 0,0065
(12,8σ, dvoustranně)** — rozdělit mezi (A) boční volnost, (B) kritérium klece,
(C) obejití záložní smyčky (= P39). Skript: `diag_p38_decomposition_20260820.py`
(python replika `expandAdvance` + `cageScoreForSquare` z commitu `74f153f2`),
korpus `corpus_baseline_20260819_data` (3 000 her). Noční řádky:
`cageadvance_20260819/dw-we_s*/diag_cageadvance_rows.jsonl` (13 600 řádků, 6 800 párů).

## ⭐ Opravy premisy od uživatele (přišly během práce, cituji)

1. **„Klec pomáhá i elfům… klec má podle mne pomáhat VŠEM."** — klec je univerzální
   ochrana nosiče, ne trpasličí doktrína. ⇒ rasová neutralita zisku (+0,098 trpaslík /
   +0,095 elf) **není důkaz proti (B)** — je to signatura, kterou pravidlo klece samo
   předpovídá. Hypotéza (B) tím byla před měřením NEROZHODNUTÁ. (Původní úvaha
   v zadání „elf klec nehraje ⇒ není to klec" byla koordinátorova a byla špatně.)
2. **„Pravidlo je univerzální, ale ROZPOČET je rasový"** — trpaslíkovi klec + blitz +
   zeď sežere celé kolo (nosič 1 + rohy 4 + blitz 1 = 6 ze 7,03 volných těl). Strop
   „klec dosažitelná v 95,6 %" i klauzule (1) v `cageScoreForSquare` počítají těla,
   která na rohy **dosáhnou**, ne těla **volná od jiné povinnosti** ⇒ měřit dvakrát,
   stropy vykázat po rasách.

---

## (1) Návrh placebo ramene — mode 7 (⛔ NEIMPLEMENTOVÁNO, jen návrh)

Identické s P38 ve všem, liší se **jedině vypuštěným `cageScoreForSquare`**.

`engine/src/macro_actions.cpp`:
- za ř. 178 (`setCageAwareAdvanceArm`): přidat `thread_local bool
  g_cageAdvancePlacebo[2]` + setter/getter `setCageAdvancePlaceboArm` /
  `cageAdvancePlaceboArm` (stejný vzor per-side jako ř. 169–178);
- ř. 1377: `if (cageAwareAdvanceArm(state.activeTeam)) {` →
  `const bool placebo = cageAdvancePlaceboArm(state.activeTeam);`
  `if (cageAwareAdvanceArm(state.activeTeam) || placebo) {`;
- ř. 1403: `if (cageScoreForSquare(state, carrier, cand) < 0) continue;` →
  `if (!placebo && cageScoreForSquare(...) < 0) continue;` — **jediný funkční rozdíl**;
- ř. 1376–1411 jinak beze změny: týž rozpočet `steps` (`carrierStallAwareSteps`),
  týž pás `prog >= maxProgress - 1` a `prog >= 1`, týž TZ filtr (ř. 1400–1402),
  totéž obejití záložní smyčky přes `armChoseSquare` (ř. 1425), **týž čítač**
  `++g_cageAwareAdvancePicks` na ř. 1409, který tiká jen při `best != target`
  (= skutečná změna volby, lekce „nulové rameno" 17.08.).

`diag_f1_cage_advance_harness.cpp`:
- ř. 244–249: `: (mode == 7) ? 127'000'000u` (disjunktní seedy);
- ř. 251–257: label `mode == 7 ? "P38-PLACEBO: tez pole, bez kriteria klece"`;
- ř. 277: `rowsName`: mode 7 → `"diag_cageadvance_placebo_rows.jsonl"`;
- ř. ~400–406 (nastavení mode 6): analogicky `bb::setCageAdvancePlaceboArm(HOME,
  mode == 7 && candHome)` / `(AWAY, mode == 7 && !candHome)` + shodit po hře;
- ř. 440–441: `pr.armEvents += … (mode == 6 || mode == 7) ? candCage …`
  (čítač je sdílený `takeCageAwareAdvancePicksInSearch`).

## (2) Predikce PŘED během (dvoustranná delta = `chessCandHome + chessCandAway − 1`)

Do `evidence/night_prereg_*.preds` pro mode 7 registruji:

- **placebo: +0,090**, pásmo **[+0,04; +0,14]** — korpus říká, že placebo odblokuje
  nosiče ve VÍCE kolech než P38 (viz bod 3), takže kanály (A)+(C) má placebo silnější;
- **rozdíl placebo − P38: +0,01**, pásmo **[−0,02; +0,06]** (srovnání průměrů dvou
  běhů, ne CRN párů — seedy jsou disjunktní);
- **rozhodovací pravidlo:** placebo ≥ P38 − 0,015 ⇒ nález se jmenuje **„nosič neumí
  uhnout do strany"** (A)+(C) a kritérium klece neneslo měřitelnou část; placebo
  < P38 − 0,015 ⇒ (B) nese aspoň část a klec si jméno zaslouží;
- čítač: placebo musí hlásit **VÍC** picků/hru než P38 (korpus: najde pole v ~98 %
  kol vs ~60 %). Kdyby ne, replika nebo implementace lže — zastavit čtení výsledku.

## (3) Co jde rozhodnout z KORPUSU bez noci — ZMĚŘENO

Replika obou ramen nad všemi 3 000 hrami, obě strany. Jmenovatele vypsané.
(a) = klauzule těl jako v rameni dnes (dosah, Chebyshev ≤ ma);
(b) = navíc vyloučena těla s jinou povinností (markují stojícího soupeře
NEBO měla v kole vlastní událost).

| | trpaslík | wood-elf |
|---|---|---|
| kol se stojícím nosičem | 17 728 | 13 401 |
| VŠE: placebo najde pole | 17 523 = 98,8 % | 12 912 = 96,4 % |
| VŠE: P38(a) najde pole | 10 625 = 59,9 % | 8 234 = 61,4 % |
| VŠE: P38(b) najde pole | **3 504 = 19,8 %** | **2 439 = 18,2 %** |
| P38(a) změní cíl proti aritmetice | 10 411 = 98,0 % z 10 625 | 8 098 = 98,3 % z 8 234 |
| P38(a) vybere TOTÉŽ pole co placebo | 3 978 = 37,4 % z 10 625 | 3 596 = 43,7 % z 8 234 |
| **IDLE kola (Δx=0, nosič bez události)** | **5 213** (volný 3 302 / TZ 1 911) | 4 663 (2 491 / 2 172) |
| IDLE: placebo najde pole | **5 106 = 97,9 %** | 4 392 = 94,2 % |
| IDLE: P38(a) najde pole | **3 070 = 58,9 %** | 2 492 = 53,4 % |
| IDLE: P38(b) najde pole | 1 792 = 34,4 % | 1 397 = 30,0 % |

Populace IDLE u trpaslíka sedí přesně na P39 (5 213 / 3 302 / 1 911 =
`carrier_idle_20260820.md`) ⇒ replika stojí na téže množině.

**Čtení:**

- ⭐⭐⭐ **(C) nepotřebuje klec — a klec mu dokonce PŘEKÁŽÍ.** V kolech P39 by
  placebo našlo pole v **97,9 %**, P38 jen v **58,9 %**: kritérium klece rameno
  ve **2 z 5 idle kol blokuje** (vrací −1 pro všechna pole ⇒ `armChoseSquare=false`
  ⇒ záložní smyčka ⇒ nosič stojí dál). Kanál „odblokování nosiče" je tedy celý
  (A)+(C); podíl klece na NĚM je nula až záporný.
- **(B) může žít jedině ve VOLBĚ pole, ne v tom, JESTLI se nosič hne.** Tam, kde P38
  pole najde, vybere jiné pole než placebo v 62,6 % (trpaslík). Jestli ta jiná volba
  něco stojí/vynáší, korpus ocenit neumí — to rozhodne jedině noc mode 7.
  ⚠️ Proto predikce výše není „placebo = P38", ale pásmo s pravidlem.
- ⭐⭐ **Samostatný nález (uživatelův bod o rozpočtu potvrzen):** vyloučení těl
  s jinou povinností sráží plnitelnost klece **~3×** (trpaslík 59,9 → 19,8 %,
  elf 61,4 → 18,2 %). Strop „95,6 % kol" je počítán z dosahu a je **hrubě
  nadhodnocený**; `cageScoreForSquare` si dnes kupuje rohy za těla, která markují
  nebo už jednala. ⚠️ Rozdíl trpaslík/elf je v TOMHLE proxy malý — proxy nevidí
  zeď/screen (v korpusu žádná značka povinnosti není); rozhodl by explicitní model
  povinností těl, který nemáme.
- ⚠️ Meze repliky: `movementRemaining = ma` (pro nosiče v idle kolech přesné, pro
  výplňová těla horní odhad — pravda leží mezi (a) a (b)); pořadí průchodu a
  tie-break kopírují C++ doslova.

## (3b) Mechanismus zisku po rasách (noční řádky, doplnění uživatele)

- Zisk vzniká u OBOU ras stejně: **konverze bezbrankových her** — trpaslík 0-TD her
  4 055 → 3 503 (−552), elf 3 576 → 3 028 (−548) z 6 800; délka hry beze změny
  (469,3 vs 469,2 akcí); attrition beze změny u obou; soupeřovy TD nezměněné
  (efekt je aditivní, ne zero-sum); párové zisky korelují r = 0,25 (hýbou se
  tytéž hry). Nenulový zisk aspoň jedné rasy v 5 221 / 6 800 párů (76,8 %).
- ⇒ Na rozlišení, které řádky umožňují, zacházejí obě rasy se ziskem **stejným
  mechanismem**, ne jen stejným číslem. Jemnější rozklad (kdy v drivu, s klecí/bez)
  z řádků nejde — jsou per hra; rozhodly by per-kolo řádky z noční větve
  (dnes se nezapisují).
- **Uživatelova předregistrovaná předpověď „klec má pomáhat všem":** rasová
  neutralita zisku ji **potvrzuje** — ale pozor, potvrzuje ji pro RAMENO jako
  celek. Jestli uvnitř ramene pomáhá právě KLEC, korpus rozhodnout neumí
  (odblokování nosiče klec prokazatelně nenese, volbu pole ocení až mode 7).
  Předpovědi nic neodporuje; rozhodnuto bude po placebo noci.

## (4) Táž vada na čtyřech místech — jedno zadání, ne čtyři

Chybějící dimenze **KAM** (engine vybírá KDO a JESTLI, ne KAM):

- `engine/src/macro_actions.cpp:1354` `expandAdvance` — cíl aritmeticky, y jen
  nudge ke středu → **opraveno ramenem P38** (mode 6);
- `engine/src/block_handler.cpp:190` `choosePushSquare` — „rovně dozadu první" (P9)
  → rameno mode 5, ⛔ noc 19.08. dala **EKVIVALENCI** (+0,28σ, CI celé v ±0,015);
- `engine/src/cage_advance.cpp:41` a `:132` — noha nosiče jde `x + dx*step`,
  jen po přímce (P32) → bez ramene;
- `engine/src/macro_actions.cpp:1491` blitz landing (P35) → rameno běželo 19.08.
  (mode dle vlastního harness), výsledek dle nočního čtení.

⚠️ Že je to jedna vada, NEznamená jednu cenu: P9 (odsun) vyšel ekvivalentní,
P38 (nosič) +12,8σ. **Dimenze KAM platí tam, kde hýbe nosičem míče.** Jedno
zadání ano — ale s prioritou podle vzdálenosti od míče, ne paušálně.

## Souhrn rozdělení +0,0827

- **(C) + (A)**: nese většinu — placebo kanál odblokování pokrývá 97,9 % kol P39
  proti 58,9 % u P38 a P38 mění cíl v 98 % kol, kde vůbec smí; nic z toho
  nevyžaduje klec.
- **(B)**: z korpusu NEROZHODNUTELNÉ, žije jen ve volbě pole (jiné pole než placebo
  v ~63 % firing kol); rozhodne noc mode 7 podle pravidla v bodě (2).
- Pojmenování „P38 = klec" je zatím **nezasloužené**; pravděpodobnější jméno je
  „nosič poprvé smí uhnout do strany + přestal zamrzat" — ale s pásmem, ne jistotou.
