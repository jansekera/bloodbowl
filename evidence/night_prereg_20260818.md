# PŘEDREGISTRACE — NOC 18.→19.08.2026
**Zapsáno PŘED spuštěním. Prahy platí tak, jak stojí tady.**

---

## ⛔ NEJDŘÍV: PROČ NE P10a *(byl to kandidát č. 1 — strop ho vyřadil)*

Uživatel si vybral P10a (hodnota bloku na soupeřova nosiče). Oprava se napsala
a **Q1 pojistka ji zastavila dřív, než stála noc** — přesně proto to pravidlo je.

* Q1 na přeplněné desce: podmínka funguje (nesporné pole se nehnulo), ale
  **sporné pole se nehnulo taky — search si nosiče bere v 98 % už bez ramene.**
* Korpus 3 000 her **se jmenovatelem**: z **3 733** příležitostí (u nosiče stojí
  naše tělo) jsme na něj udeřili v **81,5 %**, a bez souseda jsme na něj
  **6 143×** doblitzovali.
* ⇒ Strop dokonalé opravy je **~0,23 kola na zápas**, a část z toho se udeřit
  správně nemá (soupeř má u pole nosiče převahu v 54,1 %).

**ZAMÍTNUTO jako noční rameno**, táž logika jako P8 (0,056 faulu/zápas).
Kód zůstává za `carrierBlockPrior = false`. Podrobně `evidence/p10a_q1_result_20260818.md`.

---

## BĚH: P9 / P9c — CÍLOVÉ POLE ODSUNU SE VYBÍRÁ

### Co se změnilo
`choosePushSquare` (`block_handler.cpp`) skórovalo `count - i`, tedy
**„rovně dozadu první"**. Cílové pole se nehodnotilo **nijak**, kromě prázdné vs
obsazené, odmítnutí darovaného TD a Side Step / Grab.

Nové pořadí je uživatelovo (14.08.), a je to **sekvence, ne kompromis**:
1. odsunutý **přestane sousedit s rohem naší klece** *(to byl účel akce)*
2. **nepřiblíží se k našemu nosiči** *(REACH0)*
3. rovně dozadu *(dnešní chování jako tiebreak)*

Bez našeho nosiče na hřišti skóruje přesně jako dřív ⇒ **na obraně je rameno
konstrukčně no-op.**

### Strop — spočítaný PŘED během *(`evidence/push_choice_ceiling_20260818.txt`)*
Korpus 3 000 her, **se jmenovatelem**:

| | na zápas |
|---|---|
| našich odsunů | **21,93** |
| z toho **se skutečnou volbou** (≥2 prázdná pole) | **17,34** |
| **přisunuli soupeře BLÍŽ k našemu nosiči, ač šlo dál** | **1,04** |
| … z toho přilepili **přímo k nosiči** | 0,27 |
| **nechali ho u rohu naší klece, ač šlo jinam** | **0,24** |

⇒ **~1,28 prokazatelně horší volby na zápas.** Pro srovnání: P10a **0,23**,
P8 **0,056** — obojí zamítnuto. Tenhle strop je 5× a 23× vyšší.

⭐ **A mechanismus vede přes nejsilnější replikovaný prediktor, který máme.**
„Přisunout soupeře k nosiči" zvedá **REACH0**, a ten je v σ-tabulce 18.08.
**−16,7σ** a **replikuje na obou půlkách korpusu** (−12,6 / −11,1).

### Konfigurace
`MODE=5`, **PAIRS=850, SHARDS=8 ⇒ 6 800 párů** na matchup **`dw-we`**.
Cage gate je v obou ramenech **VYPNUTÁ** *(dnes zamítnutá; harness měl seznam
výjimek, který by ji mode 5 tiše zapnul — opraveno na seznam těch, kdo ji chtějí)*.

Proč `dw-we`: mechanismus je REACH0 → ztráta míče, a wood elf ho trestá
**okamžitě**, kdežto skaven na splátky (P0.5). Navíc je to matchup, kde má
projekt nejdelší historii, takže je s čím srovnávat.

