#!/usr/bin/env python3
"""Phase A-for-POLICY gate test: incremental per-player candidates (2026-07-20).

WHY: Phase A (2026-07-15, retest 07-16) gave a twice-confirmed NO-GO on the
full C++ per-player build (~492 features), but that test only ever checked
whether 7 narrow per-player-derived scalar candidates move a *value* fit
against the *structurally near-flat* mc_shaped/drive-level target G/T. It
never tested the POLICY head, which the 2026-07-17 diagnosis
(evidence/fable_policy_learning_diagnosis_20260717.md) separately proved is
plateaued (captures only ~25% of learnable signal vs the MCTS visit-count
target, flat for 3.5 weeks, architecturally capped per an overfit probe --
not a training-time issue). Per Opus's original recommendation
(team1_results_opus.md, Q6): test a cheap INCREMENTAL 70->~150 feature step
(carrier + a handful of key players) before ever considering the full
492-dim cold-start rebuild.

This script is that incremental step, but against the POLICY target instead
of value -- the one combination nobody has actually run yet.

Data: decisions now carry a raw per-player board snapshot at the exact MCTS
decision point (engine/include/bb/board_snapshot.h, added today, additive-only,
420/420 tests green) -- so unlike the original Phase A this needs NO offline
GameState reconstruction from turn-level snapshots; the candidate features are
built directly from get_policy_decisions()'s new home_players/away_players/
ball_* fields plus a race-derived static roster table (reusing
diag_perplayer_grounding.player_table, not reimplemented).

Candidate slot layout (16 dims/slot, Opus Q6 template trimmed to what's
buildable without further C++ changes): carrier + 3 nearest teammates + 2
nearest opponents (6 slots x 16 = 96 candidate dims), vs the original Phase
A's 7 scalars -- richer coverage of the same hypothesis.

Usage:
    python3 diag_perplayer_policy_gate_20260720.py collect <label> [N]
    python3 diag_perplayer_policy_gate_20260720.py fit <label> [seeds...]
"""
from __future__ import annotations

import gzip
import json
import sys
import time
from multiprocessing import Pool
from pathlib import Path

sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")

import numpy as np

import diag_perplayer_grounding as G  # noqa: E402  (reuse player_table/RACES/roster tables)

RACES = ["human", "orc", "skaven", "dwarf", "wood-elf"]
W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV, VF_BLEND, MCTS = 1200, 0.0, 100
BASE_SEED = 20260720
DATA_ROOT = Path("diag_perplayer_policy_gate_data")

NUM_FEATURES = 73
NUM_ACTION_FEATURES = 23
N_SLOTS = 6           # carrier + 3 nearest teammates + 2 nearest opponents
DIMS_PER_SLOT = 17    # 16 raw + enemy_tz_count
N_EXTRA_GLOBAL = 2     # net_st_for_block(carrier), carrier_blitzable_bfs -- Opus Q4
N_CANDIDATE = N_SLOTS * DIMS_PER_SLOT + N_EXTRA_GLOBAL  # 104


# ---------------------------------------------------------------------------
# Collection: self-play games via the production macro_mcts path, mirroring
# real training self-play (dirichlet_alpha/exploration_c left at C++ defaults
# -- 0.3/0.5 -- NOT the 0.0/1.0 analysis-mode used by diag_perplayer_grounding,
# because we want data representative of what the policy trainer actually
# learns from in production, not clean analysis snapshots).
# ---------------------------------------------------------------------------

def _game_worker(args: tuple) -> dict:
    seed, race_idx, out_path = args
    import bb_engine
    ra = RACES[race_idx % len(RACES)]
    rb = RACES[(race_idx + 1) % len(RACES)]
    hr = bb_engine.get_developed_roster(ra, TV)
    ar = bb_engine.get_developed_roster(rb, TV)
    lgr = bb_engine.simulate_game_logged(
        hr, ar, home_ai="macro_mcts", away_ai="macro_mcts",
        seed=seed, mcts_iterations=MCTS,
        weights_path=W, away_weights_path=W,
        epsilon=0.0, vf_blend=VF_BLEND,
        policy_weights_path=POLICY_PATH,
    )
    decisions = lgr.get_policy_decisions()
    rec = {
        "seed": seed, "home_race": ra, "away_race": rb,
        "home_score": lgr.result.home_score, "away_score": lgr.result.away_score,
        "decisions": decisions,
    }
    with gzip.open(out_path, "wt") as f:
        json.dump(rec, f, default=lambda o: o.tolist() if hasattr(o, "tolist") else o)
    return {"seed": seed, "hs": lgr.result.home_score, "as": lgr.result.away_score,
            "races": f"{ra}/{rb}", "n_dec": len(decisions)}


