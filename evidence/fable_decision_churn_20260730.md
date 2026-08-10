# Decision-churn: párové srovnání rozhodnutí kandidát vs šampion (R3, 30.07)

Agent R3 postavil a validoval nástroj (`diag_decision_churn_20260730.py`)
a spustil plné měření; na sepsání dokumentu už nedošlo (session limit) —
měření doběhlo celé (`diag_decision_churn_data/run_all.status=ALL_DONE`),
vyhodnoceno lokálně ze `summary.json` souborů. Konfigurace zrcadlí gate
(vf_blend=0.15, párové seedy). Board snapshoty všech divergencí jsou
v `diag_decision_churn_data/*/events.json.gz`.

## Validace nástroje (null kontrola)

Šampion vs šampion: 8/8 her identických, 637 párových rozhodnutí,
0 churn eventů (CI95 horní mez 0,6 %). Nástroj je deterministický.

## Výsledky

| pár | her | identických | churn hazard/rozhodnutí | 1. divergence po | soft churn (TV-matched mean/p90) |
|---|---|---|---|---|---|
| šampion vs item2 e2 (MCTS400 smoke) | 48 | 0 | **27,0 %** [21,0; 33,9] | ~3,7 rozh. | 0,037 / 0,105 |
| šampion vs item1 e16 sourozenec | 48 | 0 | **25,8 %** [20,1; 32,5] | ~3,9 rozh. | 0,040 / 0,115 |

Klasifikace divergencí (pair1 / pair2): equal_features 13/11,
sub_feature 18/17, **safer_same_scoring 13/12**, riskier_no_gain 4/7,
more_scoring 0/1.

Výsledky her ref vs cand v divergentních hrách (pair1): W→W 8, L→L 9,
D→W 8, D→D 7, D→L 5, L→D 4, W→D 3, W→L 3, L→W 1 — žádný směrový posun.

## Závěry

1. **Hypotéza „kandidát je behaviorálně identický šampionovi, proto 50 %
   HtH" je VYVRÁCENA** — žádná hra není identická, každé ~4. rozhodnutí
   se liší.
2. Změny jsou **laterální**: nepromítají se do výsledků her (viz matice
   přechodů) — konzistentní s R1 pooled 47,8 % a R6 plateau.
3. Slabý kvalitativní signál: bezpečnější volby převažují nad
   riskantnějšími (13:4 u e2; 12:7 u e16) — směr správný, dopad nulový.
4. Nástroj je připravený pro balík C/R4 a jako per-iterační metrika
   pro případný no-reset experiment (R5 design).
