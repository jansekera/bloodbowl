# P38 DEKOMPOZICE 20.08.2026 (Fable) — kolik je klec a kolik „nosič smí uhnout do strany"

Zadání: `fable_brief_p38_decomposition_20260820.md`. Noc P38 dala **+0,0827 ± 0,0065
(12,8σ, dvoustranně)** — rozdělit mezi (A) boční volnost, (B) kritérium klece,
(C) obejití záložní smyčky (= P39). Skript: `diag_p38_decomposition_20260820.py`
(python replika `expandAdvance` + `cageScoreForSquare` + `corridorResistance`
z commitu `74f153f2`), korpus `corpus_baseline_20260819_data` (3 000 her).
Noční řádky: `cageadvance_20260819/dw-we_s*/diag_cageadvance_rows.jsonl` (13 600).

**Validace repliky:** (1) IDLE populace trpaslíka sedí přesně na P39
(5 213 / 3 302 / 1 911 = `carrier_idle_20260820.md`); (2) replika
`corridorResistance` proti exportovanému poli `corridor_resistance`:
**0 neshod z 31 129 porovnaných kol**.

## ⭐ Vstupy od uživatele během práce (cituji, jsou jeho, ne moje)

1. **„Klec pomáhá i elfům… má pomáhat VŠEM."** — univerzální ochrana nosiče,
   ne trpasličí doktrína ⇒ rasová neutralita zisku není důkaz proti (B); je to
   signatura, kterou pravidlo samo předpovídá (předregistrovaná předpověď).
2. **„Pravidlo je univerzální, ale ROZPOČET je rasový"** — strop plnitelnosti
   klece počítá dosah, ne volnost těl ⇒ měřit dvakrát, po rasách.
3. **Zeď patří do OBRANY** — do útočného rozpočtu kola se nepočítá; útočné
   povinnosti jsou jen (i) tělo už jednalo, (ii) markuje s DRAHÝM odchodem
   (dodge_cost > 0,20; levné markování dle K30b roh nepřebíjí).
4. **„Trpaslík útočí PROLOMENÍM zdi, elf OBĚHNUTÍM."** — odpověď na zeď je
   rasová ⇒ když trpaslík získává z boční volnosti stejně jako elf, buď jeho
   zisk nese (B)/(C), nebo **engine hraje za trpaslíka elfa** — a to je
   varování, ne úspěch.

---

## (1) Návrh placebo ramene — mode 7 (⛔ NEIMPLEMENTOVÁNO, jen návrh)

Identické s P38 ve všem, liší se **jedině vypuštěným `cageScoreForSquare`**.

`engine/src/macro_actions.cpp`:
- za ř. 178 (`setCageAwareAdvanceArm`): přidat `thread_local bool
  g_cageAdvancePlacebo[2]` + setter/getter `setCageAdvancePlaceboArm` /
  `cageAdvancePlaceboArm` (týž per-side vzor jako ř. 169–178);
- ř. 1377: `if (cageAwareAdvanceArm(state.activeTeam)) {` →
  `const bool placebo = cageAdvancePlaceboArm(state.activeTeam);`
  `if (cageAwareAdvanceArm(state.activeTeam) || placebo) {`;
- ř. 1403: `if (cageScoreForSquare(state, carrier, cand) < 0) continue;` →
  `if (!placebo && cageScoreForSquare(...) < 0) continue;` — **jediný funkční rozdíl**;
