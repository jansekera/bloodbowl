#!/usr/bin/env python3
"""Macro prior-floor audit (2026-07-16, Fable): corpus-side tooling.

Companion to diag_macro_floor_audit_harness.cpp (which does the engine work).
Three subcommands:

    dump <label>            write turn-start snapshots of the corpus at
                            diag_perplayer_grounding_data/<label>/ as the
                            harness's compact text format (stdout)
    agg_counts <file>       aggregate harness `counts` output into the
                            per-macro-type candidate/prior-mass table
    agg_screen <file>       aggregate harness `screen` output: per macro type,
                            how often its best candidate is the one-ply-Q
                            argmax of the root, and what prior it had there

Usage (repo root):
    python3 diag_macro_floor_audit_20260716.py dump main_postfix > snaps.txt
    <scratch>/diag_macro_floor_audit counts  < snaps.txt > counts.out
    <scratch>/diag_macro_floor_audit screen 150 < snaps.txt > screen.out
    python3 diag_macro_floor_audit_20260716.py agg_counts counts.out
    python3 diag_macro_floor_audit_20260716.py agg_screen screen.out
"""
from __future__ import annotations

import gzip
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

DATA_ROOT = Path("diag_perplayer_grounding_data")


def cmd_dump(label: str) -> None:
    files = sorted((DATA_ROOT / label).glob("g*.json.gz"))
    for gi, f in enumerate(files):
        with gzip.open(f, "rt") as fh:
            g = json.load(fh)
        print(f"G {g['home_race']} {g['away_race']}")
        for ti, t in enumerate(g["turns"]):
            players = []
            for key in ("home_players", "away_players"):
                for p in t[key]:
                    players.append((p["id"], p["x"], p["y"], p["state"]))
            active = 0 if t["active_team"] == "home" else 1
            held = 1 if t["ball_held"] else 0
            print(f"T {gi} {ti} {t['half']} {t['turn']} {active} "
                  f"{t['home_score']} {t['away_score']} {held} "
                  f"{t['ball_carrier_id']} {t['ball_x']} {t['ball_y']} "
                  f"{len(players)} "
                  + " ".join(f"{a} {b} {c} {d}" for a, b, c, d in players))


def cmd_agg_counts(path: str) -> None:
    # per ctx per type: roots-present, total candidates, total prior mass,
    # candidate count distribution
    roots = Counter()          # ctx -> n roots
    present = defaultdict(Counter)   # ctx -> type -> roots where present
    cands = defaultdict(Counter)     # ctx -> type -> total candidates
    prior_mass = defaultdict(lambda: defaultdict(float))  # ctx->type->sum of per-root type prior mass
    per_cand_prior = defaultdict(lambda: defaultdict(float))  # ctx->type->sum(per-root mean per-cand prior)
    cand_dist = defaultdict(lambda: defaultdict(Counter))  # ctx->type->count->freq
    for line in open(path):
        if not line.startswith("ROOT"):
            continue
        parts = line.split()
        ctx = parts[3].split("=")[1]
        roots[ctx] += 1
        for tok in parts[5:]:
            ty, cnt, psum, pmax = tok.split(":")
            cnt = int(cnt)
            psum = float(psum)
            present[ctx][ty] += 1
            cands[ctx][ty] += cnt
            prior_mass[ctx][ty] += psum
            per_cand_prior[ctx][ty] += psum / cnt
            cand_dist[ctx][ty][cnt] += 1
    for ctx in sorted(roots):
        n = roots[ctx]
        print(f"\n=== ctx={ctx}  (n={n} roots) ===")
        print(f"{'type':16s} {'present%':>9s} {'cands/root':>11s} "
              f"{'mean per-cand prior':>20s} {'mean type prior mass':>21s} {'cand dist':>12s}")
        for ty in sorted(present[ctx], key=lambda t: -present[ctx][t]):
            np_ = present[ctx][ty]
            dist = cand_dist[ctx][ty]
            dist_s = " ".join(f"{k}x{v}" for k, v in sorted(dist.items())[:6])
            print(f"{ty:16s} {100*np_/n:8.1f}% {cands[ctx][ty]/np_:11.2f} "
                  f"{per_cand_prior[ctx][ty]/np_:20.4f} "
                  f"{prior_mass[ctx][ty]/np_:21.4f}   {dist_s}")


