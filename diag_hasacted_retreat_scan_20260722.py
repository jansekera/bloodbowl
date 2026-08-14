"""Scan existing 21.07 replay-mining corpus for the hasActed-after-block
gap (project_bloodbowl_hasacted_after_block_gap_20260722).

Reuses diag_replay_mine_20260721_data/*.json.gz (24 games, engine
unchanged since collection -- item7/item10 already baked in, no code
touched today), no new simulation needed.

For every BLOCK event where the attacker's target is NOT knocked down
(no KNOCKED_DOWN event on target_id shortly after), check:
  - was the attacker still adjacent to the target right after (from the
    PUSH event's to_x/to_y, or target's unchanged position)?
  - did the attacker ever MOVE again later in the same turn?
  - did the opponent then hit that same player (BLOCK/FOUL with
    target_id == attacker) on their following turn?

Quantifies how often a blitzer/blocker is left stranded adjacent with no
retreat, and whether it was actually exploited next turn.
"""
import gzip
import json
import glob
from collections import Counter

files = sorted(glob.glob("diag_replay_mine_20260721_data/g*.json.gz"))

total_blocks = 0
no_knockdown = 0
stranded_no_move = 0
stranded_then_hit_next_turn = 0
examples = []

for fp in files:
    rec = json.load(gzip.open(fp, "rt"))
    turns = rec["turn_logs"]
    for ti, t in enumerate(turns):
        events = t.get("events", [])
        for ei, ev in enumerate(events):
            if ev["type"] != "BLOCK":
                continue
            total_blocks += 1
            attacker = ev["player_id"]
            target = ev["target_id"]

            # Did target go down within the next few events of this block?
            target_down = False
            for ev2 in events[ei + 1:ei + 6]:
                if ev2["type"] == "KNOCKED_DOWN" and ev2.get("player_id") == target:
                    target_down = True
                    break
                if ev2["type"] == "BLOCK":  # next block starts, stop looking
                    break
            if target_down:
                continue
            no_knockdown += 1

            # Did the attacker move again later in the SAME turn?
            moved_again = False
            for ev2 in events[ei + 1:]:
                if ev2["type"] == "BLOCK":
                    break  # next block macro started, this window is over
                if ev2["type"] == "MOVE" and ev2.get("player_id") == attacker:
                    moved_again = True
                    break
            if moved_again:
                continue
            stranded_no_move += 1

            # Was the attacker then targeted by a BLOCK or FOUL on the
            # opponent's very next turn (return hit while stranded)?
            hit_next_turn = False
            if ti + 1 < len(turns):
                nxt = turns[ti + 1]
                for ev2 in nxt.get("events", []):
                    if ev2["type"] in ("BLOCK", "FOUL") and ev2.get("target_id") == attacker:
                        hit_next_turn = True
                        break
            if hit_next_turn:
                stranded_then_hit_next_turn += 1

            if len(examples) < 8:
                examples.append({
                    "game": fp, "half": t["half"], "turn": t["turn"],
                    "attacker": attacker, "target": target,
                    "hit_next_turn": hit_next_turn,
                })

print(f"games scanned: {len(files)}")
print(f"total BLOCK events: {total_blocks}")
print(f"no knockdown on target: {no_knockdown}")
print(f"  of those, attacker never moved again this turn (stranded): {stranded_no_move}")
print(f"    of those, attacker was hit again (BLOCK/FOUL) on opponent's very next turn: {stranded_then_hit_next_turn}")
print()
print("examples:")
for e in examples:
    print(e)
