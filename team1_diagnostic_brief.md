# Team 1 — Diagnostic Brief: "Why doesn't the policy head learn?"

**Date:** 2026-06-23
**Author:** Jan (+ Claude)
**Status:** evidence-gathering complete except iter3 gate (pending — see §4)

---

## 0. TL;DR

The neural **policy head does not learn**: training cross-entropy loss is flat at
~2.24 nats (≈ uniform over ~8.5 legal actions, `ln 8.5 ≈ 2.14`) across a full
multi-iteration run, with no downward trend. The AlphaZero-style gate keeps
**rejecting** new models, and the benchmark is **not improving** (stuck below the
frozen best of 89.0%).

We have run a battery of "ceiling" measurements. The consistent finding across
**every lever we tried** (more MCTS sims, value-in-search blend, sharper eval
config, informative priors) is that the MCTS visit-count distribution — which is
the **training target** for the policy head — stays **near-uniform** (normalized
entropy `H_norm ≈ 0.92–0.96`, top-1 mass ~23–28%). Nothing we tune concentrates
it.

**Our hypothesis (to be confirmed/refuted by you):** the root cause is upstream
of the policy head — the **value function does not discriminate between actions**,
so MCTS cannot concentrate visits, so the policy target is ~uniform, so the policy
head has nothing to fit.

**The ask:** localize the root cause **before** we add a large per-player feature
set (~492 features). We deliberately do NOT want to pile on per-player code until
we know where the signal is being lost — otherwise the codebase gets much harder
to reason about and the diagnosis gets muddier.

---

## 1. How to get the code (pull ONCE for the whole team)

The whole codebase is one git repo. **Pull it once and share within the team** —
please don't have every member clone/diff separately.

- Repo: `https://github.com/jansekera/bloodbowl.git`
- Branch: `main` — read against the **latest commit on `main`** (the one that adds
  this brief + the `evidence/` directory).

```
git clone https://github.com/jansekera/bloodbowl.git
cd bloodbowl   # you are on main; this brief is at its tip
```

Everything below references files by path so you read the **real code**, not a
lossy summary. If something important seems missing from this brief, assume the
code is the source of truth and tell us.

---

## 2. Code inventory (what to read)

### Policy head + its training target (the suspected blast radius)
- `engine/src/policy_network.cpp`, `engine/include/bb/policy_network.h`
  — the neural policy head.
- `engine/src/action_features.cpp`, `engine/include/bb/action_features.h`
  — per-action featurization. Phase 1 expanded this 15→23 dims (collision rate of
  "best action" dropped to ~0%). Loss stayed flat anyway — see §3.
- `engine/src/mcts.cpp`, `engine/src/macro_mcts.cpp`,
  `engine/include/bb/mcts.h`, `engine/include/bb/macro_mcts.h`
  — MCTS. Its **visit counts are the policy training target**.
- `python/blood_bowl/policy_trainer.py` — trains the policy head (imitation from
  MCTS visit distributions).

### Value function + state featurization (our prime suspect for the root cause)
- `engine/src/value_function.cpp`, `engine/include/bb/value_function.h`
- `engine/src/feature_extractor.cpp`, `engine/include/bb/feature_extractor.h`
  — state features feeding the value net.

### Training orchestration
- `python/blood_bowl/train_cli.py` — entry point / all hyperparameters.
- `python/blood_bowl/trainer.py`, `python/blood_bowl/training_loop.py`
- `run_iteration.py` — the outer self-play→benchmark→gate→push loop.

### Measurement scripts (these produced the evidence in §4; tracked in repo)
- `measure_ceiling1_entropy.py`, `measure_ceiling1b_priors.py`,
  `measure_ceiling1c_evalconfig.py`, `measure_ceiling3_mcts.py`,
  `measure_ceiling4_vfblend.py`, `measure_C_policy_strength.py`
- Engine support (commit `9f01091`): `simulate_game_logged` exposes
  `dirichlet_alpha` + `exploration_c` so configs can be measured directly.

