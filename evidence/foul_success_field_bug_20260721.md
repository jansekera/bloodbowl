# FOUL event `success` field hardcoded true (2026-07-21)

Found via fresh replay-mining on the post-item7/item10 engine
(`diag_replay_mine_20260721.py`, 24 self-play games, macro_mcts vs
macro_mcts, `weights_best.json`).

## What's wrong

`engine/src/foul_handler.cpp:36-37` emits the `FOUL` GameEvent with
`success` hardcoded to `true`, **before** `armourBroken` is computed
(line 39):

```cpp
emitEvent(events, {GameEvent::Type::FOUL, fouler.id, target.id,
                  fouler.position, target.position, armourRoll, true});

bool armourBroken = (armourRoll > target.stats.armour);
```

Result: every FOUL event in every replay log reports `success=True`
regardless of the actual roll. Observed in the fresh corpus: rolls of 2,
3, 4, 5 (well below any real armour value, typically 7-9) still logged
as `success=True`. Example: `g0002.json.gz half2turn8 FOUL player=14
target=9 roll=4 success=True`.

## Why it's real, not intended behavior

Compare `injury.cpp:70-94` (`resolveArmourAndInjury`, the shared helper
used elsewhere, e.g. `block_handler.cpp`'s Chainsaw/Stab paths) --
it correctly emits `ARMOR_BREAK` with `success = broken` (the real
outcome). `foul_handler.cpp` reimplements armor+injury resolution
inline instead of calling the shared helper, and its separate
`ARMOR_BREAK` event (line 42-43) IS correct (only emitted when
`armourBroken`), but the initial `FOUL` event's own `success` field is
not.

## Why it's low-risk / not a training bug

Grepped `engine/src/*.cpp` (excluding tests) for any consumer of
`event.success` on a FOUL event -- none found. No C++ decision-making,
feature extraction, or training signal reads this field. This is
**not** the same class as the 07-15 `hasActed` bug (which did corrupt
gameplay/training). It only affects human-facing narrative: any tool
reading `get_turn_logs()` and displaying/reasoning about foul outcomes
(this mining script, `python/blood_bowl/replay_viewer.py` -- though
that viewer currently doesn't display foul `success` at all, so isn't
actively misled -- and any future situational analysis) would report
"foul succeeded" when the armour roll actually failed.

## Suggested fix (not applied -- queued)

Move the `armourBroken` computation before the `emitEvent` call and use
it in place of the hardcoded `true`:

```cpp
bool armourBroken = (armourRoll > target.stats.armour);
emitEvent(events, {GameEvent::Type::FOUL, fouler.id, target.id,
                  fouler.position, target.position, armourRoll, armourBroken});
```

Small, localized, no behavior change to actual game rules (armor/injury
resolution logic untouched) -- purely fixes the logged field. Add a test
asserting `FOUL` event `success` matches `armourBroken` for both a
broken and unbroken roll (mirroring the existing pattern already used
for e.g. `MacroMCTS.SecondaryPickupPriorIsHalfOfPrimary`-style targeted
unit tests).

## Scope clarification (user question 2026-07-21)

The doubles→ejection failure mode is a **separate, already-correct**
mechanism, untouched by this bug. `foul_handler.cpp:91-99`: if
`isDoubles` and the fouler lacks `SneakyGit`, the fouler is set
`EJECTED` and a distinct `INJURY`-type event is emitted on the
*fouler's* id with `success=false` (deterministic once doubles+no
SneakyGit -- no further roll to reflect). This bug only concerns the
initial `FOUL` event's own `success` field (meant to reflect whether the
*target's* armour broke), not the ejection path.

Minor readability note: ejection reuses the `INJURY` event type (there's
no distinct `EJECTED` type in the event enum) -- a reader of raw
`turn_logs` has to check whose `player_id` an `INJURY` event carries
(fouler vs. target) to tell "fouler was sent off" apart from "someone
got hurt." **User's call (2026-07-21): both this and the `success`
hardcode are minor, but fix both together while the code is in focus --
don't let the ejection-typing nit quietly drop once the primary fix
lands.** Small option when implementing: either add a distinct
`EJECTED` event type, or at minimum keep it as `INJURY` but make it
unambiguous some other way (e.g. a dedicated field/flag) -- not designed
yet, decide at implementation time.

## Systemic sweep (checked for the same anti-pattern elsewhere)

Grepped all `emitEvent(...)` calls across `engine/src/*.cpp` ending in a
hardcoded `true`/`false` literal (51 hits total). Spot-checked the rest:
all the others are events describing a deterministic occurrence (a
completed MOVE, a triggered SKILL_USED, a KNOCKED_DOWN state change) where
hardcoding is correct, or are already properly gated behind the roll
check that determines them (e.g. `block_handler.cpp:191`'s FoulAppearance
`SKILL_USED` event only fires inside `if (faRoll == 1)`, correctly
conditioned). FOUL is the one instance where a hardcoded literal
represents an outcome that's actually computed later. Not exhaustive
(didn't trace every one of the 51 individually against a rules
reference), but no second instance jumped out.

## Status

**Queued, not applied -- two items, apply together:** (1) the
`success`-hardcode fix above, (2) the ejection/`INJURY`-typing
readability nit. Per [[feedback_bugfix_priority_over_speed]] and today's
priority order, real fixes wait behind the active `--loop 4` training
validation for item7/item10 ([[project_bloodbowl_day_20260721]]). Low
risk to apply whenever picked up (no shared state with the
currently-running training/gate logic).
