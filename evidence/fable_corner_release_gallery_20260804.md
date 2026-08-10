# Galerie DICEY situací — corner-release groundwork (04.08.2026)

Příloha k `fable_corner_release_report_20260804.md`. 62 DICEY situací z 24 her
(6 her × 4 matchupy: dwarf–skaven, dwarf–wood-elf, dwarf mirror, orc–skaven),
seedy 36M+ (disjunktní od 30/31/34/35M), produkční config (MCTS-100,
vfBlend 0,15, policyBlend 0), engine = main 51c1aa0 + merge f1-cage-fix
(5 commitů, testy 493/493). Sonda: rozšířený `diag_f1_adoption_probe.cpp`
(worktree agent-acc24c039cd798d6e).

## Legenda

- Mapa: okno 9×7 kolem carriera, JEHO směr útoku je vždy VPRAVO.
  `C`=carrier, `F`=hráč selhavší nohy plánu, `T`/`t`=spoluhráč stojící/na zemi,
  `O`/`o`=soupeř stojící/na zemi, `*`=cílové pole selhavší nohy (je-li prázdné),
  `b`=volný míč, `.`=prázdné, `#`=mimo hřiště.
- Hlavička: `dist`=vzdálenost carriera do endzóny, `step`=plánovaný krok klece,
  `req/ach`=requiredPace/achievablePace, `resist`=soupeři v koridoru,
  `built/filled/open`=rohy stojící/obsazené po tahu/otevřené, `cgfi`=carrier GFI.
- `legs`: nohy plánu v exekučním pořadí (dependency sort), `!`=selhavší noha,
  `[CAR]`=carrier, `[gfi]`=roh na 1-GFI výjimce (jde pěšky, zastaví o pole dřív).
- `FAIL`: `tzAtStart`=TZ na startu hráče, `pto`=MC-sonda pTO (K=48),
  `ceil`=práh, `execFail=1`=sonda prošla, ale greedy walk nedošel na cíl
  (`sampleEnd`/`miss`=kde jedna ukázková exekuce skončila / o kolik polí krátce).
- `CAT`: a=marked roh bez dosažitelného substituta, b=TZ stín na koridoru,
  c=žádná volná těla, d=jiné (pile spoluhráčů na trase apod.).
- `QUAD` (uživatelova čtveřice): `needDodge`=kolik rohů plánu potřebuje dodge,
  `freeSubs`=volná těla na substituci (`reach`=kolik z nich dosáhne na selhavší
  slot), `markers`=soupeři k odstranění (markeři rohu, nebo TZ-casteři
  koridoru u kat. b), `blockCov`=kolik z nich pokryjí bloky SOUSEDÍCÍCH volných
  spoluhráčů (reálné kostky vč. asistů), `blockRel`/`blitzRel`=uvolnila by
  situaci block-only / block+1 blitz vrstva, `collision=1`=potřeba >1 blitz.
- `marker P..`: `bestBlock`=nejlepší blok ze sousedství (kostky, záporné =
  volí obránce), `bestBlitz`=nejlepší path-aware blitz (item14 odhad:
  approach dodge/GFI + kostky bloku; `fail`=kombinovaná pravděpodobnost).

## Souhrn (62 situací)

| kategorie | počet | block-only uvolní | block+blitz uvolní | kolize (>1 blitz) |
|---|---|---|---|---|
| a — marked roh, substituti mimo dosah | 15 | 6 | 14 | 0 |
| b — TZ stín na koridoru | 40 | 5 | 19 | 20 |
| c — žádná volná těla | 3 | 1 | 2 | 0 |
| d — jiné (pile/stall) | 4 | 0 | 0 | 0 |
| **celkem** | **62** | **12 (19 %)** | **35 (56 %)** | **20 (32 %)** |

Per matchup: dw-sk 13, dw-we 16, dw-dw 10, orc-sk 23 DICEY.

## Matchup dwarf–skaven (13 situací)

