# Code review — _git_push meta fix + best-model restore (2026-06-12)

**Rozsah:** `539b938..d82160d` (commity `dba7edc` restore iter3 87 %, `d82160d` fix `_git_push` meta).
**Efekt:** high, recall-biased.

## Verdikt: žádné correctness nálezy ✅

Diff je 6 řádků Pythonu + obnovený `weights_best.json` blob. Ověřeno:

1. **Jediný caller** `_git_push` (`run_iteration.py:324`) — nový param `all_time_best_bm` předán; žádný jiný call-site.
2. **`all_time_best_bm` vždy definovaný** před voláním — nastaven ve všech inicializačních větvích (ř. 157, 163, 170, 175, 181), včetně `except` a first-run.
3. **Promote větev** `_git_push` zrcadlí promote blok (ř. 311-314): `max(all_time_best_bm, new_bm)`, `tv=TV`. Konzistentní.
4. **Reject větev** zapisuje `frozen_bm` + zachované `all_time_best_bm` + `tv=TV`; obě hodnoty se čtou z existujícího meta (ř. 156-157) → idempotentní, věrné. Reject + `baseline_reset` se vylučují (reset vynutí `promote=True`), takže žádná kolize.

## Dopad na gating (ověřeno logikou)
Po pushnutí meta = `{bm:0.87, tv:1200, all_time_best:0.87}`. Příští běh: `meta_tv==TV` → `baseline_reset=False` → reálný gating obnoven (BM_FLOOR 0.77, drop-limit vs all_time_best 0.87, chess anti-regression). Bug z běhu 2026-06-11 (force-promote každou iteraci) je odstraněn.

## Nevyřešeno (mimo rozsah tohoto diffu, viz project_bloodbowl)
- Chess gate šum (~200 efektivních her, ±2.5 % SD) — ANTI_REGRESSION=0.51 uvnitř CI. Doporučení: EMA + decisive-only nebo GATING_MATCHES 400→1000+.
- nil_nil ~43 % strukturální — per-player plán (70→492 features).
