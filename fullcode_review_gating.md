# Gating / Orchestration Correctness Review

**Scope:** `run_iteration.py`, `python/blood_bowl/cpp_runner.py`, `benchmark.py`, `evaluate.py`, `cli_runner.py`, `fuzz_gate.py`, `verify_run.py`

**Verdict:** The just-fixed `_git_push` meta bug is now consistent (full meta with `tv` + `all_time_best_benchmark` written in both promote/reject branches and on disk in `run_iteration` Step 5). **However the same class of bug still has TWO live gate-bypass paths**, plus a benchmark that systematically over-states the new model. Net effect: the regression gate is still leaky and the benchmark numbers it gates on are biased high. Recommend fixing C-1 and C-2 before the next long run.

Ranked findings below.

---

## CRITICAL

### C-1. `frozen_bm`/`all_time_best_bm` lost when meta read throws → silent gate bypass
**File:** `run_iteration.py:173-175` (the `except Exception:` in Step 1)

When the meta/best read block raises, the handler sets `frozen_bm = 0.0` and `all_time_best_bm = 0.0` but does **not** set `baseline_reset`. This silently disables the regression gate without the explicit "baseline reset" log line:

- Step 5 gate `if all_time_best_bm > 0 and ...` → skipped (0.0)
- `if frozen_bm > 0 and ...` → skipped (0.0)
- `chess_score < ANTI_REGRESSION and not baseline_reset` → still active, but...
- `new_bm < BM_FLOOR and not baseline_reset` → still active

So a transient JSON read error (corrupt/half-written meta, disk hiccup) drops the two benchmark-regression checks entirely while pretending the frozen baseline is 0%. The model only has to clear the 0.77 floor and the chess gate to promote — a regressed model can sail through. Worse, it then writes `all_time_best_benchmark = max(0.0, new_bm) = new_bm`, **permanently destroying the real all-time-best record** so future iterations compare against the lower bar.

**Failure scenario:** iteration N writes `weights_best_meta.json`; a concurrent/partial write or filesystem error makes it unreadable at iteration N+1 freeze. all_time_best (say 86%) is silently reset to the new model's 80%. Every later iteration now gates against 80%, regression locked in.

**Fix:** In the `except`, either set `baseline_reset = True` and log loudly, or `raise` / `sys.exit(1)` rather than continue with zeroed baselines. Do **not** let a read failure quietly zero the all-time-best. At minimum, never lower `all_time_best_benchmark` on an exception path.

### C-2. Two benchmark halves use identical `race_idx` sequence → correlated samples, biased gate input
**File:** `run_iteration.py:219-234` (`_run_benchmark`) and `:286-296` (gate uses `new_bm`)

`_run_benchmark` builds tasks as `for i in range(half_bm)` with `race_idx = i`. Both `bm_az` and `bm_tb` runs therefore cover the **same matchup distribution**, and the winner-select `if bm_tb > bm_az` picks the **max of two noisy estimates of ~200 effective games each**. Taking `max(noisy_a, noisy_b)` is a positively-biased estimator: the reported `new_bm` that feeds the regression gate (`new_bm < all_time_best_bm - 0.05`, `new_bm < BM_FLOOR`) is systematically **higher than either model's true win rate**. With SD ≈ ±2.5% per half, the max-of-two bias is roughly +1.5–2%, which is comparable to the entire `BM_DROP_LIMIT` slack. This makes the benchmark floor/drop gates easier to pass than intended — it nudges the system toward promoting regressions.

Independent issue, same lines: seeds are `random.randint(1,999999)`, so the two halves and the gate run are seeded from the **parent's unseeded global RNG**. That's fine for independence, but note the 1..999999 space across 400+400+400 draws has a non-trivial collision rate; collisions reuse a full game (same seed + same race_idx → identical game), slightly **reducing effective N** below the assumed 200. Minor compared to the max-bias.

