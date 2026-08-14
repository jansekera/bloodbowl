# Pre-registrace — OTEVŘENÁ OTÁZKA Č. 1 (14.08.2026, zapsáno PŘED výpočty)

Zapsáno po přečtení checks.txt / drives.txt / analyze_gate_20260813.py, ale PŘED
spuštěním jakéhokoli vlastního výpočtu nad korpusem.

## Očekávání

**E1 (strop, hrubý):** Remíz je 1474/3000. Odhaduji, že z 865 proher je ~50–60 %
o jeden gól. Strop „+1 náš TD v každé hře" tedy čekám kolem
Δchess ≈ 0,5 × (1474 + ~480)/3000 ≈ **+0,32** — tj. hrubý strop NENÍ vázající.

**E2 (strop realistický pro útočnou procesní změnu typu brána):** Brána klece
cílí na drivy, kde míč držíme a jsme pomalí — tedy D2 (283 z 4113 přijímacích
drivů = 7 %) a část C (ztráty jsou ale z 84,5 % „soupeř srazil nosiče", což
tempo-brána neřeší). Čekám, že realistický strop brány je
**pod +0,02 chess (2 pp)**, tedy pod rozlišením 1500 párů (2σ ≈ 2,7 pp).
Tohle čekám jako HLAVNÍ vysvětlení mezery.

**E3 (pokrytí kontrol, H2):** kontroly ofenzivy (K9a, K29) se posuzují jen ve
~40 % kol (z checks.txt). Čekám, že po odečtení D1 („krátký už od výkopu",
99,1 %) zbyde kontrolám jako reálně ovlivnitelné **< 25 % přijímacích drivů**.

**E4 (konflikty kontrol, H3):** na úrovni drivů čekám:
- tempo (Δx/kolo) × bloky/kolo: **záporná** korelace (doloženo u brány),
- čisté rohy × bloky: **kladná** („čistý roh se vyrábí bitím"),
- tempo × K34/REACH0=0: **záporná** (rychlý postup exponuje nosiče),
- idle (K31) × bloky: záporná (těla buď bijí, nebo stojí).

**E5 (power, H4):** SD párové delty ≈ 0,53 (z ±5,3 pp na 400 párech).
Na efekt 1 pp chess ⇒ potřeba ~11 000 párů; na 2 pp ~2 800 párů. Čekám tedy,
že jakýkoli reálný efekt útočné procesní změny je pod měřitelností našich A/B.

**E6 (selekce/tautologie, H5):** K9a tempo přes CELÝ drive je částečně
mechanicky svázané s TD (kdo skóroval, nutně urazil ~21 polí). Čekám, že tempo
měřené jen v PRVNÍCH 3 kolech s míčem předpovídá TD výrazně slaběji —
odhad **1–2σ místo 4,2σ**. Pokud spadne pod 2σ, je „tempo předpovídá TD"
z velké části selekce, ne páka.

**E7 (verdikt):** Nejlepší vysvětlení bude kombinace E2+E5 (malá cílová
populace × šum) s příspěvkem E6 (část prediktorů je selekce). Záporné −2σ
u dw-we považuji za šum NEBO cenu z E4 (výměna bití za tempo); rozhodnout to
tento rozbor nejspíš neumí a řeknu, co by ho rozhodlo.

## Plánované výpočty
1. Per-game výsledky + marže → strop hrubý (+1 TD), strop po kategoriích
   (konverze D2; konverze C se zrušením soupeřova TD v témže drivu; konverze
   plných drivů bez TD).
2. Citlivost: Δchess na 1 konvertovaný drive; převod pp konverze → pp chess.
3. Korelační matice kontrol na úrovni drivů (bootstrap po hrách u hraničních).
4. Tempo v prvních 3 kolech vs celodrivové tempo jako prediktor TD
   (plné drivy ≥7 kol, stejná populace jako 13.08.).
5. Power výpočet z SD párové delty.
