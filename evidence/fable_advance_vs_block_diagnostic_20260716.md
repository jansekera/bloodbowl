# Why BLOCK beat ADVANCE at the stalled-carrier decision point (2026-07-16, Fable 5)

**Question being answered:** in `diag_perplayer_grounding_data/main/g0001.json.gz`
`turns[23]` (H2, home T4, orc vs skaven, home leading 1-0) the AI's ball carrier —
Orc Thrower id3 sitting at (0,4), literally on his own goal line with 5 team-turns
left — chose BLOCK (id3 → skaven id21) over ADVANCE, and stayed stalled for 3 turns.
ADVANCE generation, `carrierStallAwareSteps()`, and the greedy-rank table were already
ruled out; the leaf heuristic has several pro-advance terms that "should" have fired.
This pass reconstructs the exact state and runs the **real engine code** on both
alternatives to find out what actually drives the decision.

## Verdict (short form)

BLOCK winning here is **not one bug but three converging mechanisms**, all now measured:

1. **Risk pricing (dominant at this exact state):** the carrier was marked by a
   Gutter Runner, so ADVANCE opens with an AG3 dodge (4+, `helpers.cpp:41`) —
   measured 23.3% turnover probability even with a team reroll. The leaf heuristic
   prices the entire successful 5-square advance at only **+0.048** over standing
   still, while a failed dodge costs **-0.32**. Break-even failure probability is
   **8.7%** — no unskilled dodge can ever clear that bar. Under the current term
   weights the search is *correctly maximizing* its (mispriced) objective.
2. **Free-block EV as an always-available alternative:** a risk-free 2-dice block
   with a Block-skilled attacker is worth **+0.017..+0.030** at the leaf (knockdown →
   `playerDiff*0.03`), i.e. **one safe block ≈ the leaf value of 5 squares of carrier
   progress**. The landscape is so flat that "hit something" ties or beats "advance"
   every single turn.
3. **Prior-floor asymmetry + visit starvation:** `expand()` floors BLOCK/CAGE priors
   at 0.12 pre-renorm (`macro_mcts.cpp:348-364`) but ADVANCE gets **no floor** unless
   trailing by 2+ (`:342-344`) — at this root: BLOCK 0.093 vs ADVANCE 0.037 (2.5x).
   With 100 iterations and C=1.0 this decides ties: even in a variant state where
   ADVANCE has the **best** one-ply Q of all 20 macros and zero risk, the full search
   still picks it only **0.8%** of the time.

The "cage advance" and "stall pacing" bonuses **do fire** — but the pacing term fires
at its **maximum for standing on the own goal line** (see Finding 4), and the cage
term is nearly flat by construction in the own half. Neither is strong enough to
matter. The trained VF plays no role (`vf_blend=0.0` in this replay batch). The 1-0
lead also plays no role in the branch comparison (score term is constant across
same-turn branches; the leading-cap in priors only touches SCORE-family macros,
none of which exist at this root).

## Methodology

New standalone harness `diag_advance_vs_block_harness.cpp` (repo root; build/run
lines in its header), linked against `engine/build/libbb_engine.so` — **no engine
source touched**. It invokes the real `getAvailableMacros`, the real
`MacroMCTSSearch::expand` (for priors), the real `greedyExpandMacro` (real dice),
the real `MacroMCTSSearch::simulate` leaf, and the real full
`MacroMCTSSearch::search` (`#define private public` before including
`macro_mcts.h` — compile-time only, calls land in the compiled .so code).

- **State reconstruction:** exact `turns[23]` snapshot (all 21 on-pitch players +
  id20 KO), stats/skills via `getDevelopedRoster("orc"/"skaven", 1200)` +
  `setupHalf()` (same id→slot mapping the grounding script verified at runtime),
  H2, home/away turnNumber 4, score 1-0, ball held by id3 at (0,4), fresh-turn
  flags (snapshot is a turn boundary). Sanity: id3 = MA5 ST3 AG3 Block+SureHands+
  Pass; id21 = MA9 ST2 AG4 Dodge (Gutter Runner) — matches roster tables.
- **Config = exact replay production config** (`diag_perplayer_grounding.py:46-49,
  165-171` → `bb_module.cpp:444-459`): iters=100, C=1.0, vfBlend=0, dirichlet=0,
  nRollouts=1, leafLookahead=false, `weights_policy.json` loaded with
  policyBlend=0.0 — policy net never evaluated, but its non-null pointer **enables
  the heuristic prior floors** in `expand()` (`macro_mcts.cpp:292`).
- **Leaf decomposition:** a term-by-term mirror of `simulate()`, asserted equal to
  the real `simulate()` to 1e-9 on every state it was used on (all MATCH).
