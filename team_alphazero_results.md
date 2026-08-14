# Tým AlphaZero — výsledky (2026-06-15)

4 Opus agenti, ověřeno proti kódu. Cíl: jak správně rozchodit plné AlphaZero.
Navazuje na `team_alphazero_brief.md`. Viz [[project-bloodbowl]].

## Hlavní závěr

Celá AZ infrastruktura je postavená, ale vypnutá **dvěma vypínači**. Rozjezd =
zapnout a zkalibrovat, ne stavět. Warm-start z 87 % funguje automaticky a bezpečně.

## Potvrzené vypínače (všichni agenti)

1. **`run_iteration.py:230 --policy-lr=0`** → `training_loop.py:94`
   `use_policy = policy_lr>0 and mcts_iterations>0` → policy trainer se neinstancuje.
2. **`--policy-blend` se nepředává → default 0.0** (`train_cli.py:74`) → guard
   `macro_mcts.cpp:188,312 config_.policy && policyBlend>0.0f` nikdy nesplněn →
   MCTS bere **heuristické priory, ne naučenou policy**.

## Opravy briefu (důležité)

- **Hot path je MAKRO MCTS** (`engine/src/macro_mcts.cpp`), ne mikro `mcts.cpp`.
  `computePriors`, progressive widening a `mcts.cpp:204-243` se v tréninku/benchmarku
  NEpoužívají. Makro má vlastní inline softmax + ruční heuristické priory + blend
  (`macro_mcts.cpp:198-323`). Jakákoli úprava priorů musí cílit na MAKRO. (Agent 2)
- **Shaping váhy jsou v Pythonu** (`trainer.py:13-29 DEFAULT_SHAPING_WEIGHTS`),
  NE v `feature_extractor.cpp` (ten dělá jen normalizované features). Shaping se
  vypíná čistě v Pythonu, **bez C++ rebuildu**. (Agent 3)
- **Warm-start je automatický.** `weights_best.json` má legacy `type:neural` →
  `load_combined_weights` (`policy_trainer.py:407-410`) načte value (zachová 87 %),
  policy začne od nuly. Žádná migrace vah. (Agent 1, 4)

## Nalezené bugy / rizika

| # | Problém | Místo | Dopad | Závažnost |
|---|---------|-------|-------|-----------|
| 1 | **Dirichlet noise hardcoded 0.3/0.25** i pro benchmark/gating | `bb_module.cpp:445-446` (`simulate_game_logged`) | explorace šum v MĚŘENÍ síly → zkreslený gating; hist. 86-87 % bylo měřeno se šumem | VYSOKÁ |
| 2 | **Blend skok 0→plný** (žádný ramp jako VF) | `training_loop.py:319-322` | slabá raná policy skokem nahradí heuristiku → riziko propadu pod 86 % | STŘEDNÍ |
| 3 | **policy_loss se neloguje do CSV** + chybí prior↔visit metrika | `training_loop.py:495` (jen print) | „nepoznáš, že se to učí" | STŘEDNÍ (NUTNÉ pro detekci) |
| 4 | **Temperature mismatch:** makro blend ignoruje `temperature_`, efekt. temp=1.0 vs Python 0.3 | `macro_mcts.cpp:206` vs `policy_network.cpp:78` | priory plošší než Python očekává | NÍZKÁ |
| 5 | **Linear `train_on_decisions` ignoruje `passes`** | `policy_trainer.py:32-103` | imitace 5 passes bez efektu u linear | NÍZKÁ |
| 6 | **hidden>64 ticho ořízne** | `policy_network.cpp:44-45` | mismatch při `--policy-hidden-size>64` | NÍZKÁ (default 32) |

## Doporučená minimální konfigurace (Agent 4)

```
run_iteration.py self-play cmd:
  --policy-lr=0.01  --policy-model=linear
  --policy-blend=0.3  --imitation-epochs=4
  (+ MCTS_ITERATIONS 100 → 200 → 400)
training-method:  zatím beze změny (mc_shaped) — viz sekvencování níže
warm-start: automatický, nic ručně
```

## Shaping: čistý AZ vs ponechat (Agent 3)

- **Silný argument vypnout shaping pro AZ:** Φ obsahuje `(1,+3.0)` my_score,
  `(2,-3.0)` opp_score a `(59,+0.8)` carrier_can_score (sytí se už při *možnosti*
  skórovat). Reziduum γΦ (γ=0.99≠1) přidává trvalý signál i remízovým hrám →
  **shaping odměňuje bezpečné držení míče = Nash remízové equilibrium.** Pravděpodobný
  spolupachatel 75 % remíz.
- **Riziko vypnutí:** remíza dává outcome z=0 → u 75 % her nulový gradient → řídký
  signál, riziko kolapsu V≈0.
- **AZ-čistá náhrada:** `td_lambda` (λ=0.85) — bootstrap z V(s') dává hustý signál
  i u remíz BEZ ručního shapingu. Caveat: deadly triad, malý lr + hlídat normu vah.
- **A/B (3 ramena, start z weights_best, 15-20 iterací):**
  A=`mc_shaped` (baseline), B=`mc` (čisté z=±1), C=`td_lambda --lambda=0.85`.
  Měřit: % remíz (primární), benchmark, TD/hru, decisive chess, value variance, grad/weight norm.
  Lze řídit jen přepnutím `--training-method`, bez C++ rebuildu.

## Sekvencování (pravidlo: jedna změna naráz)

Agent 4 (policy) a Agent 3 (shaping) jsou ortogonální. Správné pořadí, ať lze
izolovat příčinu:
1. **Logging napřed** (bug #3): přidat `policy_loss` + prior↔visit do `epoch_metrics.csv`. Jinak nic nepoznáme.
2. **Imitation-only** (`--policy-lr=0.01 --policy-blend=0 --imitation-epochs=plný`): ověřit policy_loss↓ a value benchmark se nehne (blend=0 ⇒ MCTS beze změny ⇒ gating drží ~87 %). Izoluje, že policy trénink běží a nerozbíjí value.
3. **Opravit dirichlet v benchmark/gating** (bug #1) — čisté měření před tím, než začneme hýbat blendem.
4. **MCTS_ITERATIONS 100→200** — priory dávají smysl až s hlubším stromem.
5. **Zapnout blend postupně** 0.15→0.3 (ideálně přidat ramp, bug #2), ověřovat 2-3 iterace: decisive chess↑, nil_nil↓.
6. **Až je AZ stabilní → shaping A/B** (Agent 3).
7. **Až potom per-player 70→492** (ortogonální, neblokuje; loadery mají padding).

## Gating & per-player

- Decisive-only gating (`run_iteration.py:315-316`, GATING_MATCHES=600) na AZ SEDÍ.
  Dvojí rollback safety: gate nepromuje horší model + vše v gitu.
- Per-player NEblokuje AZ a naopak. Nejdřív stabilní AZ na 70 features, pak škálovat.

## Agent ID (pokračování přes SendMessage)
- Agent 1 (policy/data): `a03063938847464c7`
- Agent 2 (MCTS priory): `a27af0d67fec570c5`
- Agent 3 (value/shaping): `afa635e151f417e7d`
- Agent 4 (bring-up plán): `a0384eceb474e04a1`