def cmd_collect(label: str, n: int) -> None:
    out_dir = DATA_ROOT / label
    out_dir.mkdir(parents=True, exist_ok=True)
    tasks = [(BASE_SEED + i, i % len(RACES), str(out_dir / f"g{i:04d}.json.gz"))
             for i in range(n)]
    t0 = time.time()
    done = 0
    with Pool(10) as pool:
        for r in pool.imap_unordered(_game_worker, tasks):
            done += 1
            print(f"[{done}/{n}] seed={r['seed']} {r['races']} "
                  f"{r['hs']}-{r['as']} decisions={r['n_dec']} "
                  f"({time.time()-t0:.0f}s)", flush=True)
    print(f"DONE {n} games in {time.time()-t0:.0f}s -> {out_dir}")


# ---------------------------------------------------------------------------
# Candidate feature extraction
# ---------------------------------------------------------------------------

SKILL_KEYS = ["Block", "Dodge", "Guard", "Wrestle"]


def _side_endzone_x(side: str) -> int:
    return 25 if side == "home" else 0


def _slot_vector(p: dict | None, table: dict, side: str, carrier_pos, valid: bool,
                 board: "G.Board | None" = None) -> list[float]:
    if not valid or p is None:
        return [0.0] * DIMS_PER_SLOT
    tpl = table.get(p["id"], {})
    ma, st, ag, av = tpl.get("ma", 0), tpl.get("st", 0), tpl.get("ag", 0), tpl.get("av", 0)
    skills = tpl.get("skills", set())
    ez_x = _side_endzone_x(side)
    dist_ez = abs(ez_x - p["x"]) / 25.0
    dist_carrier = 0.0
    if carrier_pos is not None:
        dist_carrier = max(abs(p["x"] - carrier_pos[0]), abs(p["y"] - carrier_pos[1])) / 26.0
    enemy_tz = 0.0
    if board is not None and p["state"] == 0:
        opp_side = "away" if side == "home" else "home"
        enemy_tz = sum(1 for q in board.standing(opp_side)
                       if max(abs(board.pos[q][0] - p["x"]), abs(board.pos[q][1] - p["y"])) == 1) / 3.0
    return [
        1.0,                                   # valid
        1.0 if p["state"] == 0 else 0.0,       # is_standing
        1.0 if p["state"] in (1, 2) else 0.0,  # is_prone_or_stunned
        1.0 if p["has_ball"] else 0.0,
        p["x"] / 25.0,
        p["y"] / 14.0,
        dist_ez,
        ma / 9.0, st / 5.0, ag / 5.0, av / 10.0,
        1.0 if "Block" in skills else 0.0,
        1.0 if "Dodge" in skills else 0.0,
        1.0 if "Guard" in skills else 0.0,
        1.0 if "Wrestle" in skills else 0.0,
        dist_carrier,
        enemy_tz,
    ]


def build_candidate_vector(dec: dict, table: dict) -> np.ndarray:
    """carrier + 3 nearest teammates + 2 nearest opponents, from `dec`'s
    perspective (dec['perspective'] == 'home' or 'away')."""
    board = G.Board(dec)
    perspective = dec["perspective"]
    if perspective == "home":
        mine, theirs = dec["home_players"], dec["away_players"]
    else:
        mine, theirs = dec["away_players"], dec["home_players"]

    carrier = None
    carrier_id = dec.get("ball_carrier_id", -1)
    carrier_side = None
    if dec.get("ball_held") and carrier_id != -1:
        for p in dec["home_players"]:
            if p["id"] == carrier_id:
                carrier, carrier_side = p, "home"
        for p in dec["away_players"]:
            if p["id"] == carrier_id:
                carrier, carrier_side = p, "away"
    carrier_pos = (carrier["x"], carrier["y"]) if carrier else \
        ((dec["ball_x"], dec["ball_y"]) if dec.get("ball_x", -1) >= 0 else None)

    def cheb_to_ref(p):
        if carrier_pos is None:
            return 0
        return max(abs(p["x"] - carrier_pos[0]), abs(p["y"] - carrier_pos[1]))

    mine_sorted = sorted([p for p in mine if carrier is None or p["id"] != carrier["id"]],
                         key=cheb_to_ref)
    theirs_sorted = sorted([p for p in theirs if carrier is None or p["id"] != carrier["id"]],
                           key=cheb_to_ref)

    slots = []
    slots.append(_slot_vector(carrier, table, perspective, carrier_pos, carrier is not None, board))
    for i in range(3):
        p = mine_sorted[i] if i < len(mine_sorted) else None
        slots.append(_slot_vector(p, table, perspective, carrier_pos, p is not None, board))
    for i in range(2):
        p = theirs_sorted[i] if i < len(theirs_sorted) else None
        opp_side = "away" if perspective == "home" else "home"
        slots.append(_slot_vector(p, table, opp_side, carrier_pos, p is not None, board))

    # Opus Q4 "must precompute, net can't derive from raw stats" interaction
    # features, carrier-relative only (bounding the extra BFS/assist-count
    # cost to O(1) calls per decision, not per-slot x per-opponent).
    net_st = 0.0
    blitzable = 0.0
    if carrier is not None:
        opp_side = "away" if carrier_side == "home" else "home"
        opponents = board.standing(opp_side)
        if opponents:
            nearest_opp = min(opponents, key=lambda q: max(
                abs(board.pos[q][0] - carrier["x"]), abs(board.pos[q][1] - carrier["y"])))
            net_st = G.block_dice(board, table, nearest_opp, carrier["id"]) / 3.0
            opp_ma = table.get(nearest_opp, {}).get("ma", 6)
            if G.bfs_can_blitz(board, nearest_opp, carrier_pos, opp_ma):
                blitzable = 1.0

    slots.append([net_st, blitzable])
    return np.array([v for slot in slots for v in slot], dtype=np.float64)


