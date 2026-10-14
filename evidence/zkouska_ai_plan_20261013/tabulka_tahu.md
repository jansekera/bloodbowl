# Zkouška „AI plánuje tah“ na partii lidé × orkové 12.–13.10. — tabulka tahů

Engine = značka `zkouska-ai-plan-20261013-baze`; v každé pozici 20 plánů macro_mcts (50 iterací, `weights_best.json`) a 20 plánů greedy, každý s vlastními kostkami. Přehrání ověřeno: skóre 1:1, 32 tahů, TD [(1, 5, 'lide'), (2, 7, 'orkove')].

Sloupce plánu: turnover / TD / s míčem na konci (z 20) · průměr polí do TD · rohy klece · kolik soupeřů dosáhne na nosiče · blitz a faul (z 20). „—“ = tým na konci tahu míč nedrží.

| tah | pol. | kolo | kdo | skutečnost | macro_mcts | na čem padá macro_mcts | greedy |
|---|---|---|---|---|---|---|---|
| 1 | 1 | 1 | lide | 17 do TD, rohy 2, sousedé 2, dosah 4 · blok 4 blitz 1 faul 0 přihr. 0 | TO 4 · TD 0 · míč 18 · do TD 17.56, rohy 3.78, dosah 3.89 · blitz 17 · faul 4 | FOUL 2×, MOVE/PICKUP 2× | TO 15 · TD 0 · míč 17 · do TD 15.0, rohy 0.12, dosah 3.71 · blitz 17 · faul 0 |
| 2 | 1 | 1 | orkove | bez míče · blok 0 blitz 1 faul 0 přihr. 0 | TO 3 · TD 0 · míč 0 · — · blitz 20 · faul 0 | MOVE/DODGE 2×, MOVE/GFI 1× | TO 18 · TD 0 · míč 0 · — · blitz 20 · faul 0 |
| 3 | 1 | 2 | lide | 14 do TD, rohy 1, sousedé 1, dosah 9 · blok 0 blitz 1 faul 0 přihr. 0 | TO 3 · TD 0 · míč 20 · do TD 14.0, rohy 4.0, dosah 8.3 · blitz 20 · faul 0 | MOVE/DODGE 1×, BLITZ 1×, BLOCK 1× | TO 20 · TD 0 · míč 19 · do TD 9.0, rohy 0.0, dosah 4.32 · blitz 15 · faul 0 |
| 4 | 1 | 2 | orkove | bez míče · blok 4 blitz 1 faul 0 přihr. 0 | TO 5 · TD 0 · míč 0 · — · blitz 20 · faul 0 | BLOCK 3×, MOVE/DODGE 2× | TO 20 · TD 0 · míč 0 · — · blitz 15 · faul 0 |
| 5 | 1 | 3 | lide | 2 do TD, rohy 1, sousedé 1, dosah 0 · blok 1 blitz 1 faul 0 přihr. 1 | TO 1 · TD 0 · míč 20 · do TD 13.9, rohy 4.0, dosah 9.7 · blitz 17 · faul 0 | BLOCK 1× | TO 20 · TD 0 · míč 14 · do TD 6.36, rohy 0.0, dosah 5.93 · blitz 2 · faul 0 |
| 6 | 1 | 3 | orkove | bez míče · blok 5 blitz 1 faul 1 přihr. 0 | TO 7 · TD 0 · míč 1 · do TD 24.0, rohy 0.0, dosah 3.0 · blitz 18 · faul 0 | BLOCK 3×, BLITZ/GFI 3×, BLITZ/DODGE 1× | TO 19 · TD 0 · míč 0 · — · blitz 18 · faul 0 |
| 7 | 1 | 4 | lide | 1 do TD, rohy 0, sousedé 0, dosah 0 · blok 1 blitz 1 faul 0 přihr. 0 | TO 4 · TD 16 · míč 16 · — · blitz 0 · faul 0 | MOVE/DODGE 4× | TO 9 · TD 11 · míč 11 · — · blitz 0 · faul 0 |
| 8 | 1 | 4 | orkove | bez míče · blok 0 blitz 1 faul 0 přihr. 0 | TO 7 · TD 0 · míč 0 · — · blitz 20 · faul 0 | BLITZ/DODGE 4×, MOVE/DODGE 2×, BLITZ 1× | TO 20 · TD 0 · míč 0 · — · blitz 17 · faul 0 |
| 9 | 1 | 5 | lide | TD · blok 0 blitz 0 faul 0 přihr. 0 | TO 5 · TD 15 · míč 15 · — · blitz 0 · faul 0 | MOVE/DODGE 5× | TO 5 · TD 15 · míč 15 · — · blitz 0 · faul 0 |
| 10 | 1 | 5 | orkove | 20 do TD, rohy 2, sousedé 2, dosah 1 · blok 1 blitz 1 faul 0 přihr. 0 | TO 5 · TD 0 · míč 17 · do TD 20.35, rohy 2.0, dosah 1.76 · blitz 14 · faul 0 | MOVE/PICKUP 3×, MOVE/DODGE 1×, BLOCK 1× | TO 20 · TD 0 · míč 16 · do TD 18.0, rohy 0.06, dosah 7.19 · blitz 14 · faul 0 |
| 11 | 1 | 6 | lide | bez míče · blok 1 blitz 1 faul 0 přihr. 0 | TO 1 · TD 0 · míč 0 · — · blitz 20 · faul 0 | BLITZ 1× | TO 20 · TD 0 · míč 0 · — · blitz 19 · faul 0 |
| 12 | 1 | 6 | orkove | 15 do TD, rohy 3, sousedé 3, dosah 11 · blok 1 blitz 1 faul 0 přihr. 0 | TO 2 · TD 0 · míč 20 · do TD 15.0, rohy 4.0, dosah 9.25 · blitz 20 · faul 0 | MOVE/GFI 2× | TO 20 · TD 0 · míč 19 · do TD 13.0, rohy 0.0, dosah 10.89 · blitz 16 · faul 0 |
| 13 | 1 | 7 | lide | bez míče · blok 3 blitz 0 faul 0 přihr. 0 | TO 3 · TD 0 · míč 0 · — · blitz 20 · faul 0 | MOVE/DODGE 3× | TO 19 · TD 0 · míč 0 · — · blitz 19 · faul 0 |
| 14 | 1 | 7 | orkove | 12 do TD, rohy 2, sousedé 4, dosah 11 · blok 1 blitz 1 faul 0 přihr. 0 | TO 1 · TD 0 · míč 20 · do TD 13.0, rohy 4.0, dosah 10.45 · blitz 16 · faul 0 | BLOCK 1× | TO 20 · TD 0 · míč 1 · do TD 8.0, rohy 0.0, dosah 10.0 · blitz 1 · faul 0 |
| 15 | 1 | 8 | lide | bez míče · blok 1 blitz 1 faul 0 přihr. 0 | TO 18 · TD 0 · míč 0 · — · blitz 20 · faul 0 | MOVE/DODGE 10×, BLITZ 6×, BLOCK 1×, MOVE/GFI 1× | TO 20 · TD 0 · míč 0 · — · blitz 17 · faul 0 |
| 16 | 1 | 8 | orkove | 12 do TD, rohy 1, sousedé 3, dosah 9 · blok 2 blitz 1 faul 1 přihr. 0 | TO 0 · TD 0 · míč 20 · do TD 9.85, rohy 3.3, dosah 10.45 · blitz 20 · faul 0 |  | TO 20 · TD 0 · míč 1 · do TD 5.0, rohy 0.0, dosah 10.0 · blitz 1 · faul 0 |
| 17 | 2 | 1 | orkove | 18 do TD, rohy 2, sousedé 2, dosah 8 · blok 1 blitz 1 faul 0 přihr. 0 | TO 6 · TD 0 · míč 17 · do TD 17.47, rohy 3.76, dosah 8.12 · blitz 17 · faul 0 | MOVE/PICKUP 3×, MOVE/DODGE 2×, BLOCK 1× | TO 19 · TD 0 · míč 17 · do TD 15.0, rohy 0.06, dosah 7.82 · blitz 14 · faul 0 |
| 18 | 2 | 1 | lide | bez míče · blok 1 blitz 0 faul 0 přihr. 0 | TO 6 · TD 0 · míč 0 · — · blitz 20 · faul 0 | MOVE/DODGE 4×, BLOCK 1×, BLITZ 1× | TO 19 · TD 0 · míč 0 · — · blitz 20 · faul 0 |
| 19 | 2 | 2 | orkove | 14 do TD, rohy 1, sousedé 2, dosah 9 · blok 0 blitz 1 faul 0 přihr. 0 | TO 2 · TD 0 · míč 20 · do TD 13.0, rohy 4.0, dosah 9.7 · blitz 16 · faul 0 | BLITZ/DODGE 1×, BLOCK 1× | TO 20 · TD 0 · míč 8 · do TD 11.0, rohy 0.0, dosah 9.0 · blitz 7 · faul 0 |
| 20 | 2 | 2 | lide | bez míče · blok 0 blitz 0 faul 0 přihr. 0 · ⛔ turnover | TO 8 · TD 0 · míč 2 · do TD 12.0, rohy 1.5, dosah 11.0 · blitz 20 · faul 0 | BLOCK 2×, MOVE/DODGE 2×, MOVE/PICKUP 1×, BLITZ/DODGE 1×, MOVE/GFI 1×, BLITZ 1× | TO 12 · TD 0 · míč 0 · — · blitz 12 · faul 0 |
| 21 | 2 | 3 | orkove | 13 do TD, rohy 4, sousedé 5, dosah 9 · blok 3 blitz 1 faul 0 přihr. 0 | TO 1 · TD 0 · míč 20 · do TD 13.0, rohy 3.95, dosah 8.5 · blitz 19 · faul 0 | BLOCK 1× | TO 20 · TD 0 · míč 2 · do TD 10.0, rohy 1.0, dosah 8.0 · blitz 0 · faul 0 |
| 22 | 2 | 3 | lide | bez míče · blok 0 blitz 1 faul 0 přihr. 0 | TO 3 · TD 0 · míč 0 · — · blitz 20 · faul 0 | MOVE/DODGE 3× | TO 20 · TD 0 · míč 0 · — · blitz 20 · faul 0 |
| 23 | 2 | 4 | orkove | 12 do TD, rohy 3, sousedé 4, dosah 9 · blok 1 blitz 1 faul 0 přihr. 0 | TO 0 · TD 0 · míč 20 · do TD 12.0, rohy 4.0, dosah 9.6 · blitz 20 · faul 0 |  | TO 20 · TD 0 · míč 18 · do TD 13.17, rohy 1.78, dosah 9.61 · blitz 20 · faul 0 |
| 24 | 2 | 4 | lide | bez míče · blok 0 blitz 1 faul 0 přihr. 0 | TO 1 · TD 0 · míč 0 · — · blitz 20 · faul 0 | BLOCK 1× | TO 20 · TD 0 · míč 0 · — · blitz 12 · faul 0 |
| 25 | 2 | 5 | orkove | 11 do TD, rohy 2, sousedé 5, dosah 9 · blok 4 blitz 1 faul 0 přihr. 0 | TO 6 · TD 0 · míč 19 · do TD 11.74, rohy 2.32, dosah 9.11 · blitz 19 · faul 2 | BLITZ/DODGE 4×, BLOCK 1×, MOVE/CATCH 1× | TO 19 · TD 0 · míč 18 · do TD 12.72, rohy 0.67, dosah 8.56 · blitz 12 · faul 0 |
| 26 | 2 | 5 | lide | bez míče · blok 0 blitz 1 faul 0 přihr. 0 | TO 8 · TD 0 · míč 0 · — · blitz 20 · faul 0 | BLITZ 4×, MOVE/DODGE 2×, BLOCK 1×, MOVE/CATCH 1× | TO 20 · TD 0 · míč 0 · — · blitz 18 · faul 0 |
| 27 | 2 | 6 | orkove | 11 do TD, rohy 2, sousedé 5, dosah 9 · blok 3 blitz 0 faul 0 přihr. 0 | TO 0 · TD 0 · míč 20 · do TD 11.0, rohy 3.0, dosah 8.1 · blitz 20 · faul 0 |  | TO 19 · TD 0 · míč 2 · do TD 4.0, rohy 0.0, dosah 5.0 · blitz 2 · faul 0 |
| 28 | 2 | 6 | lide | bez míče · blok 1 blitz 1 faul 0 přihr. 0 | TO 5 · TD 0 · míč 0 · — · blitz 20 · faul 0 | BLITZ 4×, BLOCK 1× | TO 18 · TD 0 · míč 0 · — · blitz 15 · faul 0 |
| 29 | 2 | 7 | orkove | TD · blok 2 blitz 1 faul 0 přihr. 1 | TO 15 · TD 2 · míč 5 · do TD 8.67, rohy 0.33, dosah 7.33 · blitz 5 · faul 0 | PASS/PASS 7×, MOVE/GFI 3×, PASS 2×, PASS/CATCH 2×, MOVE/DODGE 1× | TO 20 · TD 0 · míč 10 · do TD 12.1, rohy 0.8, dosah 8.1 · blitz 5 · faul 0 |
| 30 | 2 | 7 | lide | 15 do TD, rohy 2, sousedé 2, dosah 9 · blok 3 blitz 0 faul 0 přihr. 0 | TO 5 · TD 0 · míč 19 · do TD 15.79, rohy 3.84, dosah 4.63 · blitz 18 · faul 0 | MOVE/GFI 2×, BLITZ 2×, MOVE/PICKUP 1× | TO 20 · TD 0 · míč 16 · do TD 13.0, rohy 0.0, dosah 6.56 · blitz 14 · faul 0 |
| 31 | 2 | 8 | orkove | bez míče · blok 1 blitz 0 faul 0 přihr. 0 · ⛔ turnover | TO 6 · TD 0 · míč 0 · — · blitz 20 · faul 0 | MOVE/DODGE 3×, BLITZ/DODGE 2×, MOVE/GFI 1× | TO 20 · TD 0 · míč 0 · — · blitz 18 · faul 0 |
| 32 | 2 | 8 | lide | 15 do TD, rohy 4, sousedé 4, dosah 7 · blok 1 blitz 0 faul 0 přihr. 0 | TO 10 · TD 0 · míč 20 · do TD 11.0, rohy 4.0, dosah 7.3 · blitz 14 · faul 2 | BLOCK 6×, BLITZ/DODGE 4× | TO 19 · TD 0 · míč 20 · do TD 14.0, rohy 1.65, dosah 8.55 · blitz 9 · faul 0 |

## Součty

| kdo | AI | turnover | TD | před turnoverem hrálo jiných hráčů (průměr) | turnover hned první akcí |
|---|---|---|---|---|---|
| lide | skutečnost | 1/16 | 1 | | |
| lide | macro_mcts | 85/320 = 27% | 31 | 3.8 | 29/85 |
| lide | greedy | 276/320 = 86% | 26 | 3.1 | 44/276 |
| orkove | skutečnost | 1/16 | 1 | | |
| orkove | macro_mcts | 66/320 = 21% | 2 | 4.2 | 12/66 |
| orkove | greedy | 314/320 = 98% | 0 | 2.5 | 105/314 |
