# Sken situací S0–S10 + rozklad díry v S5 · 12.08.2026

Skript: `diag_situation_scan_20260812.py` · korpusy: `diag_replay_mine_20260811_data`
(120 her) + `..._20260811b_data` (120) + `..._20260811c_data` (40) = **280 her,
4 488 trpasličích kol**. Strom = ČÁST 1 spec `dwarf_turn_procedure_spec_20260811.md`,
rezerva = ČÁST 0.4. Konec kola = `turnLogs[i+1]`; 367 kol před TD/koncem půle
vyřazeno jen z koncových metrik, ne z klasifikace.

---

## 1) Rozložení situací (klasifikace na začátku kola)

| situace | kol | podíl |
|---|---:|---:|
| S7 boxing-in | 1 453 | 32,4 % |
| S4 nouze (rezerva < 0) | 1 242 | 27,7 % |
| S5 volný míč, dosáhneme | 698 | 15,6 % |
| S8 zabraň skóre | 360 | 8,0 % |
| S2 rozvrh (rezerva = 0) | 306 | 6,8 % |
| S3 opce (rezerva > 0) | 240 | 5,3 % |
| S10 skóruj teď | 86 | 1,9 % |
| S9 drž se na prahu | 49 | 1,1 % |
| S6 odepři | 40 | 0,9 % |
| **MIMO STROM: míč mimo hřiště** | **8** | **0,2 %** |
| S6' nedosáhne nikdo | 6 | 0,1 % |

Agregace: my držíme 1 923 kol (42,8 %) · soupeř 1 813 (40,4 %) · volný míč
752 (16,8 %). **S0/S1 (SETUP) v korpusu vůbec nejsou** — turn_logs nesou jen
odehraná kola, rozestavení se nesnímkuje.

**⭐ Nejdůležitější číslo (díra v katalogu):** tvrdě mimo strom spadlo jen
**8 kol (0,2 %)** — míč mimo hřiště (snímek mezi vyhozením a throw-inem).
Strom je jinak úplný a deterministický. **ALE** dvě větve nejsou z logu
rozhodnutelné, jen dohodnuté:

1. **S5 ∩ S6: v 440 z 698 S5 kol (63 %) dosáhnou na míč OBĚ strany.** Strom
   to řeší pořadím (napřed S5), jenže právě tady sedí „trojí volba" S5/S6 —
   podmínku „umíme ho ZAJISTIT" nelze na začátku kola spočítat, takže je to
   fakticky jedna sloučená situace, ne dvě.
2. **Hranice S2/S3/S4 stojí na `paceAch` — a ten se NELOGUJE**
   (`plan.achievable_pace` je v celém korpusu 0.0). Citlivost (počty kol):

   | paceAch | S2 | S3 | S4 |
   |---|---:|---:|---:|
   | podle MA nosiče (R1: 6→3,41 · 5→2,5 · 4→1,50) — primární | 306 | 240 | 1 242 |
   | konst. 1,73 (měřené tempo) | 6 | 0 | 1 782 |
   | konst. 2,5 (doktrína) | 121 | 36 | 1 631 |
   | konst. 3,41 (Runner) | 354 | 272 | 1 162 |

   S4 je při každé volbě dominantní stav s naším nosičem (60–93 % z nich);
   histogram rezervy (pace podle MA): <0 v 1 324 kolech, =0 v 341, >0 v 258.
   **K28 se bez X6 (zápis achievablePace do TurnLogu) nedá přibít** — S2 se
   pohne o dva řády podle volby konstanty.

Aproximace: „poslední kolo půle" = turn 8; S8 = soupeřův turn ≥ 7 (pořadí
tahů v kole neřeším); rezerva ≈ 0 čtu jako == 0.

---

## 2) Rozklad díry v S5 — ⭐ číslo 43 pb je ZASTARALÉ

Nejdřív provenience: **96 % / 53 % pochází z korpusu `20260730_data` — n = 53
volných kol, engine z 30.07.,** před rosterem 07.08., před opravou rozestavení
41c3570 i před 1kostkovými bloky a788bfb. Reprodukce na starém korpusu: dosah
96 %, pokus 51 %. **Na dnešních korpusech (11.08.) díra z větší části
neexistuje:**

| korpus | volných kol | dosah | pokus |
|---|---:|---:|---:|
| 20260730 (zdroj čísla 53 %) | 53 | 96 % | 51 % |
| 20260811 | 321 | 93 % | 81 % |
| 20260811b | 327 | 94 % | 85 % |
| 20260811c | 96 | 95 % | 85 % |

V kolech klasifikovaných jako S5 (dosáhneme): **pokus 621/698 = 89,0 %**,
z toho úspěšný sběr 537. Zbývá 77 kol bez pokusu (11 % S5). Rozklad:

**a) Dostupnost — REKONSTRUKCE, ne log** (predikát X5 chybí; dopočet
Dijkstrou z pozic: obsazená pole neprůchozí, dodge = odchod z TZ stojícího
soupeře, vstávání 3 MA, GFI +2):