```text
[DICEY] dw-sk-g0 H1 T3 HOME | carrier P11@(6,4) dist=19 step=4 req=3.80 ach=4.00 resist=2 built=4 filled=3 open=1 cgfi=0
  legs(4): 0:P4(7,5)->(11,3) 1:P6(7,6)->(11,5) 2:P8(7,7)->(9,5) !3:P11(6,4)->(10,4)[CAR]
  FAIL leg#3 P11 MA4 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=4.00 execFail=1 dist=4 budget=4 TIGHT CARRIER-LEG sampleEnd=(10,5) miss=1 to=0
  CAT=b (TZ shadow on line, budget exact) casters=1 pileOnLine=0
  QUAD: needDodge=0 freeSubs=3(reach=3) markers=1 blockCov=0 blockRel=0 blitzRel=1 collision=0
  marker P13@(9,3) ST3 bestBlock=0d(P-1) bestBlitz=1d(P4 fail=0.17)
    ....o....
    ....O....
    ..TTT.O..
    ...C...*.
    ..T.T....
    ..T.T....
    ....T....
[DICEY] dw-sk-g0 H1 T5 HOME | carrier P3@(9,4) dist=16 step=6 req=5.33 ach=6.00 resist=3 built=2 filled=3 open=1 cgfi=0
  legs(4): !0:P8(12,7)->(16,3) 1:P6(11,5)->(16,5) 2:P1(10,2)->(14,3) 3:P3(9,4)->(15,4)[CAR]
  FAIL leg#0 P8 MA4 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=4.00 execFail=1 dist=4 budget=4 TIGHT  sampleEnd=(16,7) miss=4 to=0
  CAT=b (TZ shadow on line, budget exact) casters=2 pileOnLine=0
  QUAD: needDodge=1 freeSubs=2(reach=1) markers=2 blockCov=0 blockRel=0 blitzRel=0 collision=1
  marker P22@(13,5) ST2 bestBlock=0d(P-1) bestBlitz=2d(P4 fail=0.03)
  marker P20@(15,5) ST2 bestBlock=0d(P-1) bestBlitz=2d(P4 fail=0.97)
    .oo......
    ....T.O..
    .........
    ..oCOot..
    ..TTTT.Oo
    ...tT.t..
    ......F..
[DICEY] dw-sk-g0 H2 T2 AWAY | carrier P22@(15,5) dist=15 step=3 req=2.50 ach=8.00 resist=5 built=4 filled=3 open=1 cgfi=0
  legs(3): !0:P13(14,4)->(11,4) 1:P21(15,4)->(13,4) 2:P22(15,5)->(12,5)[CAR]
  FAIL leg#0 P13 MA7 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=2.00 execFail=1 dist=3 budget=7  sampleEnd=(13,3) miss=2 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=3 pileOnLine=1
  QUAD: needDodge=0 freeSubs=4(reach=4) markers=3 blockCov=0 blockRel=0 blitzRel=0 collision=1
  marker P1@(11,5) ST3 bestBlock=0d(P-1) bestBlitz=1d(P17 fail=0.17)
  marker P4@(11,3) ST3 bestBlock=0d(P-1) bestBlitz=1d(P17 fail=0.17)
  marker P8@(10,5) ST3 bestBlock=0d(P-1) bestBlitz=1d(P17 fail=0.86)
    .........
    .......O.
    ..TTF.T*.
    ...C...OO
    ..T.Tt.O.
    ......OOO
    ....TT...
[DICEY] dw-sk-g1 H1 T4 HOME | carrier P3@(5,8) dist=20 step=5 req=5.00 ach=7.00 resist=2 built=0 filled=4 open=0 cgfi=0
  legs(5): 0:P2(15,7)->(11,7) 1:P8(11,10)->(11,9) !2:P4(6,8)->(9,7) 3:P6(5,9)->(9,9) 4:P3(5,8)->(10,8)[CAR]
  FAIL leg#2 P4 MA5 tzAtStart=3 pto=0.938 ceil=0.02 meanActs=1.38 execFail=0 dist=3 budget=5  sampleEnd=(7,7) miss=2 to=1
  CAT=c (marked mover, zero free bodies) casters=1 pileOnLine=0
  QUAD: needDodge=2 freeSubs=0(reach=0) markers=3 blockCov=2 blockRel=0 blitzRel=1 collision=0
  marker P14@(6,7) ST3 bestBlock=0d(P-1) bestBlitz=2d(P8 fail=0.19)
  marker P18@(5,7) ST3 bestBlock=2d(P10) bestBlitz=2d(P10 fail=0.03)
  marker P21@(6,9) ST2 bestBlock=2d(P6) bestBlitz=2d(P6 fail=0.03)
    .........
    ..T......
    ...OO..*.
    ..TCFt...
    ..OTO....
    .........
    ...O.....
[DICEY] dw-sk-g1 H1 T5 HOME | carrier P3@(6,9) dist=19 step=7 req=6.33 ach=8.00 resist=0 built=1 filled=3 open=1 cgfi=1
  legs(4): 0:P8(11,10)->(14,10) 1:P2(15,7)->(12,8) !2:P6(6,7)->(12,10)[gfi] 3:P3(6,9)->(13,9)[CAR]
  FAIL leg#2 P6 MA5 tzAtStart=1 pto=0.292 ceil=0.02 meanActs=3.83 execFail=0 dist=6 budget=5 TIGHT  sampleEnd=(10,10) miss=2 to=0
  CAT=a (marked mover, free bodies out of reach) casters=0 pileOnLine=1
  QUAD: needDodge=1 freeSubs=3(reach=0) markers=1 blockCov=0 blockRel=0 blitzRel=1 collision=0
  marker P14@(7,6) ST3 bestBlock=0d(P-1) bestBlitz=2d(P4 fail=0.03)
    ..o.O....
    ..TF.....
    ..T.t....
    .T.C.....
    ..ooo...T
    .........
    .........
[DICEY] dw-sk-g1 H2 T3 HOME | carrier P11@(11,5) dist=14 step=3 req=2.80 ach=5.00 resist=2 built=3 filled=3 open=1 cgfi=0
  legs(4): 0:P4(13,5)->(15,4) !1:P3(14,10)->(15,6) 2:P10(10,6)->(13,6) 3:P11(11,5)->(14,5)[CAR]
  FAIL leg#1 P3 MA6 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=1.00 execFail=1 dist=4 budget=6  sampleEnd=(13,10) miss=4 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=2 pileOnLine=0
  QUAD: needDodge=0 freeSubs=2(reach=2) markers=2 blockCov=0 blockRel=0 blitzRel=0 collision=1
  marker P18@(15,8) ST3 bestBlock=0d(P-1) bestBlitz=1d(P6 fail=0.17)
  marker P21@(16,9) ST2 bestBlock=0d(P-1) bestBlitz=2d(P6 fail=0.19)
    .........
    ...t.O...
    ..T.To...
    ...CoT...
    ..T.t..*.
    ...tTO...
    .....O.O.
[DICEY] dw-sk-g2 H1 T4 HOME | carrier P11@(9,5) dist=16 step=4 req=4.00 ach=5.00 resist=1 built=2 filled=3 open=1 cgfi=0
  legs(4): !0:P7(11,4)->(14,6) 1:P6(10,4)->(12,4) 2:P2(10,6)->(12,6) 3:P11(9,5)->(13,5)[CAR]
  FAIL leg#0 P7 MA5 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=2.00 execFail=1 dist=3 budget=5  sampleEnd=(12,4) miss=2 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=1 pileOnLine=0
  QUAD: needDodge=0 freeSubs=4(reach=1) markers=1 blockCov=0 blockRel=0 blitzRel=1 collision=0
  marker P12@(12,7) ST3 bestBlock=0d(P-1) bestBlitz=1d(P2 fail=0.17)
    ....o....
    ..oT..t..
    T...TF..O
    ...C.t...
    O.O.T...*
    ......O..
    ........o
[DICEY] dw-sk-g2 H2 T1 AWAY | carrier P21@(16,9) dist=16 step=3 req=2.29 ach=10.00 resist=2 built=0 filled=3 open=1 cgfi=0
  legs(3): !0:P18(14,8)->(12,8) 1:P17(14,6)->(14,8) 2:P21(16,9)->(13,9)[CAR]
  FAIL leg#0 P18 MA7 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=1.00 execFail=1 dist=2 budget=7  sampleEnd=(14,7) miss=2 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=2 pileOnLine=1
  QUAD: needDodge=0 freeSubs=3(reach=3) markers=2 blockCov=1 blockRel=0 blitzRel=1 collision=0
  marker P2@(12,7) ST3 bestBlock=2d(P13) bestBlitz=2d(P13 fail=0.11)
  marker P6@(11,8) ST3 bestBlock=0d(P-1) bestBlitz=1d(P14 fail=0.89)
    .....TT.O
    .T....TO.
    .....FT*O
    ...C.....
    .....T.O.
    ........O
    .........
[DICEY] dw-sk-g4 H1 T5 HOME | carrier P3@(8,8) dist=17 step=6 req=5.67 ach=7.00 resist=2 built=1 filled=4 open=0 cgfi=0
  legs(4): !0:P7(10,8)->(15,7) 1:P2(8,6)->(15,9)[gfi] 2:P6(8,5)->(13,7) 3:P3(8,8)->(14,8)[CAR]
  FAIL leg#0 P7 MA5 tzAtStart=1 pto=0.229 ceil=0.02 meanActs=4.08 execFail=0 dist=5 budget=5 TIGHT  sampleEnd=(15,7) miss=0 to=0
  CAT=a (marked mover, free bodies out of reach) casters=1 pileOnLine=0
  QUAD: needDodge=2 freeSubs=1(reach=0) markers=1 blockCov=1 blockRel=1 blitzRel=1 collision=0
  marker P16@(9,7) ST3 bestBlock=2d(P2) bestBlitz=2d(P2 fail=0.03)
    ...TtO...
    ...ToO...
    .OOTO....
    ...CTF...
    ..T.....t
    .........
    .........
[DICEY] dw-sk-g4 H2 T3 HOME | carrier P2@(13,5) dist=12 step=3 req=2.40 ach=5.00 resist=4 built=2 filled=3 open=1 cgfi=0
  legs(4): !0:P5(12,8)->(17,4) 1:P8(14,4)->(17,6) 2:P10(12,6)->(15,4) 3:P2(13,5)->(16,5)[CAR]
  FAIL leg#0 P5 MA5 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=1.00 execFail=1 dist=5 budget=5 TIGHT  sampleEnd=(12,7) miss=5 to=0
  CAT=b (TZ shadow on line, budget exact) casters=5 pileOnLine=1
  QUAD: needDodge=1 freeSubs=2(reach=1) markers=5 blockCov=3 blockRel=0 blitzRel=0 collision=1
  marker P13@(14,6) ST3 bestBlock=2d(P7) bestBlitz=2d(P7 fail=0.03)
  marker P21@(14,7) ST2 bestBlock=2d(P7) bestBlitz=2d(P7 fail=0.03)
  marker P22@(14,8) ST2 bestBlock=2d(P3) bestBlitz=2d(P3 fail=0.03)
  marker P12@(14,5) ST3 bestBlock=2d(P8) bestBlitz=2d(P8 fail=0.03)
  marker P17@(15,6) ST3 bestBlock=0d(P-1) bestBlitz=1d(P8 fail=0.86)
    .........
    .T.o.....
    ..O.T..*.
    ..TCO....
    ..TTOO...
    ...TO....
    ..FoO....
[DICEY] dw-sk-g4 H2 T4 HOME | carrier P2@(12,4) dist=13 step=4 req=3.25 ach=6.00 resist=3 built=2 filled=4 open=0 cgfi=0
  legs(5): !0:P5(12,8)->(17,3) 1:P8(14,4)->(17,5) 2:P10(11,5)->(15,3) 3:P6(11,9)->(15,5) 4:P2(12,4)->(16,4)[CAR]
  FAIL leg#0 P5 MA5 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=3.00 execFail=1 dist=5 budget=5 TIGHT  sampleEnd=(12,5) miss=5 to=0
  CAT=b (TZ shadow on line, budget exact) casters=5 pileOnLine=1
  QUAD: needDodge=1 freeSubs=1(reach=0) markers=5 blockCov=3 blockRel=0 blitzRel=0 collision=1
  marker P13@(14,6) ST3 bestBlock=2d(P1) bestBlitz=2d(P1 fail=0.03)
  marker P21@(14,7) ST2 bestBlock=2d(P7) bestBlitz=2d(P7 fail=0.03)
  marker P22@(14,8) ST2 bestBlock=2d(P3) bestBlitz=2d(P3 fail=0.03)
  marker P12@(14,5) ST3 bestBlock=2d(P1) bestBlitz=2d(P1 fail=0.03)
  marker P17@(15,6) ST3 bestBlock=0d(P-1) bestBlitz=1d(P8 fail=0.86)
    .........
    .........
    ..O.o...*
    .T.C.T...
    ..T.TO...
    ....TOO..
    ....TO...
[DICEY] dw-sk-g4 H2 T5 HOME | carrier P2@(12,4) dist=13 step=5 req=4.33 ach=6.00 resist=3 built=2 filled=4 open=0 cgfi=0
  legs(5): !0:P7(13,7)->(18,3) 1:P3(13,9)->(18,5) 2:P5(12,8)->(16,3) 3:P6(11,9)->(16,5) 4:P2(12,4)->(17,4)[CAR]
  FAIL leg#0 P7 MA5 tzAtStart=3 pto=0.312 ceil=0.02 meanActs=1.69 execFail=0 dist=5 budget=5 TIGHT  sampleEnd=(12,5) miss=6 to=0
  CAT=a (marked mover, free bodies out of reach) casters=4 pileOnLine=0
  QUAD: needDodge=2 freeSubs=2(reach=0) markers=3 blockCov=3 blockRel=1 blitzRel=1 collision=0
  marker P13@(14,6) ST3 bestBlock=2d(P1) bestBlitz=2d(P1 fail=0.03)
  marker P21@(14,7) ST2 bestBlock=2d(P9) bestBlitz=2d(P9 fail=0.03)
  marker P22@(14,8) ST2 bestBlock=2d(P3) bestBlitz=2d(P3 fail=0.03)
    .........
    .........
    ..O.o....
    .T.C.T...
    ..T.TO...
    ....TOO..
    ....FO...
[DICEY] dw-sk-g5 H1 T5 HOME | carrier P6@(5,4) dist=20 step=7 req=6.67 ach=7.00 resist=0 built=0 filled=3 open=1 cgfi=2
  legs(3): !0:P1(12,5)->(13,3) 1:P9(8,4)->(11,3) 2:P6(5,4)->(12,4)[CAR]
  FAIL leg#0 P1 MA4 tzAtStart=1 pto=0.875 ceil=0.02 meanActs=1.31 execFail=0 dist=2 budget=4  sampleEnd=(13,4) miss=1 to=1
  CAT=a (marked mover, free bodies out of reach) casters=1 pileOnLine=0
  QUAD: needDodge=1 freeSubs=4(reach=1) markers=1 blockCov=0 blockRel=0 blitzRel=1 collision=0
  marker P18@(13,5) ST3 bestBlock=0d(P-1) bestBlitz=2d(P7 fail=0.19)
    .........
    .....ot..
    ...o.T...
    ...CooT..
    ....t.t.O
    ...TT....
    .....T...
```

