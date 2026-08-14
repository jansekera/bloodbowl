# Design: Kickoff Formation Reactivity + Ball-Security vs Hitting Resource Split

Date: 2026-07-23
Status: DESIGN ONLY — no production code changes, no training runs. For human review.
Authors: two-role design team (Formation-Reactivity Designer; Resource-Allocation Designer).
Repo grounding: all citations verified against current working tree at /home/jan/claude/bloodbowl.

These are two genuinely separate sub-designs. They do not share code; the only
touchpoint is noted in section 3.

---

# PART 1 — FORMATION-REACTIVITY DESIGNER
## Receiving-team kickoff setup that reacts to the kicking team's formation

### 1.1 Current mechanism (verified in code)

Setup for both teams happens in a single deterministic pass in
`setupHalfOrDrive()` — `engine/src/game_simulator.cpp:255-302` — called via
`setupHalf()`/`setupDrive()` (`game_simulator.cpp:306-314`) from the drive loop
(`game_simulator.cpp:414, 425, 438`), always BEFORE the kickoff itself
(`resolveKickoff`, `engine/src/kickoff_handler.cpp:200`, invoked at
`game_simulator.cpp:401`).

Current formation choice logic:

- **Kicking team** reacts to exactly one signal: the *receiving roster's* average
  MA. `classifyRosterSpeed()` (`engine/src/roster.cpp:471-491`; thresholds
  avgMA > 7.0 = FAST, <= 5.0 = SLOW) is evaluated at
  `game_simulator.cpp:268-271`, and the kicking template is picked at
  `game_simulator.cpp:273-279`: FAST receiver → `*_PRESSURE_FORMATION`
  (`game_simulator.cpp:63-85`), otherwise `*_DEFENSIVE_FORMATION`
  (`game_simulator.cpp:38-61`).
- **Receiving team** is unconditional: comment at `game_simulator.cpp:281`
  literally says "Receiving team always uses deep receiver formation", selection
  at `game_simulator.cpp:282-285`, template `*_DEEP_RECEIVER_FORMATION` at
  `game_simulator.cpp:87-109`.

This confirms the 2026-07-21 finding: **zero conditional logic reacting to the
kicking team's own setup.** The receiving side doesn't even see which of the two
kicking templates was chosen, although the pointer is fully determined a few
lines above its own placement.

Mechanics that constrain any new design:

- Templates are `constexpr FormationPos[11]` of `{dx from own LOS, absolute y}`
  (`game_simulator.cpp:17`). HOME LOS x=12, AWAY LOS x=13.
- `buildTeam()` (`game_simulator.cpp:161-244`) has a **slot-ordering
  convention**: specialists are placed from slot 10 downward
  (`game_simulator.cpp:185-211`), linemen fill slots 0..specSlot
  (`game_simulator.cpp:213-231`). So template index order is semantic: low
  indices = LOS fodder (linemen), high indices = backfield specialists. Any new
  template MUST order its positions accordingly or specialists end up on the LOS.
- The kicking team's slot-10 player is granted the Kick skill after placement
  (`game_simulator.cpp:290-297`) — unaffected by this design, but another reason
  slot ordering matters.
- Legacy `HOME_FORMATION`/`AWAY_FORMATION` (`game_simulator.cpp:20-36`) are
  never selected by the setup path (only defensive/pressure/deep-receiver at
  273-285) — effectively dead templates; the "template pool + selector" idiom
  proposed below is therefore already half-present.
- `state.receiverSpeed` is also consumed by kick-targeting
  (`kickoff_handler.cpp:219-226`, short kick vs FAST receivers) and
  `simpleKickoff` (`game_simulator.cpp:347`) — untouched by this design.

### 1.2 Key structural fact the design exploits

Because both placements happen inside one function and the kicking formation is
fully determined at `game_simulator.cpp:279` **before** `buildTeam()` runs for
either side (287-288), reactive receiving setup needs no new phase, no new
game-state plumbing, and no protocol change. It also matches tabletop rules,
where the kicking team must set up first and the receiver reacts.

