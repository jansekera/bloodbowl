# A2-1: První férové vyhodnocení naučené policy hlavy (2026-07-30)

**Stav: BĚŽÍ** — skript `diag_policy_fairtest_20260730.py` (PID 373046, detached),
průběžné výsledky v `diag_policy_fairtest_20260730_results.json`, log
`diag_policy_fairtest_20260730.log`. Výsledky doplnit po doběhnutí
(odhad ~2,5–4 h; ~29 s/hra, 450 her, 6 workerů).

## Kontext

Policy síť se trénuje a akumuluje přes iterace (L2 drift 0,66→3,46 za
červenec), ale její **naučený obsah se nikde nepoužívá** — `policy_blend=0`
všude v pipeline (evidence/fable_pipeline_audit_20260730.md, nález N1).
Jediný dosavadní efekt souboru `weights_policy.json` je aktivace ručně psaných
prior floorů v `macro_mcts.cpp expand()` (gatováno na `config_.policy !=
nullptr`, ne na blend). Toto je **první měření, které naučený obsah policy
hlavy vůbec zapojí do rozhodování** — umožněno commitem ea92bd0
(per-side `away_policy_weights_path` / `away_policy_blend` v
`bb_engine.simulate_game_logged`; `away_policy_blend=-1` = zdědit
`policy_blend`, `away_policy_weights_path=''` = away sdílí home síť).

## Otázka

Zvyšuje zapojení naučené policy (blend B > 0) win rate proti **identické**
síti bez ní (blend 0)? Čistá izolace příspěvku naučeného obsahu — prior
floors jsou aktivní na obou stranách.

## Metodika

- Šampion vs šampion: `weights_best.json` obě strany (value), `vf_blend=0.15`,
  `MCTS=100`, `epsilon=0`, TV 1200.
- Obě strany sdílejí policy síť `weights_policy.json` (stash z 29.07, akumulovaná;
  away předáno `away_policy_weights_path=''` → sdílí home objekt) ⇒ prior-floor
  režim symetricky aktivní na obou stranách.
- Jediná asymetrie: kandidátní strana `policy_blend=B`, druhá strana blend 0.0.
- 3 ramena: **B = 0.1, 0.2, 0.3**; každé 75 side-swapped seed párů = 150 her
  (stejný seed hrán s kandidátem home i away — vzor
  `diag_champion_cert_20260730.py`). Stejná sada seedů (30 000 000 + idx)
  napříč rameny; rasy rotují (human/orc/skaven/dwarf/wood-elf, soupeř = další
  rasa v pořadí).
- Dirichlet α a exploration_c = engine defaulty (0.3 / 0.5), shodné s
  diag_champion_cert (produkční gate má α=0, c=1.0 — vědomá odchylka,
  odpovídá poslednímu certifikačnímu diagu).
- Vyhodnocení: W/D/L kandidáta, decisive win rate W/(W+L), Wilsonův 95% CI
  per rameno. Pozn.: šumové dno draw-rate ±8–11 pp (paměť
  feedback_draw_rate_noise_floor) — rozdíly uvnitř CI = INCONCLUSIVE.
- Žádný trénink, žádná změna produkčních souborů, žádný rebuild enginu.

## Výsledky

*(doplnit po doběhnutí — čti `diag_policy_fairtest_20260730_results.json`,
klíč `summary`)*

| Rameno B | N her | W | D | L | decisive WR | Wilson 95% CI |
|---------:|------:|--:|--:|--:|------------:|---------------|
| 0.1 | – | – | – | – | – | – |
| 0.2 | – | – | – | – | – | – |
| 0.3 | – | – | – | – | – | – |

## Verdikt

*(doplnit: pomáhá / škodí / nic — per rameno; CI přes 50 % = inconclusive)*

## Doporučení pro POLICY_BLEND v pipeline

*(doplnit po výsledcích)*