### Exact training command used for the run in §3
```
python3 -m blood_bowl.train_cli --epochs=16 --games=40 --use-cpp \
  --opponent=learning --self-play --home-race=human \
  --away-race=orc,skaven,dwarf,wood-elf --mcts-iterations=100 --lr=0.0003 \
  --model=neural --hidden-size=64 --vf-blend=0.0 --vf-ramp-epochs=10 \
  --policy-lr=0.01 --policy-model=neural --policy-blend=0.0 \
  --imitation-epochs=16 --weights=weights_az_train.json --tv=1200 \
  --training-method=mc_shaped --epsilon-start=0.35 --epsilon-end=0.1 \
  --benchmark-interval=16 --benchmark-matches=400 --skip-greedy-benchmark \
  --timeout=300 --opponent-mix-ratio=0.5 --workers=12
```

---

## 3. History — what we've already tried and ruled out

The policy "not learning" is **not new — it has persisted across the entire
effort.** This is the chronology so you don't re-run dead ends:

**(H0) Original neural-policy run (Mar 9–12, 2026).** Neural policy was on for
~3 days on top of a *frozen linear* value function, then abandoned. The
abandonment was a pivot to value training, NOT a verdict that neural policy fails
— a fair test never actually ran.

**(H1) "Shared-trunk corruption" hypothesis — RULED OUT.** Value and policy are
saved as two *separate* networks (`save_combined_weights` in `policy_trainer.py`;
loaded independently by `value_function.cpp` / `policy_network.cpp`). Policy-lr
cannot corrupt value through a shared trunk. Dead end.

**(H2) Blocker 1 — policy reset to random every iteration — FIXED + verified.**
`run_iteration.py` copied the value-only `best` weights into the training file
each iteration; the loader's legacy branch then started the policy *from scratch*
every iteration, so nothing accumulated. Fixed via `_carry_over_policy` /
`_stash_policy` (commits `8da15f7`; plus hidden-size 32→64 fix `13799c2`).
Verified mechanically: policy head L2(in→out) = 0.33 (not reset), trainer persists
across all 16 epochs. **top1_agree stayed flat ~38% anyway.**

**(H3) Blocker 1b — `policy_blend = 0` for the whole run.** With blend 0 the
neural policy never enters MCTS → can't influence benchmark/gating → can't promote
→ `best` never gains a policy head. A closed loop where the policy *structurally
cannot* improve any measured metric. Left intentionally unfixed (separate step)
until we first confirm imitation learning accumulates. (The run in §4 still has
`--policy-blend=0.0`.)

**(H4) Blocker 2 — coarse action featurization — FIXED + verified, did NOT help.**
Offline forensics on 4110 logged decisions found the smoking gun: in **49.7% of
decisions the best action had an IDENTICAL 15-dim action vector as a worse
action** (only 59.8% of action vectors were unique). The 15 features encoded move
*category* but not *identity* (which player, target x/y). → **Phase 1** expanded
action features 15→23 with move identity (commit `f1cadb4`); collisions dropped
87.5%→0%, unique-vector ratio 0.36→1.00. A short (16 games/epoch) test *appeared*
to show top1 climbing 23→39%. **The full run (40 games/epoch) revealed that climb
was a warm-up artifact** — policy_loss is perfectly flat ~2.24 over all 16 epochs
and top1_agree plateaus ~37.6%, the *same* plateau as the old 15-dim features.
**Conclusion: removing collisions was necessary but did NOT raise the achievable
ceiling.**

**Net of the history:** trainer math is correct, persistence works, action
collisions are gone — and the loss is *still* flat. That is what drove us to the
ceiling measurements (§5): the binding constraint is that the **imitation target
itself is near-uniform**, not anything in the policy training path.

### Two things that are NOT the policy's fault (please don't chase these)
- **Value-drift is a separate problem.** The value benchmark drops ~89%→82–86%
  each iteration (value retrains from `best` via `mc_shaped` and drifts down).
  This is what drives the gate REJECTs — a value-training/gating issue, distinct
  from the policy-learning question.
- **`top1_agree` is probably the wrong metric.** Many decisions are genuinely
  *equivalent* moves. Moving a player forward is broadly correct in most
  positions, and for **interchangeable linemen** the candidate moves are often the
  *literally identical* action (same piece type, same kind of forward move) — so
  several distinct "best" moves collapse onto the same featurized action and the
  same correctness. (Cf. every corner of a cage: "move forward" is equally
  correct.) A flat/multi-modal target there is *correct*, and top-1 is unhittable
  in principle. The target still separates a good cluster from a bad one (top-5/10
  captures ~70% vs ~50% uniform). The right metric is good-vs-bad separation
  (cluster recall / KL) and **real playing strength with the policy as a prior
  (`blend>0`)** — not argmax agreement.

