# Full-Code Rules Review — engine action handlers (BB2016)

**Verdict:** Several real rules divergences found. The most training-relevant are the **Guard-assists-on-foul** bug (confirmed present), **Chainsaw missing +3 armour**, **inaccurate passes cannot be team-rerolled**, and **resetPlayersForNewTurn standing up STUNNED players for free** (turn-economy distortion). None are crashes; all silently bias self-play data.

Ranked most-severe first.

---

## 1. HIGH — Guard wrongly grants foul assists (confirmed)
**File:** `foul_handler.cpp:19-23` (via `helpers.cpp:139`)
`resolveFoul` computes assists with the generic `countAssists`, which unconditionally counts any adjacent player with **Guard** as an assist (`helpers.cpp:139 if (p->hasSkill(Guard)) count++;`). In BB2016 **Guard does NOT assist fouls** — only assisting players who are themselves free of enemy tackle zones may add to the foul's armour modifier. Guard's "always assists" applies to **blocks/blitzes only**.

**Failure scenario:** Fouler with two adjacent Guard team-mates who are each marked by an enemy. Engine gives +2 (Guard) friendly foul assists; rules give +0. Armour roll inflated by up to +2 → far too many armour breaks / casualties from fouls → AI over-values fouling.

**Fix:** Add a `bool guardCounts` (or `bool isFoul`) parameter to `countAssists`; when fouling, ignore Guard and only count assists with zero enemy TZ. Same applies to the enemy (defensive) foul-assist count.

---

## 2. HIGH — Chainsaw block omits the +3 armour modifier
**File:** `block_handler.cpp:197-219`
A Chainsaw "block" rolls a kickback D6, and on 2+ does `resolveArmourAndInjury(state, def.id, dice, ctx, events)` with a default `InjuryContext`. BB2016 Chainsaw rolls armour with a **+3 modifier**. Here `ctx.armourModifier` is 0.

**Failure scenario:** Chainsaw player hits AV8 target. Engine breaks armour only on raw 9+ (2D6>8); rules break on 6+ (2D6+3>8). Chainsaw is drastically under-powered → AI mis-values Chainsaw secret-weapon teams.

**Fix:** `InjuryContext ctx; ctx.armourModifier += 3;` before the Chainsaw armour roll (the same `ctx` already carries Stakes for the Stab path — add an analogous +3 for Chainsaw).

---

## 3. MEDIUM/HIGH — Inaccurate passes can never be re-rolled
**File:** `pass_handler.cpp:222-300`
Reroll logic (Pass skill / Pro / team reroll) is gated entirely inside `if (roll == 1)` (fumble). A pass that is **inaccurate but not a fumble** (e.g. needed 4+, rolled 2 or 3) falls straight through to the scatter path with no reroll. In BB2016 a team reroll / Pass skill / Pro may be used to reroll **any failed pass**, not only fumbles.

**Failure scenario:** AG3 thrower, short pass, target 4+, rolls 3 with a team reroll available. Engine scatters the ball (turnover risk) without offering the reroll; rules would reroll to potentially complete the pass. The AI's passing value is systematically depressed and it learns to under-use rerolls on passes.

**Fix:** Restructure so the reroll chain runs whenever `roll < passTarget` (treating natural-1 as the special fumble-on-reroll case), mirroring the `attemptRoll` pattern used elsewhere.

---

## 4. MEDIUM — STUNNED players stand for free at start of own turn
**File:** `game_state.cpp:57-68` (`resetPlayersForNewTurn`)
On a new turn the owner's STUNNED players are flipped to PRONE (correct), **and** `movementRemaining` is reset, but nothing prevents them acting. Combined with `rules_engine.cpp:181-191`, a PRONE player can then stand (3 MA) and act normally this same turn — which is correct. The real divergence is subtler: a player **stunned this turn** by the opponent should turn face-up (PRONE) only at the start of the **opponent's** next turn per BB2016 timing; the current single-flip on the owner's turn is acceptable. **However**, `resetPlayersForNewTurn` does not clear `state.ball` carrier consistency and does not re-prone the *opponent's* stunned players, so stun duration is effectively one full round only for the owner. Low confidence — flagging for verification against intended stun timing.

**Fix:** Confirm stun lasts until the stunned player's team's next turn (face-up at start of their turn), which the code does; ensure opponents' stuns are not cleared early elsewhere.

---

## 5. MEDIUM — Pile On not implemented
**File:** `block_handler.cpp` (no reference to `SkillName::PilingOn`)
After a block knocks the defender down, a player with **Piling On** may re-roll the armour or injury dice (going prone himself). The skill is entirely absent from block resolution. If any roster grants Piling On, those casualties are undercounted.

