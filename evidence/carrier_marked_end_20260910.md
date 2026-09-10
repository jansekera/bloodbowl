# P42 — NOSIČ NA KONCI NAŠEHO KOLA: OVĚŘENÍ V KÓDU + STROP (10.09.2026)

Skript `diag_carrier_marked_end_20260910.py`, korpus `corpus_m11_20260909_data`
*(1000 her, engine `cf8634e8`, produkční nastavení)*, **9 496 spárovaných kol**
se stojícím nosičem na obou koncích *(z toho trpaslík 6 291)*.

---

## ⛔⛔⛔ NEJDŘÍV OPRAVA: TĚCH „53,1 %" JE ZAČÁTEK KOLA, NE KONEC — A JE TO MOJE CHYBA

`diag_cage_built_20260910.py` téhož dne napočítal *„nosič sám v soupeřově TZ
**53,1 %**"* a **tímhle číslem jsem přehodil pořadí celého okruhu KLEC.**
Číslo je čtené ze **špatného snímku**:

`captureTurnSnapshot` *(`engine/src/game_simulator.cpp:748`)* se volá **na
hranici kola** — `:851` na začátku hry a `:905` při každé změně aktivního týmu,
tedy **PŘED** `getAvailableActions`. Razítkuje si to samo v poli
`eligible_at_start` *(`:766-777`, „kdo NA ZAČÁTKU kola jednat mohl")*, a stejně
tak jsou ze začátku kola i všechna klecová razítka `cage_corners`,
`cage_corners_marked`, **`carrier_tz`** *(`:797-804`)*.

⇒ **53,1 % měří, jak často NÁM soupeř nosiče označí ve SVÉM kole.** To je
z většiny **jeho** akce, ne naše volba. Predikát `P42`/`K38` je **opačný konec
kola**.

⚠️ **Atribuce: chyba je moje, ne uživatelova** — číslo i jeho rámování
*„dominantní díra je nosič sám"* vzniklo v mém rozboru; uživatel ho jen dostal.

### ⭐ A konec kola se přečíst DÁ, korpus na to stačí

`turn_logs[i+1]` je začátek **soupeřova** kola, tedy **konec našeho**. Není to
rekonstrukce z událostí a není to nová metoda: přesně tak to dělá
`diag_rules_checks_20260812.py:382` pro `K38` **už od 12.08.**
*(docstring `:5`: „Konec kola se bere z `turn_logs[i+1]`, ne rekonstrukcí
z událostí — kdo se nehnul, nemá událost.")*
Páruje se jen tam, kde mezi snímky nic nepřestaví hřiště: **vyřazují se kola
s TD** *(605)*, **hranice poločasu** *(961)*, poslední snímek hry *(1 000)*
a kola bez míče *(19 533)*; **22 099 vyřazeno + 9 901 použito = 32 000, zbytek 0.**

⭐ **Že párování drží, se dokazuje:** v **6 499** použitých kolech se nosič
mezi `S` a `E` **HNUL** ⇒ `E` není kopie `S` *(ochrana proti off-by-one, který
by celé měření udělal tautologií)*.

**Doklad, že to je táž veličina:** na začátku kola vyjde na trpaslíkovi
**53,0 %** — tedy `cage_built` reprodukováno *(53,1 %)*. Na konci kola vyjde
**24,6 %**.

---

## ⭐⭐ STROP VE TŘECH KOŠÍCH *(doktrína uživatele 10.09., ne jedno číslo)*

*„ono není vhodné ani postavit nosiče vedle ležícího soupeře — ale to chce jeho
blitz — ale když mu dáme nosiče, proč by na to blitz nevyužil?"*
⇒ Ležící soused **není zdarma**, jen se platí **blitzem**; a nosič je nejcennější
blitz cíl na desce, takže *„ležící nepočítáme"* mlčky tvrdí, že soupeř svůj
blitz na míč neutratí. Platí [[feedback_worst_case_is_readable_not_searched]].
⭐ Ale zůstává to omezené: **N stojících = N bloků ZDARMA · N ležících ≈ JEDEN
zásah** *(blitz je 1× za kolo)* — **dvě tvrdosti, ne dvě velikosti téhož.**

**Trpaslík, 6 291 kol, KONEC našeho kola:**

| koš | | |
|---|---:|---:|
| ⛔ **ZDARMA** *(≥1 stojící soused → blok, neomezeně)* | 1 546 | **24,6 %** |
| ⚠️ **ZA BLITZ** *(0 stojících, ≥1 ležící → nejvýš 1 zásah)* | 1 207 | **19,2 %** |
| ✅ **BEZPEČNO** *(ani jeden)* | 3 538 | 56,2 % |
| *(zbytek — musí být 0)* | 0 | |
| ⇒ **EXPOZICE CELKEM** | 2 753 | **43,8 %** |

**Kolik bloků zdarma to je** *(počet stojících sousedů nosiče)*: 1 → 11,2 % ·
2 → 7,6 % · 3 → 4,6 % · 4 → 1,1 % · 5 → 0,1 % · 6 → 0,03 %.
⇒ ⭐ **Ve 13,4 % kol dáváme soupeři DVA a víc bloků zdarma na míč.**

**Po rasách** *(náš nosič)*, `ZDARMA / ZA BLITZ / expozice`:

| | ZDARMA | ZA BLITZ | EXPOZICE | *(začátek kola)* |
|---|---:|---:|---:|---:|
| trpaslík *(6 291)* | 24,6 % | 19,2 % | **43,8 %** | 53,0 % |
| ork *(1 114)* | 20,9 % | 18,1 % | 39,0 % | 40,6 % |
| human *(866)* | 25,1 % | 15,7 % | 40,8 % | 48,6 % |
| wood-elf *(646)* | 36,5 % | 15,0 % | 51,5 % | 59,4 % |
| skaven *(579)* | 43,5 % | 14,7 % | 58,2 % | 64,1 % |
| **agregát** *(9 496)* | **26,2 %** | 18,2 % | **44,3 %** | — |

⚠️⚠️ **A TEĎ NEPŘÍJEMNÁ VĚC: JE TO 2× VÍC, NEŽ CO MÁ P42 ZAPSANÉ** *(12,3 %
stojící, 38,9 % jakýkoli, `evidence/carrier_contact_20260820.md`)*. Rozdíl
**NEVYSVĚTLUJI a NEPROHLAŠUJI 12,3 % za vyvrácené** — platí
[[feedback_moving_baseline_only_paired_ab]]: **jiný korpus, jiná éra enginu**
*(od 20.08. přistálo M12/sideFree 30.08., M1/N10 ústup po blitzu 07.09., P35,
P38…)*, a navíc **jiný jmenovatel** *(já vyžaduji stojícího nosiče na OBOU
koncích, aby šel spočítat rozpočet pohybu; 20.08. jen na konci)*.
⇒ ⏰ **Otevřená položka: přeměřit 12,3 % na TOMHLE korpusu tímtéž skriptem**,
než se to číslo někde použije jako strop. Do té doby platí ⭐ **26,2 %
„zdarma" na `cf8634e8`**, ne 12,3 %.
⛔ **A z toho plyne, že i řetěz `→ 3,0 % kol ≈ 0,74 ztráty na zápas` je
postavený na jmenovateli, který dnes neplatí** — přepočítat se musí celý, ne
jen první číslo.

---

## ⭐⭐⭐ HLAVNÍ NÁLEZ: „VOLBA POLE" NENÍ TA DÍRA. DÍRA JE, ŽE NOSIČ NEUMÍ ODEJÍT.

`P42` má zapsané řešení *„splnitelný **volbou pole**, tedy dimenzí KAM"*.
Změřeno, kolik porušení tou dimenzí vůbec vzniká.

**Rozpad 1 546 porušení podle mechanismu** *(trpaslík; musí dát 1 546 — zbytek 0)*:

| | | | bylo kam jít *(horní mez)* |
|---|---:|---:|---:|
| **A** označen už na začátku ∧ **NEHNUL SE** | 1 014 | **65,6 %** | 1 014 = **100 %** |
| **B** na začátku volný ∧ **VEŠEL** do kontaktu | 64 | **4,1 %** | 31 = 48,4 % |
| **C** označen ∧ hnul se ∧ pořád označen | 468 | **30,3 %** | 464 = 99,1 % |

**Táž kola podatribuovaná Z UDÁLOSTÍ** *(události kola jsou v `S`,
`game_simulator.cpp:938`; musí dát 1 546 — zbytek 0)*:

| | | |
|---|---:|---:|
| **c1 nosič sám BLOKOVAL** *(kontakt z konstrukce)* | 596 | **38,6 %** |
| c1b nosič fauloval | 6 | 0,4 % |
| c2 soupeře k němu **přistrčil náš push** | 45 | 2,9 % |
| ⭐ **c3 CHŮZE HO V KONTAKTU NECHALA** | **7** | **0,5 %** |
| **c4 nosič se v kole VŮBEC NEPROJEVIL** | 892 | **57,7 %** |

⇒ ⭐⭐⭐ **`c3` je 0,5 %** *(a napříč rasami 0,4–2,1 %)*. **To je celá „volba
pole".** Zbytek — **96,6 % = c1 + c4** — jsou dva úplně jiné mechanismy:
**nosič si kontakt vyrobil vlastním blokem a zůstal v něm**, nebo
**se nehnul vůbec**.

**Bylo kam jít** *(volné pole bez soupeřovy TZ v rozpočtu)*: **97,6 %**
*(1 509 z 1 546)*, nebylo kam **2,4 %** *(37)*. Vzdálenost: **1 krok v 65,8 %**,
2 kroky 29,9 %, 3 kroky 1,9 %. **Úplně čisté pole** *(ani stojící, ani ležící
soused)* bylo k dispozici v **97,6 %** koše ZDARMA a **94,3 %** koše ZA BLITZ.
⇒ Rozdělení *„engine vybral špatně" vs „nebylo kam jít"* vychází **97,6 : 2,4**.

⚠️ **Rozpočet je HORNÍ MEZ:** log neveze `movementRemaining`, počítá se
`MA − chebyshev(začátek, konec)`; skutečná cesta je vždy ≥ chebyshev a GFI
ubírá dál ⇒ *„bylo kam jít"* je **nadhodnocené**. Proto se tiskne i varianta
s rozpočtem **1 krok**: A 63,8 % · B 79,7 % · C 74,6 %.
⚠️ **A „bylo kam" ≠ „mělo se jít":** odchod z TZ platí **DODGE**. Kdy se to
smí, je už zapsáno v doktríně Q3/B2 *(03.09.)*: `P(dodge selže) × zbývající
aktivace < 1`. Tenhle skript měří **PŘÍLEŽITOST, ne správné rozhodnutí.**

---

## ✅ CO JE V KÓDU UŽ HOTOVÉ — `P42` **NENÍ** „chybí rameno"

Ledger vede `P42` jako *„kontrola K38 stojí, **chybí rameno**"*. **Pro dimenzi
KAM to neplatí — zákaz tam je, a je i otestovaný:**

| kde | co |
|---|---|
| `macro_actions.cpp:2817` | rameno P38: kandidát s `countTacklezones > 0` se **zahodí** |
| `macro_actions.cpp:2847-2853` | záložní smyčka **stahuje cíl zpět**, dokud je obsazený nebo v TZ |
| `macro_actions.cpp:2878` | M12/sideFree hledá do boku, **taky jen pole bez TZ** |
| `macro_actions.cpp:2892-2911` | když žádné takové není, **`ADVANCE` RADĚJI REZIGNUJE** |
| `engine/tests/test_macro_actions.cpp:1595-1620` | test `AdvanceTargetPulledBackFromEnemyTZ` |

⭐ **A není to čerstvé:** `git log -L` na tu smyčku dává `6531ba00` *(**07.08.**,
„fix: ADVANCE target pulled back from enemy TZ (cage review finding 1)")* — tedy
**dva týdny PŘED tím, než P42 vznikla (20.08.)**. Položka byla zapsaná jako
otevřená nad kódem, který už ji plnil.
⇒ Táž třída jako **pět zastaralých tvrzení, která dnes vyšla v okruhu POHYB.**

**Pozitivní kontrola toho tvrzení** *(rozbít → ukázat pád → vrátit → dokázat
přesnost obnovy)*: podmínka na `:2849` nahrazena `false`, přeloženo:
* nosič skončil na **`x = 15` místo `12`**, `countTacklezones` **1 místo 0**,
  test `AdvanceTargetPulledBackFromEnemyTZ` **SPADL**, sada **727/728**;
* po obnově `git diff` **0 řádků** a sada zpátky **728/728**.
⇒ Ten řádek je **nosný**, ne dekorace — a je to jediný test, který ho drží.

### ⛔ A tady zákaz naopak NENÍ *(to je ta zbývající díra)*

| kde | co z toho plyne |
|---|---|
| `macro_actions.cpp:2054` | *„carrier has SCORE/ADVANCE"* — nosič je **vyřazen z obecné `REPOSITION`** ⇒ **nemá žádné makro „odejdi z kontaktu"** |
| `macro_actions.cpp:1383` | ústup po blitzu *(M9/N10, produkce od 07.09.)* nosiče **výslovně vynechává**; komentář `:1371-1374` to říká jako **rozhodnutý rozsah**, ne opomenutí |
| `macro_actions.cpp:1565-1571` | `ADVANCE` se **nabídne jen když nosič NEMŮŽE skórovat** ⇒ v dosahu endzóny žádnou cestu se zákazem nemá |
| `macro_actions.cpp:1146-1249` | útěk z kontaktu **existuje, ale jen pro LEŽÍCÍHO** *(Q3 „vstát a odejít", s cenou dodge)* — stojící nosič obdobu nemá |

⇒ **`c4` (57,7 %) sedí na `:2054`** a **`c1` (38,6 %) sedí na `:1383`.**

---

## ⇒ CO Z TOHO PLYNE *(⚠️ MOJE ODVOZENÍ, ne uživatelova věta)*

1. ⛔ **`P42` se nepřepisuje na „hotovo" ani nezůstává jako „chybí rameno".
   PŘESKUPUJE SE:** dimenze KAM *(kterou ledger jako řešení uvádí)* pokrývá
   **0,5 %** porušení a je **hotová i otestovaná od 07.08.** Zbytek jsou **dvě
   ramena, která P42 nepojmenovává.**
2. ⏰ **RAMENO 1 — ústup po blitzu i pro nosiče** *(`:1383`)*. Je to
   **rozšíření rozsahu už NASAZENÉHO mechanismu**, ne nový vzorec: cíl a hledač
   *(`:1391-1405`)* zůstávají, mizí jedna výjimka. Strop **38,6 %** porušení.
   ⚠️ **Ale mění nabídku ⇒ je to rameno a představuje se předem**, ne že se
   tiše nasadí. A ⛔ **nesmí se to vzít bez ceny dodge** — nosič po blitzu
   stojí v TZ, takže ústup je dodge, a `:1391` GFI zakazuje *(„ústup koupený
   GFI je hazard, ne hygiena")*; obdobu ceny má už Q3/B2 na `:1201-1229`.
3. ⏰ **RAMENO 2 — útěk stojícího nosiče z kontaktu** *(`:2054`)*. Strop
   **57,7 %** porušení, a v **100 %** těch kol bylo kam jít *(v 63,8 % stačil
   jediný krok)*. ⛔ **Je to NOVÉ rameno s cenou** — proto **jen návrh, nic
   nepostaveno.** Tvar má předlohu: Q3 „vstát a odejít" *(`:1146-1249`)* je
   týž problém pro ležícího, včetně `worstReplyCost` a meze
   `P_fail × zbývající < 1`. ⚠️ **U nosiče je ta mez ale jiná** — turnover
   u nosiče znamená **míč na zemi**, ne jen ztracenou aktivaci, takže vzorec
   z Q3 se **nedá opsat, jen odvodit znovu.**
4. ⚠️ **A `c4` má překryv s `P39`** *(„nosič se neaktivuje")*: 57,7 % porušení
   je *„nosič se v kole vůbec neprojevil"*. **Nevím, jestli je příčina
   „nabídka chybí" nebo „plánovač si ji nevybral"** — a rozhodne to teprve
   rameno 2, protože bez nabídky se to rozlišit nedá. ⛔ Do té doby se `P39`
   a `P42/c4` **nesmí vést jako dvě nezávislé položky.**
5. ⭐ **A pořadí uvnitř okruhu:** `P42` **zůstává první**, ale **z jiného
   důvodu, než jsem 10.09. napsal** — ne kvůli 53 %, které měřilo soupeře, ale
   protože **26,2 % kol dává soupeři blok na míč zdarma** *(a 13,4 % dokonce
   dva a víc)*, je to **jedno tělo bez koordinace**, a **strop opravitelnosti
   je 97,6 %**.