## Matchup dwarf–wood-elf (16 situací)

```text
[DICEY] dw-we-g0 H1 T2 HOME | carrier P2@(5,9) dist=20 step=6 req=3.33 ach=7.00 resist=0 built=2 filled=4 open=0 cgfi=0
  legs(5): 0:P1(8,8)->(12,8) 1:P6(7,10)->(12,10) 2:P5(7,8)->(10,8) 3:P8(7,7)->(10,10) !4:P2(5,9)->(11,9)[CAR]
  FAIL leg#4 P2 MA6 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=6.00 execFail=1 dist=6 budget=6 TIGHT CARRIER-LEG sampleEnd=(10,9) miss=1 to=0
  CAT=d (teammate pile on line (untangle gap)) casters=0 pileOnLine=1
  QUAD: needDodge=0 freeSubs=4(reach=3) markers=0 blockCov=0 blockRel=0 blitzRel=0 collision=0
    .........
    .....T...
    ...TTTT..
    ...CT....
    ....TT...
    .........
    .........
[DICEY] dw-we-g0 H1 T3 HOME | carrier P2@(5,9) dist=20 step=6 req=4.00 ach=7.00 resist=0 built=2 filled=4 open=0 cgfi=0
  legs(5): 0:P1(8,8)->(12,8) 1:P6(7,10)->(12,10) 2:P5(7,8)->(10,8) 3:P8(7,7)->(10,10) !4:P2(5,9)->(11,9)[CAR]
  FAIL leg#4 P2 MA6 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=6.00 execFail=1 dist=6 budget=6 TIGHT CARRIER-LEG sampleEnd=(10,9) miss=1 to=0
  CAT=d (teammate pile on line (untangle gap)) casters=0 pileOnLine=1
  QUAD: needDodge=0 freeSubs=4(reach=3) markers=0 blockCov=0 blockRel=0 blitzRel=0 collision=0
    .........
    .....T...
    ..oTTTT..
    ...CT....
    ....TT...
    .........
    .........
[DICEY] dw-we-g0 H1 T4 HOME | carrier P2@(9,9) dist=16 step=4 req=4.00 ach=5.00 resist=3 built=4 filled=4 open=0 cgfi=0
  legs(5): 0:P4(14,7)->(14,8) !1:P1(10,10)->(14,10) 2:P9(9,8)->(12,8) 3:P6(8,10)->(12,10) 4:P2(9,9)->(13,9)[CAR]
  FAIL leg#1 P1 MA4 tzAtStart=1 pto=0.417 ceil=0.02 meanActs=2.75 execFail=0 dist=4 budget=4 TIGHT  sampleEnd=(14,10) miss=0 to=0
  CAT=a (marked mover, free bodies out of reach) casters=3 pileOnLine=0
  QUAD: needDodge=1 freeSubs=4(reach=0) markers=1 blockCov=1 blockRel=1 blitzRel=1 collision=0
  marker P18@(11,9) ST3 bestBlock=2d(P5) bestBlitz=2d(P5 fail=0.03)
    .........
    ......O.T
    T.TTT....
    T..C.OO.O
    T.T.F...*
    .........
    .........
[DICEY] dw-we-g0 H2 T3 AWAY | carrier P22@(13,9) dist=13 step=3 req=2.60 ach=6.00 resist=3 built=4 filled=4 open=0 cgfi=0
  legs(5): !0:P16(14,8)->(9,8) 1:P13(14,10)->(9,10) 2:P20(14,7)->(11,8) 3:P18(15,10)->(11,10) 4:P22(13,9)->(10,9)[CAR]
  FAIL leg#0 P16 MA7 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=2.00 execFail=1 dist=5 budget=7  sampleEnd=(13,7) miss=4 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=3 pileOnLine=1
  QUAD: needDodge=0 freeSubs=1(reach=1) markers=3 blockCov=1 blockRel=0 blitzRel=0 collision=1
  marker P5@(11,7) ST3 bestBlock=1d(P12) bestBlitz=2d(P18 fail=0.03)
  marker P10@(11,9) ST3 bestBlock=1d(P12) bestBlitz=2d(P13 fail=0.11)
  marker P8@(10,7) ST3 bestBlock=0d(P-1) bestBlitz=1d(P18 fail=0.72)
    ...T..o..
    ..T.TOO..
    ..F.T..*.
    T..CoO...
    .TT.T....
    .o.......
    .........
[DICEY] dw-we-g1 H1 T3 HOME | carrier P2@(4,10) dist=21 step=6 req=4.20 ach=8.00 resist=0 built=1 filled=4 open=0 cgfi=0
  legs(5): 0:P3(6,9)->(11,9) 1:P8(7,10)->(11,11) 2:P4(7,9)->(9,9) 3:P7(7,8)->(9,11) !4:P2(4,10)->(10,10)[CAR]
  FAIL leg#4 P2 MA6 tzAtStart=1 pto=0.333 ceil=0.02 meanActs=4.33 execFail=0 dist=6 budget=6 TIGHT CARRIER-LEG sampleEnd=(10,10) miss=0 to=0
  CAT=a (carrier himself marked (dodge to advance)) casters=1 pileOnLine=1
  QUAD: needDodge=0 freeSubs=2(reach=2) markers=1 blockCov=1 blockRel=1 blitzRel=1 collision=0
  marker P12@(4,9) ST3 bestBlock=2d(P9) bestBlitz=3d(P5 fail=0.00)
    ......T..
    ....TTT..
    ...OTTT..
    ...CT.T..
    .........
    .........
    .........
[DICEY] dw-we-g1 H2 T3 AWAY | carrier P22@(17,8) dist=17 step=8 req=3.40 ach=10.00 resist=0 built=4 filled=3 open=1 cgfi=0
  legs(4): !0:P18(12,4)->(8,7) 1:P14(12,7)->(8,9) 2:P19(18,9)->(10,9) 3:P22(17,8)->(9,8)[CAR]
  FAIL leg#0 P18 MA7 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=1.00 execFail=1 dist=4 budget=7  sampleEnd=(12,5) miss=4 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=3 pileOnLine=0
  QUAD: needDodge=1 freeSubs=5(reach=1) markers=3 blockCov=2 blockRel=0 blitzRel=1 collision=0
  marker P7@(10,4) ST3 bestBlock=1d(P12) bestBlitz=2d(P13 fail=0.11)
  marker P5@(11,7) ST3 bestBlock=1d(P14) bestBlitz=2d(P13 fail=0.11)
  marker P8@(10,7) ST3 bestBlock=0d(P-1) bestBlitz=1d(P21 fail=0.76)
    .........
    .........
    T.T.T...T
    T..Co....
    T.T.T....
    .........
    .........
[DICEY] dw-we-g2 H2 T2 AWAY | carrier P22@(18,9) dist=18 step=3 req=3.00 ach=9.00 resist=2 built=2 filled=3 open=1 cgfi=0
  legs(4): 0:P15(13,8)->(14,8) 1:P14(13,7)->(16,8) !2:P21(19,10)->(16,10) 3:P22(18,9)->(15,9)[CAR]
  FAIL leg#2 P21 MA8 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=5.00 execFail=1 dist=3 budget=8  sampleEnd=(15,12) miss=2 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=1 pileOnLine=0
  QUAD: needDodge=0 freeSubs=2(reach=2) markers=1 blockCov=0 blockRel=0 blitzRel=1 collision=0
  marker P6@(17,10) ST3 bestBlock=0d(P-1) bestBlitz=2d(P12 fail=0.11)
    .........
    ........T
    ..TOt...T
    ...C.....
    ..F.O*.OO
    ...T.....
    .........
[DICEY] dw-we-g2 H2 T4 AWAY | carrier P21@(16,12) dist=16 step=4 req=4.00 ach=9.00 resist=1 built=0 filled=4 open=0 cgfi=0
  legs(5): !0:P12(11,4)->(11,11) 1:P18(18,6)->(11,13) 2:P16(19,6)->(13,11) 3:P15(16,11)->(13,13) 4:P21(16,12)->(12,12)[CAR]
  FAIL leg#0 P12 MA7 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=7.00 execFail=1 dist=7 budget=7 TIGHT  sampleEnd=(10,10) miss=1 to=0
  CAT=b (TZ shadow on line, budget exact) casters=2 pileOnLine=0
  QUAD: needDodge=1 freeSubs=0(reach=0) markers=2 blockCov=1 blockRel=0 blitzRel=1 collision=0
  marker P5@(11,7) ST3 bestBlock=1d(P13) bestBlitz=2d(P18 fail=0.03)
  marker P11@(11,9) ST3 bestBlock=0d(P-1) bestBlitz=1d(P18 fail=0.17)
    t..O....O
    .O..t....
    ..OTO...*
    ..TC.....
    .........
    .........
    #########
[DICEY] dw-we-g3 H2 T2 AWAY | carrier P22@(16,8) dist=16 step=3 req=2.67 ach=9.00 resist=2 built=3 filled=4 open=0 cgfi=0
  legs(4): 0:P15(13,8)->(12,9) 1:P13(13,6)->(14,7) !2:P21(17,9)->(14,9) 3:P22(16,8)->(13,8)[CAR]
  FAIL leg#2 P21 MA8 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=4.00 execFail=1 dist=3 budget=8  sampleEnd=(16,7) miss=2 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=2 pileOnLine=1
  QUAD: needDodge=0 freeSubs=3(reach=3) markers=2 blockCov=1 blockRel=0 blitzRel=1 collision=0
  marker P3@(15,9) ST3 bestBlock=1d(P19) bestBlitz=2d(P20 fail=0.03)
  marker P6@(14,10) ST3 bestBlock=0d(P-1) bestBlitz=1d(P15 fail=0.33)
    ......T..
    ......T.o
    ..T.T..TO
    ...C..T..
    ..FTO*..O
    .....O...
    .........
[DICEY] dw-we-g3 H2 T4 AWAY | carrier P22@(13,7) dist=13 step=4 req=3.25 ach=7.00 resist=3 built=1 filled=4 open=0 cgfi=0
  legs(5): !0:P15(13,8)->(8,6) 1:P12(14,6)->(8,8) 2:P16(14,4)->(10,6) 3:P21(15,9)->(10,8) 4:P22(13,7)->(9,7)[CAR]
  FAIL leg#0 P15 MA7 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=2.00 execFail=1 dist=5 budget=7  sampleEnd=(13,8) miss=5 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=4 pileOnLine=0
  QUAD: needDodge=0 freeSubs=2(reach=2) markers=4 blockCov=1 blockRel=0 blitzRel=0 collision=1
  marker P5@(12,6) ST3 bestBlock=2d(P13) bestBlitz=2d(P19 fail=0.03)
  marker P7@(11,8) ST3 bestBlock=0d(P-1) bestBlitz=1d(P21 fail=0.86)
  marker P9@(11,7) ST3 bestBlock=0d(P-1) bestBlitz=1d(P21 fail=0.97)
  marker P11@(8,5) ST3 bestBlock=0d(P-1) bestBlitz=1d(P16 fail=0.67)
    ..T.TO...
    ........O
    ..TTOo..*
    .T.C.O...
    .ooFtO...
    TT.......
    ...O.....
[DICEY] dw-we-g3 H2 T5 AWAY | carrier P22@(12,5) dist=12 step=4 req=4.00 ach=7.00 resist=3 built=1 filled=2 open=2 cgfi=0
  legs(3): !0:P18(11,4)->(7,4) 1:P21(12,3)->(7,6) 2:P22(12,5)->(8,5)[CAR]
  FAIL leg#0 P18 MA7 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=2.00 execFail=1 dist=4 budget=7  sampleEnd=(11,4) miss=4 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=2 pileOnLine=0
  QUAD: needDodge=0 freeSubs=3(reach=2) markers=2 blockCov=0 blockRel=0 blitzRel=0 collision=1
  marker P8@(9,4) ST3 bestBlock=0d(P-1) bestBlitz=1d(P21 fail=0.17)
  marker P11@(6,5) ST3 bestBlock=0d(P-1) bestBlitz=1d(P21 fail=0.58)
    .........
    ...To....
    .T..FoO.*
    ..TCT....
    ...Oo.O..
    T.T..O...
    oo.t.....
[DICEY] dw-we-g3 H2 T6 AWAY | carrier P22@(12,4) dist=12 step=6 req=6.00 ach=8.00 resist=2 built=3 filled=3 open=1 cgfi=0
  legs(4): !0:P12(11,5)->(5,3) 1:P15(12,6)->(5,5) 2:P21(12,3)->(7,5) 3:P22(12,4)->(6,4)[CAR]
  FAIL leg#0 P12 MA7 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=1.00 execFail=1 dist=6 budget=7  sampleEnd=(11,4) miss=6 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=2 pileOnLine=1
  QUAD: needDodge=0 freeSubs=4(reach=2) markers=2 blockCov=1 blockRel=0 blitzRel=1 collision=0
  marker P10@(9,4) ST3 bestBlock=1d(P18) bestBlitz=2d(P21 fail=0.03)
  marker P11@(7,3) ST3 bestBlock=0d(P-1) bestBlitz=1d(P21 fail=0.17)
    .........
    .........
    ..TTo.T.O
    ...C.oO..
    ..T.F....
    ...To.O..
    T..o..O..
[DICEY] dw-we-g4 H2 T5 AWAY | carrier P15@(11,12) dist=11 step=4 req=3.67 ach=4.00 resist=0 built=0 filled=2 open=2 cgfi=0
  legs(3): !0:P12(15,6)->(8,11) 1:P20(16,4)->(8,13)[gfi] 2:P15(11,12)->(7,12)[CAR]
  FAIL leg#0 P12 MA7 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=7.00 execFail=1 dist=7 budget=7 TIGHT  sampleEnd=(8,6) miss=5 to=0
  CAT=b (TZ shadow on line, budget exact) casters=4 pileOnLine=1
  QUAD: needDodge=0 freeSubs=0(reach=0) markers=4 blockCov=0 blockRel=0 blitzRel=0 collision=1
  marker P5@(13,7) ST3 bestBlock=0d(P-1) bestBlitz=2d(P17 fail=1.00)
  marker P9@(13,9) ST3 bestBlock=0d(P-1) bestBlitz=2d(P17 fail=0.88)
  marker P10@(11,8) ST3 bestBlock=0d(P-1) bestBlitz=2d(P17 fail=1.00)
  marker P2@(12,11) ST3 bestBlock=0d(P-1) bestBlitz=1d(P20 fail=0.91)
    .O.......
    .........
    ..O...*..
    ...C.....
    .........
    .........
    #########
[DICEY] dw-we-g5 H1 T2 HOME | carrier P2@(5,5) dist=20 step=6 req=3.33 ach=8.00 resist=0 built=2 filled=3 open=1 cgfi=0
  legs(4): !0:P1(8,5)->(12,4) 1:P7(7,6)->(12,6) 2:P8(7,7)->(10,6) 3:P2(5,5)->(11,5)[CAR]
  FAIL leg#0 P1 MA4 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=4.00 execFail=1 dist=4 budget=4 TIGHT  sampleEnd=(12,6) miss=2 to=0
  CAT=b (TZ shadow on line, budget exact) casters=2 pileOnLine=0
  QUAD: needDodge=0 freeSubs=5(reach=2) markers=2 blockCov=0 blockRel=0 blitzRel=0 collision=1
  marker P21@(10,4) ST3 bestBlock=0d(P-1) bestBlitz=1d(P5 fail=0.17)
  marker P18@(13,4) ST3 bestBlock=0d(P-1) bestBlitz=1d(P7 fail=0.42)
    ....OO...
    .........
    ....TT..O
    ...CT.F..
    ...TTT...
    .....T...
    .......O.
[DICEY] dw-we-g5 H1 T3 HOME | carrier P2@(4,4) dist=21 step=5 req=4.20 ach=6.00 resist=3 built=0 filled=4 open=0 cgfi=0
  legs(5): !0:P8(8,4)->(10,3) 1:P11(6,5)->(10,5) 2:P6(4,3)->(8,3) 3:P10(4,5)->(8,5) 4:P2(4,4)->(9,4)[CAR]
  FAIL leg#0 P8 MA4 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=1.00 execFail=1 dist=2 budget=4  sampleEnd=(8,5) miss=2 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=1 pileOnLine=0
  QUAD: needDodge=0 freeSubs=0(reach=0) markers=1 blockCov=0 blockRel=0 blitzRel=1 collision=0
  marker P21@(10,4) ST3 bestBlock=0d(P-1) bestBlitz=1d(P11 fail=0.17)
    ....o.O..
    .....OO..
    ...TtO...
    ...CTTTF.
    ...T.T...
    .O.......
    .........
[DICEY] dw-we-g5 H1 T4 HOME | carrier P2@(6,2) dist=19 step=5 req=4.75 ach=5.00 resist=3 built=1 filled=4 open=0 cgfi=0
  legs(5): !0:P7(7,4)->(12,1) 1:P8(8,4)->(12,3) 2:P9(6,4)->(10,1) 3:P11(6,5)->(10,3) 4:P2(6,2)->(11,2)[CAR]
  FAIL leg#0 P7 MA5 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=1.00 execFail=1 dist=5 budget=5 TIGHT  sampleEnd=(8,5) miss=4 to=0
  CAT=b (TZ shadow on line, budget exact) casters=1 pileOnLine=0
  QUAD: needDodge=0 freeSubs=2(reach=0) markers=1 blockCov=1 blockRel=1 blitzRel=1 collision=0
  marker P22@(7,2) ST3 bestBlock=1d(P5) bestBlitz=2d(P6 fail=0.03)
    #########
    .........
    ..o.O....
    ...CO....
    .Tt.To...
    ...TFT.O.
    .T.T.....
```