**Fix:**
1. Don't gate on `max(bm_az, bm_tb)`. Either (a) pick the candidate by a held-out criterion, then benchmark *that* candidate fresh for the gate number, or (b) pool both into one larger benchmark and gate on the pooled rate of the chosen model only.
2. Widen the seed space (`random.randint(1, 2**31-1)`) and/or vary `race_idx` between the two halves so they aren't the identical matchup set.

### C-3. Watchdog abort silently shrinks N → gate decided on a partial, biased sample
**File:** `run_iteration.py:61-81` (`_imap_watchdog`) consumed at `:227-234`, `:260-280`

On a stall the watchdog calls `pool.terminate()` and `return`s, yielding only the games completed so far. Callers then compute the score over whatever arrived (`sum(results)/len(results)`, `chess_score` over `total`). There is **no floor on completed count and no abort of the iteration** — a pool that wedges at, say, 30/400 will produce a `new_bm`/`chess_score` from 30 games (SD ≈ ±9%) and feed it straight into the promote decision. The comment even says this happened at 150/400 on 2026-06-10.

**Failure scenario:** engine hangs early in the anti-regression pool; chess_score computed from 40 games lands at 0.55 by luck → model promoted (or a good model rejected at 0.48). A wrong promotion from 40 games wastes the run.

**Fix:** Track `n_expected` vs `len(results)`; if the watchdog fired (returned early), treat the iteration as failed — do **not** promote. e.g. have `_imap_watchdog` signal truncation (raise or return a sentinel) and in Step 5 force `promote=False` / re-run, never promote on a truncated pool.

---

## HIGH

### H-1. `frozen_bm`/`all_time_best_bm` derived from meta that may be stale vs `weights_best.json`
**File:** `run_iteration.py:148-172`

Step 1 copies `weights_best.json → weights_frozen.json` (the actual weights), but reads the baseline win-rate from the **separate** `weights_best_meta.json`. If the two ever get out of sync (e.g. `weights_best.json` updated by a git pull from the server but meta not, or vice-versa — and the server *does* `git pull`, per project memory), the frozen weights and the `frozen_bm` they're compared against describe **different models**. The gate would then compare the new model against the wrong baseline number. The `_git_push` fix keeps them in sync *locally*, but nothing guarantees the server's pull lands both atomically.

**Fix:** Store benchmark metadata *inside* `weights_best.json` (single file, atomic) or verify a hash/version field linking meta to weights at freeze time; refuse to gate if they don't match.

### H-2. Non-atomic meta writes can produce the corrupt-meta that triggers C-1
**File:** `run_iteration.py:312-314`, `:355-356`; also `cpp_runner` log writes

`json.dump` directly onto the destination path (`open(..., 'w')`). A crash/kill mid-write (the watchdog uses `pool.terminate`, runs are long and killable) leaves a truncated `weights_best_meta.json`. Next iteration's Step-1 read throws → triggers C-1's silent baseline zeroing. The two bugs compound.

**Fix:** Write to a temp file in the same dir and `os.replace()` into place (atomic on POSIX). Apply to both meta writes and `weights_best.json`.

### H-3. `git reset --hard origin/main` can silently discard a just-promoted model on push failure
**File:** `run_iteration.py:341-378`

The reset correctly reads gate/frozen bytes into memory first and re-applies them. But if `git push` fails (line 371-375), the function prints "weights uloženy lokálně" and returns — yet the working tree is now at `origin/main` **plus** the re-applied weights as uncommitted/committed-but-unpushed changes. The *next* iteration's `_git_push` does another `git fetch` + `git reset --hard origin/main`, which **discards the unpushed commit** from the previous iteration. The previous promotion's weights survive only because Step 1 re-froze them into `weights_best.json`... but the committed-but-unpushed history is gone, and if Step 1 of the next run also hit C-1/H-1 the promoted weights can be lost. Fragile chain.

**Fix:** On push failure, do not advance; surface a hard error so the operator resolves the unpushed state before the next reset. Check `git commit` return code too (currently ignored — an empty commit or commit failure is swallowed, then push "succeeds" pushing nothing).

