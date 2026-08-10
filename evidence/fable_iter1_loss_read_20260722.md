# Fable read-only pass: iter1 gate-loss diagnostic batch (24 games, 2026-07-22)

Corpus: `diag_iter1_loss_mine_20260722_data/g0000–g0023.json.gz`, seeds 99720722–99720745. Result 5W-16D-3L (cand). All confirmed-in-data unless marked speculative.

## Confirmed observations

**1. All 16 draws are 0-0; scoring is a desert.** 8 TDs in 24 games (0.33/game), no game has >1 TD, and every TD lands on half-turn 3–7 (e.g. g0012 H2T3, g0009 H1T7) — never turns 8+. After the single score, the remaining ~12–25 turns produce nothing in every game.

**2. The draw mechanism is midfield carrier passivity, and the candidate is measurably worse at it than the frozen champion.**
- Ball is held in 69% of turn snapshots (532/768), yet the possessing side's carrier makes **zero MOVE events in 61% of cand possession turns (70/114) vs 46% for frozen (56/122)**.
- Split by score: **tied game — cand idle 64% (66/103) vs frozen 44% (40/90)** (nominal z≈2.7; turns cluster by game, so treat as a lead, not proof).
- Not explained by marking: with **no standing opponent adjacent**, cand still idles **55% (38/69) vs frozen 41% (35/86)**.
- Idle carriers sit at median **15 (cand) / 14 (frozen) squares from the endzone** — this is midfield stalling, not tactical EZ-stalling. When carriers do move, both advance fine (net +3.7 / +4.0 squares per moving turn).
- **Entire zero-event turns** (no actions at all): cand 26 vs frozen 16 of 384 own-turns each. Example: g0020 (seed 99720742) H1T3 — cand has all 11 players standing (state 0), its own carrier p11 holding the ball, and takes literally no action.

**3. The 3 losses are ordinary variance, not a shared mechanism:**
- **g0005** (seed 99720727): cand (orc) 6 turnovers vs 1; H2T1 cand p21 picks up, throws PASS-FAIL, **frozen p9 catches it** → frozen converts to TD at H2T4. A gifted interception.
- **g0008** (seed 99720730): H1T3 frozen knocks down cand carrier p7, p14 picks up and scores H1T4 (first-drive steal); then the ball lies loose at (10,4) from H2T6 to game end with both sides failing to recover.
- **g0009** (seed 99720731): clean frozen wood-elf passing drive — H1T6 p1 PASS→p8 CATCH, H1T7 →p4 CATCH+TOUCHDOWN. Cand never threatened.

Otherwise event aggregates are near-symmetric (blocks 377/385, dodges 99/106, turnovers 70/68), and this batch's decisive split (5W-3L) does not reproduce the 43.2% gate direction — expected at N=8 decisives.

## Speculative

The tied-state passivity gap is the only cand-vs-frozen asymmetry found and is the natural mechanism for the elevated 67% draw rate; whether it also explains the 600-game gate loss (draws burn decisive-share) is untestable from this sample. Note this stalling persists **with vf_blend=0.15 active**, so the old "vf_blend=0 dilutes scoring pull" story does not fully cover it.

## Verdict

One concrete lead worth attention: the candidate net is measurably more ball-passive than the frozen champion — its carrier stands still in 64% of tied-game possession turns (vs 44%), even when unmarked, typically ~15 squares from the endzone, including whole turns with zero actions (26 vs 16). The three losses themselves are unremarkable variance (one intercepted pass, one first-drive carrier knockdown, one clean opposing passing drive), but the passivity asymmetry is a specific, checkable hypothesis for both this batch's 67% draw rate and — speculatively — the gate's draw-heavy 43.2% result, and it survives the obvious confounds (marking, score state) at this sample size.