Minimal hook: reorder the two `buildTeam()` calls at `game_simulator.cpp:287-288`
to kicker-first (they are currently HOME-then-AWAY unconditionally), then compute
signals **from the actually-placed kicking XI** (positions/stats/skills all live
in `state.players` at that point), then select the receiving template. Analyzing
placed players rather than the template pointer is deliberately more general: it
stays correct if kicking setup later becomes roster-dependent, randomized, or
human-controlled (webapp), and it automatically prices in specialist placement.

### 1.3 Signals to extract — `KickSetupSignals`

All O(11) point geometry over the placed kicking players; no pathfinding.

| Signal | Definition | What it detects |
|---|---|---|
| `losCount` | kicking players at dx==0 | LOS commitment (3 vs 4) |
| `boxCount` | players within dx<=2 of LOS | press/compact intent (PRESSURE has 10/11 in the box, DEFENSIVE 9/11 but with wide y-spread, legacy standard 8/11) |
| `deepCount` | players with dx>=4 | sweepers/safeties behind (DEFENSIVE 2, PRESSURE 1) |
| `leftCount`/`rightCount` | standing players with y<7 vs y>7 | lateral skew ("heavy one side") |
| `wideSpread` | max y − min y over the front two rows (dx<=1) | wide columns vs narrow wall |
| `stNearLOS` | sum of ST for players with dx<=1 | bash wall vs positional line |

Classifier (v1 thresholds, derived from the three templates the engine can
actually produce today, but written generically so human/webapp setups also map):

- `COMPACT_PRESS` if `boxCount >= 10` and `deepCount <= 1` (matches PRESSURE).
- `WIDE_COLUMNS` if `losCount == 3` and `wideSpread >= 6` and `deepCount >= 2`
  (matches DEFENSIVE).
- `SKEWED` overlay flag if `|leftCount − rightCount| >= 3` (cannot arise from
  today's symmetric templates, but arises after injuries with <11 players, from
  human setups, and future kicking-side variants — cheap future-proofing).
- else `BALANCED`.

### 1.4 Response formations

Three receiving responses (plus the existing default), all as new
`constexpr FormationPos[11]` tables in the existing idiom:

1. **vs COMPACT_PRESS → `DEEP_SAFE` template.** Threat model: a crowded LOS box
   plus the BLITZ kickoff-table event (`kickoff_handler.cpp:132-141` moves every
   standing kicking player 1 sq toward the LOS) puts pressure on shallow ball
   retrieval; PRESSURE also short-kicks vs FAST receivers
   (`kickoff_handler.cpp:219-226`). Response: keep only the minimum on the LOS
   (3), pull the second row to dx=-2/-3, put two retrievers at dx=-6 flanking
   center and the slot-10 specialist at dx=-7 center. Rationale: their compact
   box has at most 1 deep player, so conceding the LOS and receiving deep gives
   a safe pickup plus running room around the box's flanks.
2. **vs WIDE_COLUMNS → `OVERLOAD_LEFT` / `OVERLOAD_RIGHT`.** The 3-column
   defensive spread (`game_simulator.cpp:41-61`) defends each flank with ~3
   players + 1 safety. Response: concentrate 6 receiving players (LOS pair +
   second-row trio + one runner) into one half-pitch lane (y in 2..6 or 8..12),
   retrievers behind that lane, to create a local numbers advantage (~6v4) for
   the opening drive down that flank. Side selection: the side with fewer
   defenders (`leftCount` vs `rightCount`); tie → random or fixed. LEFT/RIGHT
   are y-mirrors — implement one table + a `mirrorY()` helper
   (y → 14−y) rather than four hand-written tables; HOME/AWAY already differ
   only in dx sign in the existing tables.
3. **SKEWED overlay:** if the kicker is heavy one side, overload the *weak*
   side regardless of the base class.
4. **BALANCED → current `DEEP_RECEIVER`** unchanged (regression anchor).

All new tables must respect the slot-ordering convention from §1.1 (backfield =
high slots) so `buildTeam()` keeps placing specialists deep.

### 1.5 Interface (concrete)

