"""Test 2 analysis: REPOS search spread at MCTS-400 vs MCTS-100 baseline.
Pre-registered rule: evidence/fable_teacher_signal_report_20260810.md par 1.2.
baseline_100 comes from test-1 output (same engine). Reads rows_m400_*.jsonl,
writes m400_spread_results.json/.out into evidence/diag_teacher_signal_20260810/.
"""
import json
import os

import numpy as np

DIR = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = "/home/jan/claude/bloodbowl/evidence/diag_teacher_signal_20260810"
FILES = ["rows_m400_dwsk_a.jsonl", "rows_m400_dwsk_b.jsonl",
         "rows_m400_wesk.jsonl"]
TYPE_REPOS = 8


def stats(decs):
    spreads = [float(d["vis"].max() - d["vis"].min()) for d in decs]
    cms = [float((d["n_vis"] == d["n_vis"].max()).sum()) / len(d["n_vis"])
           for d in decs]
    return dict(n=len(decs),
                search_spread_mean=float(np.mean(spreads)),
                search_spread_median=float(np.median(spreads)),
                chance_maxset_mean=float(np.mean(cms)))


def main():
    decs = []
    for fn in FILES:
        path = os.path.join(DIR, fn)
        if not os.path.exists(path):
            print(f"WARN missing {fn}")
            continue
        with open(path) as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    d = json.loads(line)
                except json.JSONDecodeError:
                    print(f"WARN malformed line in {fn}")
                    continue
                race = d["race_h"] if d["persp"] == "home" else d["race_a"]
                cands = [c for c in d["cands"] if c["t"] == TYPE_REPOS]
                if len(cands) < 3:
                    continue
                total = sum(c["v"] for c in cands)
                decs.append(dict(
                    race=race,
                    vis=np.array([c["v"] for c in cands]),
                    n_vis=np.array([c["n"] for c in cands]),
                ))
    out = {}
    for scope in ["dwarf", "wood-elf", "skaven", "all"]:
        sel = [d for d in decs if scope == "all" or d["race"] == scope]
        if sel:
            out[scope] = stats(sel)

    # pre-registered rule vs baseline_100 from test 1 (same engine)
    base_path = os.path.join(OUT_DIR, "teacher_value_results.json")
    if os.path.exists(base_path):
        base = json.load(open(base_path))["dwarf"]
        b100 = base["search_spread_mean"]
        cm100 = base["chance_maxset_mean"]
        s400 = out["dwarf"]["search_spread_mean"]
        cm400 = out["dwarf"]["chance_maxset_mean"]
        if s400 >= 2 * b100 or cm400 <= 0.30:
            verdict = "search se rozpoctem PROBUDI"
        elif s400 <= 1.5 * b100 and cm400 >= 0.40:
            verdict = "search NEROZLISI ani s 4x rozpoctem"
        else:
            verdict = "neprukazne (mezi prahy)"
        out["baseline_100_dwarf"] = dict(search_spread_mean=b100,
                                         chance_maxset_mean=cm100)
        out["verdict_preregistered"] = verdict
    else:
        out["verdict_preregistered"] = "baseline z testu 1 chybi"

    with open(os.path.join(OUT_DIR, "m400_spread_results.json"), "w") as f:
        json.dump(out, f, indent=1)
    lines = []
    for k, v in out.items():
        lines.append(f"{k}: {v}")
    text = "\n".join(lines)
    print(text)
    with open(os.path.join(OUT_DIR, "m400_spread_results.out"), "w") as f:
        f.write(text + "\n")


if __name__ == "__main__":
    main()
