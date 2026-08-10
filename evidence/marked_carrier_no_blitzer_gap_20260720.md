# Marked carrier, nobody to blitz the marker away — a macro-generation gap (2026-07-20)

## How this surfaced

Found while calibrating `MidGameSafeWalkInPrefersScore`, the regression test
added today for the SCORE-availability prior-floor patch
(`proposals_score_availability_20260714.md` Patch A, see
`evidence/score_availability_patch_20260720.md`). The test's original design
(never actually run by its authors) placed a marker adjacent to the carrier's
starting square. A diagnostic replay (raw `childVisits` dump) showed the
search preferred BLITZ over SCORE even after both Patch A (0.30 prior floor)
and Patch B (carrier-activation guard) were applied — not because either
patch was broken, but because in that specific scenario **no teammate was
close enough to blitz the marker away**, so BLITZ (with the carrier excluded
by Patch B) resolved to a no-op, and the "safe walk-in" SCORE macro was not
actually safe: the carrier starts adjacent to a standing opponent, so the
first step of any movement requires a Dodge roll. The test itself was fixed
by moving the marker off the carrier's adjacent squares (a true zero-risk
walk-in, matching what `directSafeWalkIn` is supposed to represent) — but the
underlying tactical situation the marker-adjacent version accidentally
constructed is real, general, and — per user's framing — not something this
project had written down anywhere before: **what should the AI do when its
ball carrier is marked by a single opponent and no teammate is positioned to
blitz that marker away?**

## Two compounding generation-side gaps

