# Gating side-swap patch (d90227b): tier (b) empirical null-gate validation

**Datum:** 2026-07-15 (Fable 5) · **Patch pod testem:** `d90227b` (fix(gate):
side-swap orientation in gating + benchmark), commit navazuje na `c7b1ed6`.
**Nástroj:** `diag_sideswap_null_20260715.py` (nový, commit `a9dcaaf` spolu
s malou úpravou `diag_utils.run_arm` — tally teď rozpozná i 3-tuple
`(cs, fs, cand_is_away)` side-swap gate výsledek, ne jen legacy 2-tuple).
**Data:** `arm_sideswap_null_20260715.json` (N=300, base_seed=20260715,
`weights_best.json` vs `weights_best.json`, MCTS=100, TV=1200, vf_blend=0.0,
policy_path=`weights_policy.json` — produkční `_gate_game` 10-tuple s
`cand_is_away = i % 2 == 1`, přesně produkční orientation schedule).
Log: `diag_sideswap_null_20260715.log`. Spec: `proposals_gating_sideswap_20260714.md`
sekce 3(b).

---

## VERDIKT: **PASS**

Empiricky potvrzeno, ne jen konstrukcí kódu: po side-swap patchi null gate
(kandidát == frozen) sedí ~50 % decisive, uvnitř 2σ tolerance definované
v návrhu. Patch je bezpečný k pushnutí/nasazení z hlediska tohoto testu.

## Čísla

- **N = 300, 300/300 dokončeno, 0 watchdog-skipped.**
- Pooled: 62 W / 169 D / 69 L (candidate perspective). Draw rate 56.3 %.
- **Pooled decisive share (candidate) = 47.3 %**, decisive n = 131.
  95% CI = ±8.5 pp (Wald, `Z95 · sqrt(p(1-p)/n)`).
- **Odchylka od 50 % = −2.7 pp.** σ = 0.5/√131 = 4.4 %.
  |odchylka|/σ = **0.61** — hluboko pod prahem 2σ (= 8.7 pp) z proposalu.
- **Kontrast se starým unswapped nálezem:** stará null-test metodika (bez
  side-swap patche) dávala ~59.9 % decisive share, McNemar p=0.02, tj.
  +1.5σ NAD gate-prahem (viz `proposals_gating_sideswap_20260714.md` §1).
  Po swapu je odchylka od 50 % **6× menší** (2.7 pp vs ~10 pp) a se
  správným znaménkem k nule.

### Orientation-arm symmetrie (diagnostika, proposal §3b druhé kritérium)

| rameno | n | W | D | L | win% | draw% | loss% |
|---|---|---|---|---|---|---|---|
| cand@HOME | 150 | 28 | 87 | 35 | 18.7% | 58.0% | 23.3% |
| cand@AWAY | 150 | 34 | 82 | 34 | 22.7% | 54.7% | 22.7% |

- Symetrie: cand@HOME win rate (18.7 %) vs cand@AWAY loss rate (22.7 %) —
  delta −4.0 pp. Očekávaný vzorec (strukturní edge se přelévá symetricky
  mezi ramena) je přibližně vidět, ale s n=150/rameno je σ ramene ≈ ×√2
  oproti poolu (~5-6 pp na této n), takže −4.0 pp je v šumu jednoho
  ramene — proposal to sám označuje jako monitor, ne vstup verdiktu
  (§4, poslední bod).
- Draw-rate gap mezi rameny: +3.3 pp (58.0 % cand@H vs 54.7 % cand@A) —
  proposal žádal "bez velkého rozdílu"; 3.3 pp na n=150/rameno je malé,
  kritérium splněno.

## Interpretace

1. **Primární kritérium (pooled decisive share ≈ 50 %, |dev| < 2σ): PASS**
   s velkou rezervou (0.61σ vs práh 2σ). Toto je ten rozhodující test z
   proposalu — přímá replika staré null-test metodiky, jen s patchnutým
   gate/benchmark kódem.
2. **Sekundární kritéria (arm symmetrie, draw-rate gap): konzistentní s
   očekáváním, uvnitř šumu jednoho ramene** — žádný signál, který by
   verdikt zpochybnil.
3. Draw rate 56.3 % na tomto N=300/seed=20260715 běhu je vyšší než
   nedávné referenční hodnoty z jiných diagnostik tento týden (42-50%
   rozsah v paměti projektu) — to je očekávaná seed-batch/N variance
   zdokumentovaná v `feedback_draw_rate_noise_floor.md` (±8-11pp šumové
   dno), NENÍ součástí side-swap kritéria a nemění PASS verdikt výše.
4. Patch `d90227b` je tímto empiricky ověřen tier (b) ze svého vlastního
   proposalu, navazuje na tier (a) offline stub testy
   (`python/tests/test_gate_sideswap.py`, zelené) a plnou test suite
   (Python 151/151, C++ 414/414). Tier (c) — kontrola `Side audit:` řádku
   v prvním ostrém produkčním gate běhu — zůstává jako regresní kanárek
   pro budoucnost, mimo scope tohoto úkolu.

## Poznámka ke scope

Tento běh NEŘEŠÍ interpretační strop gatu při `vf_blend=0` (null-weights
finding, `project_bloodbowl_gating_null_test_finding.md`) — side-swap
opravuje slotový bias, ne váhovou necitlivost HtH. To je samostatná
master-list položka, proposal to sám explicitně odlišuje (§5 bod 4).