# ---------------------------------------------------------------------------
# Small neural policy trainer with a configurable state width (production's
# NeuralPolicyTrainer hardcodes NUM_FEATURES from features.py for the
# state/action split point, so it silently drops any extra candidate dims --
# this is a straight copy with that one hardcode replaced by a parameter).
# ---------------------------------------------------------------------------

class FlexPolicyTrainer:
    def __init__(self, state_width: int, hidden_size: int = 64,
                 learning_rate: float = 0.01, seed: int = 0):
        self.state_width = state_width
        self.n_features = state_width + NUM_ACTION_FEATURES
        self.hidden_size = hidden_size
        self.lr = learning_rate
        rng = np.random.default_rng(seed)
        scale1 = np.sqrt(2.0 / self.n_features)
        self.W1 = rng.standard_normal((self.n_features, hidden_size)) * scale1
        self.b1 = np.zeros(hidden_size)
        scale2 = np.sqrt(2.0 / hidden_size)
        self.W2 = rng.standard_normal(hidden_size) * scale2
        self.b2 = 0.0

    def _forward(self, inputs):
        z1 = inputs @ self.W1 + self.b1
        h1 = np.maximum(0, z1)
        logits = h1 @ self.W2 + self.b2
        return z1, h1, logits

    def train_on_decisions(self, decisions, passes=8, batch_size=32):
        for _ in range(passes):
            idx = np.random.permutation(len(decisions))
            dW1 = np.zeros_like(self.W1); db1 = np.zeros_like(self.b1)
            dW2 = np.zeros_like(self.W2); db2 = 0.0
            n_acc = 0
            for i in idx:
                inputs, targets = decisions[i]
                if targets.sum() <= 0:
                    continue
                targets = targets / targets.sum()
                z1, h1, logits = self._forward(inputs)
                probs = np.exp(logits - logits.max())
                probs /= probs.sum()
                d_logits = probs - targets
                dW2 += h1.T @ d_logits
                db2 += d_logits.sum()
                d_h1 = np.outer(d_logits, self.W2)
                d_z1 = d_h1 * (z1 > 0)
                dW1 += inputs.T @ d_z1
                db1 += d_z1.sum(axis=0)
                n_acc += 1
                if n_acc >= batch_size:
                    s = 1.0 / n_acc
                    self.W1 -= self.lr * dW1 * s; self.b1 -= self.lr * db1 * s
                    self.W2 -= self.lr * dW2 * s; self.b2 -= self.lr * db2 * s
                    dW1[:] = 0; db1[:] = 0; dW2[:] = 0; db2 = 0.0; n_acc = 0
            if n_acc > 0:
                s = 1.0 / n_acc
                self.W1 -= self.lr * dW1 * s; self.b1 -= self.lr * db1 * s
                self.W2 -= self.lr * dW2 * s; self.b2 -= self.lr * db2 * s
        np.clip(self.W1, -5.0, 5.0, out=self.W1)
        np.clip(self.W2, -5.0, 5.0, out=self.W2)

    def eval_ce(self, decisions):
        """Returns mean CE, mean H(target), mean top1 agreement."""
        ce_sum = h_sum = 0.0
        n_top1 = 0
        n = 0
        for inputs, targets in decisions:
            if targets.sum() <= 0:
                continue
            targets = targets / targets.sum()
            _, _, logits = self._forward(inputs)
            probs = np.exp(logits - logits.max())
            probs /= probs.sum()
            ce_sum += -np.sum(targets * np.log(probs + 1e-8))
            nz = targets > 1e-8
            h_sum += -np.sum(targets[nz] * np.log(targets[nz]))
            if np.argmax(probs) == np.argmax(targets):
                n_top1 += 1
            n += 1
        return ce_sum / max(n, 1), h_sum / max(n, 1), n_top1 / max(n, 1), n