### H-4. Draws counted consistently *within* a metric but the two gates measure different things at threshold 0.51
**File:** benchmark `:104` (`home_score > away_score`, draws = loss) vs gate `:267-278` (draws = 0.5)

This is correct *as designed* (benchmark vs `random` is a win-rate; chess vs frozen is a chess-score with draws=0.5), so not a scoring inversion. But flagging per scope: with 66–72% draws, the chess `(wins + 0.5*draws)/total` is dominated by the draw term and sits ~0.50 mechanically; `ANTI_REGRESSION = 0.51` is inside the noise band (your own context). The gate as written is **near-random** and contributes little signal — combined with C-2's upward benchmark bias, the system leans toward promotion. Not a code bug, but the threshold should move outside the SD band or the chess gate should be computed over decisive games only (`wins/(wins+losses)`).

---

## MEDIUM

### M-1. `verify_run.py` restore is not crash-safe and can leave the repo on a stale baseline
**File:** `verify_run.py:27-64`

If the process is killed during `run_iteration` (before the `finally`), the `_verify_bak/` copies are never restored — the "shrunk" 24-game run's weights/meta become the real baseline for the next long run, exactly the mis-promotion this review is about. The backup is also a plain copy, so a kill mid-`shutil.copy2` in the restore loop leaves a half-written file (feeds C-1/H-2).

**Fix:** Restore via `os.replace`; document that a killed verify requires manual `git checkout` of the weights files.

### M-2. `bm_tb`/`train_best` path missing is treated as 0.0, then compared with `>` 
**File:** `run_iteration.py:237-248`

If `train_best.json` doesn't exist, `bm_tb = 0.0`. Then `if bm_tb > bm_az` is False → falls to `az_train` branch, which is correct. But the inverse — if `az_train` benchmark legitimately scored 0.0 (engine error returning all losses) and `bm_tb` is also 0.0 — silently gates on `az_train` at 0%, which then fails the floor (good) unless `baseline_reset` is set (then force-promoted at 0%). Low-probability but the 0.0 sentinel overloads "missing" and "genuinely lost everything."

**Fix:** Distinguish "not benchmarked" (None) from "benchmarked at 0%".

### M-3. `BENCHMARK_MATCHES // 2` halving — not a bug, but couples the two estimates' N
**File:** `run_iteration.py:217`

`half_bm = 400 // 2 = 200`. Each candidate gets 200 games, not 400; the gate number `new_bm` thus has SD ≈ ±2.5% as noted, not the ±1.8% a full 400 would give. Combined with C-2's max-of-two, the effective precision feeding the floor/drop gates is worse than the constants imply. Documented here so it isn't mistaken for a 400-game benchmark.

---

## LOW / Non-issues

- **`evaluate.py` `avg_score` divides by `matches` not `len(results)`** (`:43-44`): if some matches failed silently the average is diluted. evaluate.py isn't on the gating path, so low impact.
- **`cpp_runner._simulate_parallel` seeds** (`:253`): `random.randint(0, 2**31-1)` per task from the parent RNG — independent and reproducible enough; workers don't reseed, and each task carries its own seed to the C++ engine. **No correlated-RNG bug here** (good — the engine seed is per-task, not per-worker).
- **`_pool_init` sys.path injection** is consistent between `run_iteration` and `cpp_runner`; no import-path divergence found.
- **Draw scoring in `cli_runner`/`cpp_runner` aggregation** (`home_wins/away_wins/draws`): consistent three-way classification, no double-count.
- **`_git_push` reads weights before reset:** confirmed correct; the in-memory read + re-apply ordering is sound. The remaining risk is the push-failure chain (H-3), not the reset itself.

---

## Priority for next run
1. **C-1** (silent baseline zeroing on meta read error) — smallest fix, biggest gate-integrity win.
2. **C-3** (promote-on-truncated-pool) — directly causes wrong promotions.
3. **C-2** (max-of-two benchmark bias) — biases every gate decision upward.
4. **H-2 / M-1** (atomic writes) — prevents the corrupt-meta that feeds C-1.