- **Known reconstruction unknowns:** reroll counts (tested 0 and 2 — same story),
  kicking team/weather (unused by these code paths), exact RNG stream of the
  original in-game search (not reproducible — the policy object persists across a
  game; replaced by a 400-seed distribution).

Full output: `evidence/fable_advance_vs_block_harness_20260716.out`; all numbers
below are from that run.

## The state

Home (orc, active, leading 1-0, T4 → turnsLeft=5): carrier id3 (0,4); nearest
teammate id9 (4,3) — the **only** teammate within 4 squares; main scrum at x=6-11;
id5 prone. Away (skaven): id21 (0,3) **adjacent to the carrier** (it dodged the
length of the pitch the previous turn specifically to mark him); 8 more at x=11-16
(id12, id13 prone; id20 KO). Orc endzone: x=25, i.e. dist=25.

## Findings

### 1. The real decision is reproduced, robustly — it's systematic, not noise

Full production search from the reconstructed state, 400 seeds:

| macro | chosen | avg visit share | root prior |
|---|---|---|---|
| **BLOCK p3→21** (what the game did) | **72.5%** | 14.0% | 0.093 |
| BLOCK p6→18 | 22.5% | 12.9% | 0.093 |
| BLOCK p1→18 | 4.0% | 10.6% | 0.093 |
| CAGE | 0.8% | 10.5% | 0.093 |
| **ADVANCE p3** | **0.2%** | **3.6%** | 0.037 |

(21 root macros total; the rest — 7 PASS_ACTION, BLITZ t21, FOUL, 6 REPOSITION,
END_TURN — never chosen.) Rerolls=0 variant: same picture (BLOCK p3→21 63%,
ADVANCE 0/200). Note ADVANCE's visit share (3.6%) ≈ its prior (3.7%): it gets
explored at its floorless prior rate and never accumulates evidence.

### 2. One-ply expected values: BLOCK genuinely maximizes the current leaf heuristic

`greedyExpandMacro` + real `simulate()`, K=3000 per macro (root leaf = +0.792):

| macro | mean Q | p(turnover) | Q if no TO | Q if TO |
|---|---|---|---|---|
| BLOCK p3→21 | **+0.809** | 0.001 | +0.809 | +0.470 |
| BLOCK p6→18 / p1→18 | +0.810 | 0.013 | +0.810 | +0.762 |
| END_TURN / REPOSITION | +0.776..0.792 | 0 | — | — |
| CAGE | +0.772 | 0 | — | — |
| **ADVANCE p3** | **+0.756** | **0.233** | **+0.840** | **+0.481** |
| PASS_ACTION (7 targets) | +0.578..0.635 | 0.61-0.83 | +0.82..0.90 | ~+0.51 |

The successful advance **is** the best non-scoring outcome on the board (+0.840,
highest Q|noTO of any safe macro) — the pro-advance terms do work — but it hangs
behind a 4+ dodge out of id21's tackle zone. p(fail) ≈ 25% with one team reroll,
measured 23.3%. Failure = carrier prone + ball loose at his own goal line (leaf
+0.470). Expected value: 0.767×0.840 + 0.233×0.481 = **0.756 < 0.809**. For
ADVANCE to beat BLOCK the dodge would have to fail ≤ **8.7%** of the time — even a
3+ dodge with reroll (11%) wouldn't qualify. Under these term magnitudes a marked
carrier should *never* dodge forward, anywhere on the pitch.

Two aggravators specific to this state:
- BLOCK p3→21 is exceptionally safe: 2-dice (ST3 vs ST2) + attacker has Block →
  p(TO) ≈ 0.1%. Its EV gain (+0.017) comes from the knockdown chance via
  `playerDiff*0.03` — there is no explicit "unmark my carrier" term, but pushing
  id21 out of contact is also what the block physically does.
- The BLITZ t21 macro expands to *the carrier himself* blocking id21
  (`expandBlitz` picks the adjacent 2-dice attacker), so even the blitz candidate
  is "carrier hits the marker", not "someone else frees the carrier".

Also note the trap the search cannot see at macro level: blocking with the carrier
**consumes the carrier's activation** (`hasActed`), so "push the marker away, then
advance" is not available in the same turn — and next turn a MA9 skaven can simply
re-mark him. One ST2 Gutter Runner pins the entire orc drive at ~zero cost. In the
real game home spent *three* activations (block, second block, foul) neutralizing
id21 that turn while the ball stood still.

### 3. Counterfactual: even an UNMARKED carrier doesn't advance — priors + flat landscape

Variant with id21 moved away (carrier free, ADVANCE has **zero** dodge risk):

- One-ply Q: **ADVANCE = +0.812 is the single best macro** (BLOCK p6→18 +0.810,
  END_TURN +0.792, p(TO)=0.000).
