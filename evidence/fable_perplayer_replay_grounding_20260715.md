# Per-player feature candidates vs observed AI behavior — replay grounding (2026-07-15)

**Question being answered:** `team1_brief_per_player.md` Okruh 1-4 (cage corner ST/Guard,
`carrier_blitzable` BFS-vs-Chebyshev, `is_free_receiver`, `net_st_for_block`,
`adjacent_to_sideline`) was built from Blood Bowl tactical theory + web research, not from
watching THIS engine/AI actually fail. The "Okruh 5: Situace kde AI dnes chybuje" checklist
item was never done. This is that missing grounding pass — small-N, qualitative-plus-counts,
not a statistical test. It does **not** duplicate the 07-02/07-14 replay mining (which found
mechanical bugs: pickup-miss 82%, SCORE-after-move, FOUL overuse, loose-ball scramble) — this
pass asks specifically whether the *positional/per-player* blind spots the candidate features
target are actually observed.

## Methodology

**Data used:** none of the existing artifacts had per-player positions — `training_logs/*/game_*.jsonl`
(written by `cpp_runner.py`) logs only aggregate 73-dim feature vectors + running score, and the
07-02/07-14 mining worked off those plus the MCTS decision logs (also feature-vector-only). The
brief itself names the actual per-player data source: `get_turn_logs()` (already exposed via
pybind, `engine/python/bb_module.cpp:197-266`) returns id/x/y/state/has_ball per player per turn
boundary + event list — this is exactly what `replay_viewer.py` renders, but `cpp_runner.py`
never persists it (confirmed: no `game_*.jsonl` file contains a `home_players`/`away_players`
key; `evidence/fable_replay_mining_20260714.md` §"Limity dat" already flagged this as a
should-turn-on-before-next-run gap). So a small fresh batch was generated instead of reusing
existing arm/log files.