def _build_rows(dec: dict, table: dict | None):
    """Returns (inputs[n_actions, state_width+23], targets[n_actions]) for
    baseline (table=None) or combined (table given) arms."""
    state_feats = np.asarray(dec["state_features"], dtype=np.float64)
    if table is not None:
        cand = build_candidate_vector(dec, table)
        state_full = np.concatenate([state_feats, cand])
    else:
        state_full = state_feats
    visits = dec.get("visits", [])
    n = len(visits)
    if n == 0:
        return None
    inputs = np.zeros((n, len(state_full) + NUM_ACTION_FEATURES))
    targets = np.zeros(n)
    for i, v in enumerate(visits):
        af = np.asarray(v["action_features"], dtype=np.float64)
        inputs[i, :len(state_full)] = state_full
        inputs[i, len(state_full):len(state_full) + len(af)] = af
        targets[i] = v["visit_fraction"]
    return inputs, targets


def load_games(label: str):
    out_dir = DATA_ROOT / label
    games = []
    for f in sorted(out_dir.glob("g*.json.gz")):
        with gzip.open(f, "rt") as fh:
            games.append(json.load(fh))
    return games


def cmd_fit(label: str, seeds: list[int]) -> None:
    games = load_games(label)
    print(f"{len(games)} games loaded from {label}")
    total_dec = sum(len(g["decisions"]) for g in games)
    print(f"{total_dec} total decisions")

    results = []
    for seed in seeds:
        rng = np.random.default_rng(seed)
        idx = rng.permutation(len(games))
        n_test = max(1, int(0.2 * len(games)))
        test_idx = set(idx[:n_test].tolist())

        train_baseline, test_baseline = [], []
        train_combined, test_combined = [], []
        for gi, g in enumerate(games):
            table = G.player_table(g["home_race"], g["away_race"])
            for dec in g["decisions"]:
                rb = _build_rows(dec, None)
                rc = _build_rows(dec, table)
                if rb is None or rc is None:
                    continue
                (test_baseline if gi in test_idx else train_baseline).append(rb)
                (test_combined if gi in test_idx else train_combined).append(rc)

        np.random.seed(seed)
        base_trainer = FlexPolicyTrainer(NUM_FEATURES, seed=seed)
        base_trainer.train_on_decisions(train_baseline, passes=8)
        ce_b, h_b, top1_b, n_b = base_trainer.eval_ce(test_baseline)

        comb_trainer = FlexPolicyTrainer(NUM_FEATURES + N_CANDIDATE, seed=seed)
        comb_trainer.train_on_decisions(train_combined, passes=8)
        ce_c, h_c, top1_c, n_c = comb_trainer.eval_ce(test_combined)

        results.append(dict(seed=seed, ce_b=ce_b, h_b=h_b, top1_b=top1_b, n_b=n_b,
                            ce_c=ce_c, h_c=h_c, top1_c=top1_c, n_c=n_c))
        print(f"seed={seed} n_train={len(train_baseline)} n_test={n_b} | "
              f"baseline CE={ce_b:.4f} H={h_b:.4f} KL={ce_b-h_b:.4f} top1={top1_b:.3f} | "
              f"combined CE={ce_c:.4f} H={h_c:.4f} KL={ce_c-h_c:.4f} top1={top1_c:.3f}")

    kl_b = np.array([r["ce_b"] - r["h_b"] for r in results])
    kl_c = np.array([r["ce_c"] - r["h_c"] for r in results])
    print()
    print(f"KL(target||policy) baseline : mean={kl_b.mean():.4f} std={kl_b.std():.4f}")
    print(f"KL(target||policy) combined : mean={kl_c.mean():.4f} std={kl_c.std():.4f}")
    print(f"delta KL (combined-baseline): mean={(kl_c-kl_b).mean():.4f} "
          f"(negative = combined fits BETTER)")
    top1_b_arr = np.array([r["top1_b"] for r in results])
    top1_c_arr = np.array([r["top1_c"] for r in results])
    print(f"top1 agreement baseline: {top1_b_arr.mean():.3f}  combined: {top1_c_arr.mean():.3f}")


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    cmd = sys.argv[1]
    label = sys.argv[2]
    if cmd == "collect":
        n = int(sys.argv[3]) if len(sys.argv) > 3 else 150
        cmd_collect(label, n)
    elif cmd == "fit":
        seeds = [int(x) for x in sys.argv[3:]] if len(sys.argv) > 3 else [20260720, 20260721, 20260722, 20260723, 20260724]
        cmd_fit(label, seeds)
    else:
        print(__doc__)
        sys.exit(1)


if __name__ == "__main__":
    main()