- ř. 1376–1411 jinak beze změny: týž rozpočet (`carrierStallAwareSteps`), týž pás
  `prog >= 1 && prog >= maxProgress - 1`, týž TZ filtr (ř. 1400–1402), totéž
  obejití záložní smyčky přes `armChoseSquare` (ř. 1425), **týž čítač**
  `++g_cageAwareAdvancePicks` (ř. 1409) tikající jen při `best != target`
  (skutečná změna volby — lekce „nulové rameno" 17.08.).

`diag_f1_cage_advance_harness.cpp`:
- ř. 244–249: `: (mode == 7) ? 127'000'000u` (disjunktní seedy);
- ř. 251–257: label mode 7 `"P38-PLACEBO: tez pole, bez kriteria klece"`;
- ř. 277: `rowsName` mode 7 → `"diag_cageadvance_placebo_rows.jsonl"`;
- ř. ~400–406: `bb::setCageAdvancePlaceboArm(HOME, mode == 7 && candHome)` /
  `(AWAY, mode == 7 && !candHome)`, shodit po hře jako u mode 6;
- ř. 440–441: `pr.armEvents += … (mode == 6 || mode == 7) ? candCage …`
  (čítač je sdílený `takeCageAwareAdvancePicksInSearch`).

## (2) Predikce PŘED během (dvoustranně, `chessCandHome + chessCandAway − 1`)

Do `evidence/night_prereg_*.preds` pro mode 7 registruji:

- **placebo: +0,090**, pásmo **[+0,04; +0,14]** — placebo má kanály (A)+(C)
  SILNĚJŠÍ než P38 (odblokuje 97,9 % idle kol proti 58,9 %), ale nemá ochranu
  klece a ~2× častěji si dojde pro vyšší odpor koridoru (6,7 % vs 3,5 %);
- **rozdíl placebo − P38: 0,00**, pásmo **[−0,04; +0,05]** (srovnání průměrů
  dvou běhů, seedy disjunktní — ne CRN páry);
- **rozhodovací pravidlo:** placebo ≥ P38 − 0,015 ⇒ nález se jmenuje **„nosič
  neuměl uhnout do strany"** a (B) nenese měřitelnou část; placebo < P38 − 0,015
  ⇒ (B) nese aspoň část a klec si jméno zaslouží;
- **kontrola čítače:** placebo MUSÍ hlásit víc picků/hru než P38 (korpus: najde
  pole v ~98 % kol vs ~60 %). Když ne, lže replika nebo implementace — nečíst
  výsledek dál.

## (3) Co jde rozhodnout z KORPUSU bez noci — ZMĚŘENO

(a) = klauzule těl jako v rameni dnes (dosah, Chebyshev ≤ ma);
(b) = navíc vyloučena těla s útočnou povinností dle uživatelova bodu 3.

| | trpaslík | wood-elf |
|---|---|---|
| kol se stojícím nosičem | 17 728 | 13 401 |
| VŠE: placebo najde pole | 17 523 = 98,8 % | 12 912 = 96,4 % |
| VŠE: P38(a) najde pole | 10 625 = 59,9 % | 8 234 = 61,4 % |
| VŠE: P38(b) najde pole | **3 504 = 19,8 %** | **2 638 = 19,7 %** |
| P38(a) změní cíl proti aritmetice | 10 411 = 98,0 % z 10 625 | 8 098 = 98,3 % z 8 234 |
| P38(a) vybere TOTÉŽ pole co placebo | 3 978 = 37,4 % z 10 625 | 3 596 = 43,7 % z 8 234 |
| **IDLE kola (Δx=0, nosič bez události)** | **5 213** (volný 3 302 / TZ 1 911) | 4 663 (2 491 / 2 172) |
| IDLE: placebo najde pole | **5 106 = 97,9 %** | 4 392 = 94,2 % |
| IDLE: P38(a) najde pole | **3 070 = 58,9 %** | 2 492 = 53,4 % |
| IDLE: P38(b) najde pole | 1 792 = 34,4 % | 1 503 = 32,2 % |

**Čtení:**

- ⭐⭐⭐ **(C) nepotřebuje klec — a klec mu PŘEKÁŽÍ.** V kolech P39 by placebo
  našlo pole v **97,9 %**, P38 jen v **58,9 %**: kritérium klece rameno ve
  **2 z 5 idle kol blokuje** (všechna pole −1 ⇒ `armChoseSquare=false` ⇒ záložní
  smyčka ⇒ nosič stojí dál). Kanál „odblokování nosiče" je celý (A)+(C).
- **(B) žije jen ve VOLBĚ pole** (jiné pole než placebo v ~63 % firing kol
  trpaslíka) — a korpus mu poprvé něco přiznává, viz zeď níže. Cenu v TD
  rozhodne jedině noc mode 7.
- ⭐⭐ **Rozpočet (uživatelův bod 2 potvrzen):** vyloučení těl s útočnou
  povinností sráží plnitelnost klece **~3×** (59,9 → 19,8 % trpaslík,
  61,4 → 19,7 % elf). Strop „95,6 % kol" počítá dosah a je hrubě nadhodnocený;
  `cageScoreForSquare` si kupuje rohy za těla, která už jednala nebo draze
  markují. ⚠️ U trpaslíka (AG2) je KAŽDÝ odchod z TZ drahý (p_fail 0,5 > 0,2),
  takže „drahé markování" = „jakékoli markování"; u elfa (AG4, p_fail 0,17)
  je markování levné a víc ho škrtí utracená kola. Rozdíl stropů po rasách je
  v TOMHLE proxy malý — proxy nevidí obranné role (v korpusu značka povinnosti
  není); rozhodl by explicitní model povinností.
- ⚠️ Meze repliky: `movementRemaining = ma` (pro nosiče v idle kolech přesné,
  pro výplně horní odhad — pravda mezi (a) a (b)); pořadí průchodu i tie-break
  kopírují C++ doslova.

## (3b) Zeď: OBĚHNUTÍ vs PROLOMENÍ (uživatelův diskriminátor, bez noci)

Odpor koridoru (`corridorResistance`, K9b, −9,6σ v σ-tabulce) z pole vybraného
ramenem vs z pole základu; jmenovatel = kola, kde pick ≠ pole základu.

| | trpaslík P38 | trpaslík placebo | elf P38 | elf placebo |
|---|---|---|---|---|
| picků ≠ základ | 10 411 | 17 025 | 8 098 | 12 598 |
| OBĚHNUTÍ (strana, odpor klesá) | 44,7 % | 41,1 % | 48,4 % | 44,1 % |
| PROLOMENÍ (vpřed, odpor neklesá) | **0,3 %** | 0,1 % | 0,2 % | 0,1 % |
| strana, odpor stejný | 51,3 % | 52,1 % | 48,2 % | 48,7 % |
| strana, odpor ROSTE | 3,5 % | 6,7 % | 3,1 % | 7,0 % |
| pick má VYŠŠÍ odpor než základ | **3,5 %** | **6,7 %** | 3,1 % | 7,1 % |
| odpor KLESÁ vs SKUTEČNÝ konec základu (po pull-backu) | 47,8 % | 48,7 % | 51,3 % | 51,9 % |
| odpor ROSTE vs skutečný konec základu | 5,5 % | 8,7 % | 5,7 % | 9,0 % |

- ⭐⭐⭐ **Rameno prodává oběma rasám totéž zboží: boční pohyb.** PROLOMENÍ je
  u trpaslíka 0,3 % picků — **trpaslík ramenem neprolamuje, obíhá jako elf.**
  To je uživatelova možnost (ii): +0,0827 je z velké části zisk získaný MIMO
  doktrínu rasy a nemusí přežít proti soupeři, který obíhání trestá (Z17:
  lajna je přesně místo, kde se obíhání platí). ⚠️ Poctivá výhrada: pohyb
  nosiče zeď prolomit ani neumí — prolomení dělají bloky/blitz — takže rameno
  volby nosiče „prolomení" vyjádřit nemohlo; nález je, že engine nemá ŽÁDNÉ
  místo, kde by trpasličí odpověď na zeď (bloky napřed, pak nosič) koordinoval.
- ⭐⭐ **Defekt potvrzen, ale s překvapením:** `cageScoreForSquare` odpor
  koridoru nezná (v celé funkci není zmínka) a P38 si dojde pro VYŠŠÍ odpor
  ve 3,5 % picků (361 z 10 411 u trpaslíka). Jenže **placebo to dělá ~2×
  častěji (6,7 %)** — čistota rohů s nízkým odporem koreluje. Kritérium klece
  tu tedy poprvé měřitelně něco KUPUJE: méně chůze do zdi. Oprava „skóruj
  odpor v ranku ramene" zůstává levná a konkrétní — nenavrhuji ji teď, zadání
  je rozklad.

## (3c) Mechanismus zisku po rasách (noční řádky)

- Zisk vzniká u obou ras stejně: **konverze bezbrankových her** — trpaslík
  0-TD her 4 055 → 3 503 (−552), elf 3 576 → 3 028 (−548) z 6 800; délka hry
  beze změny (469,3 vs 469,2 akcí); attrition beze změny u obou; soupeřovy TD
  nezměněné (efekt aditivní, ne zero-sum); párové zisky korelují r = 0,25.
  Nenulový zisk aspoň jedné rasy v 5 221 / 6 800 párů (76,8 %).
- **Uživatelova předpověď „klec má pomáhat všem":** rasová neutralita zisku
  ramene ji **potvrzuje** — ale pro RAMENO jako celek. Rozpad zdi výše říká,
  že sdílený mechanismus je **boční pohyb, ne klec**: obě rasy dostávají elfí
  obíhání. Předpovědi to neodporuje (klec může pomáhat všem A ZÁROVEŇ nebýt
  tím, co tu neslo zisk); rozhodne placebo noc podle pravidla v bodě (2).
  Jemnější rozklad (kdy v drivu, s klecí/bez) z per-hra řádků nejde — rozhodly
  by per-kolo řádky z noční větve (dnes se nezapisují).

## (4) Táž vada na čtyřech místech — jedno zadání, ne čtyři

Chybějící dimenze **KAM** (engine vybírá KDO a JESTLI, ne KAM):

- `engine/src/macro_actions.cpp:1354` `expandAdvance` — cíl aritmeticky
  → opraveno ramenem P38 (mode 6);
- `engine/src/block_handler.cpp:190` `choosePushSquare` — „rovně dozadu první"
  (P9) → ⛔ noc 19.08. dala **EKVIVALENCI** (+0,28σ, CI celé v ±0,015);
- `engine/src/cage_advance.cpp:41` a `:132` — noha nosiče jen po přímce
  `x + dx*step` (P32) → bez ramene;
- `engine/src/macro_actions.cpp:1491` blitz landing (P35) → rameno běželo,
  čtení dle noci P35/P38.

⚠️ Jedna vada ≠ jedna cena: P9 (odsun) vyšel ekvivalentní, P38 (nosič) 12,8σ.
**Dimenze KAM platí tam, kde hýbe nosičem míče** — jedno zadání ano, priorita
podle vzdálenosti od míče.

## Souhrn rozdělení +0,0827

- **(A)+(C) nesou kanál „nosič se vůbec pohne a smí do strany"** — placebo ho
  má silnější než P38 (97,9 vs 58,9 % idle kol; cíl se mění v 98 % firing kol);
  nic z toho klec nevyžaduje.
- **(B)** z korpusu v TD nerozhodnutelné; korpus mu přiznává měřitelný bonus
  ve volbě pole (2× méně chůze do vyššího odporu než placebo). Rozhodne mode 7.
- ⚠️ **Ať vyjde mode 7 jakkoli, platí varování z rozpadu zdi:** trpaslíkův zisk
  je dnes vydělaný elfím obíháním (PROLOMENÍ 0,3 % picků), tedy mimo doktrínu
  rasy — proti soupeři, který obíhání trestá, se může část z +0,0827 vrátit.
- Jméno „P38 = klec" je zatím nezasloužené; pravděpodobnější je „nosič poprvé
  smí uhnout do strany + přestal zamrzat" — s pásmem, ne jistotou.