def cmd_agg_screen(path: str) -> None:
    """Per macro type: how often its best candidate is the root's one-ply-Q
    argmax; when it is, what prior did it have vs the root max prior, and how
    big is its Q margin over the best other-type candidate."""
    root = None
    macros: list[dict] = []

    stats = defaultdict(Counter)     # ctx -> counter
    topq = defaultdict(Counter)      # ctx -> type -> times argmaxQ
    starved = defaultdict(Counter)   # ctx -> type -> argmaxQ AND prior<0.5*maxprior
    margins = defaultdict(lambda: defaultdict(list))  # ctx->type->list of (margin, prior, maxprior, g, t, label, pto)

    def flush():
        if root is None or not macros:
            return
        ctx = root["ctx"]
        stats[ctx]["roots"] += 1
        best = max(macros, key=lambda m: m["q"])
        maxprior = max(m["prior"] for m in macros)
        # margin over best candidate of a DIFFERENT type
        others = [m["q"] for m in macros if m["type"] != best["type"]]
        margin = best["q"] - max(others) if others else 0.0
        topq[ctx][best["type"]] += 1
        if best["prior"] < 0.5 * maxprior:
            starved[ctx][best["type"]] += 1
        margins[ctx][best["type"]].append(
            (margin, best["prior"], maxprior, root["g"], root["t"],
             best["label"], best["pto"]))

    for line in open(path):
        if line.startswith("SROOT"):
            flush()
            parts = line.split()
            root = dict(g=int(parts[1].split("=")[1]), t=int(parts[2].split("=")[1]),
                        ctx=parts[3].split("=")[1])
            macros = []
        elif line.startswith("Q "):
            d = {}
            for tok in line[2:].split(maxsplit=4):
                k, v = tok.split("=", 1)
                d[k] = v
            macros.append(dict(type=d["type"], prior=float(d["prior"]),
                               q=float(d["q"]), pto=float(d["pto"]),
                               label=d["label"].strip()))
    flush()

    for ctx in sorted(stats):
        n = stats[ctx]["roots"]
        print(f"\n=== ctx={ctx} (n={n} screened roots) ===")
        print(f"{'type':16s} {'argmaxQ%':>9s} {'argmaxQ & prior<50%max':>23s}")
        for ty, c in topq[ctx].most_common():
            print(f"{ty:16s} {100*c/n:8.1f}% {starved[ctx][ty]:15d} ({100*starved[ctx][ty]/c:.0f}% of argmax)")
        # strongest starved examples per type: argmaxQ, low prior, clear margin, low risk
        print("\n  starved argmaxQ examples (margin>=0.01, pto<=0.05, prior<50% of root max):")
        for ty in topq[ctx]:
            ex = [m for m in margins[ctx][ty]
                  if m[0] >= 0.01 and m[6] <= 0.05 and m[1] < 0.5 * m[2]]
            ex.sort(reverse=True)
            for m in ex[:6]:
                print(f"    {ty:14s} margin={m[0]:+.4f} prior={m[1]:.4f} "
                      f"maxprior={m[2]:.4f} pto={m[6]:.3f} g={m[3]} t={m[4]} {m[5]}")


if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "dump":
        cmd_dump(sys.argv[2])
    elif cmd == "agg_counts":
        cmd_agg_counts(sys.argv[2])
    elif cmd == "agg_screen":
        cmd_agg_screen(sys.argv[2])
    else:
        raise SystemExit(f"unknown command {cmd}")
