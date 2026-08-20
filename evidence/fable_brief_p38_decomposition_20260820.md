# ZADÁNÍ PRO FABLE 20.08.2026 — KOLIK Z P38 JE KLEC A KOLIK JEN TO, ŽE NOSIČ SMÍ UHNOUT DO STRANY?

## Proč se to ptáme

Noc 19.→20.08. dala **největší efekt, jaký jsme letos změřili**:
P38 (cílové pole nosiče se vybírá podle klece, která z něj vyjde)
**+0,0827 ± 0,0065 (+12,8σ)** na 6 800 párech, 8/8 shardů kladných,
leak 0, nulový test přesně 0,0.

⛔ **Jenže rozklad po rasách říká, že to nejspíš NENÍ o kleci.**
Rameno je v A/B zapnuté vždy jedné straně, a pomáhá **tomu, kdo ho drží** —
oběma rasám skoro stejně:

| | bez ramene | s ramenem | zisk |
|---|---:|---:|---:|
| **trpaslík** | 0,4401 TD/hru | 0,5378 | **+0,098** |
| **wood-elf** | 0,5513 TD/hru | 0,6466 | **+0,095** |

**Wood-elf nehraje trpasličí klec.** Kdyby efekt nesla klec jako doktrína,
elf by z ní nemohl mít tolik co my. ⇒ Podezření: nese ho něco **rasově
neutrálního**, co je v rameni přibalené.

## Co je v tom rameni doopravdy zabalené *(čtení kódu, `macro_actions.cpp`)*

Základní `expandAdvance` vybírá cíl **aritmeticky**: `targetX = x + dx*steps`,
a `y` se posune o **jedno pole ke středu**. Všechny zvažované cíle leží
prakticky na jedné přímce. Zapnuté rameno prochází **celý čtverec**
`[-budget, budget]²`. Přibaleny jsou tedy **tři** změny naráz:

1. **(A) boční volnost** — nosič smí poprvé skončit jinde než rovně vpřed
   *(táž chybějící dimenze KAM jako P9, P32, P35 — „engine vybírá KDO
   a JESTLI, ale ne KAM")*;
2. **(B) kritérium klece** — mezi těmi poli se řadí podle toho, jestli z pole
   vyjde plná čistá klec (`cageScoreForSquare`);
3. **(C) obejití záložní smyčky** — v základu se cíl při obsazeném poli nebo
   TZ **stahuje zpět**, a když se nedojde nikam, `if (steps <= 0) return;`
   ⇒ **nosič se nehne vůbec**. To je doslova **P39** *(ve 37,7 % splnitelných
   kol se nosič nehne, a v 83,8 % těch kol nemá žádnou událost)*.

⇒ Je docela dobře možné, že **P38 omylem z velké části opravilo P39** —
a že se to celé jmenuje špatně.

## Otázka pro tebe

**Rozděl těch +0,0827 mezi (A), (B) a (C). Kolik z efektu zůstane, když se
kritérium klece nahradí čistým „jdi co nejdál, TZ-free, kamkoli"?**

## Co má vrátit

1. **Návrh placebo ramene (mode 7)** — ⚠️ musí být identické s P38 ve VŠEM
   *(týž krokový rozpočet, týž `prog >= maxProgress - 1` pás, týž TZ filtr,
   totéž obejití záložní smyčky, týž per-side čítač)* a lišit se **jedině
   vypuštěným `cageScoreForSquare`**. Napiš, které řádky přesně.
   ⭐ Když placebo dá totéž co P38, nález se jmenuje **„nosič neumí uhnout
   do strany"**, ne „klec" — a je to větší a levnější věc než doktrína.
2. **Predikce PŘED během**, číslem, pro placebo i pro rozdíl placebo↔P38
   *(pře-registrace je vstup běhu, `evidence/night_prereg_*.preds`)*.
3. **Co z toho jde změřit z KORPUSU bez noci** — hlavně (C): v kolech
   s Δx = 0 a bez události nosiče ověř, jestli by rameno pole našlo.
   To rozhodne (C) za pár minut místo za 14 hodin.
4. **Kde je ta trojice pravidel ještě jednou** — `choosePushSquare` (P9),
   `cage_advance.cpp` posun jen po přímce (P32), blitz landing (P35).
   Jestli je to jedna vada na čtyřech místech, chceme jedno zadání, ne čtyři.

## Co NEmá dělat

* ⛔ neopravovat — tohle je **rozhodnutí mezi příčinami**, ne oprava;
* ⛔ nepsat čísla v kroužcích (⓵⓶⓷) — psát (1) (2) (3);
* ⛔ nevyvozovat z nočního čísla jednostranný efekt: **delta je dvoustranná**
  (`chessCandHome + chessCandAway − 1` = my s ramenem vs my proti rameni),
  jednostranně je to ≈ **+4,1 pp**, ne +8,3.

## Doklady

`cageadvance_20260819/chain.log` · `evidence/night_prereg_20260819.md` ·
commit `74f153f2` (P38) · `evidence/cage_ma_cap_20260819.md` (P39) ·
`evidence/corner_gap_20260819.md` (P34/P35/P9 jako jedna dimenze).