### ⛔ Nulová kontrola
Matchup s nulovou expozicí **neexistuje** — odsunuje se v každém. Proto:
* `CONTROL_MODE2=1` jako **smoke test seedování** *(tautologie, nic víc)*,
* a **skutečná kontrola je per-pair `MOVED WITHOUT THE ARM ACTING`**, kde se
  „rameno jednalo" počítá jako **počet odsunů, které rameno opravdu
  PŘESMĚROVALO** — ne počet odsunů. Čítač, který tiká i tam, kde se s „rovně
  dozadu" shodne, by řekl „rameno běželo" vždycky a leak test by byl k ničemu.

### Metrika a prahy *(pre-registrováno)*
* **Primární:** párová delta chess na `dw-we`, 6 800 párů, mean přes **VŠECHNY** páry.
* **POMÁHÁ:** delta ≥ **+0,015** · **ŠKODÍ:** delta ≤ **−0,015** ·
  mezi tím **NEROZHODNUTO — a zapíše se to jako NEROZHODNUTO.**
* Práh se **neposouvá**, ať `n_nonzero` vyjde jakkoli; nízké `n_nonzero` se
  zapíše jako „nedostatečná síla" s uvedením čísla.

### ⚠️ Poctivé výhrady
1. **CRN tady nejspíš moc nepomůže.** Změřeno dnes na bráně: u ramene, které
   sahá na hodně kol, dalo **6 %** redukce SE místo slíbených 15–25 %, protože
   páry, které se hnou, se hnou víc. Rozpočet párů proto stojí na šumovém dně
   **bez** CRN. Odsun ale rameno přesměruje jen ~1,3× za zápas, takže je vzácnější
   než brána — kdyby CRN pomohlo víc, je to samo o sobě výsledek a patří zapsat.
2. **`CORPUS=0`** ⇒ do předregistrace nesmí žádná předpověď o kontrolách.
   *(Přesně na tom zakopla noc 17.→18.08.: dvě ze šesti předpovědí byl běh
   od začátku neschopen zodpovědět. Spouštěč to teď odmítne spustit.)*
3. **Není to čistý test P9c.** Rameno mění geometrii **všech** našich odsunů,
   ne jen těch na pollutera. Kdyby vyšlo záporně, neví se bez korpusu, jestli
   je špatné pravidlo, nebo jen jeho dopad mimo klec.

### Předregistrované předpovědi *(strojově kontrolované, `night_prereg_20260818.preds`)*
| | čekám | proč |
|---|---|---|
| delta chess dw-we | **+0,005 až +0,02** | 1,28 opravené volby na zápas přes REACH0 |
| `n_nonzero` | **35–65 %** | rameno přesměruje ~1,3 odsunu na zápas |
| `MOVED WITHOUT THE ARM ACTING` | **0** | jinak se delta nečte |
| `arm_acted` | **≥ 0,95** | odsunuje se skoro v každém zápase |
| mode 2 smoke test | exaktně 0 | tautologie |

### ⚠️ PŘIZNÁNÍ: smoke test 12 párů běžel PO napsání předpovědí

Než se spustila noc, proběhl 12párový smoke test ramene (mode 5). Ukázal:
`arm acted 12/12` · `MOVED WITHOUT THE ARM ACTING: 0` · `cage plans adopted 0,00`
*(brána opravdu vypnutá v obou ramenech)* · a **`n_nonzero` 83,3 %**.

To je **nad** pre-registrovaným pásmem 35–65 %. ⛔ **Předpověď se NEPOSOUVÁ.**
Dvanáct párů není důvod přepsat předpověď — a hlavně: posunout ji po pohledu na
data je přesně ta věc, kvůli které předregistrace existuje. Když `n_nonzero`
vyjde nad 65 %, zapíše se to jako **MIMO** a je to informace o rameni
*(odsun přesměrujeme častěji, než jsme mysleli)*, ne selhání běhu.
Zapsáno sem, aby bylo dohledatelné, že jsem ta data viděl.

### Pořadí čtení výsledku
1. `MOVED WITHOUT THE ARM ACTING` = 0? Ne ⇒ konec, výsledek se nečte.
2. `arm acted in N/M` — jak velký je skutečný vzorek?
3. `n_nonzero`.
4. Teprve pak delta a práh.

*(Od 18.08. to všechno tiskne `night_summarize.py` sám, v tomhle pořadí,
a k tomu PŘEDPOVĚĎ vs VÝSLEDEK.)*