```cpp
// game_simulator.cpp (anonymous namespace, next to the templates)
struct KickSetupSignals {
    int losCount, boxCount, deepCount, leftCount, rightCount, wideSpread, stNearLOS;
};
KickSetupSignals analyzeKickingSetup(const GameState& state, TeamSide kicking);

enum class ReceiveFormation { DEEP_RECEIVER, DEEP_SAFE, OVERLOAD_LEFT, OVERLOAD_RIGHT };
ReceiveFormation chooseReceivingFormation(const KickSetupSignals& sig);

// setupHalfOrDrive() new flow (replacing game_simulator.cpp:282-288):
//   buildTeam(kickingSide, kickForm)            // kicker first — rules-accurate
//   sig  = analyzeKickingSetup(state, kicking)  // reads placed players
//   recv = chooseReceivingFormation(sig)
//   buildTeam(receivingSide, table(recv))
```

Data available at decision time: full placed kicking XI (positions, stats,
skills), receiving roster, plus persistent drive context — `setupDrive()`
preserves score and turnNumber (`game_simulator.cpp:250-254`), so a later v2
could condition on "trailing late → riskier overload". Deliberately **out of
v1** to keep the first change auditable.

### 1.6 Risks / interactions

- **Self-play distribution shift**: both sides share this code, so every
  training game's opening states change. Gate with paired-seed A/B (draw-rate
  noise floor is ±8–11pp at N=150 → decide at N>=400 paired), plus benchmark
  non-regression, per house rules (1 change/commit, gate on draw-rate).
- **QUICK_SNAP** (`kickoff_handler.cpp:121-130`) moves receivers 1 sq toward the
  LOS — deep templates give up a little of that event's value; acceptable.
- **HIGH_KICK / touchback / KOR** paths (`kickoff_handler.cpp:77-88, 252-304`)
  use closest-player logic and are formation-agnostic — no changes needed, but
  the DEEP_SAFE retriever placement should sit near the deep-kick landing zone
  (kickX 22/3 for non-FAST receivers, `kickoff_handler.cpp:219-226`) so
  touchback/closest-player logic naturally selects a retriever.
- **Under-11 players** (post-injury drives): `buildTeam` fills what it can;
  signals must divide by actual on-pitch count, not 11.

### 1.7 Validation plan (design-time, no training)

1. Static harness (same pattern as `diag_macro_floor_audit_harness.cpp` in repo
   root): set up all 3 kicking templates × both sides, assert the classifier
   maps DEFENSIVE→WIDE_COLUMNS, PRESSURE→COMPACT_PRESS, and that chosen
   receiving templates place 11 legal, non-overlapping, own-half positions with
   3+ on LOS.
2. Paired-seed A/B self-play, N>=400: metrics = TDs per receiving drive,
   turnovers in receiving turns 1–3, draw rate.
3. Forced-NEUTRAL arm (classifier always returns DEEP_RECEIVER) must reproduce
   current behavior bit-for-bit — cheap regression proof that the refactor
   (kicker-first ordering) is itself a no-op.

---

# PART 2 — RESOURCE-ALLOCATION DESIGNER
## Ball-security vs hitting: an explicit mode signal for MCTS priors

### 2.1 Current state (verified in code)

Priors are computed in `MacroMCTSSearch::expand()` —
`engine/src/macro_mcts.cpp:242-473`. Heuristic floors/caps per macro type live
in the switch at `macro_mcts.cpp:313-439`, renormalized at 440-446, blended with
the policy net at 448-461. Context available there today:
`turnsRemaining`/`scoreDiff`/`trailing2plus`/`leading` (`macro_mcts.cpp:296-300`)
and the single mode-like bit `onDef` (opponent holds the ball,
`macro_mcts.cpp:303-306`).

What's missing, concretely:

- On **offense** there is no distinction at all between "protect the carrier"
  and "go hit": BLOCK gets an unconditional 0.12 floor
  (`macro_mcts.cpp:368-370`) for *every* 2+-dice pair emitted by generation
  (`engine/src/macro_actions.cpp:415-429` — one candidate per favorable pair,
  ball-irrelevant or not), CAGE a flat 0.12 (`macro_mcts.cpp:371-385`), and
  offensive BLITZ has **no floor** (`macro_mcts.cpp:365-367` is onDef-gated —
  this is the already-flagged blitz-offense-floor gap, 2026-07-22).
- Generation encodes both modes *implicitly* but chooses by positional
  fall-through, not situation: REPOSITION offense = hunter/receiver/cage-screen
  (`macro_actions.cpp:592-631`), defense = cage-tag/intercept/safety/marker/
  endzone-guard/screen (`macro_actions.cpp:632-729`); BLITZ target scoring has
  defense-aware bonuses (`macro_actions.cpp:327-341`) but offense only
  +2 near-carrier / +5 carrier (`macro_actions.cpp:342-350`).
