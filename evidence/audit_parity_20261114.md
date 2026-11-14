# AUDIT PARITY PHP × C++ — 14.11.2026 (páteční kontrola, posunuto z 13.11.)

Rozsah: commity v `src/Engine`, `src/AI`, `engine/src`, `engine/include` od auditu 6.11. (`e185d8f0`): 32 PHP, 27 C++.
Audit čtením commitů a řádků knihy úkolů. Většina pravidlových commitů je dvojitá („both engines“ / „oba enginy“):
P256, P258/P259, P263–P266, P264/P271, P275, P276, P280/P281, P283, P284, P287/P290, P291, T5.23.

## Jednostranné změny — protějšek ověřen

| commit | engine | co | druhý engine |
|---|---|---|---|
| `011a6165` P296, `affd1833` P297, `34cfa40c` P299, `2b434d2f` P301 | PHP | pohyb po krocích, jedna aktivace, akce jiného hráče uzavře předchozí, blitz jen bez pohybu, reset drivu | **P298 (`e68fd96a`, 14.11.)**: C++ měl jednu aktivaci (`currentActivationId`), kontrolu velkého hráče 1× za kolo a reset drivu; doplněny 2 mezery (blitz po pohybu, ohlášený blitz neuzavřen) ✅ |
| `9363b0b4` P255/P257 | C++ | Hail Mary jen mimo pravítko, legalita přihrávky na jednom místě | srovnání C++ **podle** PHP (`PassResolver`) ✅ |
| `167a8f30` P269/P270 | PHP | hozený hráč smí po přistání táhnout; PHP AI hází spoluhráče | srovnání PHP **podle** C++ (stav po přistání, makro THROW_TEAM_MATE P268) ✅ |
| `610bb0d2` P277 | PHP | Leap: libovolné pole do 2, bez zón, GFI, 1× za kolo | PHP srovnáno **podle** C++ ✅ |
| `66c95728` P278/P279 · P282 | PHP · C++ | TTM: dopad na hráče, přistání s míčem = turnover · odskok volného míče | každý engine opraven k pravidlům, ve kterých byl druhý správně ✅ |
| `acb8a166` P285 | C++ | zraněná tajná zbraň zůstává zraněná | C++ **podle** PHP ✅ |
| `68ee83ad` P286 | C++ | Leap z lehu (vstát a skočit) | PHP Leap z lehu nabízí (P277 přes `Pathfinder` + vstání) — v knize jako omezení volby C++, ne pravidla ✅ |
| `52cd699f` P289, `d9e8fd05` P292, `b997aa51` P272 | PHP | kouč-člověk dostane otázky na přehoz (velcí hráči, Leap, přihrávka, bomba, TTM) a na bombu | C++ se kouče ptal už dřív (P130, `coachDecides`, `9be135e6` P272 C++) ✅ |
| `97afec8a` P251b | C++ | živý kouč se ptá na přehoz bomby | PHP v P292 ✅ |
| `a89d6055` P268, `61f95682` P267, `989c3437`/`dbd99626` P302 | C++ | AI makra (bomba, TTM), soupiska goblinů, měřidlo a obestoupení míče | jen AI / data, netýká se pravidel PHP |

## Výsledek

**Z commitů od 6.11. žádná nová mezera.** Jediné otevřené rozdíly z tohoto období (P296–P301) převedl P298 dnes.
**Nový nález při ověřování (uživatel 14.11.: „prověř jump up blok v obou enginech“) ⇒ P303:** blok z lehu s Jump Up (ř. 8196-8203, hod AG +2) — **PHP ho dovolí bez hodu** (vada pravidla), **C++ ho nenabízí** (chybí volba, dřív T4.5). Zapsáno, opraví se v obou enginech.
Otevřené z dřívějších auditů beze změny (viz `audit_parity_20261019.md`).