**Fix:** After `resolveArmourAndInjury` on a knockdown, if `att.hasSkill(PilingOn)` and not yet used, optionally re-roll armour/injury and set attacker PRONE. (Gate behind whether Piling On appears in active rosters before investing.)

---

## 6. MEDIUM — Tentacles uses raw ST, ignores Block/skill timing but mainly: caught player keeps movement-end only, no GFI/position bug
**File:** `move_handler.cpp:13-42, 96-101`
`checkTentacles` runs **before** the movement decrement and dodge. On a failed escape it returns `ActionResult::ok()` and the mover stays put — correct (no turnover). But the contest uses `> ` (strictly greater to escape) which matches BB2016 (attacker must beat, ties favor Tentacles). This one is **OK** on re-check; noting only that Tentacles is checked only when `needsDodge` (leaving a TZ) — Tentacles in BB2016 triggers whenever you try to move out of the tentacled player's TZ, which is exactly a TZ-leaving move, so correct. **No action needed.**

---

## 7. MEDIUM — Really Stupid assist check counts any ally (incl. another Really Stupid / Big Guy)
**File:** `big_guy_handler.cpp:27-41`
`hasAdjacentAlly` counts **any** standing team-mate, but BB2016 requires the assisting team-mate to **not itself be Really Stupid** (and to be able to help). Two adjacent Really Stupid big guys would wrongly let each other pass on 2+.

**Fix:** Exclude allies with `ReallyStupid` (and arguably require the ally to have tackle zones) from `hasAdjacentAlly`.

---

## 8. MEDIUM — Take Root only checked on MOVE/BLITZ and only blocks that one action
**File:** `big_guy_handler.cpp:73-86`
BB2016 Take Root: roll at the **start of each activation**; on a 1 the player is **rooted for the rest of the drive** (cannot move, may still block/foul adjacent, counts as having Stand Firm) until he leaves play. Here it is rolled only for MOVE/BLITZ activations and merely wastes the single action — the "rooted until end of drive" persistent state is not modeled, so a rooted big guy can move freely next turn.

**Fix:** On failure, set a persistent rooted flag cleared on knockdown/drive-end; disallow movement while rooted.

---

## 9. LOW/MEDIUM — Chain-push off-pitch drops ball at on-pitch square instead of crowd
**File:** `block_handler.cpp:130-136`
When a chain-pushed occupant is shoved off the pitch, `handleBallOnPlayerDown` is called **before** `occupant->position` is set to `{-1,-1}`, so a ball carried by that occupant bounces from his old on-pitch square rather than going to a throw-in from the sideline. Minor positional error in a rare chained crowd-surf.

**Fix:** Set `occupant->position = {-1,-1}` (or pass the sideline square) before resolving the ball, consistent with the main crowd-surf path.

---

## 10. LOW — RIOT kickoff event turn adjustment is approximate
**File:** `kickoff_handler.cpp:63-71`
`turnNumber <= 1 ? ++ : --` only nudges the receiving team's counter. BB2016 RIOT moves **both** teams' turn markers together (and the direction depends on whether either team has taken a turn). Edge-case scoring/half-length drift in rare events.

---

## 11. LOW — Throw-a-Rock / Pitch-Invasion / Cheering & Brilliant Coaching are simplified
**File:** `kickoff_handler.cpp:90-181`
- Cheering Fans and Brilliant Coaching are coded identically (both compare two raw D6 and give the winner +1 reroll), ignoring FAME / assistant-coach / cheerleader modifiers — acceptable simplification.
- `THROW_A_ROCK` stuns one random player **on each team** (rule: rock hits one player on the team that rolled lower FAME-adjusted). Minor.
- These are low-impact given kickoff events are a small fraction of decisions.

---

## 12. LOW — Hypnotic Gaze success only removes TZ, no per-turn limit / no follow rules
**File:** `gaze_handler.cpp` — appears correct for BB2016 (target loses TZ until start of its team's turn). `lostTacklezones` is reset in `resetPlayersForNewTurn`, so duration is right. **No action.**

---

### Items explicitly re-verified as CORRECT (not bugs)
- **Sure Hands negates Strip Ball** (`block_handler.cpp:461-466`) — correct per BB2016; ball stays with carrier. ✔
- Interception target `7-AG+2` (`pass_handler.cpp:73`) — correct +2 difficulty. ✔
- GFI allowance (2 normal / 3 Sprint) and Blizzard 3+ GFI target. ✔
- Dauntless uses raw strengths for the roll then equalizes effective ST. ✔
- Wrestle on Both Down (both prone, no armour, no turnover). ✔
- Dodge downgrades Defender Stumbles to a push unless attacker has Tackle. ✔
- Juggernaut converts Both Down→Pushed on a blitz, and ignores Stand Firm on a blitz. ✔