- Leaf eval (`macro_mcts.cpp:~545-751`) has security terms (cage-advance bonus
  578-590, bash-exposure penalty 679-697) and hitting-adjacent terms
  (opponent-carrier marking bonus 667-677, player-count diff +0.03/player
  720-729) all unconditionally on — no arbitration.

### 2.2 Core insight: mode ≠ macro type

Blitzing the marker off your own carrier IS ball security; a 2-dice block on a
lineman 8 squares from the ball is pure attrition. A scalar that just tilts
"BLOCK/BLITZ vs CAGE/REPOSITION" would mis-tax protective hits. The design
therefore has two axes:

**(a) Per-candidate purpose class** — computable in `expand()` from
`macro.targetId`/`macro.playerId` (`engine/include/bb/macro_actions.h:30-36`)
with distance checks only, no generation changes:

- `PROTECTIVE`: BLOCK/BLITZ whose target is within 2 squares of our own carrier
  (removes/threatens markers).
- `BALL_ATTACK`: target is the opposing carrier, or is adjacent to a loose ball.
- `ATTRITIONAL`: every other BLOCK/BLITZ/FOUL.

**(b) Per-node mode scalar** — which purpose class deserves prior mass right now.

### 2.3 The mode signal (v1: 3-state enum, computed once per node)

Computed next to `onDef` at `macro_mcts.cpp:303-306`; all inputs already exist
or are O(22):

- `carrierTZ` = tacklezones on own carrier (same call as leaf eval 672).
- `escorts` = own standing players within 2 sq of carrier.
- `carrierBlitzable` = reuse `carrierIsBlitzable()`
  (`macro_actions.cpp:853`) — currently static in macro_actions.cpp, needs a
  header export (mechanical).
- `numbersDiff` = standing-player differential (same loops as leaf eval
  720-729).
- `scoreDiff`, `turnsRemaining` (already at 296-298).

```
MODE_SECURE  if we hold the ball AND (carrierTZ >= 1 OR (carrierBlitzable AND escorts < 2))
             OR (leading AND turnsRemaining <= 3 AND we hold the ball)      // protect the win
MODE_STRIKE  if (we hold the ball AND carrierTZ == 0 AND escorts >= 2 AND numbersDiff >= +2)
             OR (onDef AND oppCarrierTZ >= 2)                               // finish the takeaway
MODE_NEUTRAL otherwise  →  bit-for-bit today's numbers (regression anchor)
```

### 2.4 v1 integration — priors only, floors only

Inside the existing switch (`macro_mcts.cpp:313-439`); floors are preferred over
caps so MCTS can still override on merit (Q), matching the house philosophy that
floors fix *starvation*, not *ranking* (see ADVANCE fix rationale at
`macro_mcts.cpp:347-364` and CAGE at 371-385):

| Macro / class | NEUTRAL (today) | SECURE | STRIKE |
|---|---|---|---|
| CAGE | 0.12 | **0.18** | 0.12 |
| BLOCK (PROTECTIVE) | 0.12 | 0.12 | 0.12 |
| BLOCK (ATTRITIONAL) | 0.12 | **0.06** | **0.16** |
| BLITZ offense (PROTECTIVE / BALL_ATTACK) | none | **0.12** | **0.12** |
| BLITZ offense (ATTRITIONAL) | none | none | **0.10** |
| ADVANCE | 0.12/0.15 | 0.12/0.15 (unchanged) | 0.12/0.15 |
| REPOSITION (onDef) | 0.08 | n/a (offense) | 0.08 |

Notes:

- The offense-BLITZ rows also *subsume the open blitz-offense-floor gap*
  (2026-07-22 memory item): rather than a blanket offensive floor, the floor is
  granted only to purpose-relevant blitzes — a strictly more targeted fix. If
  the gap gets fixed independently first, this table adjusts to modulate that
  baseline instead.