| dostupnost sběru | všech S5 | z toho pokus | bez pokusu |
|---|---:|---:|---:|
| A — čistý (0 dodge, 0 GFI) | 566 (81 %) | 528 (93 %) | 38 |
| B — jen s GFI | 98 (14 %) | 83 (85 %) | 15 |
| C — nutný dodge | 34 (5 %) | 10 (29 %) | 24 |
| D — nedosažitelný po cestě | 0 | — | 0 |

Chebyshev dosah nikdy nelhal směrem „po cestě to nejde" — tier D je prázdný.

**b) 77 kol bez pokusu:**
- **42 (55 %) ukončil TURNOVER dřív, než na sběr došlo** (A:19 · B:9 · C:14)
  — kolo umřelo na jiné riskantní akci. To není vada S5, to je pořadí rizika
  (S2.14 / Z14): sběr měl přijít před akcí, která kolo zabila.
- **24 kol tier C (nutný dodge)** — z toho 10 bez turnoveru; nechat míč ležet
  je tady obhajitelné (S5.5: bez zajištění neber; dodge + pickup bez Sure Hands
  na cestě je přesně hod, který doktrína zakazuje).
- **Čistá vada volby: tier A bez turnoveru = 19 kol = 2,7 % S5.** Co dělal
  místo sběru (přes všech 77: pohyb jinam 22 · žádná naše akce 22 · bloky+pohyb
  jinam 16 · pohyb k míči, nedošel 12 · jen bloky 5). Z těch 19 s platným
  koncem: 4× aspoň stálo naše tělo u míče (odepření), 6× míč zůstal nekrytý —
  to jsou jediná opravdu nevysvětlená kola.

---

## 3) Verdikt: vada volby, NE generátoru — a hlavně vada DAT

Generátor maker jsem přečetl (`engine/src/macro_actions.cpp:551–635`):
`PICKUP` makro se při míči na zemi emituje pro nejlepšího (a druhého) sběrače
z **všech volných hráčů s Chebyshev dosahem ≤ movementRemaining+2** — tedy
tatáž podmínka, kterou je definované S5. **Generátor nabídku nedluží;**
tier D je navíc prázdný, takže ani „nabídnuto, ale fyzicky neproveditelné"
nenastává. Předregistrovaná varianta „S5 se stěhuje do generátoru akcí"
**NENASTALA** — fronta úkolů se kvůli tomu nepřeskupuje.

Skutečný obsah bývalých „43 pb": ① zastaralé měření (n=53, engine o šest
oprav starší) — dnes je díra 11 pb; ② z toho víc než polovina = turnover
zabil kolo před sběrem (pořadí rizika, ne S5); ③ ~třetina = dodge-only
situace, kde neber je legitimní; ④ zbytek 2,7 % = vada volby searche.

---

## 4) S5.3 a S5.4 — poprvé změřeno

- **S5.3 (záloha u míče před hodem): 155/698 = 22,2 %** kol S5 má na začátku
  kola stojícího našeho souseda míče (v kolech s pokusem 138/621 = 22,2 %).
  Aproximace: měřím startovní snímek, ne okamžik hodu — pohyby uvnitř kola
  před sběrem nevidím (chybí pořadí maker, X3/X4).
- **S5.4 (nosič po úspěšném sběru krytý): 138/479 = 28,8 %** — nosič
  nedosažitelný soupeřovým blitzem (Chebyshev MA+2 na sousedství, bez cest
  a TZ ⇒ hrozbu NADSAZUJI, 28,8 % je dolní odhad krytí).
- Bonus k S5.2: v tier-A kolech drží nejlepší čistý dosah Runner v 405/566
  (72 %).

**Obě povinnosti se plní kolem čtvrtiny až třetiny** — po zániku „53 %" jsou
tohle nové největší měřené díry S5.

---

## 5) Co jsem NEZMĚŘIL a proč

- **S0/S1** — setup se do turn_logs nesnímkuje; četnost rozestavovacích chyb
  z tohoto korpusu nejde získat.
- **Skutečná hranice S2/S3/S4** — `achievable_pace` se loguje jako 0.0;
  bez X6 je to volba konstanty, ne měření (viz citlivostní tabulka).
- **„Nabídnuto vs. zvoleno" uvnitř searche** — množina legálních maker se
  neloguje (X5); dostupnost je rekonstrukce z pozic a čtení zdrojáku
  generátoru, ne log rozhodnutí.
- **S5.3 „před hodem" přesně** — bez pořadí maker (X3) měřím jen start kola.
- **S5.4 po cestě** — blitz dosah počítám Chebyshevem bez obsazených polí
  a TZ; skutečné krytí je o něco lepší než 28,8 %.
- **Pořadí tahů v kole** (kdo hraje v kole první) — S8 hranice je aproximace
  turn ≥ 7.