## Matchup dwarf mirror (10 situací)

```text
[DICEY] dw-dw-g0 H2 T4 HOME | carrier P3@(9,8) dist=16 step=4 req=4.00 ach=5.00 resist=5 built=1 filled=3 open=1 cgfi=0
  legs(4): !0:P11(11,9)->(14,7) 1:P7(9,6)->(14,9) 2:P10(8,7)->(12,9) 3:P3(9,8)->(13,8)[CAR]
  FAIL leg#0 P11 MA4 tzAtStart=1 pto=0.729 ceil=0.02 meanActs=1.81 execFail=0 dist=3 budget=4  sampleEnd=(14,7) miss=0 to=0
  CAT=c (marked mover, zero free bodies) casters=4 pileOnLine=0
  QUAD: needDodge=2 freeSubs=0(reach=0) markers=1 blockCov=0 blockRel=0 blitzRel=0 collision=0
  marker P15@(11,8) ST3 bestBlock=0d(P-1) bestBlitz=1d(P8 fail=0.86)
    ...TOO...
    ...T.oOt.
    ..TTOOO.*
    ...C.O...
    ....tF...
    .........
    .........
[DICEY] dw-dw-g1 H2 T3 AWAY | carrier P13@(25,7) dist=25 step=6 req=5.00 ach=8.00 resist=0 built=2 filled=4 open=0 cgfi=0
  legs(4): 0:P14(23,6)->(18,8) 1:P21(20,8)->(20,6) 2:P18(18,5)->(20,8) !3:P13(25,7)->(19,7)[CAR]
  FAIL leg#3 P13 MA6 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=6.00 execFail=1 dist=6 budget=6 TIGHT CARRIER-LEG sampleEnd=(20,7) miss=1 to=0
  CAT=d (teammate pile on line (untangle gap)) casters=0 pileOnLine=1
  QUAD: needDodge=0 freeSubs=3(reach=3) markers=0 blockCov=0 blockRel=0 blitzRel=0 collision=0
    ###......
    ###......
    ###.TT...
    ###CT....
    ###.T...T
    ###......
    ###.....O
[DICEY] dw-dw-g1 H2 T4 AWAY | carrier P13@(25,7) dist=25 step=7 req=6.25 ach=8.00 resist=0 built=2 filled=4 open=0 cgfi=1
  legs(5): 0:P19(18,6)->(17,6) 1:P21(20,8)->(17,8) 2:P18(18,5)->(19,6) 3:P14(23,6)->(19,8) !4:P13(25,7)->(18,7)[CAR]
  FAIL leg#4 P13 MA6 tzAtStart=0 pto=0.062 ceil=0.25 meanActs=7.00 execFail=1 dist=7 budget=7 TIGHT CARRIER-LEG sampleEnd=(19,7) miss=1 to=0
  CAT=d (teammate pile on line (untangle gap)) casters=0 pileOnLine=1
  QUAD: needDodge=0 freeSubs=3(reach=1) markers=0 blockCov=0 blockRel=0 blitzRel=0 collision=0
    ###......
    ###......
    ###.TT...
    ###CT....
    ###.T...T
    ###......
    ###.....O
[DICEY] dw-dw-g2 H1 T3 HOME | carrier P9@(7,8) dist=18 step=4 req=3.60 ach=4.00 resist=1 built=0 filled=4 open=0 cgfi=0
  legs(5): 0:P1(8,8)->(12,7) 1:P6(7,9)->(12,9) !2:P7(7,7)->(10,7) 3:P8(7,6)->(10,9) 4:P9(7,8)->(11,8)[CAR]
  FAIL leg#2 P7 MA5 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=2.00 execFail=1 dist=3 budget=5  sampleEnd=(8,5) miss=2 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=1 pileOnLine=0
  QUAD: needDodge=0 freeSubs=3(reach=2) markers=1 blockCov=0 blockRel=0 blitzRel=1 collision=0
  marker P18@(10,6) ST3 bestBlock=0d(P-1) bestBlitz=1d(P1 fail=0.17)
    ........O
    ...T..O.t
    .T.Fo.*..
    ..TCT....
    T..T.....
    .........
    .........
[DICEY] dw-dw-g2 H1 T5 HOME | carrier P7@(10,6) dist=15 step=5 req=5.00 ach=5.00 resist=4 built=1 filled=3 open=1 cgfi=0
  legs(4): !0:P6(11,6)->(16,5) 1:P2(10,8)->(16,7) 2:P8(9,7)->(14,7)[gfi] 3:P7(10,6)->(15,6)[CAR]
  FAIL leg#0 P6 MA5 tzAtStart=2 pto=0.583 ceil=0.02 meanActs=2.83 execFail=0 dist=5 budget=5 TIGHT  sampleEnd=(13,5) miss=3 to=1
  CAT=a (marked mover, free bodies out of reach) casters=1 pileOnLine=0
  QUAD: needDodge=2 freeSubs=2(reach=0) markers=2 blockCov=1 blockRel=0 blitzRel=1 collision=0
  marker P16@(11,7) ST3 bestBlock=2d(P2) bestBlitz=2d(P2 fail=0.03)
  marker P17@(12,7) ST3 bestBlock=0d(P-1) bestBlitz=1d(P8 fail=0.17)
    .....t.t.
    ....O....
    ...oo..o.
    ..tCFt.o.
    .oTtOO...
    ...T.O...
    .........
[DICEY] dw-dw-g2 H2 T4 AWAY | carrier P13@(18,5) dist=18 step=5 req=4.50 ach=5.00 resist=4 built=3 filled=3 open=1 cgfi=0
  legs(3): !0:P18(17,4)->(12,4) 1:P14(12,7)->(14,6) 2:P13(18,5)->(13,5)[CAR]
  FAIL leg#0 P18 MA5 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=1.00 execFail=1 dist=5 budget=5 TIGHT  sampleEnd=(17,5) miss=5 to=0
  CAT=b (TZ shadow on line, budget exact) casters=3 pileOnLine=1
  QUAD: needDodge=1 freeSubs=0(reach=0) markers=3 blockCov=1 blockRel=0 blitzRel=0 collision=1
  marker P1@(15,4) ST3 bestBlock=1d(P20) bestBlitz=1d(P20 fail=0.17)
  marker P10@(14,4) ST3 bestBlock=0d(P-1) bestBlitz=1d(P12 fail=1.00)
  marker P11@(13,4) ST3 bestBlock=0d(P-1) bestBlitz=1d(P12 fail=1.00)
    ...O.....
    .........
    ....FTOOO
    .O.C.too.
    TTT.TT...
    .....O.O.
    .........
[DICEY] dw-dw-g4 H1 T3 HOME | carrier P2@(5,8) dist=20 step=6 req=4.00 ach=8.00 resist=0 built=2 filled=3 open=1 cgfi=0
  legs(4): !0:P7(8,6)->(12,7) 1:P1(8,8)->(12,9) 2:P8(7,8)->(10,9) 3:P2(5,8)->(11,8)[CAR]
  FAIL leg#0 P7 MA5 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=1.00 execFail=1 dist=4 budget=5  sampleEnd=(8,7) miss=4 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=4 pileOnLine=0
  QUAD: needDodge=0 freeSubs=5(reach=2) markers=4 blockCov=0 blockRel=0 blitzRel=0 collision=1
  marker P16@(10,6) ST3 bestBlock=0d(P-1) bestBlitz=1d(P1 fail=0.17)
  marker P18@(10,7) ST3 bestBlock=0d(P-1) bestBlitz=1d(P1 fail=0.17)
  marker P19@(11,7) ST3 bestBlock=0d(P-1) bestBlitz=1d(P1 fail=0.17)
  marker P20@(11,6) ST3 bestBlock=0d(P-1) bestBlitz=1d(P6 fail=0.17)
    .........
    ....TTF.O
    ....TT..O
    ...C.TT..
    ....T....
    .....T...
    .....O...
[DICEY] dw-dw-g4 H2 T3 AWAY | carrier P14@(25,10) dist=25 step=5 req=5.00 ach=7.00 resist=1 built=1 filled=4 open=0 cgfi=0
  legs(5): 0:P15(18,9)->(19,9) 1:P17(19,8)->(19,11) 2:P18(19,7)->(21,9) 3:P12(21,8)->(21,11) !4:P14(25,10)->(20,10)[CAR]
  FAIL leg#4 P14 MA6 tzAtStart=1 pto=0.333 ceil=0.02 meanActs=4.33 execFail=0 dist=5 budget=6 CARRIER-LEG sampleEnd=(20,10) miss=0 to=0
  CAT=a (carrier himself marked (dodge to advance)) casters=1 pileOnLine=0
  QUAD: needDodge=0 freeSubs=3(reach=3) markers=1 blockCov=0 blockRel=0 blitzRel=1 collision=0
  marker P3@(24,11) ST3 bestBlock=0d(P-1) bestBlitz=2d(P12 fail=0.03)
    ###......
    ###....T.
    ###.T....
    ###C....*
    ###.O....
    ###......
    ###......
[DICEY] dw-dw-g4 H2 T4 AWAY | carrier P14@(25,10) dist=25 step=7 req=6.25 ach=7.00 resist=1 built=1 filled=4 open=0 cgfi=1
  legs(5): 0:P15(18,9)->(17,9) !1:P21(16,9)->(17,11) 2:P17(19,8)->(19,9) 3:P19(18,8)->(19,11) 4:P14(25,10)->(18,10)[CAR]
  FAIL leg#1 P21 MA4 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=1.00 execFail=1 dist=2 budget=4  sampleEnd=(15,9) miss=2 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=1 pileOnLine=0
  QUAD: needDodge=0 freeSubs=2(reach=1) markers=1 blockCov=0 blockRel=0 blitzRel=1 collision=0
  marker P6@(16,11) ST3 bestBlock=0d(P-1) bestBlitz=1d(P15 fail=0.17)
    ###......
    ###......
    ###.T....
    ###C.....
    ###.O....
    ###ot....
    ###......
[DICEY] dw-dw-g5 H2 T3 AWAY | carrier P22@(21,6) dist=21 step=5 req=4.20 ach=6.00 resist=0 built=4 filled=3 open=1 cgfi=1
  legs(4): !0:P17(18,7)->(15,7) 1:P18(18,5)->(17,5) 2:P14(20,7)->(17,7) 3:P22(21,6)->(16,6)[CAR]
  FAIL leg#0 P17 MA5 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=1.00 execFail=1 dist=3 budget=5  sampleEnd=(18,8) miss=3 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=2 pileOnLine=0
  QUAD: needDodge=0 freeSubs=5(reach=3) markers=2 blockCov=0 blockRel=0 blitzRel=0 collision=1
  marker P6@(16,8) ST3 bestBlock=0d(P-1) bestBlitz=1d(P12 fail=0.17)
  marker P9@(14,8) ST3 bestBlock=0d(P-1) bestBlitz=1d(P12 fail=0.86)
    .........
    .........
    ..T.T.T..
    ...C..TT.
    ..T.T.F..
    ........O
    .........
```

