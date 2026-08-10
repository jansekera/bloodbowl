# Finding: home dodges INTO more danger when a risk-free escape was one step away

Found via fresh replay-mining (`diag_replay_mine_20260721.py`), game
`g0014.json.gz` (home=wood-elf, away=human, final 1-0), half 2, turn 1,
home's very first action of the drive (right after kickoff -- see
"Context" below, established via a multi-round back-and-forth with the
user that corrected two earlier misreadings of this same situation).

## Context (corrected understanding, for anyone reading only this file)

This is home's first turn since the half-time kickoff. Away (human,
correctly classified MIXED-speed not FAST -- confirmed against real
Blood Bowl tactical sources) received a deliberately deep kick
(`kickoff_handler.cpp:218-224`, "deep vs slow/mixed") that landed at
`(23,6)`, deep in away's own half. Away's preceding turn recovered the
ball and built a small escort around the carrier without advancing it.
Both teams' LOS players are naturally adjacent across the line of
scrimmage (standard kickoff formation contact, not a mistake by either
side). None of this involves a missed scoring chance or a
multi-turn defensive-reactivity gap -- those were earlier wrong readings
of the same data, corrected in `evidence/situation_missed_scoring_chance_20260721.md`.

## Additional context: how player 2 got to (11,6) (checked per user's
## question "did humans make a mistake leaving an elf standing at the LOS")

Not a human mistake -- the opposite. Away's OWN preceding turn actively
blocked home's LOS: `BLOCK player 15 -> target 2, success, PUSH (12,7)->
(11,6)` (player 2 shoved back, not knocked down) and separately `BLOCK
player 13 -> target 1, success, PUSH then KNOCKED_DOWN` (player 1 downed,
armour held). Humans used their strength advantage on the LOS exactly as
expected -- they did not passively leave anyone standing. Player 2's
position at `(11,6)` at the start of home's turn is the RESULT of having
just been pushed there by a won opposing block, not an arbitrary starting
spot. Doesn't change the core finding below (computed from the actual
position at the start of home's turn, regardless of how it arose) -- just
fills in why the AI may have been drawn to deal with this specific player.

## The finding

Home player 2 starts at `(11,6)`, adjacent to exactly **one** away tackle
zone (player 15 at `(12,7)`). Computed all 8 neighboring squares:

```
(10,5): FREE -- zero dodge needed
(10,6): FREE -- zero dodge needed
(10,7): occupied
(11,5): still adjacent to away [13]
(11,7): occupied
(12,5): still adjacent to away [13, 17]  <-- what the AI actually chose
(12,6): still adjacent to away [15]
(12,7): occupied
```

**Two fully risk-free escape squares existed one step away** (`(10,5)`,
`(10,6)`, moving away from the LOS -- zero opposing tackle zones, no
dodge roll needed at all). Instead, the macro-search moved player 2 to
`(12,5)` -- **toward** the opponent's side, landing in contact with
**two** tackle zones (players 13 and 17) instead of the original one.
That second, worse square is what forced the follow-up dodge that then
failed, ending the turn (immediate turnover, any failed dodge/GFI is
instant).

This is not "took a risk that didn't pay off" -- it's a move that
**increased** the immediate danger (1 tackle zone -> 2) when a
zero-risk alternative was directly available, then had to gamble a
second time to get out of the self-created worse position.

## Turn-ordering question -- CONFIRMED as a foundational BB principle (elevated priority)

User's follow-up: within a single team-turn, safe/risk-free repositioning
should happen before any risky (dodge/GFI-bearing) action -- a failed
risky action ends the turn immediately, so gating safe value behind it
is strictly worse than sequencing safe-first. **Verified via web search,
2026-07-21, multiple independent tactical sources**: this is not a niche
opinion, it's a foundational, universally-taught Blood Bowl principle --
"All rookies are regularly told to play their turn starting from the
safest actions to the riskiest ones... take as many no-risk moves...
before doing anything else... crucial 1-die blocks and other high-risk
actions [go] last." (sources below.)

Checked `macro_actions.cpp`/`macro_mcts.cpp` for an explicit "safe
actions before risky ones" turn-level sequencing rule -- **found none**.
There IS a per-action `risk_level` feature (`macro_actions.cpp:1395-1433`,
estimated failure probability per macro type) fed into the search, but
that's a per-decision input feature, not a turn-level ordering
principle. The search picks whichever single macro-action currently
scores best at each decision point, re-evaluating from scratch each
time -- no apparent mechanism weighs "the cost of forfeiting the rest of
this turn if this action fails," which is exactly the reasoning behind
the safe-first principle.

Sources:
- [BloodBowl: Managing Risk -- Frontline Gaming](https://frontlinegaming.org/2020/08/30/bloodbowl-managing-risk/)
- [The Goonhammer Blood Bowl Combine: Risk Management Basics](https://www.tabletopbattles.com/the-goonhammer-blood-bowl-combine-risk-management-basics)
- [Blood Bowl - Moving From Beginner to Intermediate - Goonhammer](https://www.goonhammer.com/blood-bowl-moving-from-beginner-to-intermediate/)

**Still N=1 concretely** (one mined situation) -- the PRINCIPLE is
confirmed as foundational and important, but whether the engine actually
violates it SYSTEMATICALLY (vs. this being an unlucky one-off given the
search's inherent variance) is not yet established. Elevated priority
for investigation, but still needs more mined examples (or a direct
code-level check of whether/how turn-level risk-ordering is or isn't
represented in the search) before treating a fix as justified -- same
"don't act on N=1" discipline as everything else today.

## Priority / next steps

Elevated from "lowest urgency" given the principle's confirmed
foundational importance (not a niche/debatable tactic) -- but still
queued behind the currently-running training and the FOUL/GATE_VF_BLEND
fixes, since this needs a proper investigation (systemic check across
more mined games, or a code-level audit of the search's turn-level
structure) before any patch is justified, not a quick fix like the other
two.