- Full search, 400 seeds: **BLOCK p6→18 chosen 69.5%, CAGE 16.8%, BLOCK p1→18
  12.8%, ADVANCE 0.8%** (visit share 7.7% vs prior 4.1%).

With Q differences of ±0.002 on a +0.79 baseline, PUCT visit allocation is decided
by the priors: BLOCK/CAGE floored to 0.099, ADVANCE unfloored at 0.041
(`macro_mcts.cpp:342-391`; floors renormalized over n=20-21 candidates). 100
iterations spread over ~20 children never give ADVANCE enough visits to become
most-visited even when its Q is the highest. This is the same structural effect the
2026-07-03 CAGE-floor fix corrected for CAGE-vs-BLOCK — ADVANCE never got the
equivalent treatment (its only floor is `trailing2plus`, and home was *leading*).

**Validated against the real game:** on home T5 (`turns[25]`) the carrier stood
*unmarked* (id21 had been blocked down and fouled) — and the AI still didn't
advance him; the carrier did nothing at all that turn, exactly as this variant
predicts. The stall is therefore **not fully explained by the dodge risk** —
remove the risk and the prior starvation + flat landscape still hold the carrier
in place.

### 4. Audit of the pro-advance leaf terms at this state (all real, decomposed, asserted vs `simulate()`)

Root state decomposition (perspective HOME): `scoreDiff*0.5`=+0.500,
possession=+0.100, `playerDiff*0.03`=+0.060, **cageAdvance=+0.032**,
**stallPacing=+0.100**, proximity=+0.000 → total +0.792.

- **Stall pacing (`macro_mcts.cpp:543-549`) fires at its MAXIMUM for standing on
  the own goal line.** idealDist = turnsLeft×MA = 5×5 = 25 = dist → pacing = 1.0 →
  +0.100. Every square advanced is "ahead of schedule" and *reduces* the term by
  0.004/square. It does not invert the forward gradient (proximity +0.010/sq keeps
  the total positive) but it cancels 40% of it exactly in the deep-own-half regime
  where the carrier most needs to move. Working as coded; the design itself is a
  stall term for any carrier with turnsLeft×MA ≥ dist.
- **Cage advance (`macro_mcts.cpp:513-532`) fires** (cageN=1: id9) but is nearly
  flat by construction: it averages *escorts'* endzone proximity, which in the own
  half is ~0.13-0.25 → term ~0.03-0.07 no matter what moves. It slightly *favors*
  ADVANCE (carrier moving to x=5 puts 4 teammates inside the radius: +0.018) — too
  weak to matter, and it cannot ever exceed 0.20.
- **Net forward gradient** (hypothetical carrier x, marker removed, everything else
  fixed): total leaf +0.822 at x=0 → +0.870 at x=5 → +0.947 at x=12. That is
  ~+0.010/square — less than one third of a single knockdown's `playerDiff` value,
  and less than the +0.017 EV of one free block for 5 squares of movement.
- Safe-walk-in (+0.4), GFI (+0.2), urgency, one-turn-TD terms: all correctly zero
  here (dist=25, turnsLeft=5) — they only kick in near the endzone / late turns,
  so they can't help *start* a drive from deep.

### 5. What this adds up to (mechanism of the 3-turn stall)

Each home turn the search faces the same menu: a risk-free block worth +0.017 EV,
CAGE/REPOSITION worth ~0, and ADVANCE worth +0.02 gross (unmarked) or -0.05 net
(marked), priced at a 2.5x prior disadvantage. So it hits things, turn after turn,
and the ball moves only when a SCORE-family or desperation macro finally becomes
generatable — here the T6 PASS_ACTION (which per the MC table carried a 60-80%
turnover risk and simply got lucky). The drive never scored; home won 1-0 on the
earlier TD.

Relations to earlier findings: this is the measured root cause behind the
"search-side #2" cage-advance patch's limited effect (2026-06-25 — the signal it
added is ~flat where it's needed); it's a second instance of the prior-floor
imbalance mechanism from the 2026-07-03 CAGE fix; and it directly supports the
per-player brief's `carrier_blitzable` risk-weighted refinement — the engine
currently prices *any* required dodge above *any* amount of same-turn progress.

## Fix (b) SHIPPED + VALIDATED (2026-07-16, ~15:44 CEST)

Implemented lever (b): `engine/src/macro_mcts.cpp`, `MacroType::ADVANCE` prior
floor raised from floorless-unless-trailing-2+ to an unconditional 0.12
(0.15 when trailing 2+), mirroring the 2026-07-03 CAGE-floor fix exactly.
420/420 C++ tests pass (no dedicated unit test for the floor value itself --
not cleanly unit-testable in isolation, same as the CAGE precedent; verified
via full suite + paired-seed A/B, matching house practice for this class of
MCTS-prior change).

