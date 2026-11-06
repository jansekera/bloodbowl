# AUDIT PARITY PHP × C++ — 6.11.2026 (páteční kontrola)

Rozsah: commity v `src/Engine`, `src/AI`, `engine/src`, `engine/include` od auditu 19.10. (31 PHP, 62 C++). Audit čtením.
Většina pravidlových commitů už je dvojitá (v popisu „C++ i PHP“ nebo „parita s C++“): Diving Catch (P221/P223/P235/P237),
Grab a Side Step (P192/P222/P224), Ball & Chain Grab jen proti soupeři, Stand Firm otázka a past na Frenzy (P225/P226),
Decay a Regenerace po smrti (P192), blitz do kostek, kde volí obránce (P227).

## Ověřeno v kódu

| commit | co | druhý engine |
|---|---|---|
| `d15c8ce2` PHP P221 | Regenerace jednou a až po volbě lékárníka (ř. 8406-8411) | C++ `injury.cpp:83-89` se ptá na lékárníka synchronně, Regenerace až potom (`injury.cpp:193`) ✅ |
| `0d2c8a0e` PHP P223 review | touchback po odrazu míče z výkopu ven / do kopající poloviny (ř. 279-281, FAQ 9315-9318) | commit je „parita s C++ landBall“ — PHP srovnáno podle C++ ✅ |
| `21cb814b`, `ccd12695` C++ | Sprint u Bloodlust (GFI 3), přehrání doběhu blitzu přes GFI | jen AI makra C++ (`macro_actions.cpp`); PHP AI Bloodlust krmení nemá, PHP `Pathfinder` Sprint = 3 GFI ✅ |
| `5b9409a5` C++ | turnover soupeře při bloku na roh (ř. 366-371, Wrestle 8670-8677) | jen odhad v AI klece (`cage_advance.cpp`) — netýká se PHP |
| `5fcf0962` | kopající s Diving Catch u výkopu neskáče do touchbacku (ř. 8066) | týž commit v obou ✅ |

## P247 — přehoz nepřesné přihrávky (otevřené z 3.11.)

- **Pravidla:** týmový přehoz smí přehodit **kterýkoli hod** kromě zbroje, zranění a Casualty (ř. 929-931); Pass přehazuje
  **nepřesnou přihrávku i fumble** (ř. 8336-8337). ⇒ **Oba enginy jsou v pravidlech** — liší se jen volba, kdy přehoz utratit.
- **C++** (`pass_handler.cpp:329-375`): Pass na nepřesnou i fumble ✅; kouč-člověk se ptá u obojího ✅; **AI (engine) bere Pro
  a týmový přehoz jen na fumble** (komentář: „volba trenéra, ne oprava“).
- **PHP** (`PassResolver.php:124-160`): Pass, Pro i týmový přehoz na **každou nepřesnou** přihrávku.
- ⚠️ Premisa C++ („jen fumble hrozí turnoverem“) neplatí úplně: nepřesná přihrávka se rozptýlí a když ji nechytí spoluhráč,
  je to taky turnover. ⇒ **rozhodne uživatel**, která volba platí; změna C++ mění měření síly AI (přihrávky).

## Výsledek

**Žádná nová mezera v pravidlech z commitů od 19.10.** Review P247 týž den ale našlo dvě vady pravidel: **P252** (PHP Safe Throw) a **P254** (C++ Hail Mary ve vánici + legalita hodu až po puštění míče) — obě opraveny 6.11. P247 = rozdíl ve volbě AI — uživatel 6.11. „přehoď i nepřesnou vždy“ ⇒ C++ srovnáno s PHP týž den. Týž princip jinde: **P251** (Always Hungry u TTM přehazovalo jen PHP; přesnost TTM a bomby nepřehazoval ani jeden) — uživatel 6.11.: Always Hungry ano, přesnost TTM ne, bomba jen fumble a Loner bombař bez týmového přehozu ⇒ hotovo v obou enginech týž den. **P253** (AI cení přihrávku s přehozy, přednost Pass/Catch) hotovo.
Otevřené z dřívějších auditů beze změny (viz `audit_parity_20261019.md`).
