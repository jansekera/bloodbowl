# Prázdné blitzy — výsledek 21. 10. 2026

Zadání (uživatel 20. 10., k diagnostice P220): prověřit, proč u 1/3 blitzů agilních hráčů vyšlo „nic“
(cíl nesražen a nepohnul se). Sonda `sonda.py` (aktuální main, P219 odstraněn), stejné nastavení her jako
`p220_20261020/diag.py` (elfové + skaveni × trpaslík/ork/člověk, útok i obrana), 36 poločasů, 198 našich
blitzů, 0 chyb. Vyhodnocení `vyhodnot.py blitzy.pkl`.

**Kontrola měřidla:** podíl „nic“ vyšel 33 % (agilní 34 %), totéž jako v diagnostice P220 (0,34).
Sraženo 76, jen odsunuto 52 ⇒ měřidlo rozlišuje všechny tři výsledky.

## „Nic“ u agilních blitzujících (39 ze 114)

| příčina | počet | co to znamená |
|---|---|---|
| útočník padá (zvolená kostka) | 10 | **padl blitzující** ⇒ turnover |
| oba padají, Block má jen obránce | 4 | **padl blitzující** ⇒ turnover |
| turnover před blokem (úhyb/GFI) | 3 | **padl blitzující** |
| cíl má Stand Firm (odtlačen / klopýtne) | 9 | skutečně nic — cíl se odtlačit nedá |
| oba padají, oba mají Block | 8 | skutečně nic |
| oba padají ⇒ obránce použil Wrestle | 5 | **oba leží** — Wrestle nedává KNOCKED_DOWN, diagnostika to vedla jako „nic“ |

⇒ **„Nic“ nejsou tři stejné případy, ale tři různé:**
1. **Blitz skončil pádem našeho hráče** — 17 z 39 (≈ 15 % všech agilních blitzů). Není to prázdný blitz, je to nejhorší výsledek.
2. **Skutečně bez účinku** — 17 z 39: Stand Firm (9, z toho 8 proti trpaslíkům) a oboustranný Block (8).
3. **Vada měřidla** — 5 z 39: Wrestle položí oba hráče bez události KNOCKED_DOWN ⇒ cíl leží, ale diagnostika P220 ho vedla jako „nic“.

## Vedlejší nálezy (k rozhodnutí, neopraveno)

- **Blitz do kostek, kde volí obránce:** u „útočník padá“ zvolená stěna 0, přestože padla i lepší — (0,2)×5, (0,1)×2, (0,4)×1 a jednou **3 kostky (0,2,4)**. Útočník by takovou stěnu nevolil ⇒ volil obránce, tj. blitz na silnějšího. Zda šlo o blitz na nosiče míče (kdy to může dávat smysl), sonda nezaznamenala.
- **Stand Firm:** 9 případů, 8 proti trpaslíkům; v 7 z 9 blitzující po bloku dál táhl. Blitz na hráče se Stand Firm může přinést jen sražení — „odsunout / uvolnit spoluhráče“ (účel z 20. 10.) na něm nefunguje.
- **Blitz bez bloku a bez turnoveru (6 ze všech 198):** Treeman (Take Root) a Rat Ogre (Wild Animal) neprošli hodem na začátku akce — blitz za kolo propadl. Pravidlově správně; otázka je jen, zda na ně utrácet jediný blitz kola.

## Důsledek pro P220 / přehodnocení P219

Kategorie „nic“ v `p220_20261020/diag.py` se má rozdělit na **padl blitzující / bez účinku (Stand Firm, oboustranný Block) / oba leží (Wrestle)** a Wrestle počítat jako cíl na zemi.

## Blitz do kostek, kde volí obránce — druhý běh 21. 10. (uživatel: „prověř blitzy kostek“)

Druhý běh sondy (`blitzy2.pkl`, tytéž hry a semínka, navíc příznak „cíl je nosič“). Volbu obránce poznám jen tehdy,
když zvolená stěna je pro útočníka horší než jiná hozená (`kostky.py`) ⇒ **dolní odhad**.

| blitz s blokem | n | cíl nosič | cíl sražen | turnover |
|---|---|---|---|---|
| prokazatelně volí obránce | **22** (12 %) | 3 | **1** (5 %) | **9** (41 %) |
| ostatní | 162 | 10 | 75 (46 %) | 10 (6 %) |

- Oba blitzy na **3 kostky proti** (elf ST2 → ork, skaven ST2 → trpaslík) **nemířily na nosiče**.
- Na nosiče míče mířily jen 3 z 22. Zbytek: náš ST2–3 proti ST3 s převahou asistencí soupeře.
- Uživatel 21. 10.: *„Tři proti nedává smysl ani proti nosiči.“* O dvou proti rozhodne podle těchto čísel.
- Příčina v kódu: `buildMacroOffer` skóruje cíl `dice * 2` bez prahu (v obraně nosič +10, hrozba TD +4, volný +2), `pickBlitzer` bere blitzujícího bez prahu kostek.