**Paired-seed A/B (N=300, same house methodology):**

```
[baseline (pre-fix)]  51W 190D 59L  draws 63.3%  TD/game 0.43  decisive share 46.4% (n=110)
[candidate (post-fix)] 83W 141D 76L  draws 47.0%  TD/game 0.73  decisive share 52.2% (n=159)

PAIRED A/B (draw)      post-fix 47.0% vs pre-fix 63.3%  delta -16.3pp  SE 4.1pp
                       95% CI [-24.5, -8.2]pp  McNemar z=-3.84  p=0.0002
                       VERDICT: CONFIRMED -- post-fix draw rate LOWER

PAIRED A/B (home_win)  post-fix 27.7% vs pre-fix 17.0%  delta +10.7pp  SE 3.3pp
                       95% CI [+4.2, +17.2]pp  McNemar z=+3.17  p=0.0020
                       VERDICT: CONFIRMED -- post-fix home_win rate HIGHER

PAIRED TD/game         post-fix 0.73 vs pre-fix 0.43  delta +0.297  SE 0.051
                       95% CI [+0.197, +0.396]

watchdog skips: pre-fix 0/300, post-fix 0/300
```

**Unlike the two throw-in fixes, this is a genuine, statistically confirmed
behavior shift, exactly in the direction the diagnostic predicted:** TD/game
up 70% (0.43->0.73), draw rate down 16.3pp, no crash/watchdog regression.
This is strong, independent confirmation of the three-mechanism diagnosis
above -- specifically that mechanism 3 (prior-floor starvation) was a real,
not just theoretical, drag on scoring behavior across the whole 150-game-
equivalent population, not only the one hand-picked stalled-carrier state.

Scripts: `diag_advance_floor_fix_ab_20260716.py`, `run_advfloor_ab_20260716.sh`;
log: `diag_advfloor_ab_20260716.log`; arms: `arm_advfloor_base_20260716.json` /
`arm_advfloor_fix_20260716.json`. Pushed to `origin/main` per house push policy
(tests + validation clean -> push, no manual review gate).

**Remaining levers (a) risk-aware carrier-progress pricing and (c) reshaped
stallPacing are NOT yet implemented** -- with (b) alone already producing a
large, confirmed effect, the next step should be deciding whether (a)/(c) are
still worth pursuing (diminishing returns vs. the win already banked) rather
than assuming all three are needed.

## Confidence / limits

- **High confidence:** root candidate set + priors, one-ply EVs, term decomposition
  (mirror asserted == real `simulate()` on every state used), 400-seed choice
  distributions, and the 8.7% break-even math — all from the real compiled engine
  on the reconstructed state; the reconstruction reproduces the game's actual
  choice at 72.5% and its T5 behavior in the unmarked variant.
- **Medium confidence:** attribution *between* mechanism 2 and 3 in the unmarked
  case (flat landscape vs prior starvation) — both push the same way; I did not
  run a floor-equalized ablation (would require an engine change, out of scope for
  this read-only pass).
- **Not reproduced:** the original game's exact 100-iteration search (RNG stream
  not recoverable); the distributional replacement is stronger evidence anyway.
- **Unknowns that don't move the result:** true reroll counts (0 vs 2 tested,
  same ordering), away-side deeper-tree priors (away turnNumber assumed 4).
- No fix is proposed or applied here (confirm+measure only, per convention). The
  three obvious levers, in measured-impact order: dodge-risk-aware pricing of
  carrier progress (or a real "carrier is marked" term), an ADVANCE prior floor
  symmetric with BLOCK/CAGE's 0.12, and reshaping stallPacing so it stops paying
  +0.10 for standing on the own goal line ("on schedule" should not be a reward
  ceiling at maximum distance).

## Files

- Harness (new, read-only diagnostic): `diag_advance_vs_block_harness.cpp`
- Raw harness output: `evidence/fable_advance_vs_block_harness_20260716.out`
- Replay: `diag_perplayer_grounding_data/main/g0001.json.gz` `turns[23]` (also 25, 27)
- Engine refs: `engine/src/macro_mcts.cpp:292-414` (priors), `:483-693` (leaf; pacing
  `:543-549`, cage `:513-532`), `engine/src/macro_actions.cpp:272-279` (ADVANCE gen),
  `:415-432` (BLOCK gen), `:846-887` (stall-aware steps + expandAdvance),
  `engine/src/helpers.cpp:41` (dodge target), `engine/python/bb_module.cpp:444-459` +
  `diag_perplayer_grounding.py:46-49,165-171` (production config)