---

## 4. Evidence A — the live training run (`--loop 4`, 2026-06-22→23)

Full log: `evidence/training_full.log` (committed snapshot; the live `training.log`
is gitignored). A grep of just the loss/gate lines is in
`evidence/training_run_loss_and_gates.txt`. Key signals:

### Policy head is flat across the whole run
Per-epoch `policy_loss` and `top1_agree` (policy's argmax matching MCTS argmax):

| iteration | policy_loss (range over 16 epochs) | top1_agree |
|---|---|---|
| 1 | 2.21 – 2.26 | 37–40% |
| 2 | 2.20 – 2.28 | 37–42% |
| 3 (training done, gate pending) | 2.21 – 2.27 | 35–41% |

No downward trend in any iteration. `ln(8.5) ≈ 2.14` (uniform over the mean number
of legal actions), so loss ~2.24 means the head sits **at roughly uniform**.
NOTE: an earlier short Phase-1 test appeared to show top1 climbing 23→39%; the full
run revealed that was a **warm-up artifact**, not learning.

### Value / benchmark not improving, gate rejecting
- iter1: benchmark new=**85.0%**, head-to-head vs frozen 49.4% decisive → **REJECTED** (threshold 58%)
- iter2: benchmark new=**87.5%**, head-to-head vs frozen 42.9% decisive → **REJECTED** (threshold 56%)
- all-time best benchmark stuck at **89.0%**; new models never beat the frozen one
  (head-to-head is mostly draws: ~444–446 of 600 games end 0-0).
- iter3: benchmark new=**85.5%**, head-to-head vs frozen 44.0% decisive → **REJECTED** (threshold 57.8%). Same flat picture; the run was stopped here (iter4 not run).

---

## 5. Evidence B — ceiling measurements (the target is near-uniform under every lever)

All raw outputs are committed under `evidence/` (copied from `/tmp/*.out`). Metrics:
`H` = entropy of MCTS visit distribution (nats); `H_norm` = entropy / ln(n_actions),
so 1.0 = perfectly uniform; `top1` = visit mass on the most-visited action;
`near%` = fraction of decisions with a near-tie at the top.

### Strop 3 — MCTS simulation budget (does more search sharpen the target?)
```
  sims   H(nats)   H_norm    top1    top3  near-tie%
   100     2.141    0.961   0.234   0.509       37.0
   400     2.088    0.949   0.250   0.523       36.6
   800     2.011    0.930   0.280   0.557       32.5
```
→ More sims sharpen the target only marginally; even at 800 sims it's still
`H_norm 0.93` (near-uniform). Undersimulation is at most a minor contributor.

### Strop 4 — value-in-search blend at MCTS=400 (does the value net sharpen search?)
```
 vfBlend   H(nats)   H_norm    top1    top3  near-tie%
     0.0     2.111    0.954   0.245   0.517       33.9
     0.5     2.090    0.947   0.247   0.534       43.3
     1.0     1.988    0.932   0.266   0.574       45.0
```
→ Turning value fully on barely sharpens the target (`0.954 → 0.932`). If value
discriminated strongly, we'd expect a much sharper distribution at vfBlend=1.0.

### Strop 1b — priors: uniform vs heuristic (do informative priors sharpen it?)
```
                config   H(nats)   H_norm    top1    top3   near%
  BEZ policy (uniform)     2.055    0.958   0.249   0.537    36.6
  S policy (heuristic)     2.034    0.944   0.262   0.563    38.3
```
→ Priors make almost no difference. (This also means earlier ceiling runs using
uniform priors were not badly biased.)

### Strop 1c — training config vs evaluation config
```
              config   H(nats)   H_norm    top1    top3   near%
  train  (D0.3,C2.0)     2.028    0.938   0.265   0.566    35.4
  no-dir (D0.0,C2.0)     2.107    0.963   0.223   0.489    47.8
  low-C  (D0.3,C1.0)     2.009    0.921   0.280   0.575    40.3
  eval   (D0.0,C1.0)     1.968    0.935   0.260   0.542    56.8
```
→ **Important caveat:** the script's auto-printed interpretation claims "eval is
much sharper than train → flatness is exploration NOISE". The actual numbers
**contradict** that: train `top1 0.265 / H_norm 0.938` vs eval `top1 0.260 /
H_norm 0.935` are essentially identical. By the script's own decision rule
("eval ≈ train → not noise → value/search genuinely doesn't discriminate"), this
points to a **fundamental** flat target, not a noise artifact. Please sanity-check
this yourselves — it's the crux.

### C_short — policy as a prior: does a higher blend win more games?
```
 blend    W    D    L    win%   scoreΔ
   0.0    3    9    0   62.5%    +0.33
   0.3    3    9    0   62.5%    +0.25
```
→ Increasing the policy blend does not increase win rate → the learned policy adds
no playing strength as a prior. (Small sample; treat as directional.)

---

## 6. Our working hypothesis (confirm or refute)

A single upstream cause is consistent with all of the above:

> **The value function does not discriminate between candidate actions/positions.**
> Most positions evaluate to ~the same value, so MCTS visit counts can't
> concentrate (target stays ~uniform), so the policy head's imitation target
> carries no signal (loss flat at ~uniform), so new models don't out-play the
> frozen one (head-to-head ~all draws), so the gate rejects.

If true, **per-player features won't fix it** until the value head can actually
separate good from bad — adding 492 inputs to a head that can't discriminate just
adds noise and code surface.

**Why per-player is still a candidate (mechanistic, for slice (b)).** State
features today are **70 aggregates** (counts/averages + tactical indicators in
`feature_extractor.cpp`) with **no per-player positional slot**. Two positions
that differ by a single move produce nearly identical 70 features → the value
literally cannot see the difference. So the flat target is a **mix**: (a) moves
that are genuinely equivalent (per-player won't help — flatness is correct there)
+ (b) moves that really differ but are invisible in 70 aggregates (per-player
*would* help). That's the mechanistic case for per-player as a targeted fix for
slice (b) — but it only *gives* the value information it must then *learn to use*
(70→492 ≈ 7×). Hence: confirm the value can discriminate at all **before** we
build it.

### Suggested lines of investigation (not prescriptive)
1. **Value discrimination:** for a batch of states, what is the spread of
   `value(s)` and of `value` across the legal successor states? Is it collapsed
   near a constant (e.g. ~0.5)? Histogram it.
2. **Value training health:** is the value loss actually decreasing? Any sign of
   collapse/saturation (`mean_abs_vf` hovers ~0.55–0.65 in the logs — is that
   real signal or saturation)?
3. **Is the game just drawish — and does the reward make scoring learnable?**
   0-0 dominates self-play and head-to-head. Escaping 0-0 requires a **risky,
   multi-step sequence**: pick the ball up (a dice roll that can fail and turn the
   ball over) and then carry it all the way into the TD zone. The question is
   whether the reward/horizon (`--tv=1200`, `mc_shaped`, gamma=0.99) actually
   reinforces that pickup→carry→score path — or whether the value net mostly sees
   indistinguishable 0-0 trajectories with nothing to separate. (This is framing
   for *why* value may be flat, not a prescription for how to play.)
4. **Policy path sanity:** with a deliberately sharp synthetic target, does
   `policy_trainer.py` + `policy_network.cpp` actually fit it? (Rules out a plain
   bug in the policy training path vs. a flat-target problem.)
5. **Reconcile Strop 1c** (§5) — is the flatness noise or fundamental?

---

## 7. Constraints / what we want back

- **Do NOT build the per-player feature set yet.** First localize where the signal
  is lost (value? search? policy path? reward/game structure?).
- Keep changes minimal and isolated — we implement incrementally and verify each
  change on its own before the next.
- **Deliverable:** a root-cause call (with evidence) on *where* the learning
  signal dies, and the smallest experiment that would confirm it. English is fine
  — we'll translate on our side as needed.

---

## 8. Appendix — pointers
- Full run log snapshot: `evidence/training_full.log` (+ loss/gate grep in
  `evidence/training_run_loss_and_gates.txt`)
- Ceiling raw outputs: `evidence/` (strop1c, strop1b, C_policy_strength_short,
  strop4_vfblend, strop3_mcts_sims)
- Prior internal notes (Czech, optional context): `team_neural_policy_brief.md`,
  `project_neural_policy_rootcause.md` (in repo root / our memory).
