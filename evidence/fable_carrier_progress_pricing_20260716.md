# Carrier-progress pricing levers (a) + (c): design + harness measurement (2026-07-16, Fable 5)

**Task:** design and measure the two levers of the advance-vs-block diagnostic
(`evidence/fable_advance_vs_block_diagnostic_20260716.md`) that the shipped
ADVANCE-prior-floor fix (b) did NOT address:

- **(a)** dodge-risk-aware pricing of carrier progress / a real "carrier is
  marked" term (mechanism 1: break-even dodge-failure for advancing is 8.7%,
  so a marked carrier never advances anywhere), and
- **(c)** reshaping `stallPacing` so it stops paying its maximum (+0.10) for
  standing on the own goal line (mechanism 2 aggravator: it cancels ~40% of the
  forward-progress gradient exactly where advancing matters most).

**This is a DESIGN + MEASUREMENT pass, not a shipping pass.** No production
source was modified; no commit/push. All modified-term measurements ran on a
separate scratch build (`libbb_engine_lever.so`, env-gated terms) while the
production `engine/build` (in use by the live ~12h training run) was left
untouched. Baseline arm = today's already-shipped state **including** the
ADVANCE-floor fix.

---

## MEASUREMENT SECTIONS FILLED IN BELOW (placeholder while matrix runs)

---

## Confidence / limits

(to be completed)
