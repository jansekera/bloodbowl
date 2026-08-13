"""Test 1 analysis: is the teacher's leaf value flat across REPOSITION
candidates? Pre-registered protocol + thresholds:
evidence/fable_teacher_signal_report_20260810.md par 1.1 (written 14:25-14:40
UTC before any results existed). Reads vrows_*.jsonl, writes
teacher_value_results.json/.out into evidence/diag_teacher_signal_20260810/.
"""
import json
import os

import numpy as np

DIR = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = "/home/jan/claude/bloodbowl/evidence/diag_teacher_signal_20260810"
FILES = ["vrows_dwsk_a.jsonl", "vrows_dwsk_b.jsonl", "vrows_wesk.jsonl"]


def load():
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
                if len(d["cands"]) < 3:
                    continue
                race = d["race_h"] if d["persp"] == "home" else d["race_a"]
                decs.append(dict(
                    race=race,
                    v015=np.array([c["v015"] for c in d["cands"]]),
                    vheur=np.array([c["vheur"] for c in d["cands"]]),
                    vnn=np.array([c["vnn"] for c in d["cands"]]),
                    vis=np.array([c["v"] for c in d["cands"]]),
                    n_vis=np.array([c["n"] for c in d["cands"]]),
                    tz=np.array([c["tz"] for c in d["cands"]]),
                    prog=np.array([c["prog"] for c in d["cands"]]),
                    base=d["v015_base"],
                ))
    return decs


def stats_for(decs, scope):
    sel = [d for d in decs if scope == "all" or d["race"] == scope]
    if not sel:
        return None
    s_val = np.array([d["v015"].max() - d["v015"].min() for d in sel])
    s_heur = np.array([d["vheur"].max() - d["vheur"].min() for d in sel])
    s_nn = np.array([d["vnn"].max() - d["vnn"].min() for d in sel])
    s_search = np.array([d["vis"].max() - d["vis"].min() for d in sel])
    chance_maxset = np.array([
        (d["n_vis"] == d["n_vis"].max()).sum() / len(d["n_vis"]) for d in sel])
    tz_range = np.array([d["tz"].max() - d["tz"].min() for d in sel])
    prog_range = np.array([d["prog"].max() - d["prog"].min() for d in sel])

    # doctrinal pairs: |tz_i - tz_j| >= 2 within a decision
    pair_dv, decs_with_pair = [], 0
    for d in sel:
        tz, v = d["tz"], d["v015"]
        found = False
        for i in range(len(tz)):
            for j in range(i + 1, len(tz)):
                if abs(int(tz[i]) - int(tz[j])) >= 2:
                    pair_dv.append(abs(float(v[i] - v[j])))
                    found = True
        decs_with_pair += int(found)

    # diagnostic: within-decision Pearson corr(v015, visit fraction)
    corrs = []
    for d in sel:
        if d["v015"].std() > 1e-9 and d["vis"].std() > 1e-9:
            corrs.append(float(np.corrcoef(d["v015"], d["vis"])[0, 1]))

    # diagnostic: move vs stay (best candidate value - base value)
    best_minus_base = np.array([d["v015"].max() - d["base"] for d in sel])

    return dict(
        n=len(sel),
        S_val_median=float(np.median(s_val)),
        S_val_mean=float(np.mean(s_val)),
        S_val_p90=float(np.percentile(s_val, 90)),
        S_heur_median=float(np.median(s_heur)),
        S_nn_median=float(np.median(s_nn)),
        search_spread_mean=float(np.mean(s_search)),
        search_spread_median=float(np.median(s_search)),
        chance_maxset_mean=float(np.mean(chance_maxset)),
        D_val_median=float(np.median(pair_dv)) if pair_dv else None,
        n_doctrinal_pairs=len(pair_dv),
        coverage_doctrinal=decs_with_pair / len(sel),
        tz_range_median=float(np.median(tz_range)),
        prog_range_median=float(np.median(prog_range)),
        corr_v015_visits_mean=float(np.mean(corrs)) if corrs else None,
        corr_n=len(corrs),
        best_minus_base_median=float(np.median(best_minus_base)),
    )


def verdict(dw):
    """Pre-registered rule, par 1.1."""
    s = dw["S_val_median"]
    d = dw["D_val_median"]
    cov = dw["coverage_doctrinal"]
    homog = (dw["tz_range_median"] <= 1) and (dw["prog_range_median"] <= 0.08)
    if s < 0.010:
        if cov < 0.05 and homog:
            return "(c) pozice se pozorovatelne nelisi (vsechny podminky c)"
        return "(b) slepa value funkce (median S_val < 0.010)"
    if s >= 0.030:
        return "(a) malo iteraci (median S_val >= 0.030)"
    if d is not None and d < 0.010:
        return "(b) seda zona, doktrinalni pary plochE (D_val < 0.010)"
    return "(a) slabe - seda zona, doktrinalni pary se propisuji (D_val >= 0.010)"


def main():
    decs = load()
    out = {}
    for scope in ["dwarf", "wood-elf", "skaven", "all"]:
        st = stats_for(decs, scope)
        if st:
            out[scope] = st
    out["verdict_preregistered_dwarf"] = verdict(out["dwarf"])
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, "teacher_value_results.json"), "w") as f:
        json.dump(out, f, indent=1)
    lines = [f"decisions with >=3 REPOS cands: {sum(o['n'] for k, o in out.items() if isinstance(o, dict) and k != 'all')}"]
    for scope, st in out.items():
        if not isinstance(st, dict):
            continue
        lines.append(f"\n=== {scope} (n={st['n']}) ===")
        for k, v in st.items():
            if k != "n":
                lines.append(f"  {k} = {v}")
    lines.append(f"\nVERDIKT (predregistrovane pravidlo, dwarf): "
                 f"{out['verdict_preregistered_dwarf']}")
    text = "\n".join(lines)
    print(text)
    with open(os.path.join(OUT_DIR, "teacher_value_results.out"), "w") as f:
        f.write(text + "\n")


if __name__ == "__main__":
    main()
