# VÝSLEDEK VÍKENDOVÉHO BĚHU 14.→15.08. + VADY SPOUŠTĚNÍ
*(vyhodnoceno 17.08.2026; předregistrace `evidence/weekend_prereg_20260814.md`)*

Běh doběhl **celý** v so 15.08. v 07:06 UTC a **od té doby ležel nevyhodnocený**
(so + ne bez běhu — dvě prázdná strojová okna, viz „co se nestalo").

---

# 1. VERDIKT: P13 DAUNTLESS **PROŠLO**

| matchup | rameno vyskočilo? | párová delta chess | SE | z |
|---|---|---|---|---|
| **dw-orc** *(ta otázka)* | **ano**, ~1 739 nabídek/hru | **+4,08 pp** | 0,80 | **+5,1** |
| dw-sk *(pravý null)* | **NE — 0× v 6 000 hrách** | +2,28 pp | 0,98 | +2,3 |
| orc-sk *(A/A null)* | **NE — 0× v 6 000 hrách** | −1,30 pp | 0,91 | −1,4 |

3 000 párů na matchup, 4 shardy × 750, mode 4, HEAD `1dc9ecd2`.
Předregistrovaný práh: **PROŠLO**, když je trpasličí rameno ≥ +0,02 a žádné < 0.
**dw-orc +4,08 pp, dw-sk +2,28 pp, kontrola orc-sk uvnitř 2 SE ⇒ PROŠLO.**

## ⛔ ALE: PRÁH SPLNILO I RAMENO, KTERÉ SE ANI JEDNOU NESPUSTILO

`cand_daunt = 0` v **6 000/6 000** her na `dw-sk` i `orc-sk`. Čítač se zvyšuje
právě a jen ve větvi `dauntlessInOffer && hasSkill(Dauntless) && defST > attST`
(`macro_actions.cpp:156`) — jediném místě, kde se ramena liší. **Obě ramena tam
tedy běžela na stejném kódu**, a přesto se `dw-sk` posunul o +2,28 pp (+2,3 SE)
a formálně **splnil pre-registrované „PROŠLO"**.

⇒ **Pre-registrovaný práh je splnitelný ramenem, které nedělá nic.** Není to
chyba té předregistrace — je to vlastnost páru: „pár" tady neznamená *tatáž hra
s jedním přehozeným bitem*, ale *dvě různé hry na spřízněných seedech*
(orientace 0 hraje `seed*2`, orientace 1 `seed*2+1`, a MCTS seedy se liší o 1).
Redukce rozptylu je proto mnohem slabší, než se čekalo.

**Poctivé čtení efektu** = dw-orc proti sdružené nule:

| | |
|---|---|
| dw-orc | **+4,08 pp** |
| sdružený null (6 000 her, rameno prokazatelně mrtvé) | **+0,49 pp** ± 0,67 |
| **rozdíl** | **+3,59 pp**, SE 1,04 ⇒ **~3,4 σ** |

⇒ **Efekt je reálný a v předpovězeném směru** (Fable čekal +1 až 2 pp — vyšlo
víc), ale **z naměřených 4,08 pp je ~0,5–2,3 pp podlaha aparátu**, ne Dauntless.
Bez těch dvou nul by se +4,08 pp četlo jako čistý zisk. **Dvě nezávislé nuly
byly to nejcennější rozhodnutí celé předregistrace.**

## Q1 sweep to potvrzuje mechanismem, ne jen výsledkem
`q1_sweep_20260814.txt`, 36 geometrií × 12 seedů = 432 pozic na rameno:
volba Black Orka **76,8 % → 83,3 %** (OFF → ON), akcí 228 → 257.
⇒ Rameno **mění volbu cíle tím směrem, kvůli kterému vzniklo.**

---

# 2. ⛔ KORPUS SE NESMÍ SROVNÁVAT S BASELINE — JINÝ ENGINE

Druhá povinná metrika („měna drivů", rozklad A/B/C/D1/D2 proti
`night_big_20260813/`) **je neplatná jako měření ramene**:

| | korpus 14.08. | baseline 13.08. |
|---|---|---|
| HEAD | `1dc9ecd2` | `e4b99ee` |
| **poslední commit do `engine/`** | **`e273a369`** | **`9f98070c`** |

Mezi nimi je **šest commitů do enginu**, z toho **čtyři mění chování**:

* `38dcad6d` hand-off se oceňuje jako hand-off, ne jako přihrávka **(P5)**
* `4f2c658d` hand-off vyžaduje, aby byl **nosič špatný** **(P5)**
* `eb231c5c` **odmítnutí darovaného TD** v `block_handler` **(P9a)**
* `e273a369` + `e7d93ed1` čítače *(inertní, dokud je rameno vypnuté)*

Navíc rozklad drivů běžel s opravou atribuce TD (`14c7d035`), kterou baseline
neměla — proto má baseline sekci **ANOMÁLIE (9 řádků)** a nový korpus **nulu**.

⇒ **Kategorie A vzrostla 17 % → 21 % (plné drivy) a C klesla 41 % → 37 %, ale
připsat to Dauntlessu NELZE** — ve stejném balíku jede P9a a dvě změny hand-offu.
Předregistrované předpovědi *(K33 nahoru: 76,6 → 78,9 % ✔ · C proti orkovi dolů
z 59 %: 57 % ✔ · REACH0 beze změny: 41,0 → 41,5 % ✔ · K9a tempo −3,10 → −2,93)*
**vyšly, ale nejsou přiřaditelné.**

⚠️ Popisná hodnota korpusu zůstává; **jako druhý odečet efektu je nepoužitelný**.
Verdikt proto stojí **jen na A/B**, kde obě ramena sdílejí jednu binárku.

---

# 3. ⛔⛔ HAND-OFF: **0 VÝSKYTŮ VE 3 000 HRÁCH**

Fronta si 14.08. výslovně vymínila: *„Nejdřív ověřit hand-off kritérium na
doběhlém korpusu — kdyby nefungovalo, nesmí být v baseline."*
**Neověřilo se — a v korpusu, který v baseline JE, je `HAND_OFF` nula.**

Události ve 3 000 hrách: `BLOCK` 44,7/hru · `PUSH` 40,2 · `PASS` 0,30 ·
`CATCH` 1,97 · **`HAND_OFF` 0,00**. Přitom logování hand-offu bylo přidáno
právě proto (`3b11d33b`) a Q1 sweep dává na postavených pozicích **18,3 %**.

Dvě čtení, obě levně rozhodnutelná a **obě nutná před dalším nasazením P5**:
* **(a)** situace *„Longbeard nese a vedle stojí volný Runner"* je v reálné hře
  tak vzácná, že za 3 000 her nenastala — pak je P5 správná, ale bezcenná oprava;
* **(b)** hand-off se v makro cestě resolvuje jinudy než `pass_handler.cpp`,
  a log tu cestu nepokrývá — pak nevíme vůbec nic.

⇒ **Úkol: spočítat výskyt té situace ve snímcích korpusu** (nosič ≠ Runner
a volný Runner v sousedství). To rozhodne (a)/(b) bez dalšího běhu.

---

# 4. VADY SPOUŠTĚNÍ — PROČ SE V PÁTEK SPOUŠTĚLO DVAKRÁT
*(a proč to dopadlo dobře jen štěstím)*

Sled 14.08.: **14:58** Q1 test na **jedné** postavené pozici → Black Orc 0/112 →
**běh zabit po ~5 minutách**; **15:14** sweep přes 36 geometrií ukázal, že ta
pozice byla **patologická** (76,8 → 83,3 %); **15:15 běh spuštěn znovu.**
Rozhodnutí zabít i rozhodnutí spustit znovu byla obě správná. **Vadný byl
aparát, ne úsudek** — a v něm byly čtyři věci, které z druhého spuštění dělají
loterii. Všechny opraveny 17.08.:

| # | vada | čím se projeví | oprava |
|---|---|---|---|
| **1** | zámek `mkdir .lock` + `trap EXIT` **nepřežije `kill -9`** | druhé spuštění tiše skončí hláškou v `chain.log`, který se v tu chvíli nečte | `night_lock`: do zámku se píše **PID**; zámek po mrtvém procesu se sebere a **zaloguje**; zámek živého procesu drží dál |
| **2** | 12 shardů na `&`; **zabití rodiče nezabije děti** | sirotci píšou do týchž adresářů jako druhé spuštění ⇒ promíchané řádky. 14.08. to prošlo jen proto, že se shardy nestihly rozjet | `night_run_bg` + `night_cleanup` na **EXIT/INT/TERM** |
| **3** | `chain.log` **nemá první spuštění** — první řádek je až 15:15 | noc vypadá jako jedno čisté spuštění; *snímek se vydává za stav* | `night_init` čísluje **POKUS n** a loguje i ten, co skončí abortem; do logu se nikdy nepíše přes `>` |
| **4** | řádky se otevíraly `fopen(..., "a")` | zabitý a znovu spuštěný běh **přidá druhou sadu řádků** a nic to neřekne — po deduplikaci podle seedu to i vypadá správně | `fopen(..., "w")` + poznámka do `run.log`, když soubor existoval |
| **5** | baseline se **neověřuje proti commitu** | oddíl 2 tohoto dokumentu | `night_stamp_head` + `night_check_baseline`; otisk **doplněn zpětně** do `night_big_20260813/` i `dauntless_corpus_20260814/` |

⇒ **`run_night_lib.sh`** — táž křehká kopie zámku byla v **devíti** spouštěčích,
proto lib, ne desátá kopie. **Každý nový noční spouštěč ho musí sourcovat.**

## A dvě vady VYHODNOCENÍ, ne spouštění *(opraveny v harnessu)*

* **binárka a předregistrace se neshodly na prahu**: `run.log` tiskl
  `>= +0.03`, dokument říkal **±0,02**. Teď se tiskne práh z dokumentu i s jeho
  jménem.
* **hlavička attrition v mode 4 tvrdila `cageAdvance on/off`** — v pondělí by se
  četlo rameno brány klece místo Dauntlessu. Teď `Dauntless ON/off`.
* **nově se tiskne `ARM Dauntless: N offers … NEVER FIRED => TRUE NULL`** —
  otázka *„spustilo se to rameno vůbec?"* je nejcennější řádek pondělního čtení
  a dosud ji **netiskl nikdo**; musel jsem ji 17.08. dolovat skriptem z řádků.

---

# 5. CO SE NESTALO

* **So + ne bez běhu.** Předregistrace počítala s **třemi** A/B (běhy 2 a 3 měly
  jít v sobotu a v neděli večer, výběr „až po vyhodnocení"). Vyhodnocení
  nepřišlo ⇒ **dvě 14hodinová okna propadla.**
* **Sobotní výběr dalšího ramene se neudělal** — zásobník (P17 Wrestle ·
  P2+P9c · blitz roh vs zeď · P10a) je pořád nedotčený.
* **P13 se nezapnul v produkci.** `dauntlessInOffer` je dál `default false`
  (`mcts.h:40`). Verdikt „prošlo" na to zatím nesáhl.
