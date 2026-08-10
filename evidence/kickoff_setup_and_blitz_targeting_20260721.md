# Kickoff setup reactivity + Blitz targeting doctrine (2026-07-21)

Follow-up investigation from the same situation as
`evidence/situation_wrong_direction_dodge_20260721.md` (half 2, turn 1,
`g0014.json.gz`), prompted by the user's tactical framework: receiving
team sets up SECOND (after seeing the kicker's formation) and should use
that to guarantee at least as many LOS blocks as the kicker has LOS
players (ideally more); split players between ball-security and hitting;
always Blitz something each turn, targeting the weakest exposed
opponent, falling back to a lineman if everyone's covered.

## 1. Does receiving-team setup react to the kicker's actual placement?

**No.** Both `setupHalfOrDrive` (`game_simulator.cpp:255-302`) and the
formation tables it draws from (`HOME_DEFENSIVE_FORMATION`,
`HOME_PRESSURE_FORMATION`, `HOME_DEEP_RECEIVER_FORMATION`, etc.,
`game_simulator.cpp:20-90+`) are **fixed `constexpr` position tables**.
The only branch is a coarse `classifyRosterSpeed` check (FAST vs not) on
the RECEIVING team, which picks which of two static kicking-formation
templates the kicker uses. Nothing computes or reacts to the specific
realized positions of the opposing formation -- the receiving team does
not get any modeled advantage from "seeing" the kicker's placement first,
even though that's how the real game's phase order works.

## 2. Is there deliberate resource-splitting (ball-security vs. hitting)?

Not as an explicit mechanism. What was observed in the mined situation
(some away players building an escort around the ball carrier, others
engaging the LOS) is best explained as an **emergent side-effect** of
independent per-decision MCTS scoring (each macro scored on its own
merits at the moment it's chosen), not a deliberate "allocate N players
to ball security, M to hitting" doctrine. No code found that explicitly
partitions available players between these two roles.

## 3. Blitz targeting -- partially already sound

`macro_actions.cpp:294-373` (`getAvailableMacros`'s BLITZ candidate
block): for every standing enemy, scores every possible blitzer against
them (`getBlockDiceCount(...) * 2` as the base term), plus additive
bonuses: ball-carrier priority (+10, defense only), opponent
scoring-threat proximity (+4), **"free opponent -- no friendly tacklezone
on them" (+2)** -- this last one is exactly the user's "target the
uncovered/exposed opponent" principle, already present. Candidates are
sorted by score descending; top 2 kept on defense, top 1 on offense.

**User's fallback case -- "if the opponent covers everyone, blitz at
least a lineman":** the candidate list is generated for EVERY standing
opponent regardless of whether any is free -- it's never empty just
because nobody is uncovered. When no "free opponent" bonus applies
anywhere, the ranking collapses to the `dice*2` term alone, which is
driven by relative Strength/assists -- a lineman (weaker, typically no
Block skill) will generically produce a more favorable dice count for
the blitzer than a tougher/skilled opponent would, so the sort would
naturally favor the lineman anyway even without special-case code.
**This means the desired fallback behavior is very likely already an
emergent property of the existing dice-count scoring** -- plausible
but NOT verified against real decision data (would need to mine turns
where every opponent is covered and check whether BLITZ still targets
the weakest one).

## Reinforced by the user: Blitz is use-it-or-lose-it, every turn

User's framing: the Blitz action is a once-per-turn resource that exists
specifically to be spent -- "od toho tam jeden je" (that's what it's
there for). Not using it when any reasonable target exists is value left
on the table, independent of whether the ideal (uncovered/weakest)
target is available -- ties directly into the fallback case above (even
a covered lineman is a better outcome than not blitzing at all). This is
the strongest form of the doctrine: the bar for "don't blitz this turn"
should be very high, not just "no free target visible."

## What's confirmed vs. still open

- Confirmed code-level facts: static formation templates (both sides),
  no resource-split mechanism, BLITZ scoring formula as described.
- NOT verified empirically: whether BLITZ is actually chosen by the
  search as often as the "always blitz something" doctrine would want,
  and whether the lineman-fallback reasoning holds up against real mined
  turns where every opponent is covered. Would need a targeted mining
  pass (filter turns where a BLITZ was available with no free target,
  check what got targeted) to confirm rather than infer from the scoring
  formula alone.

## Priority

Documentation/investigation only, no code changes. Items 1 and 2 (static
formations, no resource split) look like real, if not urgent, structural
gaps -- consistent with yesterday's "direction 5" coordinated-defense
research being flagged as a bigger design question, not a quick fix. Item
3 (blitz targeting) looks likely fine already; lowest priority of the
three, would just need empirical confirmation if ever revisited.
