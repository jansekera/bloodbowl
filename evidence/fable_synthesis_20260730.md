# Syntéza Fable analýzy 30.07.2026 — „Učíme se doopravdy, a umíme to změřit?"

Vstupy: fable_gate_forensics (R1), fable_pipeline_audit (R2),
fable_noreset_design (R5), fable_crossera (R6, sekce B dopočítána z
diag_crossera_20260730_results.json po výpadku limitu), decision-churn
data (R3, vyhodnoceno ze summary.json — agent padl na limit před
sepsáním, měření doběhlo celé).

## Doplněné výsledky R3 (decision churn; null kontrola validní: 8/8 her identických, 0 churn)

| pár | churn hazard/rozhodnutí | 1. divergence po | identické hry | klasifikace divergencí |
|---|---|---|---|---|
| šampion vs item2 e2 (MCTS400) | 27,0 % [21,0; 33,9] | ~3,7 rozh. | 0/48 | safer_same_scoring 13, equal/sub-feature 31, riskier_no_gain 4 |
| šampion vs item1 e16 sourozenec | 25,8 % [20,1; 32,5] | ~3,9 rozh. | 0/48 | safer 12, equal/sub 28, riskier 7, more_scoring 1 |

**Závěr R3: kandidáti se chovají VÝRAZNĚ jinak než šampion** (žádná hra
identická, každé ~4. rozhodnutí jiné) → hypotéza „50% HtH protože jsou
behaviorálně identičtí" VYVRÁCENA. Změny jsou ale laterální — nepromítají
se do výher (viz R1 pooled 47,8 %). Mírně převažují bezpečnější volby
(13:4 u e2) — slabý kvalitativní signál zlepšení, bez dopadu na skóre.

## Doplněné výsledky R6 (cross-era turnaj, 390 her, 6 kotev, MCTS=100, vf_blend=0.15)

Chess-score žebříček: **jul29 (item2 e2) 55,8 %** > jun19 55,0 % >
šampion 54,2 % > jul21 52,7 % > jul28 (item1 e16) 46,9 % > **may06 35,4 %**.

Klíčové párové výsledky (decisive % pro prvního): may06 vs šampion 18 %,
may06 vs jul21 13 % → **květen→červen = velké reálné zlepšení, učení
prokazatelně fungovalo**. jun19 vs šampion 73 % (11 dec., malé N),
šampion vs jul29 43 %. Od poloviny června všechny kotvy v pásmu
~47-56 % ≈ šum → **plateau od poloviny června je REÁLNÉ, ne artefakt
měření**. item2 e2 (MCTS=400) je nahoře a porazil šampiona 8-6 — slabý
pozitivní signál pro MCTS=400, hluboko v šumu.

## Celkový obraz (všech 5 balíků dohromady)

1. **Hypotéza zahazovaných malých zisků: NEPOTVRZENA.** R1: pooled HtH
   kandidátů 47,8 % CI [46,3; 49,3] (pod 50 %); R6: plateau reálné;
   retro-judging: měkčí prahy by promotovaly max 1 běh za 5 týdnů.
   Mechanismus (přísný práh + plný reset, nejhorší kombinace vs
   AlphaZero) existuje, ale zatím není co zahazovat — VE VALUE KANÁLU.
2. **Učení se ale děje — jen laterálně a v neviditelném kanálu.** R3:
   chování se mění hodně; R2 N1: policy hlava (jediná akumulující část)
   se nepoužívá NIKDE v pipeline (epoch_blend vždy 0, gate/benchmark
   blend 0) — 100 % policy signálu je odpojeno od rozhodování i měření.
3. **Měřicí infrastruktura má 3 nové bugy (R2)**: N2 benchmark vs random
   běží s VF_BLEND=0 → null-test řídící tier/HARD-REJECT; N3
   replay_buffer se každou iteraci vrací na únorový snapshot; N4 šampion
   není gate-promoted (mimo-gate přepis 22.07, necertifikovaná kotva).

## Doporučené další kroky (pořadí)

1. **Fix N2** (VF_BLEND v _run_benchmark) — triviální, vysoká hodnota.
2. **Rozhodnout N4**: certifikační HtH současného šampiona vs promoted
   842c200 (16.06); R6 naznačuje, že současný šampion je OK (≈ jun19),
   ale kotva má být certifikovaná.
3. **Férový test policy hlavy** — přidat away_policy_weights_path do
   bindingu + zapojit policy_blend>0 aspoň v evaluaci; tohle je teď
   hlavní frontier otázky „učíme se?" (kanál, kde se učení možná děje
   a nikdo ho nikdy neviděl).
4. Fix N3 (replay buffer buď untrack, nebo re-apply po git reset).
5. Teprve pak rozhodnout: dlouhý item2 run (slabý pozitivní signál z R6)
   vs R5 no-reset experiment (~28-45 h) — oba by dnes měřily zčásti
   rozbitou infrastrukturou.