## Matchup orc–skaven (23 situací)

```text
[DICEY] orc-sk-g0 H1 T2 HOME | carrier P11@(5,8) dist=20 step=6 req=3.33 ach=8.00 resist=0 built=1 filled=2 open=2 cgfi=0
  legs(3): !0:P8(8,6)->(12,7) 1:P10(6,9)->(12,9) 2:P11(5,8)->(11,8)[CAR]
  FAIL leg#0 P8 MA6 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=3.00 execFail=1 dist=4 budget=6  sampleEnd=(11,5) miss=2 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=3 pileOnLine=0
  QUAD: needDodge=0 freeSubs=6(reach=6) markers=3 blockCov=0 blockRel=0 blitzRel=0 collision=1
  marker P16@(10,7) ST3 bestBlock=0d(P-1) bestBlitz=2d(P4 fail=0.11)
  marker P21@(10,8) ST2 bestBlock=0d(P-1) bestBlitz=2d(P10 fail=0.03)
  marker P13@(13,7) ST3 bestBlock=0d(P-1) bestBlitz=2d(P7 fail=0.91)
    .........
    .....TF..
    .....T..O
    ...C.TT.O
    ....TTT.O
    .........
    .........
[DICEY] orc-sk-g0 H1 T3 HOME | carrier P11@(9,5) dist=16 step=4 req=3.20 ach=5.00 resist=3 built=1 filled=4 open=0 cgfi=0
  legs(4): !0:P2(9,4)->(14,4) 1:P8(9,6)->(14,6) 2:P10(9,3)->(12,4) 3:P11(9,5)->(13,5)[CAR]
  FAIL leg#0 P2 MA5 tzAtStart=0 pto=0.188 ceil=0.02 meanActs=4.44 execFail=0 dist=5 budget=5 TIGHT  sampleEnd=(14,2) miss=2 to=0
  CAT=b (TZ shadow on line, budget exact) casters=2 pileOnLine=1
  QUAD: needDodge=0 freeSubs=1(reach=0) markers=2 blockCov=2 blockRel=1 blitzRel=1 collision=0
  marker P15@(11,4) ST3 bestBlock=2d(P1) bestBlitz=2d(P9 fail=0.03)
  marker P18@(13,4) ST3 bestBlock=1d(P9) bestBlitz=1d(P9 fail=0.17)
    .........
    .O.T.....
    ...FTO.O*
    ..TCTTT..
    ...T..t..
    ....TO...
    ....O.O..
[DICEY] orc-sk-g0 H1 T4 HOME | carrier P11@(9,5) dist=16 step=4 req=4.00 ach=5.00 resist=2 built=4 filled=4 open=0 cgfi=0
  legs(4): !0:P5(11,5)->(14,4) 1:P7(10,4)->(14,6) 2:P10(9,3)->(12,4) 3:P11(9,5)->(13,5)[CAR]
  FAIL leg#0 P5 MA4 tzAtStart=0 pto=0.958 ceil=0.02 meanActs=2.33 execFail=0 dist=3 budget=4  sampleEnd=(13,3) miss=1 to=1
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=1 pileOnLine=0
  QUAD: needDodge=0 freeSubs=3(reach=2) markers=1 blockCov=0 blockRel=0 blitzRel=1 collision=0
  marker P18@(13,4) ST3 bestBlock=0d(P-1) bestBlitz=2d(P7 fail=0.11)
    .o.......
    .T.T.....
    ..T.To.O*
    ...C.Ft..
    ..T.T.t..
    ....tO...
    ....O....
[DICEY] orc-sk-g0 H2 T2 AWAY | carrier P20@(17,5) dist=17 step=3 req=2.83 ach=10.00 resist=1 built=3 filled=4 open=0 cgfi=0
  legs(4): 0:P18(16,6)->(13,4) 1:P17(18,4)->(15,4) 2:P22(18,6)->(15,6) !3:P20(17,5)->(14,5)[CAR]
  FAIL leg#3 P20 MA9 tzAtStart=1 pto=0.271 ceil=0.02 meanActs=3.19 execFail=0 dist=3 budget=9 CARRIER-LEG sampleEnd=(14,5) miss=0 to=0
  CAT=a (carrier himself marked (dodge to advance)) casters=1 pileOnLine=1
  QUAD: needDodge=0 freeSubs=1(reach=1) markers=1 blockCov=1 blockRel=1 blitzRel=1 collision=0
  marker P8@(16,4) ST3 bestBlock=1d(P21) bestBlitz=2d(P17 fail=0.03)
    .........
    .........
    ..TTO...O
    ...Ct.*T.
    ..T.T..T.
    .......TO
    .......T.
[DICEY] orc-sk-g1 H1 T2 HOME | carrier P10@(9,9) dist=16 step=3 req=2.67 ach=6.00 resist=5 built=3 filled=3 open=1 cgfi=0
  legs(4): 0:P4(12,8)->(13,8) 1:P7(10,8)->(13,10) 2:P6(8,8)->(11,8) !3:P10(9,9)->(12,9)[CAR]
  FAIL leg#3 P10 MA6 tzAtStart=1 pto=0.167 ceil=0.02 meanActs=3.50 execFail=0 dist=3 budget=6 CARRIER-LEG sampleEnd=(10,8) miss=2 to=1
  CAT=a (carrier himself marked (dodge to advance)) casters=2 pileOnLine=2
  QUAD: needDodge=0 freeSubs=3(reach=3) markers=1 blockCov=1 blockRel=1 blitzRel=1 collision=0
  marker P17@(10,10) ST3 bestBlock=1d(P2) bestBlitz=2d(P11 fail=0.03)
    .........
    .......TO
    ..T.T.T.o
    ...CtT*..
    ..T.OO...
    ....OOO..
    .........
[DICEY] orc-sk-g1 H1 T3 HOME | carrier P10@(9,10) dist=16 step=4 req=3.20 ach=7.00 resist=2 built=1 filled=3 open=1 cgfi=0
  legs(4): 0:P9(9,5)->(14,9) !1:P2(11,10)->(14,11) 2:P4(11,9)->(12,9) 3:P10(9,10)->(13,10)[CAR]
  FAIL leg#1 P2 MA5 tzAtStart=2 pto=0.375 ceil=0.02 meanActs=2.88 execFail=0 dist=3 budget=5  sampleEnd=(14,11) miss=0 to=0
  CAT=a (marked mover, free bodies out of reach) casters=1 pileOnLine=0
  QUAD: needDodge=2 freeSubs=1(reach=0) markers=2 blockCov=1 blockRel=0 blitzRel=1 collision=0
  marker P20@(10,10) ST2 bestBlock=3d(P4) bestBlitz=3d(P11 fail=0.00)
  marker P22@(12,11) ST2 bestBlock=0d(P-1) bestBlitz=2d(P11 fail=0.51)
    .......TO
    ........o
    ...TtT...
    ...COF...
    ...TT.O.*
    ....oo...
    .........
[DICEY] orc-sk-g1 H1 T4 HOME | carrier P10@(10,10) dist=15 step=6 req=3.75 ach=7.00 resist=0 built=3 filled=4 open=0 cgfi=0
  legs(5): !0:P3(13,7)->(17,9) 1:P2(11,10)->(17,11)[gfi] 2:P11(9,11)->(15,9) 3:P9(9,5)->(15,11) 4:P10(10,10)->(16,10)[CAR]
  FAIL leg#0 P3 MA5 tzAtStart=1 pto=0.333 ceil=0.02 meanActs=3.67 execFail=0 dist=4 budget=5  sampleEnd=(16,9) miss=1 to=0
  CAT=a (marked mover, free bodies out of reach) casters=1 pileOnLine=0
  QUAD: needDodge=1 freeSubs=4(reach=0) markers=1 blockCov=0 blockRel=0 blitzRel=1 collision=0
  marker P16@(14,7) ST3 bestBlock=0d(P-1) bestBlitz=2d(P9 fail=0.03)
    ......FO.
    .......o.
    ..TtT....
    ...CT....
    ..TToo...
    ...oo....
    .........
[DICEY] orc-sk-g1 H1 T5 HOME | carrier P10@(10,10) dist=15 step=6 req=5.00 ach=8.00 resist=0 built=3 filled=4 open=0 cgfi=0
  legs(5): !0:P1(12,5)->(17,9) 1:P3(13,7)->(17,11) 2:P11(9,11)->(15,9) 3:P2(11,10)->(15,11) 4:P10(10,10)->(16,10)[CAR]
  FAIL leg#0 P1 MA5 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=5.00 execFail=1 dist=5 budget=5 TIGHT  sampleEnd=(13,10) miss=4 to=0
  CAT=b (TZ shadow on line, budget exact) casters=5 pileOnLine=0
  QUAD: needDodge=1 freeSubs=4(reach=0) markers=5 blockCov=2 blockRel=0 blitzRel=0 collision=1
  marker P15@(14,5) ST3 bestBlock=1d(P9) bestBlitz=2d(P2 fail=0.11)
  marker P16@(14,7) ST3 bestBlock=1d(P3) bestBlitz=2d(P2 fail=0.11)
  marker P19@(15,6) ST2 bestBlock=0d(P-1) bestBlitz=1d(P3 fail=0.86)
  marker P18@(15,10) ST3 bestBlock=0d(P-1) bestBlitz=2d(P4 fail=0.11)
  marker P21@(17,10) ST2 bestBlock=0d(P-1) bestBlitz=2d(P11 fail=0.94)
    ......TO.
    .......o.
    ..TtT....
    ...CT...O
    ..TToo...
    ...oo....
    .........
[DICEY] orc-sk-g1 H2 T4 AWAY | carrier P20@(13,7) dist=13 step=4 req=3.25 ach=7.00 resist=3 built=1 filled=4 open=0 cgfi=0
  legs(5): !0:P14(12,8)->(8,6) 1:P21(16,9)->(8,8) 2:P22(18,7)->(10,6) 3:P15(13,8)->(10,8) 4:P20(13,7)->(9,7)[CAR]
  FAIL leg#0 P14 MA7 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=5.00 execFail=1 dist=4 budget=7  sampleEnd=(7,9) miss=3 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=4 pileOnLine=0
  QUAD: needDodge=1 freeSubs=0(reach=0) markers=4 blockCov=0 blockRel=0 blitzRel=0 collision=1
  marker P4@(12,6) ST4 bestBlock=0d(P-1) bestBlitz=0d(P-1 fail=1.00)
  marker P6@(11,6) ST4 bestBlock=0d(P-1) bestBlitz=0d(P-1 fail=1.00)
  marker P7@(10,7) ST4 bestBlock=0d(P-1) bestBlitz=0d(P-1 fail=1.00)
  marker P11@(8,7) ST3 bestBlock=0d(P-1) bestBlitz=1d(P18 fail=0.58)
    ...O.....
    ..t......
    .TOoOO..*
    ..OC..O.O
    .T.TF....
    T........
    .t.......
[DICEY] orc-sk-g1 H2 T5 HOME | carrier P10@(5,9) dist=20 step=7 req=6.67 ach=7.00 resist=2 built=2 filled=3 open=1 cgfi=1
  legs(4): 0:P3(10,7)->(13,8) !1:P5(9,7)->(13,10) 2:P4(8,7)->(11,8) 3:P10(5,9)->(12,9)[CAR]
  FAIL leg#1 P5 MA4 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=4.00 execFail=1 dist=4 budget=4 TIGHT  sampleEnd=(13,9) miss=1 to=0
  CAT=b (TZ shadow on line, budget exact) casters=1 pileOnLine=0
  QUAD: needDodge=0 freeSubs=5(reach=1) markers=1 blockCov=1 blockRel=1 blitzRel=1 collision=0
  marker P17@(11,10) ST3 bestBlock=1d(P1) bestBlitz=2d(P3 fail=0.03)
    .........
    ..T...TFT
    ....TT...
    ..TC.....
    .o..T.O..
    ......O.T
    .........
[DICEY] orc-sk-g2 H1 T2 HOME | carrier P10@(6,10) dist=19 step=4 req=3.17 ach=7.00 resist=1 built=2 filled=4 open=0 cgfi=0
  legs(5): 0:P4(9,8)->(11,9) 1:P8(8,8)->(11,11) 2:P5(7,8)->(9,9) !3:P1(8,11)->(9,11) 4:P10(6,10)->(10,10)[CAR]
  FAIL leg#3 P1 MA5 tzAtStart=1 pto=0.333 ceil=0.02 meanActs=1.00 execFail=0 dist=1 budget=5  sampleEnd=(9,11) miss=0 to=1
  CAT=c (marked mover, zero free bodies) casters=1 pileOnLine=0
  QUAD: needDodge=1 freeSubs=0(reach=0) markers=1 blockCov=1 blockRel=1 blitzRel=1 collision=0
  marker P16@(8,10) ST3 bestBlock=3d(P2) bestBlitz=3d(P8 fail=0.00)
    .........
    ....TTT..
    ....TT...
    ...CTO...
    ....TF*..
    .........
    .........
[DICEY] orc-sk-g2 H1 T4 HOME | carrier P10@(9,10) dist=16 step=4 req=4.00 ach=6.00 resist=4 built=0 filled=4 open=0 cgfi=0
  legs(5): !0:P8(9,8)->(14,9) 1:P4(10,7)->(14,11) 2:P5(8,10)->(12,9) 3:P11(7,9)->(12,11) 4:P10(9,10)->(13,10)[CAR]
  FAIL leg#0 P8 MA6 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=2.00 execFail=1 dist=5 budget=6  sampleEnd=(9,8) miss=5 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=4 pileOnLine=1
  QUAD: needDodge=1 freeSubs=1(reach=0) markers=4 blockCov=1 blockRel=0 blitzRel=0 collision=1
  marker P14@(10,10) ST3 bestBlock=2d(P2) bestBlitz=2d(P2 fail=0.11)
  marker P18@(11,9) ST3 bestBlock=0d(P-1) bestBlitz=1d(P11 fail=0.72)
  marker P19@(11,10) ST2 bestBlock=0d(P-1) bestBlitz=2d(P6 fail=0.85)
  marker P21@(12,8) ST2 bestBlock=0d(P-1) bestBlitz=2d(P4 fail=0.85)
    ....T..O.
    ...F..O..
    .T.TtO..*
    ..TCOO...
    .o.T.o...
    .........
    .........
[DICEY] orc-sk-g3 H1 T2 HOME | carrier P9@(9,5) dist=16 step=3 req=2.67 ach=3.00 resist=0 built=3 filled=4 open=0 cgfi=0
  legs(4): 0:P2(12,6)->(13,6) 1:P5(10,4)->(11,4) 2:P6(10,6)->(11,6) !3:P9(9,5)->(12,5)[CAR]
  FAIL leg#3 P9 MA6 tzAtStart=1 pto=0.229 ceil=0.02 meanActs=1.77 execFail=0 dist=3 budget=6 CARRIER-LEG sampleEnd=(10,5) miss=2 to=1
  CAT=a (carrier himself marked (dodge to advance)) casters=0 pileOnLine=1
  QUAD: needDodge=0 freeSubs=4(reach=4) markers=1 blockCov=0 blockRel=0 blitzRel=1 collision=0
  marker P18@(8,4) ST3 bestBlock=0d(P-1) bestBlitz=2d(P3 fail=0.03)
    .........
    .........
    ..O.T..TO
    ...C.T*.o
    ..T.T.T.o
    .......TO
    .....T...
[DICEY] orc-sk-g4 H1 T3 HOME | carrier P11@(12,7) dist=13 step=3 req=2.60 ach=6.00 resist=2 built=1 filled=3 open=1 cgfi=0
  legs(4): !0:P2(12,6)->(16,6) 1:P10(12,8)->(16,8) 2:P5(11,6)->(14,6) 3:P11(12,7)->(15,7)[CAR]
  FAIL leg#0 P2 MA5 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=2.00 execFail=1 dist=4 budget=5  sampleEnd=(12,6) miss=4 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=2 pileOnLine=0
  QUAD: needDodge=0 freeSubs=4(reach=2) markers=2 blockCov=1 blockRel=0 blitzRel=1 collision=0
  marker P16@(14,7) ST3 bestBlock=1d(P3) bestBlitz=2d(P10 fail=0.03)
  marker P21@(14,5) ST2 bestBlock=0d(P-1) bestBlitz=2d(P9 fail=0.84)
    ....OO...
    .....O...
    .TTF...*.
    .TtCTO...
    ..oT.o...
    .T.T.....
    ..T.O....
[DICEY] orc-sk-g4 H1 T4 HOME | carrier P11@(13,8) dist=12 step=3 req=3.00 ach=5.00 resist=1 built=4 filled=4 open=0 cgfi=0
  legs(5): 0:P3(14,9)->(17,7) 1:P8(12,9)->(17,9) !2:P2(12,7)->(15,7) 3:P5(11,6)->(15,9) 4:P11(13,8)->(16,8)[CAR]
  FAIL leg#2 P2 MA5 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=1.00 execFail=1 dist=3 budget=5  sampleEnd=(12,6) miss=3 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=1 pileOnLine=1
  QUAD: needDodge=0 freeSubs=3(reach=3) markers=1 blockCov=1 blockRel=1 blitzRel=1 collision=0
  marker P16@(15,6) ST3 bestBlock=1d(P10) bestBlitz=2d(P3 fail=0.03)
    ....O....
    TT...O...
    TtFoT*...
    .o.Co....
    T.T.T....
    ...T.....
    ....O....
[DICEY] orc-sk-g4 H1 T5 HOME | carrier P11@(12,8) dist=13 step=5 req=4.33 ach=6.00 resist=3 built=0 filled=4 open=0 cgfi=0
  legs(5): !0:P3(14,9)->(18,7) 1:P8(12,9)->(18,9) 2:P2(12,7)->(16,7) 3:P9(10,6)->(16,9) 4:P11(12,8)->(17,8)[CAR]
  FAIL leg#0 P3 MA5 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=2.00 execFail=1 dist=4 budget=5  sampleEnd=(14,9) miss=4 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=2 pileOnLine=0
  QUAD: needDodge=0 freeSubs=3(reach=0) markers=2 blockCov=0 blockRel=0 blitzRel=0 collision=1
  marker P16@(14,7) ST3 bestBlock=0d(P-1) bestBlitz=1d(P2 fail=0.33)
  marker P18@(16,8) ST3 bestBlock=0d(P-1) bestBlitz=1d(P8 fail=0.72)
    ....OO...
    .TT..O...
    .TtToO...
    ..oCto.O.
    .T.T.F...
    ....T....
    .....O...
[DICEY] orc-sk-g4 H2 T3 AWAY | carrier P19@(12,13) dist=12 step=9 req=2.40 ach=10.00 resist=0 built=0 filled=3 open=1 cgfi=0
  legs(4): !0:P17(10,12)->(2,12)[gfi] 1:P20(13,13)->(4,12) 2:P21(13,11)->(4,14) 3:P19(12,13)->(3,13)[CAR]
  FAIL leg#0 P17 MA7 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=7.00 execFail=1 dist=8 budget=7 TIGHT  sampleEnd=(4,13) miss=2 to=0
  CAT=b (TZ shadow on line, budget exact) casters=1 pileOnLine=0
  QUAD: needDodge=2 freeSubs=1(reach=0) markers=1 blockCov=0 blockRel=0 blitzRel=0 collision=0
  marker P1@(6,12) ST3 bestBlock=0d(P-1) bestBlitz=0d(P-1 fail=1.00)
    ....o.OO.
    ..T......
    ...O.F...
    ..TC.....
    .........
    #########
    #########
[DICEY] orc-sk-g5 H1 T2 HOME | carrier P11@(9,4) dist=16 step=3 req=2.67 ach=7.00 resist=1 built=3 filled=4 open=0 cgfi=0
  legs(5): 0:P5(10,5)->(13,3) 1:P4(12,8)->(13,5) !2:P2(8,3)->(11,3) 3:P6(8,5)->(11,5) 4:P11(9,4)->(12,4)[CAR]
  FAIL leg#2 P2 MA5 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=5.00 execFail=1 dist=3 budget=5  sampleEnd=(12,2) miss=1 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=1 pileOnLine=1
  QUAD: needDodge=0 freeSubs=2(reach=2) markers=1 blockCov=1 blockRel=1 blitzRel=1 collision=0
  marker P21@(10,3) ST2 bestBlock=3d(P1) bestBlitz=3d(P10 fail=0.00)
    .........
    .........
    ..FTO*...
    ...CT..TO
    ..T.T...o
    .........
    .......TO
[DICEY] orc-sk-g5 H1 T3 HOME | carrier P11@(9,4) dist=16 step=4 req=3.20 ach=5.00 resist=1 built=4 filled=3 open=1 cgfi=0
  legs(4): !0:P9(13,4)->(14,3) 1:P2(8,3)->(12,3) 2:P4(12,8)->(12,5) 3:P11(9,4)->(13,4)[CAR]
  FAIL leg#0 P9 MA6 tzAtStart=1 pto=0.479 ceil=0.02 meanActs=1.00 execFail=0 dist=1 budget=6  sampleEnd=(14,3) miss=0 to=0
  CAT=a (marked mover, free bodies out of reach) casters=1 pileOnLine=0
  QUAD: needDodge=1 freeSubs=3(reach=2) markers=1 blockCov=0 blockRel=0 blitzRel=1 collision=0
  marker P15@(14,4) ST3 bestBlock=0d(P-1) bestBlitz=2d(P8 fail=0.03)
    .........
    .........
    ..TtTo..*
    ...CO..FO
    ..T.T...o
    .........
    .......TO
[DICEY] orc-sk-g5 H1 T5 HOME | carrier P11@(13,5) dist=12 step=4 req=4.00 ach=6.00 resist=3 built=2 filled=4 open=0 cgfi=0
  legs(5): !0:P9(14,4)->(18,4) 1:P3(13,7)->(18,6) 2:P7(12,6)->(16,4) 3:P5(12,7)->(16,6) 4:P11(13,5)->(17,5)[CAR]
  FAIL leg#0 P9 MA6 tzAtStart=1 pto=0.729 ceil=0.02 meanActs=1.96 execFail=0 dist=4 budget=6  sampleEnd=(16,2) miss=2 to=1
  CAT=a (marked mover, free bodies out of reach) casters=2 pileOnLine=0
  QUAD: needDodge=2 freeSubs=1(reach=0) markers=1 blockCov=0 blockRel=0 blitzRel=0 collision=0
  marker P18@(15,4) ST3 bestBlock=0d(P-1) bestBlitz=1d(P8 fail=0.65)
    .........
    .o.T.....
    .oOTFO..*
    ..TCoo...
    ..T..O...
    ..TTO....
    ..T.o....
[DICEY] orc-sk-g5 H2 T3 AWAY | carrier P22@(14,7) dist=14 step=3 req=2.80 ach=6.00 resist=3 built=4 filled=4 open=0 cgfi=0
  legs(5): !0:P18(15,8)->(10,6) 1:P21(16,9)->(10,8) 2:P13(13,6)->(12,6) 3:P15(13,8)->(12,8) 4:P22(14,7)->(11,7)[CAR]
  FAIL leg#0 P18 MA7 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=3.00 execFail=1 dist=5 budget=7  sampleEnd=(14,8) miss=4 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=3 pileOnLine=2
  QUAD: needDodge=2 freeSubs=0(reach=0) markers=3 blockCov=1 blockRel=0 blitzRel=0 collision=1
  marker P1@(13,5) ST3 bestBlock=1d(P13) bestBlitz=1d(P13 fail=0.33)
  marker P2@(12,7) ST3 bestBlock=2d(P13) bestBlitz=2d(P13 fail=0.11)
  marker P5@(11,6) ST4 bestBlock=0d(P-1) bestBlitz=0d(P-1 fail=1.00)
    ....O....
    .TO.O....
    .tTtT.O*.
    ...CTO...
    ..F.T....
    .T.......
    ...OOO...
[DICEY] orc-sk-g5 H2 T4 AWAY | carrier P22@(15,7) dist=15 step=4 req=3.75 ach=9.00 resist=3 built=0 filled=4 open=0 cgfi=0
  legs(4): !0:P17(15,5)->(10,6) 1:P21(16,7)->(10,8) 2:P15(13,7)->(12,6) 3:P22(15,7)->(11,7)[CAR]
  FAIL leg#0 P17 MA7 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=1.00 execFail=1 dist=5 budget=7  sampleEnd=(15,6) miss=5 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=2 pileOnLine=1
  QUAD: needDodge=1 freeSubs=0(reach=0) markers=2 blockCov=1 blockRel=0 blitzRel=1 collision=0
  marker P1@(13,5) ST3 bestBlock=0d(P-1) bestBlitz=1d(P18 fail=0.86)
  marker P5@(12,7) ST4 bestBlock=1d(P13) bestBlitz=1d(P13 fail=0.33)
    ...o.O...
    ...F.O...
    ..t.t...*
    ..TCTTO..
    ...TOTTo.
    .........
    .....OO..
[DICEY] orc-sk-g5 H2 T5 AWAY | carrier P22@(15,6) dist=15 step=5 req=5.00 ach=9.00 resist=4 built=1 filled=4 open=0 cgfi=0
  legs(5): !0:P20(14,8)->(9,5) 1:P21(15,8)->(9,7) 2:P18(16,7)->(11,5) 3:P15(13,6)->(11,7) 4:P22(15,6)->(10,6)[CAR]
  FAIL leg#0 P20 MA9 tzAtStart=0 pto=0.000 ceil=0.02 meanActs=1.00 execFail=1 dist=5 budget=9  sampleEnd=(14,7) miss=5 to=0
  CAT=b (TZ shadow on line (dodge risk on walk)) casters=3 pileOnLine=0
  QUAD: needDodge=1 freeSubs=0(reach=0) markers=3 blockCov=0 blockRel=0 blitzRel=0 collision=1
  marker P5@(12,7) ST4 bestBlock=0d(P-1) bestBlitz=1d(P18 fail=0.17)
  marker P6@(12,8) ST4 bestBlock=0d(P-1) bestBlitz=0d(P-1 fail=1.00)
  marker P9@(12,4) ST3 bestBlock=0d(P-1) bestBlitz=1d(P18 fail=0.94)
    .....t...
    ...o.OO..
    .........
    ..tCtT...
    ..T...O..
    ...TFtOo.
    .....o...
```
