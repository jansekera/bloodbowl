# Zkouška režimu „AI plánuje tah“ na partii 02.–03.10. — tabulka tahů

Engine `4e142640` + vazba z `d07d7ed2`; v každé pozici 20 plánů macro_mcts (50 iterací, `weights_best.json`) a 20 plánů greedy, každý s vlastními kostkami. Přehrání ověřeno: 534 akcí shodně s `partie_vypis.md`, skóre 2:0.

Sloupce plánu: turnover / TD / s míčem na konci (z 20) · průměr polí do TD · rohy klece · kolik soupeřů dosáhne na nosiče · blitz a faul (z 20). „—“ = tým na konci tahu míč nedrží.

| tah | pol. | kolo | kdo | skutečnost | macro_mcts | na čem padá macro_mcts | greedy |
|---|---|---|---|---|---|---|---|
| 1 | 1 | 1 | trpaslici | 18 do TD, rohy 4, sousedé 4, dosah 7 · blok 1 blitz 1 faul 0 přihr. 0 | TO 3 · TD 0 · míč 18 · do TD 18.33, rohy 3.78, dosah 6.11 · blitz 16 · faul 1 | MOVE/PICKUP 2×, FOUL 1× | TO 19 · TD 0 · míč 18 · do TD 16.0, rohy 0.33, dosah 10.06 · blitz 17 · faul 0 |
| 2 | 1 | 1 | elfove | bez míče · blok 0 blitz 1 faul 0 přihr. 0 | TO 20 · TD 0 · míč 1 · do TD 8.0, rohy 0.0, dosah 11.0 · blitz 20 · faul 0 | BLITZ/DODGE 13×, MOVE/DODGE 6×, MOVE/GFI 1× | TO 18 · TD 0 · míč 0 · — · blitz 12 · faul 0 |
| 3 | 1 | 2 | trpaslici | 12 do TD, rohy 4, sousedé 4, dosah 11 · blok 0 blitz 1 faul 0 přihr. 0 | TO 12 · TD 0 · míč 20 · do TD 12.0, rohy 4.0, dosah 10.7 · blitz 19 · faul 8 | BLITZ/DODGE 11×, BLITZ/GFI 1× | TO 18 · TD 0 · míč 16 · do TD 10.0, rohy 0.0, dosah 7.62 · blitz 15 · faul 0 |
| 4 | 1 | 2 | elfove | bez míče · blok 1 blitz 1 faul 0 přihr. 0 | TO 19 · TD 0 · míč 0 · — · blitz 20 · faul 0 | MOVE/DODGE 8×, BLITZ/DODGE 7×, BLITZ 2×, BLOCK 2× | TO 20 · TD 0 · míč 0 · — · blitz 14 · faul 0 |
| 5 | 1 | 3 | trpaslici | 10 do TD, rohy 4, sousedé 5, dosah 10 · blok 0 blitz 1 faul 0 přihr. 0 | TO 7 · TD 0 · míč 20 · do TD 10.75, rohy 1.2, dosah 10.9 · blitz 4 · faul 0 | MOVE/DODGE 6×, BLITZ/DODGE 1× | TO 20 · TD 0 · míč 3 · do TD 4.0, rohy 0.0, dosah 10.0 · blitz 3 · faul 0 |
| 6 | 1 | 3 | elfove | bez míče · blok 0 blitz 1 faul 0 přihr. 0 | TO 10 · TD 0 · míč 3 · do TD 15.67, rohy 1.67, dosah 6.33 · blitz 20 · faul 1 | MOVE/DODGE 5×, BLITZ/DODGE 2×, BLOCK 2×, BLITZ 1× | TO 18 · TD 0 · míč 0 · — · blitz 11 · faul 0 |
| 7 | 1 | 4 | trpaslici | 8 do TD, rohy 3, sousedé 4, dosah 10 · blok 0 blitz 1 faul 0 přihr. 0 | TO 15 · TD 0 · míč 20 · do TD 9.05, rohy 0.25, dosah 9.55 · blitz 19 · faul 2 | MOVE/DODGE 13×, FOUL 1×, BLOCK 1× | TO 20 · TD 0 · míč 14 · do TD 10.0, rohy 3.86, dosah 9.79 · blitz 16 · faul 0 |
| 8 | 1 | 4 | elfove | bez míče · blok 1 blitz 1 faul 0 přihr. 0 | TO 18 · TD 0 · míč 0 · — · blitz 20 · faul 1 | MOVE/DODGE 8×, BLITZ/DODGE 5×, BLOCK 3×, BLITZ 2× | TO 19 · TD 0 · míč 0 · — · blitz 6 · faul 0 |
| 9 | 1 | 5 | trpaslici | 6 do TD, rohy 4, sousedé 4, dosah 8 · blok 4 blitz 1 faul 0 přihr. 0 | TO 0 · TD 0 · míč 20 · do TD 7.8, rohy 2.55, dosah 9.35 · blitz 13 · faul 1 |  | TO 15 · TD 5 · míč 5 · — · blitz 0 · faul 0 |
| 10 | 1 | 5 | elfove | bez míče · blok 1 blitz 0 faul 0 přihr. 0 · ⛔ turnover | TO 6 · TD 0 · míč 0 · — · blitz 19 · faul 0 | BLITZ 2×, BLITZ/DODGE 2×, MOVE/DODGE 1×, BLOCK 1× | TO 19 · TD 0 · míč 1 · do TD 20.0, rohy 1.0, dosah 7.0 · blitz 16 · faul 0 |
| 11 | 1 | 6 | trpaslici | 3 do TD, rohy 4, sousedé 4, dosah 7 · blok 2 blitz 1 faul 0 přihr. 0 | TO 1 · TD 18 · míč 19 · do TD 6.0, rohy 4.0, dosah 9.0 · blitz 16 · faul 0 | MOVE/DODGE 1× | TO 10 · TD 10 · míč 10 · — · blitz 0 · faul 0 |
| 12 | 1 | 6 | elfove | bez míče · blok 0 blitz 0 faul 0 přihr. 0 | TO 8 · TD 0 · míč 2 · do TD 22.0, rohy 0.5, dosah 7.5 · blitz 19 · faul 3 | BLITZ/DODGE 3×, FOUL 2×, MOVE/DODGE 1×, BLOCK 1×, BLITZ 1× | TO 20 · TD 0 · míč 0 · — · blitz 13 · faul 0 |
| 13 | 1 | 7 | trpaslici | 1 do TD, rohy 4, sousedé 4, dosah 8 · blok 3 blitz 1 faul 0 přihr. 0 | TO 3 · TD 17 · míč 18 · do TD 3.0, rohy 3.0, dosah 9.0 · blitz 7 · faul 0 | MOVE/DODGE 3× | TO 9 · TD 11 · míč 11 · — · blitz 0 · faul 0 |
| 14 | 1 | 7 | elfove | bez míče · blok 0 blitz 0 faul 0 přihr. 0 | TO 13 · TD 0 · míč 0 · — · blitz 20 · faul 0 | MOVE/DODGE 9×, BLOCK 2×, BLITZ/DODGE 2× | TO 20 · TD 0 · míč 0 · — · blitz 9 · faul 0 |
| 15 | 1 | 8 | trpaslici | TD · blok 0 blitz 0 faul 0 přihr. 0 | TO 0 · TD 20 · míč 20 · — · blitz 0 · faul 0 |  | TO 0 · TD 20 · míč 20 · — · blitz 0 · faul 0 |
| 16 | 1 | 8 | elfove | bez míče · blok 2 blitz 1 faul 0 přihr. 0 | TO 15 · TD 0 · míč 20 · do TD 18.0, rohy 4.0, dosah 1.4 · blitz 20 · faul 1 | MOVE/DODGE 14×, FOUL 1× | TO 20 · TD 0 · míč 16 · do TD 15.0, rohy 0.0, dosah 3.94 · blitz 14 · faul 0 |
| 17 | 2 | 1 | elfove | 22 do TD, rohy 0, sousedé 0, dosah 0 · blok 2 blitz 0 faul 0 přihr. 0 | TO 4 · TD 0 · míč 19 · do TD 18.0, rohy 4.0, dosah 3.89 · blitz 16 · faul 0 | BLOCK 3×, MOVE/PICKUP 1× | TO 19 · TD 0 · míč 19 · do TD 15.0, rohy 0.0, dosah 4.32 · blitz 16 · faul 0 |
| 18 | 2 | 1 | trpaslici | bez míče · blok 0 blitz 1 faul 0 přihr. 0 | TO 11 · TD 0 · míč 0 · — · blitz 20 · faul 1 | BLOCK 8×, MOVE/DODGE 3× | TO 20 · TD 0 · míč 0 · — · blitz 16 · faul 0 |
| 19 | 2 | 2 | elfove | 22 do TD, rohy 1, sousedé 1, dosah 1 · blok 1 blitz 1 faul 0 přihr. 0 | TO 8 · TD 0 · míč 19 · do TD 14.68, rohy 2.68, dosah 8.63 · blitz 19 · faul 0 | BLITZ 4×, MOVE/DODGE 4× | TO 18 · TD 0 · míč 14 · do TD 11.0, rohy 0.0, dosah 4.29 · blitz 8 · faul 0 |
| 20 | 2 | 2 | trpaslici | bez míče · blok 5 blitz 1 faul 0 přihr. 0 | TO 9 · TD 0 · míč 1 · do TD 13.0, rohy 2.0, dosah 9.0 · blitz 20 · faul 0 | BLITZ/DODGE 6×, MOVE/DODGE 3× | TO 19 · TD 0 · míč 0 · — · blitz 19 · faul 0 |
| 21 | 2 | 3 | elfove | 24 do TD, rohy 0, sousedé 0, dosah 1 · blok 0 blitz 1 faul 0 přihr. 0 | TO 6 · TD 0 · míč 20 · do TD 15.7, rohy 1.05, dosah 9.1 · blitz 18 · faul 4 | BLITZ/DODGE 4×, MOVE/DODGE 2× | TO 20 · TD 0 · míč 17 · do TD 11.0, rohy 0.0, dosah 5.35 · blitz 4 · faul 0 |
| 22 | 2 | 3 | trpaslici | bez míče · blok 4 blitz 1 faul 0 přihr. 0 | TO 12 · TD 0 · míč 0 · — · blitz 20 · faul 3 | BLITZ/DODGE 6×, BLOCK 3×, FOUL 1×, MOVE/GFI 1×, MOVE/DODGE 1× | TO 18 · TD 0 · míč 0 · — · blitz 13 · faul 1 |
| 23 | 2 | 4 | elfove | 19 do TD, rohy 0, sousedé 0, dosah 5 · blok 0 blitz 0 faul 0 přihr. 0 | TO 9 · TD 0 · míč 18 · do TD 15.89, rohy 0.44, dosah 8.39 · blitz 19 · faul 10 | BLITZ 4×, FOUL 3×, BLITZ/DODGE 2× | TO 20 · TD 0 · míč 20 · do TD 13.0, rohy 0.0, dosah 4.8 · blitz 18 · faul 0 |
| 24 | 2 | 4 | trpaslici | 13 do TD, rohy 2, sousedé 2, dosah 7 · blok 3 blitz 1 faul 0 přihr. 0 | TO 10 · TD 0 · míč 10 · do TD 6.4, rohy 0.0, dosah 6.5 · blitz 20 · faul 1 | BLOCK 7×, MOVE/PICKUP 1×, MOVE/GFI 1×, FOUL 1× | TO 19 · TD 0 · míč 0 · — · blitz 15 · faul 0 |
| 25 | 2 | 5 | elfove | bez míče · blok 0 blitz 0 faul 0 přihr. 0 | TO 14 · TD 0 · míč 0 · — · blitz 20 · faul 3 | MOVE/DODGE 5×, BLITZ 3×, FOUL 2×, BLOCK 2×, BLITZ/DODGE 2× | TO 20 · TD 0 · míč 0 · — · blitz 9 · faul 0 |
| 26 | 2 | 5 | trpaslici | 7 do TD, rohy 4, sousedé 4, dosah 7 · blok 2 blitz 1 faul 0 přihr. 0 | TO 1 · TD 0 · míč 20 · do TD 7.0, rohy 4.0, dosah 7.45 · blitz 20 · faul 4 | FOUL 1× | TO 20 · TD 0 · míč 16 · do TD 5.0, rohy 0.0, dosah 7.5 · blitz 11 · faul 0 |
| 27 | 2 | 6 | elfove | bez míče · blok 0 blitz 1 faul 0 přihr. 0 | TO 19 · TD 0 · míč 1 · do TD 16.0, rohy 0.0, dosah 11.0 · blitz 20 · faul 1 | MOVE/DODGE 13×, BLITZ/DODGE 2×, BLITZ 2×, BLOCK 1×, FOUL 1× | TO 20 · TD 0 · míč 0 · — · blitz 17 · faul 0 |
| 28 | 2 | 6 | trpaslici | 4 do TD, rohy 4, sousedé 4, dosah 6 · blok 0 blitz 1 faul 0 přihr. 0 | TO 2 · TD 0 · míč 20 · do TD 6.2, rohy 1.1, dosah 5.05 · blitz 17 · faul 7 | FOUL 2× | TO 0 · TD 20 · míč 20 · — · blitz 0 · faul 0 |
| 29 | 2 | 7 | elfove | bez míče · blok 0 blitz 0 faul 0 přihr. 0 | TO 6 · TD 0 · míč 0 · — · blitz 20 · faul 0 | BLITZ/DODGE 5×, BLITZ 1× | TO 20 · TD 0 · míč 0 · — · blitz 8 · faul 0 |
| 30 | 2 | 7 | trpaslici | 1 do TD, rohy 3, sousedé 4, dosah 5 · blok 3 blitz 0 faul 0 přihr. 0 · ⛔ turnover | TO 0 · TD 20 · míč 20 · — · blitz 0 · faul 0 |  | TO 9 · TD 11 · míč 11 · — · blitz 0 · faul 0 |
| 31 | 2 | 8 | elfove | 16 do TD, rohy 0, sousedé 0, dosah 2 · blok 0 blitz 1 faul 0 přihr. 0 | TO 3 · TD 0 · míč 0 · — · blitz 20 · faul 0 | BLITZ 2×, BLITZ/DODGE 1× | TO 17 · TD 0 · míč 0 · — · blitz 11 · faul 0 |
| 32 | 2 | 8 | trpaslici | TD · blok 0 blitz 1 faul 0 přihr. 0 | TO 11 · TD 0 · míč 1 · do TD 8.0, rohy 0.0, dosah 2.0 · blitz 20 · faul 4 | MOVE/DODGE 4×, BLITZ 3×, MOVE/GFI 2×, BLOCK 2× | TO 18 · TD 0 · míč 0 · — · blitz 19 · faul 1 |

## Součty

| kdo | AI | turnover | TD | před turnoverem hrálo jiných hráčů (průměr) | turnover hned první akcí |
|---|---|---|---|---|---|
| trpaslici | skutečnost | 1/16 | 2 | | |
| trpaslici | macro_mcts | 97/320 = 30% | 75 | 3.5 | 18/97 |
| trpaslici | greedy | 234/320 = 73% | 77 | 3.2 | 76/234 |
| elfove | skutečnost | 1/16 | 0 | | |
| elfove | macro_mcts | 178/320 = 56% | 0 | 3.2 | 56/178 |
| elfove | greedy | 308/320 = 96% | 0 | 3.3 | 38/308 |