1. **SCORE's own "safe walk-in" check never looks at the carrier's starting
   adjacency.** Both the base generation condition
   (`macro_actions.cpp:151-158`, `dist <= movementRemaining + 2`) and Patch
   A's `directSafeWalkIn` floor boost (`macro_mcts.cpp`, `dist <=
   movementRemaining`) are pure distance/movement checks. Neither checks
   `countTacklezones(state, carrier->position, ...)` at the carrier's
   *current* square. A carrier standing in a marker's tacklezone gets exactly
   the same "safe walk-in" treatment (same 0.30 prior floor, same +0.4
   `scoringBonus` in the leaf heuristic at `macro_mcts.cpp:568-571`) as one
   standing completely free — even though the marked carrier faces a real,
   uncosted Dodge roll (AG-dependent, typically ~33-42% fail chance without
   Dodge skill) on the very first step, which on failure is a turnover.

2. **`carrierStuck` (gating HAND_OFF_SCORE/PASS_SCORE generation,
   `macro_actions.cpp:167` and `:191`) requires `carrierTZ >= 2`.** A carrier
   marked by exactly one opponent (`carrierTZ == 1`) is *not* considered
   "stuck," so hand-off/pass alternatives to a positioned teammate are never
   even generated as candidates — regardless of whether such a teammate
   exists and is safely reachable. Checked `expandHandOffScore`
   (`macro_actions.cpp:1179-1221`): if the carrier is *already* adjacent to
   the receiver (`dist <= 1`), Step 1 (movement) is skipped entirely and the
   hand-off executes with **no movement and no Dodge roll at all** — a
   genuinely risk-free alternative in exactly the situation where the direct
   walk-in is not.

## Combined effect

When a carrier is marked by exactly one opponent and no teammate is in
BLITZ range of that marker (the scenario that accidentally showed up in the
unit test), the search's only offensive candidate that makes progress is the
direct SCORE macro — which (a) the engine's own heuristics treat as
risk-free when it isn't, and (b) has no risk-mitigated fallback offered even
when one might exist and be strictly safer (an adjacent teammate for a
zero-risk hand-off). BLOCK/FOUL/BLITZ-with-the-carrier are correctly
excluded by today's Patch B; CAGE/REPOSITION/END_TURN remain available but
make no scoring progress. The carrier is left choosing between "take the
uncosted dodge risk" and "do nothing productive this turn" — with the
heuristic not even correctly informing that choice.

## What "correct" looks like — and what doesn't have a clean answer

Per the user's own tactical framing (verified against real BB2016 mechanics
during this same investigation, see
`project_bloodbowl_divingtackle_rules_deviation_20260720` memory for the
related SideStep/StandFirm/Tackle/DivingTackle rules check, which came back
correct): a lone marker with no available blitz support is a genuinely hard
situation in real Blood Bowl too — there is no guaranteed-safe answer, and a
real coach sometimes just has to accept the dodge risk or ground the drive
for a turn. So the goal isn't "make this always safe," it's narrower and
achievable:

1. **The heuristic/prior should know the walk-in is risky when the carrier
   starts marked**, instead of silently treating it as identical to a truly
   free walk-in. This affects both Patch A's floor (today's patch) and the
   `scoringBonus` leaf term — both currently overstate this specific case.
2. **Hand-off/pass alternatives should be considered starting at
   `carrierTZ >= 1`, not `>= 2`**, so a genuinely safer option (an already-
   adjacent teammate) is at least *visible* to search when it exists. This
   doesn't force the AI to take it — Q-value arbitration decides that — it
   just stops silently omitting the safer branch from consideration.

## The upstream, proactive half of the same gap

User's follow-up observation, checked against the code: the reactive fix
above (offer hand-off when TZ>=1, price the risk correctly) only helps once
the carrier is *already* marked with no support. The better fix is upstream
and proactive — keep a free blitzer positioned near the carrier's likely
path throughout the drive, so that *whenever* a marker does show up, someone
is already in range to clear it. Checked whether anything like this exists:

- `expandCage` (`macro_actions.cpp:901-934`) fills each of the 4 diagonal
  cage corners with `findNearestFreePlayer` — purely proximity-based, **no
  role distinction at all** between "cage corner" and "reserve blitzer/path
  clearer." Every free player is interchangeable in this logic.
- `expandReposition` and the REPOSITION macro generally have no concept of
  "keep this player unengaged and within blitz range of anything that might
  mark the carrier" either.
- This is the same *shape* of gap as the already-documented deployment/
  positioning finding from 07-15
  (`evidence/fable_perplayer_replay_grounding_20260715.md`, "is_free_receiver
  / pass-lane blindness": 21.8% of turns have no standing teammate at all
  positioned ahead of the carrier — a positioning gap, not a recognition
  gap) — except for blitz-support instead of receiving. Also matches the
  brief's own offensive-formation table
  (`team1_brief_per_player.md`, "Blitzer 1–2: čistí cestu, útočí na
  soupeřovy bloky, před cage, side") which names this role conceptually but
  it has never been implemented as actual macro-generation/positioning logic
  — it's a feature-design aspiration, not a measured or built behavior.

So the full picture has two layers, reactive and proactive, and both are
undocumented until now:
1. **Reactive** (this note, main section): when marked with no support,
   the AI doesn't even see its safest option (hand-off) and doesn't
   correctly price the risk of the option it does see.
2. **Proactive** (this addendum): nothing during cage/reposition formation
   tries to prevent "marked with no support" from happening in the first
   place by holding one player in a blitz-ready reserve role.

Fixing (1) without (2) still helps (better decisions in the situation once
it arises); fixing (2) reduces how often the situation arises at all. Both
are new, undesigned findings — neither has a diff or a measurement plan yet.

**Third layer, also user-flagged: hand-off itself needs its own preparation,
not just a lowered TZ threshold.** `HAND_OFF_SCORE`'s generation condition
(`macro_actions.cpp:167-183`) demands the receiver satisfy TWO things at
once: reachable from the carrier (`adjDist <= 2`) AND itself able to reach
the endzone (`receiverDist <= receiverMaxReach`) — i.e. a receiver who is
both nearby AND has a clear scoring lane. Nothing during CAGE/REPOSITION
formation cultivates that combination on purpose; a receiver satisfying both
only exists opportunistically, by whatever the nearest-free-player
proximity fill happens to produce. So lowering `carrierStuck`'s threshold to
`TZ >= 1` (item 1 above) would make the search's tree correctly *offer*
hand-off more often, but if no teammate happens to be both near-and-clear
when the carrier gets marked, there is still nothing to offer — same
underlying issue as the reserve-blitzer gap (item 2), applied to a
receiver's positioning instead of a blitzer's. All three items point at the
same root cause: **the current macro system reasons about roles/setup only
opportunistically at generation time, never proactively during earlier-turn
formation** — matching the per-player brief's aspirational role table
(Blitzer, Catcher/receiver, etc.) that has never been implemented as actual
positioning logic.

## First-draft candidate calculations (design sketch only — not implemented, not measured)

Written out because the user asked for the actual reasoning, not just the
gap description. These are starting points for a future design pass, not
diffs — same status caveat as the rest of this note.

### Reserve blitzer selection

Goal: while forming/holding the cage (i.e. `expandCage`/`expandReposition`,
carrier not yet marked), hold back exactly one otherwise-free teammate from
the cage-fill pool as a designated "marker-clearer," instead of treating
every free player as interchangeable cage-corner filler.

1. Compute the carrier's **threat zone** for the next ~2 turns: squares
   within Chebyshev distance 1 of the carrier's current position plus its
   projected path (`carrierStallAwareSteps` already computes the intended
   per-turn advance — reuse that instead of a fresh projection).
2. Among free (non-carrier, not already cage-committed) teammates, score
   each as a blitz-reserve candidate by
   `reserve_score(p) = -max(0, dist(p, nearest threat-zone square) - p.MA)`
   i.e. 0 (fully qualified: already in range) or increasingly negative the
   further outside their own move range they'd need to reach — mirrors
   `carrierIsBlitzable`'s existing Chebyshev-vs-MA reachability check
   (`macro_actions.cpp:840-850`), just evaluated from the reserve's own
   position rather than an opponent's.
3. Break ties by attacker quality for a favorable block: prefer
   `Block`/`Guard`-equipped, higher effective ST (reuse `block_dice`'s
   assist-aware net-ST logic rather than raw ST, since a lone high-ST player
   without assist support can still lose the dice).
4. The winner is excluded from `findNearestFreePlayer`'s cage-corner
   candidate pool for this turn (one line change: skip the reserve's id in
   `expandCage`'s search) and instead gets a REPOSITION target that
   maximizes 2's reachability margin (stand where the threat-zone coverage
   is best, not necessarily adjacent to the carrier at all).
5. Re-evaluate every turn (cheap — same info `carrierIsBlitzable` already
   computes) rather than committing to one player for the whole drive: as
   the carrier advances, the best-positioned reserve candidate changes.

### Hand-off receiver preparation

Goal: proactively keep one teammate positioned so that IF a hand-off becomes
necessary later, a qualifying receiver (near AND with a clear lane) already
exists, instead of only checking opportunistically at generation time.

1. Candidate pool: free teammates with `!hasSkill(NoHands)`, ranked by
   `agility*5 + (hasSkill(Catch) ? 5 : 0) - passDist`-style score already
   used in `PASS_SCORE`'s target selection (`macro_actions.cpp:209-210`) —
   reuse that scoring function rather than inventing a new one, just apply
   it a turn earlier as a *positioning* target instead of a same-turn pass
   target.
2. A receiver only "counts" as prepared if BOTH legs of `HAND_OFF_SCORE`'s
   own generation condition are kept true one turn ahead:
   `dist(receiver, carrier_next_turn_position) <= 2` (reachable) AND
   `distToEndzone(receiver) <= receiver.MA + 2` (has a lane) — i.e. don't
   just pull a receiver close to the carrier, keep them on the endzone side,
   not boxed in behind the cage.
3. Positioning target for REPOSITION: the square within the receiver's own
   move range that minimizes `dist(receiver, carrier_next_turn_position)`
   subject to keeping `distToEndzone(receiver) <= receiver.MA + 2` satisfied
   — i.e. trail/flank the carrier on the scoring side, not directly behind
   it (a receiver behind the carrier relative to the endzone fails leg 2
   even if leg 1 holds).
4. Same reachability math as the reserve-blitzer case, different objective
   (be adjacent-with-a-lane vs. be within-blitz-range-of-the-threat-zone) —
   the two roles could in principle be evaluated together (score every free
   teammate for both roles, assign whichever pairing maximizes total
   coverage) rather than sequentially, since a team only has so many free
   players to allocate across cage corners, reserve blitzer, and receiver.

None of this has been tested even offline (no ridge-fit/decision-mining gate
like Phase A used for per-player features) — it is a plausible first design,
not a validated one. Treat it exactly like item 7/item 10's "ready diffs":
still needs its own negative-control test design and decision-level sanity
check before touching production code, per the project's standing
discipline ([[feedback_bugfix_priority_over_speed]],
[[feedback_implementation_style]]).

## Fourth confirmation: CHAIN_SCORE has the exact same shape of gap

User independently described (before being shown this code) a
pass-then-relay-then-handoff-then-score sequence and asked whether it
requires careful setup. It's an existing macro, `CHAIN_SCORE`
(`macro_actions.cpp:224-268`): carrier passes to a "relay," relay hands off
to a "scorer," scorer runs it in. Generation requires ALL THREE roles to
already be correctly positioned in the SAME turn:
`carrierStuck` (dist-only, see inconsistency below) triggers the search, then
a relay must be within pass range (1-10 squares) of the carrier, AND a
scorer must be within hand-off range (<=2) of THAT relay, AND the scorer
must have a clear endzone lane (`scorerDist <= scorerMaxReach`) — nothing
positions a relay or scorer candidate proactively; exactly the same
opportunistic-only pattern as the reserve-blitzer and hand-off-receiver
gaps above, now confirmed for a third distinct role-pair.

**Also found while checking this: the four "is the carrier in trouble"
conditions across SCORE/HAND_OFF_SCORE/PASS_SCORE/CHAIN_SCORE are mutually
inconsistent**, which compounds the whole family of gaps:

| macro | "stuck"/generation condition | checks TZ (marked-ness)? |
|---|---|---|
| SCORE | `dist <= movementRemaining + 2` | no |
| HAND_OFF_SCORE / PASS_SCORE | `dist > maxReach` OR `TZ >= 2` | yes, but only at TZ>=2 |
| CHAIN_SCORE | `dist > maxReach` | no |

A carrier marked by exactly one opponent but still within walking distance
(this note's whole scenario) trips none of the TZ-aware branches and gets
offered only the (mispriced-risk) direct SCORE macro — CHAIN_SCORE isn't
even considered as an option because its condition never looks at TZ at
all, regardless of whether a relay+scorer pair happens to be available.

## Status: documented, not designed or applied

This is a **new finding**, unlike item 7/item 10 (`proposals_item7_..." /
"proposals_item10_..."`), which already have full diffs+tests ready to apply.
This one needs its own design pass (exact threshold/condition changes,
regression tests, negative controls) before it's a "same-day apply"
candidate — flagged here per the user's request to write it up properly
rather than let it evaporate the way the SCORE-availability patch nearly did
for six days. Do not apply anything from this note without designing it
first, same discipline as everything else already queued
([[project_bloodbowl_unapplied_proposals_audit_20260720]]).
