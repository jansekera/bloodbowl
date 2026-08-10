# Situation: home's turn with the opponent's carrier far out of reach (CORRECTED)

Found via fresh replay-mining (`diag_replay_mine_20260721.py`), game
`g0014.json.gz` (home=wood-elf, away=human, final score 1-0), half 2,
turn 1, home team's turn.

**Revised 2026-07-21 after user questioning the starting layout
("startovní rozestavení mi nesedí") surfaced a real error in the first
write-up -- see "Correction" below before reading anything that cites
the old version.**

## Correction (what was wrong the first time)

Player IDs run 1-11 for home and 12-22 for away in this engine's board
snapshots. The original write-up called the ball carrier "home player
22" and described two other players as opposing markers -- both wrong.
**Player 22, and the two players next to him (20, 21), are ALL on the
AWAY team** -- teammates escorting their own carrier, not defenders
closing him down. Separately, every home player (ids 1-11) was clustered
at x=7-12, nowhere near the away carrier at x=23. So the original framing
("AI ignored its own golden scoring chance") was backwards on whose ball
it was and backwards on who was actually near it.

## How the ball actually got there (traced from turn 1 of the match)

- Half 1, turn 1: ball starts loose at `(3,5)`. **Home (wood-elf)
  receives the opening kickoff** and recovers it in their own turn 1
  (`PICKUP`), carried by player 11 from then on.
- Half 1, turn 6: home's drive scores a TOUCHDOWN (1-0), player 11.
- Half 1, turn 6 (away): away receives the ensuing kickoff, recovers the
  ball, and runs a long advancing drive across many players/moves,
  pushing all the way to `(25,10)` before losing/dropping it there.
- Half 1, turns 7-8: the ball changes hands/bounces a couple more times
  (turnovers, recoveries) between roughly `(25,10)` and `(13,3)`.
- Half 2, turn 1 (away, acting first): away recovers the ball again and
  pushes it to `(23,6)` -- deep in HOME's defensive territory (away's own
  scoring target is the opposite end, x=0, so this is away's carrier far
  from his own goal, not close to scoring).
- Half 2, turn 1 (home, the turn this situation is actually about):
  board snapshot at the start of home's turn --

```
home (all far from the ball): 1(12,3) 2(11,6) 3(12,10) 4(11,4) 5(11,7)
      6(11,10) 7(10,4) 8(10,7) 9(10,10) 10(7,5) 11(7,9)
away (carrier + escort): 12(17,2) 13(12,4) 14(19,2) 15(12,7) 16(14,3)
      17(13,4) 18(20,3) 19(21,4) 20(22,6) 21(23,5) 22(23,6, HAS BALL)
```

No home player is anywhere close enough to threaten the away carrier this
turn (nearest home player is at x=10-12, the carrier is at x=23).

## What home actually did with this turn

With no play available near the ball, home used the turn on player 2
(at `(11,6)`, nowhere near the ball either) attempting a risky
double-dodge:

```
DODGE  player 2  (11,6)->(12,5)  roll 5  success True
MOVE   player 2  (11,6)->(12,5)  roll 0  success True
DODGE  player 2  (12,5)->(13,5)  roll 5  success False   <- failed
ARMOR_BREAK player 2  (13,5)     roll 8  success True
INJURY player 2  (13,5)          roll 7  success False   <- stunned, not KO'd
TURNOVER  roll 2  success True
```

The second dodge failed, ending home's turn immediately (any failed
dodge/GFI is an instant turnover) -- for zero gain, since this action had
nothing to do with the ball or the opponent's advanced position anyway.

## The question worth discussing (revised)

Since no home player could have reached the away carrier this turn
regardless, the earlier "did the AI skip its best play" framing doesn't
apply. The real question is narrower: **was risking a double-dodge with
player 2 -- for no apparent tactical payoff (not near the ball, not
threatening the carrier, not covering the likely advance lane) -- a
reasonable use of a turn where nothing else urgent was happening, or is
this closer to the AI taking a pointless risk when passing/repositioning
safely would cost nothing?** Not yet investigated in `macro_actions.cpp`
-- worth deciding with the user whether this is worth digging into before
speculating further.

## Related context

Not the same theme as originally thought (the "marked carrier, no
blitzer available" gap from
[[project_bloodbowl_twolevel_perplayer_synthesis_20260720]] /
`evidence/marked_carrier_no_blitzer_gap_20260720.md`) -- that finding was
about the AI's OWN carrier being marked with no rescue available; this
situation, correctly read, is about the OPPONENT's carrier being safely
out of reach and the AI choosing a seemingly gratuitous risk elsewhere
instead. Distinct question, kept separate.
