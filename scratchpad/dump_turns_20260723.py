"""Dump detailed turn-by-turn narrative for one game (read-only)."""
import gzip
import json
import sys

fp = sys.argv[1]
lo = int(sys.argv[2]) if len(sys.argv) > 2 else 0
hi = int(sys.argv[3]) if len(sys.argv) > 3 else 999

rec = json.load(gzip.open(fp, "rt"))
print(f"{rec['home_race']}(home,->x25) vs {rec['away_race']}(away,->x0) "
      f"final {rec['home_score']}-{rec['away_score']}")
for i, t in enumerate(rec["turn_logs"]):
    if not (lo <= i <= hi):
        continue
    car = t["ball_carrier_id"] if t["ball_held"] else None
    print(f"\n[{i}] h{t['half']}t{t['turn']} {t['active_team']} "
          f"score {t['home_score']}-{t['away_score']} "
          f"ball=({t['ball_x']},{t['ball_y']}) carrier={car} "
          f"turnover={t['turnover']} td={t['touchdown']}")
    # players near the ball at start of turn
    bx, by = t["ball_x"], t["ball_y"]
    near = []
    for side, pl in (("H", t["home_players"]), ("A", t["away_players"])):
        for p in pl:
            d = max(abs(p["x"] - bx), abs(p["y"] - by))
            if d <= 3:
                near.append(f"{side}{p['id']}@({p['x']},{p['y']})"
                            f"{'*' if p['state'] != 0 else ''}d{d}")
    print("  near ball:", " ".join(near) if near else "(nobody within 3)")
    for e in t["events"]:
        s = "" if e["success"] else " FAIL"
        extra = f" roll={e['roll']}" if e["roll"] else ""
        tgt = f"->tgt{e['target_id']}" if e["target_id"] >= 0 else ""
        print(f"  {e['type']} p{e['player_id']}{tgt} "
              f"({e['from_x']},{e['from_y']})->({e['to_x']},{e['to_y']})"
              f"{extra}{s}")