**Generated:** `diag_perplayer_grounding.py` (new, throwaway analysis script, left in place per
today's naming convention) — 150 mirror games (`weights_best.json` vs itself), MCTS=100,
`vf_blend=0`, `epsilon=0`, TV=1200 (developed/skilled rosters — the ones actually used in
production gating, so Guard/StripBall/etc. are present), `dirichlet_alpha=0`, `exploration_c=1`,
`weights_policy.json` loaded (matches self-play's prior-floor regime), races cycling
human/orc/skaven/dwarf/wood-elf pairs — i.e. as close to the production gate/self-play config as
a read-only script can get. Each game's full `get_turn_logs()` output (all ~32 turn-boundary
snapshots, all in-turn events) was persisted to
`diag_perplayer_grounding_data/main/g****.json.gz`. Run log: `diag_perplayer_grounding_run_20260715.log`
(150/150 games, 1038s wall, 10 parallel workers). Result mix: 80 draws (76 nil-nil) / 70 decisive
— consistent with the ~50-60% draw rate seen in other 2026-07 mirror runs, so this batch is not
an outlier sample.

**Per-player stat/skill table:** re-derived from `engine/src/roster.cpp:500-589` (TV1200
rosters) and the slot-assignment order in `buildTeam()` (`engine/src/game_simulator.cpp:161-215`
— specialists fill slots 10→down in template order, linemen fill the rest). The mapping was
**verified at runtime** against `bb_engine.setup_half()`'s actual `Player.stats` for all 22 ids
across all 5 race pairs before any game was run (`verify_mapping()` in the script) — not
assumed from reading the C++.

**Analysis definitions (all derived read-only from turn snapshots + events, no engine change):**
- *Cage corner quality*: at each snapshot where the defender is about to move and 3+ of the
  attacker's standing players occupy the 4 diagonals around the carrier, classify the worst
  occupied corner (tier 2 = ST≤2, tier 1 = ST3 without Guard/Block, tier 0 = solid) and check
  whether a standing teammate with strictly better ST/Guard was idle within 3 squares.
  "Broken" = carrier prone, ball loose, or carrier changed by the next snapshot.
- *carrier_blitzable*: compared the engine's actual Chebyshev flag (`dist ≤ opponent.MA`,
  `feature_extractor.cpp:356-366`) against a Python BFS respecting opponent tackle zones (per
  the brief's own spec, Okruh 5 Q2), scored against what the engine's own AI actually did that
  turn (did it attack the carrier).
- *Free receiver*: on attacker-to-move snapshots, does a standing teammate (not the carrier,
  not in opponent TZ) sit closer to the target endzone than the carrier, and how close.
- *net_st_for_block*: for every chosen BLOCK action, computed the brief's own assist algorithm
  (Guard bypasses TZ-adjacency exclusion) to get net dice class, flagged net<0 (opponent gets
  dice choice) as objectively bad, and checked whether a strictly better block (net≥0,
  attacker-choice) was available to the acting side at that same moment.
- *Sideline/crowd-surf*: opponent standing on y=0/14 adjacent to one of the acting side's
  standing players = a surf opportunity; checked whether a PUSH-off-pitch event followed.

**Sample-size honesty:** 150 games, ~32 snapshots/game ≈ 1750 defender-to-move and ~1400
attacker-to-move observations for the high-frequency candidates (cage, blitzable, blocks) — big
enough for directional read on those. Crowd-surf opportunities are rare in this engine's play
style (45 total across 150 games) — that finding is directional only.

---

## Per-candidate verdicts

| Candidate feature group | Verdict | Basis |
|---|---|---|
| **Cage corner ST/Guard visibility** | **PARTIALLY SUPPORTED** | Formation-quality gap real and large (59.2% of formed cages have a weak worst-corner *and* a strictly better idle teammate within 3 squares — the AI is not assigning corners by player quality). But the *punishment* side of the theory (weak corners get broken more) is **not** observed: break rate is actually slightly *lower* for tier-2 (severe, 4.3%) than tier-0 solid corners (9.0%), on n=574 cage-turns. Defenders DO preferentially target the weakest corner when they choose to hit a corner at all (74.7% of corner-blocks), but rarely convert it into a cage break within that one turn. |
| **carrier_blitzable Chebyshev vs BFS** | **SUPPORTED** | Clean, large-sample signal. The Chebyshev flag is a false positive in 22.6% of all defender-to-move snapshots (396/1752): it says "carrier in blitz range" but a TZ-respecting BFS says no safe path exists — and behaviorally this is confirmed (only 3.5% of those get attacked, vs 62.1% when both Chebyshev and BFS agree there's real danger, vs 1.8% baseline). The reverse error (BFS finds a path Chebyshev misses) is negligible (0.7% of snapshots, 12 total). So the fix's value is concentrated in **reducing false-danger over-flagging**, not catching missed threats — worth noting for how the feature should be framed if implemented. |
| **is_free_receiver / pass-lane blindness** | **NOT SUPPORTED as stated — different root cause identified** | A genuinely open receiver near the target endzone (≤6 squares, no opponent TZ, ahead of carrier) exists in only 0.5% of attacker turns (7/1399), and in nil-nil games specifically only 0.3% (2/669). The AI is not "missing an open receiver" — there is almost never one to miss. 21.8% of turns have *no standing teammate at all* positioned ahead of the carrier toward the endzone; the rest have teammates too far back (bucket ≥8 squares) or in opponent TZ. This points to a **deployment/positioning gap** (nobody is being sent forward as a receiving option in the first place) rather than a **recognition** gap (an available receiver goes unused) — a materially different mechanism than what Okruh 1's `is_free_receiver` feature targets, and one per-player features alone would not fix without also changing how the AI positions non-carrier players during a drive. Consistent with 07-14 mining's finding that PASS is chosen in only 1.4% of decisions and SCORE macro is nearly invisible to search — the bottleneck looks earlier (deployment/macro generation), not at receiver recognition. |
| **net_st_for_block** | **SUPPORTED (revised: 8.0%, was 9.3%)** | After the Wrestle correction below, **8.0%** of all chosen BLOCK actions (255/3183, was 295/3183=9.3%) remain genuinely net-negative with no tactical excuse, and 51.4% of those (131/255, was 53.9%) had a strictly better block available at the same moment — materially the same picture as the original finding, just a cleaner number. 21.4% of the original bad-block set result in the attacker being knocked down within the same turn. Concrete example below (turn 1 of a game) remains valid despite the attacker having Wrestle: net dice was -3 (3-dice, full defender choice), where Wrestle's protection is largely moot (the defender can simply pick "Attacker Down" and bypass "Both Down" entirely), and the target was not the ball carrier — so this specific example does not fall under the Wrestle exemption. |
| **adjacent_to_sideline / crowd surf** | **UNCLEAR / directional only** | Genuine crowd-surf opportunities are rare in this engine's play (45 across 150 games, ~0.9% of all turns) — too sparse for a confident verdict. Of those 45, only 2 (4.4%) were converted into an actual push-off-pitch that turn. Directionally consistent with under-use, but n is too small to call this SUPPORTED; would need a much larger sample or a scripted matchup (Dwarf attacking, opponent pinned near sideline) to confirm. |

---

## Concrete examples

### 1. Cage corner: weak GutterRunner corner gets hit and broken (g0002.json.gz, snapshots 3→5)

Skaven (home) forms a cage; ST2 GutterRunner (id9) sits in a diagonal corner at (9,4) while
ST3 Lineman+Wrestle (id3) and Blitzer+StripBall (id6) are also on the field. Dwarf (away)
targets the weak corner directly:

```
Half 1 turn 3 (away to move) score 0-0 [skaven H vs dwarf A]
  id3 H Lineman+Wrestle ST3 MA7 at (9,6)
  id6 H Blitzer+StripBall ST3 MA7 at (11,6)
  id9 H GutterRunner ST2 MA9 at (9,4)      <- weak corner
  ...
  BLOCK id17 -> id9 OK
  PUSH id9 (9,4)->(8,5)
  KNOCKED_DOWN id9
  ARMOR_BREAK id9
  BLOCK id15 -> id5 OK
  PUSH id5 (9,5)->(8,4)
  KNOCKED_DOWN id5
  ARMOR_BREAK id5
  INJURY id5
  BALL_BOUNCE id-1
  PICKUP id19 roll 4 OK
```

The dwarf blitz lands directly on the ST2 corner player, breaks armor, and follow-up work
knocks the carrier (id5) down too — ball goes loose and is recovered by the away side. This is
a real, observed instance of "weak corner gets exploited" — but it is one of only a minority of
tier-2-corner cages that actually broke this turn (9/209 = 4.3%), which is why the aggregate
verdict above is PARTIALLY, not fully, supported.

### 2. carrier_blitzable false alarm (g0004.json.gz, snapshot 25)

```
Half 2 turn 5 (home to move) score 1-0 [wood-elf H vs human A]
   01234567890123456789012345
   --------------------------
 2|...............oCb........|
 3|.................T........|
 4|.................+b.......|
 5|............+.L..lcc......|
 6|.........B........@.......|   <- carrier @ (19,6)
 7|...........Ll..+.b.b......|
   --------------------------
```

Several human (lowercase) players sit within Chebyshev MA-range of the wood-elf carrier at
(19,6), so `feature_63` (today's aggregate `carrier_blitzable`) would flag danger. But those
players are boxed in by wood-elf tackle zones (uppercase cluster to the left) — a TZ-respecting
BFS finds no free path to an adjacent square within their MA. The engine's own defender AI does
not attack the carrier this turn; play continues elsewhere. This is one of 396 such false-danger
snapshots in the sample (22.6% of all defender-to-move moments) — a large, systematic gap
between what the aggregate Chebyshev feature would say and what is actually true.

### 3. net_st_for_block: turn-1 blitz into a stacked defense (g0001.json.gz, snapshot 1)

```
Half 1 turn 1 (away to move) score 0-0 [orc H vs skaven A]
   01234567890123456789012345
 4|.......K.....lwb..........|
 6|.......K....L.............|
 8|.....B.....KK.+...........|   <- id8 (orc Blitzer+StripBall) heavily assisted
 9|....@.......................|
10|.....B.....B.wbr..........|
  id8 H Blitzer+StripBall ST3 MA6 at (11,10)
  id14 A Lineman+Wrestle ST3 MA7 at (13,10)
  -> id14 moves to (12, 9) (1 steps)
  BLOCK id14 -> id8 OK
  KNOCKED_DOWN id14
  ARMOR_BREAK id14
  INJURY id14
  TURNOVER id-1
```

Skaven's away side opens the game by blitzing an orc Blitzer surrounded by two Black
Orc+Guard and a second Blitzer (multiple defensive assists, net dice strongly favors the
defender per the brief's own formula: computed dice class -3, i.e. 3-dice defender choice). The
attacker (id14) is Knocked Down, Armor Break, Injury, Turnover — on the very first action of the
match. A same-moment better alternative (net ≥ +1, attacker choice) existed elsewhere on the
board. This is exactly the kind of decision `net_st_for_block` as an explicit per-player scalar
is meant to prevent the search from choosing blind.

---

## Overall recommendation

**The human's "too abstract" concern is partially validated, but the candidate list should not
be pruned wholesale — it should be re-weighted and one entry (`is_free_receiver`) should be
re-scoped before Phase A.**

- **Keep as-is, high confidence:** `carrier_blitzable` (BFS fix) and `net_st_for_block` — both
  show large, clean, behaviorally-confirmed gaps in this sample (22.6% false-danger rate; 9.3%
  of blocks net-negative with a better option available >50% of the time). These are the
  strongest candidates to prioritize in Phase A's ridge fit.
- **Keep, but narrow the framing:** cage-corner ST/Guard. The *formation* gap is real and large
  (59.2%) but the brief's implicit narrative — "weak corners get punished, so seeing corner
  quality will fix nil-nil" — is not what this sample shows; break rate does not track corner
  weakness. Frame the feature as fixing a **formation-quality blind spot**, not a **punishment
  avoidance** mechanism, when interpreting Phase A's ridge results (a positive R² gain here
  would say "V can now see who should be in the corner," not "V now predicts cage survival
  better").
- **Re-scope before Phase A:** `is_free_receiver`. As specified (recognize an available open
  receiver) it is testing something that essentially never occurs in this engine's actual play
  (0.5% of turns). If Phase A's ridge fit is run with this feature computed as specified, it
  will almost certainly show ~zero R² contribution — not because per-player features can't help
  passing, but because the real problem sits one step earlier: receivers are not being deployed
  forward at all. Recommend either dropping this candidate from the Phase A batch or replacing
  it with something that measures deployment (e.g. "count of standing teammates within N squares
  of the target endzone, independent of TZ status") so the ridge fit tests the mechanism that
  actually varies in the data.
- **Downgrade priority:** `adjacent_to_sideline` — not enough observed opportunities in normal
  play to ground a verdict either way; fine to keep in the Phase A candidate list (cheap to
  compute, no reason to block it) but don't expect it to move R² much given how rarely the
  situation arises, and don't read a null Phase A result on it as informative without a
  larger/targeted sample.
- **Bigger-sample follow-up, if ever prioritized:** the cage-corner break-rate result (tier-2
  breaking *less* than tier-0, n=574) is counterintuitive enough that it deserves a dedicated,
  larger paired run before trusting it either way — this pass is not powered to resolve it, and
  it directly affects how much weight to put on the cage-corner candidate in Phase A's
  interpretation.

**Net effect on the roadmap:** proceed with Phase A (`proposals_value_signal_roadmap_20260714.md`
§4.2) using this evidence to set expectations and to fix the `is_free_receiver` candidate
definition — do not skip Phase A, and do not read this pass as a reason to abandon per-player
features; two of five candidate groups are strongly grounded, one is grounded but mis-framed,
one needs redefinition, and one is inconclusive. That is a normal, mixed grounding outcome, not
a wholesale rejection of Okruh 1-4.

---

## Follow-up: does Dodge (corner) x Tackle (attacker) explain the tier-2-breaks-less result?

**Hypothesis under test** (raised in review): the cage-corner section above found tier-2 (ST≤2)
corners break *less* than tier-0 (solid) corners — counterintuitive if corner quality were
purely about ST/Guard. Candidate explanation: **Tackle** on the attacking blitzer negates
**Dodge** (cancels the reroll, including on Stumble, per Okruh 2's skill table) — if the AI's
blitzers rarely carry Tackle, a Dodge-skilled weak corner might survive contact more often than
its raw ST suggests, masking the ST effect.

**Method:** re-mined the same 150 saved games (no new data) — for every BLOCK event this run
already logged where the target was a corner occupant of a formed cage, recorded the corner's
own tier (2/1/0), whether the corner player has Dodge, whether the attacking blitzer has Tackle,
then cross-tabbed against (a) immediate knockdown of the corner player and (b) the same
turn-level cage-broken flag used in the original section. New code: `analyze_corner_dodge_tackle()`
in `diag_perplayer_grounding.py` (`python3 diag_perplayer_grounding.py dodge_tackle main`).

**Result — structural confound first:** every ST≤2 positional in the TV1200 roster pool used for
this whole exercise (Human/Wood-Elf Catcher, Skaven Gutter Runner) **has Dodge by roster
definition**. There is no "tier-2, no Dodge" cell in this data — not a sampling gap, a property
of the roster pool itself. So the clean 2x2 the hypothesis calls for doesn't fully exist; the
comparison that *is* available is Tackle-present vs Tackle-absent **within** tier-2 corners (all
of which have Dodge):

| worst corner tier | corner has Dodge | attacker has Tackle | n | knockdown rate | cage-broken rate (this turn) |
|---|---|---|---|---|---|
| solid (tier 0) | no | no | 32 | 37.5% | 12.5% |
| soft (tier 1) | no | no | 23 | 56.5% | 4.3% |
| **severe (tier 2)** | **yes** | **no** | **22** | **31.8%** | **13.6%** |
| **severe (tier 2)** | **yes** | **yes** | **9** | **55.6%** | **22.2%** |
| solid (tier 0) | no | yes | 1 | 100% | 0% |
| soft (tier 1) | no | yes | 2 | 50.0% | 0% |

(cells with n<5 are noise, shown only for completeness — solid/soft-with-Tackle rows are not
readable.)

**Reading it:** within tier-2 corners (n=31 total, small but the two cells that matter aren't
tiny), attacking **with** Tackle roughly **doubles** both the knockdown rate (55.6% vs 31.8%) and
the cage-broken rate (22.2% vs 13.6%) compared to attacking **without** Tackle. Tier-2-without-
Tackle (13.6% broken) sits close to tier-0's baseline (12.5%) — i.e. a Dodge-skilled weak corner
attacked by a non-Tackle blitzer survives about as often as a solid corner, exactly the pattern
that would produce the original section's counterintuitive aggregate (most corner-attacks in
this sample happen to come from non-Tackle blitzers — only 9/31 = 29% of tier-2 corner-attacks
used a Tackle attacker).

**Verdict: hypothesis SUPPORTED directionally, on a small sample.** The Dodge(corner) x
Tackle(attacker) interaction is a real, large-effect-size pattern in this data and plausibly
explains most of the original counterintuitive result — but n=9 vs n=22 within tier-2 is thin,
and the "tier-2 without Dodge" comparison group cannot be tested at all from this roster pool
(would need a non-standard/mutated roster or a different race to get an ST≤2 positional without
Dodge). **Recommendation for Phase A:** refine the cage-corner candidate to condition on Dodge x
Tackle, not raw ST/Guard alone — e.g. an `effective_corner_st` that only credits the Dodge
survivability bonus when the nearest threatening attacker lacks Tackle. Treat the original
"weak corners don't break more" finding as **explained, not refuted** — corner ST still matters,
it's just gated by this skill interaction, which is exactly the kind of thing per-player features
(not today's 73-dim aggregate) are positioned to capture.

---

## Situation survey (broader, for human review)

**Purpose:** the sections above went deep on the original 5 candidate groups; this section trades
depth for breadth — a varied set of concrete situations pulled from the same 150 saved games,
meant to substitute for playing a test game yourself. Prioritized by how clearly each illustrates
a real, recurring, decision-relevant AI behavior (one-off oddities last). Only situations actually
found in the data are included — nothing manufactured. Diagrams use the `render_roles()` renderer
added to `diag_perplayer_grounding.py` (role-letter legend below each board; `@`=ball carrier,
`*`=loose ball, `+`=prone, `_`=stunned; HOME=UPPERCASE, away=lowercase). Reproduce any of these
with `python3 diag_perplayer_grounding.py show main <file> <snapshot>`.

### 1. Opening deployment + immediate first-turn scramble

`g0000.json.gz`, snapshot 0 (Human home vs Orc away, half 1 turn 1)

```
   01234567890123456789012345
   --------------------------
 0|..........................|
 1|..........................|
 2|..........................|
 3|..........................|
 4|o..........C.lkk..........|
 5|.........B..L.....b.......|
 6|...........CL.............|
 7|.......B....Llkb..........|
 8|...........TO.............|
 9|.........B........b.......|
10|...........B.tkb..........|
11|..........................|
12|..........................|
13|..........................|
14|..........................|
   --------------------------
```
Events: PICKUP id11 (kick scattered to the sideline) → BLOCK id4→id13 (knockdown, armor
break) → PASS id11 FAIL → BALL_BOUNCE → TURNOVER, all in the receiving side's very first turn.

Standard deployment: cage-shaped cluster with a Thrower+Ogre anchor, both Blitzers held back,
no player pushed forward as a pass target near the opponent's endzone. Home immediately commits
to recovering a scattered kick and firing off a quick pass that fails, ending the drive on turn
1. **Relevant to:** `is_free_receiver`/deployment (nobody positioned forward from turn 1 — see
"Not observed" verdict above) and general risk-taking (choosing a marginal pass over consolidating
position). **Not a uniquely bad or good decision** — representative baseline of how a drive opens.

### 2. Coordinated multi-block sequence (good execution) followed by a foul

`g0000.json.gz`, snapshot 9 (half 1 turn 5)

```
   01234567890123456789012345
   --------------------------
 0|..........................|
 1|..........................|
 2|.........b................|
 3|.......C.bk...............|
 4|....B..B.Lb..+............|
 5|.....BC.+k@.+.............|
 6|.......+...k.O+...........|
 7|.........L.....+..........|
 8|...........+..............|
 9|..........................|
10|..........................|
11|..........................|
12|..........................|
13|..........................|
14|..........................|
   --------------------------
```
Events this turn: BLOCK id22→id2 (**3-dice**, attacker choice, OK) → knockdown+armor break; BLOCK
id17→id5 (2-dice, OK) → knockdown+armor break; BLOCK id20→id10 (2-dice, OK) → push; then FOUL
id21→id2 (on the just-downed player).

**KOREKCE (doplněno od uživatele 2026-07-15, přepočítáno přesně přes `block_dice()`/assist
algoritmus, ne odhad):** první blok (id22→id2) byl ve skutečnosti **3-dice** (4 offensive
assisti: id17, id18, id19, id20 — dva s Guardem, dva volní — proti 0 defensive assistům; net
+4), ne 2-dice jak text tvrdil. Bloky 2 a 3 byly správně 2-dice (net +1 každý). Všechny tři
tedy byly **minimálně 2-dice, attacker-choice** — žádný nebyl rizikový. Nuance: id17
(BlackOrc+Guard) **nemá Block skill** (jen Guard) — na rozdíl od id22 a id20 (oba Blitzer
varianty s Block), takže druhý blok postrádal Block-skill pojistku proti Both-Down výsledku,
i když šance byla nízká (2-dice attacker-choice). Foul id21→id2 měl 3 offensive assisty
(id17, id19, id20).

Three consecutive **objectively good blocks** (all independently confirmed against
`good_block` scan hits) chain together to knock down 3 human players in one away turn — this
is the AI executing `net_st_for_block`-style reasoning well when the assist math favors it,
a useful counterpoint to the bad-block example in the candidate section above. **GOOD
decision.** Immediately followed by a FOUL on the player it just downed — consistent with the
07-14 mining's "FOUL overuse" finding, now visible in situ.

### 3. Fouling instead of picking up a loose ball 2 squares away

`g0002.json.gz`, snapshot 7 (Skaven home vs Dwarf away, half 1 turn 4)

```
   01234567890123456789012345
   --------------------------
 0|..........................|
 1|..........................|
 2|......W..g..bg............|
 3|.....BRR..................|
 4|.....L.*+b+...............|
 5|.....WBg+s.+.g............|
 6|..........................|
 7|.............+............|
 8|.............+............|
 9|..........................|
10|..........+t..............|
11|..........................|
12|..........................|
13|..........................|
14|..........................|
   --------------------------
```
Loose ball (`*`) sits at (7,4), one square from a prone teammate and two from the away side's
own standing players. Away's turn: several repositioning moves, one DODGE (success), then
**FOUL id19→id9** instead of anyone moving to collect the ball. Recurring pattern — 231 such
"foul while a loose ball sits within 2 squares of a standing teammate" instances across 150
games (~1.5/game). **Relevant to:** not one of the 5 original per-player candidates directly, but
reinforces the 07-14 mining's mechanical finding (FOUL crowds out PICKUP in macro selection) —
per-player features alone would not fix this; it is a macro-priority issue.

### 4. Attrition: a mid-drive casualty as part of ongoing multi-block pressure

`g0001.json.gz`, snapshot 27 (Orc home vs Skaven away, half 2 turn 6, score 1-0)

```
   01234567890123456789012345
   --------------------------
 0|..........................|
 1|..........................|
 2|......K......+..+.........|
 3|+B.BB.K.t...+.............|
 4|@........+................|
 5|.B.........L+b+........r..|
 6|............w.............|
 7|...........L.r............|
 8|..........................|
 9|..........................|
10|..........................|
11|..........................|
12|..........................|
13|..........................|
14|..........................|
   --------------------------
```
BLOCK id2→id14 → PUSH → KNOCKED_DOWN → ARMOR_BREAK → **CASUALTY** (permanent injury,
skaven Lineman+Wrestle removed from the game) mid-drive, in the same turn as a completed PASS
and a second successful block. This matches the brief's "nobody wants to take a block" universal
principle in action — the leading side (orc, 1-0) is grinding the opponent down with repeated
contact rather than just racing the clock. 912 INJURY/CASUALTY events recorded turn≥2 across 150
games — attrition is a large, recurring part of how these games are actually played, not just a
theoretical concern from Okruh 4.

### 5. Defensive screen absent → conceded one-turn touchdown

`g0007.json.gz`, snapshot 6 (Skaven home vs Dwarf away, half 1 turn 4, score 0-0)

```
   01234567890123456789012345
   --------------------------
 0|..........................|
 1|..........................|
 2|..........................|
 3|..........T...............|
 4|...........W+gg...........|
 5|...........B.gg...........|
 6|...........R+b+...........|
 7|..........+.W.............|
 8|.........B...+..R.........|
 9|.................@........|
10|........t.................|
11|..........................|
12|..........................|
13|..........................|
14|..........................|
   --------------------------
```
Zero dwarf players positioned between the skaven Gutter Runner carrier (8 squares from the
away endzone) and that endzone (measured and confirmed via the `screen_absent` scan — this
snapshot has 0 screeners by the fixed geometry, see script note below). One MOVE (8 steps) and a
TOUCHDOWN follow immediately, no resistance offered. **Relevant to:** the Okruh 4 "defensive
screen" tactical sequence — a clean instance of a scoring threat going completely unchallenged
positionally, unrelated to a single decision error, more a standing-formation gap.

*(Note: an earlier draft of the screen-detection code in this survey measured proximity to the
wrong endzone — it's fixed now; `analyze in cmd_survey` `defensive screen` section of
`diag_perplayer_grounding.py`, screen_present/absent counts above reflect the corrected version.)*

### 6. Screen present (5 defenders) but broken by a cleared path + fast dodge-and-GFI dash

`g0009.json.gz`, snapshot 6 (Wood Elf home vs Human away, half 1 turn 4, score 0-0)

```
   01234567890123456789012345
   --------------------------
 0|..........................|
 1|..........................|
 2|...........L.BC...........|
 3|.............+T..+........|
 4|.........L....C.@c....b...|
 5|.................l........|
 6|.........O.L.+..t.......bb|
 7|.............L+...........|
 8|.............+............|
 9|..........................|
10|...............b..........|
11|..........................|
12|..........................|
13|..........................|
14|..........................|
   --------------------------
```
5 human defenders sit between the wood-elf Catcher carrier and the target endzone. The AI
first **BLOCKs a defender out of the lane** (id1→id16, OK, push), then the carrier (MA8+Dodge+
Sprint) **DODGEs** out of a tackle zone (success) and takes **two GFIs** (both succeed) to cover
the remaining distance for a touchdown. **GOOD, notably sophisticated decision** — a genuine
2-step combo (clear the lane, then commit the fast player through the gap) that a simple
screen-count heuristic would have called "well defended." Worth flagging honestly: this also
means a naive `screen_count` per-player feature could be *misleading* on its own for fast/Dodge
carriers — screens matter less against high-MA Dodge players than the raw defender count
suggests.

### 7. Loose-ball scramble after a turnover (multi-turn, two balls loose at once)

`g0003.json.gz`, snapshots 16-19 (Dwarf home vs Wood Elf away, half 2 turns 1-2, score 0-0)

Turn sequence (condensed): away PICKUP → PASS FAIL → BALL_BOUNCE → TURNOVER (snap 16); home
repositions without touching the ball (snap 17); away dodges twice, blocks, ball still loose
(snap 18); home dodges, blocks, **FOULs the just-downed defender**, second armor break (snap
19) — four consecutive turn-boundaries with the ball never cleanly recovered by either side.
Directly consistent with the 07-14 mining's loose-ball-scramble finding (40.8% of turn
boundaries in nil-nil games have the ball on the ground) — this is what that statistic looks
like turn-by-turn. **Relevant to:** not one specific Okruh candidate, but underlines that
`carrier_blitzable`/`net_st_for_block` refinements alone will not fix nil-nil without also
addressing pickup mechanics (already flagged as master-list item 7, outside this per-player scope).

### 8. Carrier takes a risky double-GFI gamble and loses the ball

`g0048.json.gz`, snapshot 11 (Dwarf home vs Wood Elf away, half 1 turn 6, score 0-0)

```
   01234567890123456789012345
   --------------------------
 0|..........................|
 1|..........................|
 2|..........................|
 3|.......B.@b....c..........|
 4|.........Sb++T............|
 5|.....GG...+.l.............|
 6|.......G.T++..............|
 7|....G.....................|
 8|..............+...........|
 9|............+..l..........|
10|............+.............|
11|..........................|
12|..........................|
13|..........................|
14|..........................|
   --------------------------
```
Wood-elf carrier (id19) already outside safe movement range attempts **two consecutive GFI
rolls** to close the distance — first succeeds, second **FAILS**, triggering an armor-break
knockdown and a loose ball → TURNOVER. A block earlier the same turn (id22→id4) had gone well,
but the double-GFI carrier gamble undoes it. **Relevant to:** `carrier_can_score`/risk assessment
— the same game later (see #11) shows the opposite, more conservative posture once ahead,
suggesting risk tolerance shifts with game state but isn't always well-calibrated in the moment
(17 such carrier-risked-a-failed-dodge/GFI-into-turnover instances across 150 games).

### 9. Hand-off completes a productive turn (clear a path, then hand off)

`g0016.json.gz`, snapshot 10 (Orc home vs Skaven away, half 1 turn 6, score 0-1)

```
   01234567890123456789012345
   --------------------------
 0|..........................|
 1|..........................|
 2|.......K..................|
 3|.........l................|
 4|.......K..................|
 5|......BK...Lw.r.r.........|
 6|.....@.B....L..b..........|
 7|......B..r...Tt...........|
 8|............K.+...........|
 9|.......wb.................|
10|...............r..........|
11|..........................|
12|..........................|
13|..........................|
14|..........................|
   --------------------------
```
BLOCK id2→id15 (2-dice, OK, knockdown, armor break) clears a defender, then a confirmed
hand-off (adjacency-verified, no bounce/pass event — `CATCH id2 roll 3 OK` right after the
carrier ends its move next to id2) transfers the ball forward, plus a second good block
(id4→id16). A tidy 3-action combo. Tempered by one gratuitous **FOUL** (id3→id13) on an
unrelated part of the board the same turn. 84 confirmed clean hand-offs found across 150 games
(stricter geometric check than the raw CATCH-without-PASS heuristic, which over-counts bounce
recoveries as hand-offs — noted here for anyone reusing the script). **GOOD decision** on the
main sequence.

### 10. A successful long pass

`g0014.json.gz`, snapshot 27 (Wood Elf home vs Human away, half 2 turn 6, score 1-0)

```
   01234567890123456789012345
   --------------------------
 0|*.........................|
 1|..........................|
 2|..................c.b.....|
 3|..................c.......|
 4|..........TLL......b......|
 5|.......B.....l......b.....|
 6|..........+...............|
 7|..........C.ll............|
 8|...........+..t...........|
 9|...........o..............|
10|..........COL.b...........|
11|..........................|
12|..........................|
13|..........................|
14|..........................|
   --------------------------
```
PICKUP id10 → BLOCK (clears a defender) → **PASS id10 roll 4 OK** → **CATCH id7 roll 2 OK** →
another BLOCK to protect the new carrier. One of only 139 PASS events across 150 games (~0.9/
game) — genuinely rare, but when it happens here it's part of a coherent multi-step sequence
(pick up, clear a lane, throw, protect the catch), not a desperate heave. **Relevant to:**
`is_free_receiver` / passing-game candidates — supports the "deployment is the bottleneck, not
recognition" framing above: this pass worked because a receiver happened to already be
positioned to catch it, which is the exception rather than the rule in this sample.

### 11. Stalling while leading: a literal zero-action turn

`g0048.json.gz`, snapshot 23 (Dwarf home vs Wood Elf away, half 2 turn 4, score 0-1) — same
game as #8, later in the match

```
   01234567890123456789012345
   --------------------------
 0|..........................|
 1|..........................|
 2|...........G...B..........|
 3|..............G..o+.......|
 4|.................lt.c.....|
 5|...........G...S.ll@......|
 6|.............LG..lc.b.....|
 7|............T..B..+.......|
 8|..........................|
 9|..........................|
10|..........................|
11|..........................|
12|..........................|
13|..........................|
14|..........................|
   --------------------------
```
Away (wood-elf) leads 1-0 in the second half, holds the ball (`@`), and this turn has **zero
logged events** — no move, no block, nothing. Confirmed against the raw turn record, not a
rendering artifact. 39 such "leading, holds ball, doesn't advance" turns found across 150 games.
**Relevant to:** clock-management/stalling behavior mentioned in the brief's Okruh 5 — the AI
does appear capable of "protecting a lead" by simply not acting, distinct from the more concerning
nil-nil stalling (this game is not nil-nil; the lead is real and the pass-turn is a rational
clock-killing move, not obviously a bug).

### 12. Crowd-surf casualty (rare, one-off in this sample)

`g0076.json.gz`, snapshot 27 (Orc home vs Skaven away, half 2 turn 6, score 0-0)

```
   01234567890123456789012345
   --------------------------
 0|.@w.......................|
 1|.............w............|
 2|.............b............|
 3|....B.....................|
 4|..........B....K..........|
 5|..............+.LK.....r..|
 6|..........+++.+.b.........|
 7|..............r...........|
 8|..........................|
 9|..........................|
10|................K.........|
11|............B.............|
12|..........................|
13|..........................|
14|..........................|
   --------------------------
```
BLOCK id11→id14 pushes the skaven player off the pitch at the (2,0) corner → **INJURY,
CASUALTY** without an armor roll (crowd-surf rules). Only 2 genuine crowd-surf conversions found
in 45 opportunities across the whole 150-game sample (matches the UNCLEAR verdict for
`adjacent_to_sideline` above) — included here as a real, if rare, observed instance rather than
a manufactured one. **Relevant to:** `adjacent_to_sideline` — confirms the mechanism exists and
works when it triggers, just triggers rarely in this play style/roster pool.

---

## Correction: `is_free_receiver` reframed as relative mobility advantage

**Why re-test:** the original `is_free_receiver` definition (standing teammate, no opponent TZ,
≤6 squares from the endzone, geometrically AHEAD of the carrier) found only 0.5% prevalence and
was judged NOT SUPPORTED — reframed as a deployment gap. The human reviewing this raised a fair
objection: a useful receiving/advancing option does not need to be ahead of the carrier at all.
What matters is **relative mobility** — does a teammate have a safe path to advance that the
carrier currently lacks, wherever that teammate happens to be standing (including behind the
carrier)? This is closer to what `is_free_receiver` is actually meant to capture: "is there a
better ball-progression option than what the carrier can do right now."

**Method (same 150 saved games, no new data):** extended the BFS/TZ machinery already built for
`carrier_blitzable` (`bfs_can_blitz`) into a general-purpose flood fill —
`bfs_safe_reachable(pos, ma, blocked_tz, occupied)` returns every square a player can reach in
`ma` steps without ever entering an opponent tackle zone or an occupied square (0-dodge, same
approximation as the carrier_blitzable candidate). This is exactly the "one flood-fill per
player, shared across features" pattern Opus's Q2 answer scoped for the C++ implementation
(`team1_results_opus.md`) — reused here as a second consumer of the same machinery, not new
infrastructure. For every attacker-to-move snapshot (same 1,399-snapshot set as the original
receiver analysis): computed `safe_progress = current_dist_to_endzone − best_reachable_dist_to_endzone`
for the carrier and for every standing teammate. **Relative mobility advantage** = a teammate
whose `safe_progress` beats the carrier's by **≥3 squares** (`MOBILITY_ADVANTAGE_THRESHOLD = 3`
in `diag_perplayer_grounding.py`; `python3 diag_perplayer_grounding.py mobility main`). "Capitalized"
= the AI threw a PASS, or the advantaged teammate specifically caught the ball (hand-off), that
same turn.

**Result — prevalence is dramatically higher than the original definition:**

| | original (`ahead of carrier`) | reframed (`relative mobility`) |
|---|---|---|
| Overall prevalence | 0.50% (7/1,399) | **30.81% (431/1,399)** |
| Nil-nil-game turns | 0.30% (2/669) | **29.75% (199/669)** — essentially the same rate as non-nil-nil turns, so this factor does not concentrate specifically in stalled/nil-nil drives, it's a constant background condition |
| Capitalized when present | n/a (too rare to measure) | **3.0% (13/431)** |

The reframing was right: this is not a rare edge case, it is present in roughly **3 of every 10
attacker turns** — the carrier is frequently NOT the best-positioned player on the pitch to keep
advancing, and the AI capitalizes on that almost never (97% of the time it does something else —
mostly combat, per the examples).

**Race breakdown (attacking side's own race) — Wood Elf finding:**

| race | attacker-turns | mobility-advantage rate | capitalized when present |
|---|---|---|---|
| skaven | 217 | 57.1% | 1.6% (2/124) |
| **wood-elf** | 234 | **41.0%** | **3.1% (3/96)** |
| human | 307 | 32.3% | 7.1% (7/99) — highest of all 5 |
| dwarf | 308 | 20.8% | 1.6% (1/64) |
| orc | 333 | 14.4% | 0.0% (0/48) |

Both parts of the coordinator's question resolve clearly:

1. **Is the situation itself more common for Wood Elf?** Yes — 41.0%, well above the pooled
   30.8% average, consistent with the brief's own reasoning (high MA/AG spread — Wardancer MA8
   vs Lineman MA7 vs Treeman MA2 creates more mobility differentials than a flatter roster like
   Orc's, whose 14.4% is the lowest of the five).
2. **Does the AI capitalize on it more for Wood Elf, given passing is supposed to be their core
   strategy?** **No** — 3.1% capitalization, statistically indistinguishable from Skaven (1.6%)
   or Dwarf (1.6%), well below Human's 7.1%, and Wood Elf has *zero* structural excuse (unlike
   Dwarf, which per the brief's own racial table has no Pass skill on its roster at all — Wood
   Elf's Wardancers and Thrower explicitly carry Pass/Catch/Block for exactly this purpose).
   **This is the sharper finding**: a race whose entire strategic identity in the brief's own
   Okruh 3 table is built around exploiting mobility differences via passing shows no measurable
   sign of doing so more than a Dwarf team that structurally cannot pass at all. That is strong
   evidence the bottleneck is not receiver *availability* (Wood Elf has more of it) and not
   racial *capability* (Wood Elf has Pass/Catch) but something upstream and race-agnostic —
   consistent with the 07-14 mining finding that PASS is chosen in only 1.4% of decisions
   game-wide and SCORE-family macros are largely invisible to search, i.e. a macro-generation/
   search-prioritization gap that per-player features alone will not fix.

**Concrete example** (`g0004.json.gz`, snapshot 6, Wood Elf home vs Human away, half 1 turn 4,
score 0-0):

```
   01234567890123456789012345
   --------------------------
 3|...........L..............|
 4|........B....b............|
 5|.........C..+.............|
 6|..........O.L+............|
 7|............BtLb..........|
 8|..........l.@L+...b.......|
 9|...........T..c...........|
10|.............cb...........|
   --------------------------
  id8  H Catcher ST2 MA8 at (12,8)   <- carrier, boxed in, safe_progress=0
  id11 H Wardancer+StripBall ST3 MA8 at (8,4)  <- safe_progress=8, wide open
```
The carrier (`@`, id8) is surrounded by human tackle zones and cannot safely advance a single
square toward the endzone this turn (`safe_progress=0`). Teammate id11, a Wardancer with the
same MA, sits with a completely clear 8-square lane. The turn instead plays out as a chain of
five BLOCK actions (mostly successful) and ends in a TURNOVER — no PASS, no hand-off toward id11.

**Verdict: reframed candidate SUPPORTED, recommend it replace the original `is_free_receiver`
definition for Phase A.** At 30.8% overall prevalence (vs. 0.5%) this is a real, frequent
pattern, not a rare edge case — the original geometric "ahead of carrier" framing was simply the
wrong measure. The near-zero capitalization rate across all five races (including Wood Elf,
whose entire game plan depends on exactly this) points at a macro-generation/search bottleneck
shared with the already-known PASS/SCORE visibility problems from the 07-14 mining, rather than
at recognition per se — so the practical recommendation is: keep a mobility-advantage-style
per-player feature in the Phase A candidate set (it is cheap, reuses BFS machinery already
planned for `carrier_blitzable`, and the underlying signal is real and frequent), but do not
expect it alone to fix passing behavior — pair it with the macro-generation fix already flagged
in the roadmap, or Phase A's ridge fit will likely show the feature *predicts* good outcomes
(when capitalized, which is rare) without much practical uptake until the search-side gap closes.

---

## Correction: Wrestle filter on the net_st_for_block "bad blocks" count

**Concern raised:** the original `net_st_for_block` analysis flagged every net-negative
(opponent dice-choice) block as "objectively bad." Per the brief's own Okruh 2 skill table, a
**Wrestle** attacker blitzing the ball carrier deliberately accepts a net-negative situation —
Wrestle converts a "Both Down" result into a safe knockdown for the Wrestler (no armor/injury
roll), so forcing Both Down on the carrier (optionally combined with Strip Ball to also dislodge
the ball) is a legitimate tactical goal, not a search/evaluation mistake.

**Method:** re-ran `analyze_blocks()` (same 150-game dataset, same 3,183 chosen blocks) with a
breakdown of the 295 original net-negative blocks by attacker skills
(`python3 diag_perplayer_grounding.py blocks_wrestle main`).

| | count | % of the 295 flagged blocks |
|---|---|---|
| Attacker has Wrestle | 40 | 13.6% |
| ...and target is the ball carrier | 20 | 6.8% |
| ...and attacker also has StripBall (Wrestle+StripBall-on-carrier) | **0** | 0.0% |
| Attacker does NOT have Wrestle (genuinely bad) | 255 | 86.4% |

**Wrestle+StripBall-on-carrier never occurs** in this roster pool — structural, not a sampling
gap: no TV1200 positional in the 5-race pool combines both skills on the same player (mirrors the
Dodge/ST≤2 confound found in the cage-corner follow-up above). So the "intentional strip" case
the brief specifically calls out cannot be tested from this data at all; only the plain-Wrestle
case (20/295 = 6.8%, carrier-targeted, no strip) is observable.

**Corrected genuinely-bad rate: 8.0% of all chosen blocks (255/3,183), down from 9.3%** — a real
but modest correction (14% relative reduction), not a material change to the SUPPORTED verdict.
Of the corrected 255, 51.4% (131/255, was 53.9%) still had a strictly better block available at
the same moment.

**Honest caveat on the correction itself:** Wrestle's protection is strongest when the block
would otherwise be net=-1 (2-dice, defender choice) — a result set where "Both Down" is a live
option the defender might reasonably pick. At net≤-2 (3-dice, full defender choice), the defender
can simply choose "Attacker Down" and bypass Both Down entirely, making Wrestle's mitigation
largely moot — so not every excluded Wrestle-attacker block is equally "intentional-good"; this
correction is directional (exclude all Wrestle blocks, per the coordinator's instruction) rather
than dice-severity-aware. The concrete turn-1 example kept in the main candidate table above
happens to be a clean illustration of this nuance: the attacker (id14) has Wrestle, but the
block was net=-3 against a non-carrier target — Wrestle offers little real protection there, and
the outcome (Knocked Down → Armor Break → Injury → Turnover) bears that out. **Recommendation for
Phase A:** if `net_st_for_block` is implemented as a per-player feature, consider a Wrestle-aware
variant that only treats Wrestle as risk-mitigating at net=-1 against the carrier specifically,
not as a blanket exemption at any negative-dice level.
