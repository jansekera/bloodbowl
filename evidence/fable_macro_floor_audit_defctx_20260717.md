# Macro-floor audit: defensive context (2026-07-17)

Follow-up to `evidence/fable_advance_vs_block_diagnostic_20260716.md` and
today's `off`/`loose` screen (which found the REPOSITION bug). The original
harness's `screen` command explicitly skips `ctx=def`
(`if (ctx != "off" && ctx != "loose") continue;`), so the defensive context
was never checked for the same argmaxQ-vs-prior starvation pattern. This
pass extends the screen to `ctx=def` (1875 roots, K=25, corpus `main_postfix`).

## Result: clean. No new floor bug found.

| type | argmaxQ% | starved (prior<50%max) | read |
|---|---|---|---|
| BLITZ | 47.5% | 0 (0%) | clean — already has an unconditional onDef floor of 0.20 |
| END_TURN | 34.3% | 431 (67%) | **not a bug** — END_TURN is deliberately *capped* at 0.10 pre-renorm (`priors[i] > 0.10f && n > 2 -> 0.10f`), an anti-passivity design choice, not a missing floor. High argmax-but-suppressed rate here is the cap doing its job. |
| BLOCK | 17.7% | 0 (0%) | clean — already has an unconditional 0.12 floor |
| FOUL | 0.5% | 6 (67% of its rare argmax hits) | FOUL has no floor treatment anywhere in the code (falls through `default:`), but is the argmax candidate only 0.5% of the time in defense — absolute impact is tiny (6 roots out of 1875). Same low-priority pattern as FOUL's showing in `ctx=off`/`ctx=loose` today. Not worth a fix on its own. |
| REPOSITION | 0.1% | 1 (100% of its 1 argmax hit) | effectively clean — REPOSITION already gets its onDef floor (0.05) in this context (this predates today's fix, which only extended the floor to non-defensive contexts). Single example: margin +0.0464, prior 0.0568 vs maxprior 0.1591 -- small, isolated. |

## Verdict

The `ctx=def` gap in today's audit methodology is now closed. Unlike
`ctx=loose` (which found REPOSITION's real, unconditional-floor-missing bug)
and `ctx=off` (already fixed for ADVANCE on 2026-07-16), the defensive
context does not hide a comparable bug — every macro type that gets
starved either already has an adequate floor here (BLOCK, BLITZ,
REPOSITION) or is suppressed by deliberate design (END_TURN's cap) or is
too rare to matter (FOUL, 6 roots). No fix proposed from this pass.

Combined with today's `off`/`loose` results, the systematic macro-floor
audit across all three MCTS root contexts is now complete. REPOSITION
(loose-ball, unconditional floor missing) remains the one confirmed new
finding of the day at the single-state/screen level; its game-level
paired-A/B effect is still being resolved (N=150 inconclusive with a
concerning wrong-direction point estimate, N=400 rerun in progress as of
this writing).

## Methodology notes

- Harness: scratch-modified copy of `diag_macro_floor_audit_harness.cpp`
  with the context filter widened to include `"def"`, built against the
  unmodified production `engine/build/libbb_engine.so` (read-only
  diagnostic, no engine changes).
- Corpus: `scratch_macro_floor_audit/snaps.txt` (reused from today's
  earlier `off`/`loose` dump, `main_postfix` label).
- `screen 25` (K=25 samples/candidate, same reduced-from-default K=150
  used for today's other screens for time-budget reasons).
- Aggregation: `diag_macro_floor_audit_20260716.py agg_screen
  scratch_macro_floor_audit/screen_def_k25.out`.