- Renormalization interaction (`macro_mcts.cpp:440-446`): lesson from item 7
  documented at `macro_mcts.cpp:390-397` — family floor mass dilutes everyone
  else post-renorm. The SECURE row deliberately *lowers* attritional BLOCK when
  raising CAGE, keeping total floored mass roughly constant on typical nodes
  (BLOCK averages ~2.17 candidates per node per the 2026-07-03 measurement at
  `macro_mcts.cpp:372-377`, so −0.06×~2 ≈ +0.06 CAGE + slack for the
  protective-blitz floor). A prior-mass audit is still mandatory (§2.7).
- Do not stack with `trailing2plus` adjustments: `trailing2plus` already implies
  not-`leading`, so the SECURE stall clause can't co-fire with it; STRIKE's
  numbersDiff clause is score-agnostic by design.

### 2.5 Fuller version (v2+, separate later commits, one lever each)

1. **Generation-side BLITZ scoring** (`macro_actions.cpp:317-350`): SECURE adds
   +4 to targets marking our carrier; STRIKE adds +1–2 globally (sideline-trap
   bonuses at 320-325 already reward finishing positions). Changes *which*
   targets surface, not just their prior.
2. **REPOSITION strategy gating** (`macro_actions.cpp:592-631`): offense hunter
   (599-613) only in STRIKE/NEUTRAL; SECURE forces shield/cage-screen targets
   even for MA>=7 players. Defense screen density (719-727) unchanged (onDef has
   its own working doctrine).
3. **Leaf-eval arbitration** (`macro_mcts.cpp:679-697`): scale bash-exposure
   penalty ×1.5 under SECURE; add the missing symmetric term — *our* carrier
   being marked is currently punished only indirectly, while marking *their*
   carrier earns +0.08/TZ (667-677); add −0.06×min(carrierTZ,3) under SECURE.
4. **Continuous scalar A∈[0,1]** replacing the enum, floor deltas scaled by A —
   only if v1 shows signal but coarse thresholds visibly misfire.
5. **Policy feature**: expose mode inputs (carrierTZ, escorts, numbersDiff) as
   state features (`engine/include/bb/feature_extractor.h:8`, NUM_FEATURES=73).
   Deferred: policy plateau diagnosis (2026-07-17, ~25% of teachable signal)
   makes prior-side integration the higher-leverage path today.

### 2.6 Known-bug coupling (flag for reviewer)

The HasActed-after-block gap (2026-07-22, unfixed: hasActed=true
unconditionally after every block, so a blitzer can never retreat to safety, and
REPOSITION generation excludes players adjacent to enemies —
`macro_actions.cpp:573-584`) inflates the *real* cost of STRIKE-mode blitzing
relative to a rules-correct engine. Recommendation: either land that fix first,
or interpret STRIKE-arm A/B results as a lower bound on its value.

### 2.7 Measurement plan

1. **Prior audit, zero games**: `expandRootPriorsForTest()`
   (`macro_mcts.cpp:475-483`) + constructed states (exposed carrier / safe cage
   / defense takeaway), assert (a) NEUTRAL arm reproduces today's priors
   exactly, (b) SECURE/STRIKE shift mass in the intended direction with total
   floored-family mass within ~±10% of baseline. Reuse the
   `diag_macro_floor_audit_*` harness pattern.
2. **Paired-seed A/B**, N>=400 (draw-rate noise floor ±8–11pp @150). Metrics:
   draw rate (gate), TD/game, own-carrier-sacked rate (carrier knocked down or
   turnover while held), casualties inflicted/received, and decision mix (share
   of chosen BLOCK/BLITZ by purpose class — logged from root visit counts,
   `lastChildVisits_` at `macro_mcts.cpp:217`).
3. Ship as one lever per commit (agent-team rule), NEUTRAL-anchored so each
   commit has a built-in no-op proof.

---

# 3. Why these stay two separate designs

Formation reactivity is a **one-shot decision at drive setup** consuming static
template geometry inside `setupHalfOrDrive()`; the resource split is a
**per-node modulation of search priors** inside `MacroMCTSSearch::expand()`.
They share no code, no data structure, and no tuning surface. The only
interaction: an OVERLOAD receiving formation creates an early local numbers
advantage on one flank, which the mode scalar's `numbersDiff`/local-numbers
inputs read *naturally* — no explicit coupling needed or wanted. Forcing a
unified "doctrine system" over both would add an abstraction layer with exactly
two clients that never call each other.
