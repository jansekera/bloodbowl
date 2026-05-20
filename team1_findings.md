# Team 1 — Findings (2026-05-20)
## Analýza stagnace AI, navrženo od nuly

---

## KONSENSUS VŠECH AGENTŮ — nejvyšší priorita

### 1. carrier_can_score (idx 59) — sníženo bez důvodu
- **Aktuálně: 0.6** (trainer.py:22)
- **Při rekordu 96.7%: 0.8**
- Komentář v kódu: "sníženo z 0.8 (kompromis pro stabilnější trénink)"
- **Akce: vrátit na 0.8** — první ze dvou postupných změn

### 2. stall_incentive (idx 35) = 0.5 — false incentive
- Odměňuje stání s míčem, ne pohyb
- Přímá příčina "klece bez pohybu" — plateau strategie
- **Akce: snížit na 0.0** — druhá ze dvou postupných změn
- Implementovat ZVLÁŠŤ, po pozorování efektu carrier_can_score

### 3. Benchmark šum — gating je částečně loterie
- BENCHMARK_MATCHES=200 OK, ale GATING_MATCHES=100 (chess games)
- 95% CI při 100 hrách = ±9.8% → threshold 50% je uvnitř šumového pásma
- **Akce: zvýšit GATING_MATCHES na 200**

---

## VF_BLEND kontradikcce — vyžaduje rozhodnutí

| Agent | Doporučení | Zdůvodnění |
|-------|-----------|-----------|
| ML/RL Architect | VF_BLEND=0.0 je strukturální bug → okamžitě opravit | Neural VF se trénuje ale nikdy nepoužívá v MCTS |
| Training Loop Expert | Vrátit na 0.0, rampovat až po BM ≥ 85% | Špatná VF síť (před 85%) v MCTS škodí více než pomáhá |

**Aktuální stav: VF_BLEND=0.3** (commit caa99da, 2026-05-20)
**Platný argument Training Experta:** model je aktuálně na 78% → VF pravděpodobně ještě není dostatečně dobrá.
**Závěr: sledovat výsledky iterací s 0.3 — pokud benchmark neklesne, ponechat.**

---

## C++ Engine Analyst — překvapivé nálezy (prohlédl kód)

### Velký architekturální limit
- 70 features **agreguje 22 hráčů do průměrů** — pozice a atributy jednotlivých hráčů ztraceny
- Ideál: ~492 features (per-player: pozice, síla, agility, klíčové skills)
- Toto je největší dlouhodobý limit

### C++ výkonnostní hotspoty
- `replayToNode()` — potenciál 20-30% zrychlení MCTS přes GameState snapshoty
- `countTacklezones()` — precompute grid, update inkrementálně → 15-25% zrychlení
- Feature [29] (bias constant = 1.0) je zbytečná — měla by být odstraněna

### MCTS a stochasticita
- Standardní MCTS má aliasing problem při high-RNG (Blood Bowl kostky)
- Root sampling (fix dice per simulation) je pragmatický fix bez restrukturace stromu
- UCB konstanta c=1.41 je podkalibrovaná pro high-variance hru; doporučení: c=2.5

---

## Game Domain Expert — reward signal

### Chybějící features v shaping
Přidat do DEFAULT_SHAPING_WEIGHTS (trainer.py):
- `carrier_tz_count` (idx 40): váha -0.5 (nosič v tackle zones = nebezpečí)
- `carrier_blitzable` (idx 63): váha -0.4
- `opp_scoring_threat` (idx 42): váha -0.8

### Credit assignment problem
- MC signal pro tah 1 stejný jako pro tah 15 → early decisions underfitted
- gamma=0.99, 150 kroků: 0.99^75 ≈ 0.47 → long-horizon signal zachován, ale slabý
- Experiment: TD(λ) s lambda=0.8 jako alternativa k čistému MC

### Hierarchie kompetencí AI
- **80%+**: cage formation, pickup + carry, score when able, avoid turnover cascade
- **90%+**: cage breakdown, screen play, risk calibration, late game adjustments
- **95%+**: endgame state management, blitz sequencing, crowd surf exploitation

---

## Training Loop Expert — statistika gating

### Kolik her je potřeba (binomiální test, α=0.05)

| Her | Chess gate power (+5%) | Benchmark CI |
|-----|------------------------|--------------|
| 100 | ~55%                   | ±10% |
| 200 | ~75%                   | ±7% |
| 300 | ~87%                   | ±5.7% |

**Klíčový závěr:** Oscilace 77–88% při n=200 benchmarkových hrách je pravděpodobně ČISTÝ ŠUM (3-sigma). Model mohl konvergovat a my to nevidíme.

### Diagnostické testy pro stagnaci
```python
# (a) Local minimum — weight drift check
cos_sim = np.dot(w1, w2) / (np.linalg.norm(w1) * np.linalg.norm(w2))
# cos_sim > 0.98 → weights se nehýbají

# (b) VF kalibrace — měl by korelovat s výsledkem
for turn in range(1, 17):
    avg_vf = mean([vf.evaluate(s) for s in states_at_turn(turn)])
# Správně: VF koreluje se skórem
# Špatně: VF ~ 0.0 všude

# (c) EMA místo raw benchmark pro detekci trendu
ema_bm = 0.8 * ema_bm_prev + 0.2 * new_bm
```

---

## Systems Architect — observability gaps

Metriky které NEJSOU logovány a měly by být:
1. `nil_nil_rate` per epoch — Nash equilibrium indikátor
2. `gradient_norm` per layer — detekce exploding/vanishing gradients
3. `mean |V(s)|` per game — VF saturace (>0.8 = problém)
4. `weight_norm_change` per iteraci — jak moc se váhy mění

Logovat do `epoch_metrics.csv` (přidat sloupce).

---

## POŘADÍ IMPLEMENTACE (dohodnuto)

1. **HOTOVO:** ANTI_REGRESSION 0.35→0.50, BM_DROP_LIMIT 0.10→0.05 (commit 2f2380f)
2. **HOTOVO:** VF_BLEND 0.0→0.3 s dokumentovaným komentářem (commit caa99da)
3. **TODO #1:** `carrier_can_score` 0.6→0.8 (trainer.py:22) — po skončení aktuálního --loop 8
4. **TODO #2:** `stall_incentive` 0.5→0.0 (trainer.py:21) — po pozorování efektu #1 (3+ iterace)
5. **TODO #3:** GATING_MATCHES 100→200 — zvýšit spolehlivost chess gate
6. **TODO (dlouhodobé):** Per-player features (~492), MCTS root sampling, c_puct=2.5
