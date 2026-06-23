"""Team1 v2 / T0 — REUSABLE policy-fit metrics module.

Replaces the misleading `top1_agree` yardstick. Given MCTS visit-distribution
targets (the policy's training target) and the trained policy loaded, report:

  - KL(target || policy)   vs   KL(target || uniform)   -> does policy beat uniform?
  - top-k recall (default k=3)                            -> is MCTS-best in policy's top-k?
  - cluster recall (target top-m captured by policy top-k)
  - target entropy H (nats) and H_norm = H / ln(n)        -> is the target separable at all?
  - frac near-tie@top                                      -> is top1 unhittable by construction?

Two data sources for decisions:
  --source logs   (default) read logged decisions under training_logs/  (FAST, deterministic)
  --source sim    freshly simulate games (uses bb_engine, like team1C_metric.py)

This is a LIBRARY first (importable functions) + a CLI. The strength-as-prior
measurement lives in measure_C_policy_strength.py; this script's `strength`
subcommand shells out to it so there is one code path for game-truth.

Run:
  PYTHONPATH=$(pwd)/engine/build:$(pwd)/python venv/bin/python3 measure_policy_metrics.py [args]

Examples:
  # baseline on current weights, from logs (no engine needed):
  ... measure_policy_metrics.py --source logs --max-decisions 4000
  # from fresh sims (loads engine):
  ... measure_policy_metrics.py --source sim --games 4
  # playing strength as prior (delegates to measure_C_policy_strength.py):
  ... measure_policy_metrics.py strength
"""
import argparse, glob, json, math, os, sys

import numpy as np

# ---- constants pulled from the trainer so we stay in sync with the engine ----
NUM_FEATURES = 70           # blood_bowl.features.NUM_FEATURES
NUM_ACTION_FEATURES = 23    # blood_bowl.policy_trainer.NUM_ACTION_FEATURES (phase-1)
POLICY_INPUT_SIZE = NUM_FEATURES + NUM_ACTION_FEATURES


# --------------------------------------------------------------------------- #
#  Policy loading + forward pass (mirrors policy_trainer.predict exactly)
# --------------------------------------------------------------------------- #
class LoadedPolicy:
    """Thin wrapper that loads a trained policy (neural or linear) and produces
    a probability distribution over the candidate actions of a decision.

    Forward pass matches policy_trainer's TRAINING softmax (no temperature),
    which is what the imitation loss is computed against. The engine applies a
    temperature when it uses the head as an MCTS prior; pass temperature= to
    inspect that, but the fit metrics use temperature=1.0 to match the loss.
    """

    def __init__(self, policy_path, hidden_size=None):
        with open(policy_path) as f:
            data = json.load(f)
        self.path = policy_path
        self.kind = data.get('policy_type', 'linear')
        if self.kind == 'neural':
            self.hidden_size = hidden_size or data['policy_hidden_size']
            W1 = np.array(data['policy_W1'], dtype=np.float64)
            self.W1 = W1.reshape(POLICY_INPUT_SIZE, self.hidden_size)
            self.b1 = np.array(data['policy_b1'], dtype=np.float64)
            self.W2 = np.array(data['policy_W2'], dtype=np.float64)
            self.b2 = float(data.get('policy_b2', 0.0))
        else:
            w = np.array(data.get('policy_weights', []), dtype=np.float64)
            if len(w) < POLICY_INPUT_SIZE:
                padded = np.zeros(POLICY_INPUT_SIZE)
                padded[:len(w)] = w
                w = padded
            self.weights = w
            self.bias = float(data.get('policy_bias', 0.0))

    def _inputs(self, state_feats, action_mat):
        n = action_mat.shape[0]
        inp = np.zeros((n, POLICY_INPUT_SIZE))
        ns = min(len(state_feats), NUM_FEATURES)
        na = min(action_mat.shape[1], NUM_ACTION_FEATURES)
        inp[:, :ns] = state_feats[:ns]
        inp[:, NUM_FEATURES:NUM_FEATURES + na] = action_mat[:, :na]
        return inp

    def probs(self, state_feats, action_mat, temperature=1.0):
        inp = self._inputs(np.asarray(state_feats, float), np.asarray(action_mat, float))
        if self.kind == 'neural':
            h = np.maximum(0.0, inp @ self.W1 + self.b1)
            logits = h @ self.W2 + self.b2
        else:
            logits = inp @ self.weights + self.bias
        logits = logits / max(temperature, 1e-6)
        logits = logits - logits.max()
        e = np.exp(logits)
        return e / e.sum()


# --------------------------------------------------------------------------- #
#  Per-decision metrics
# --------------------------------------------------------------------------- #
def decision_metrics(target, probs, topk=3, cluster_m=3, near_ratio=1.25):
    """All scalar metrics for one decision. `target` and `probs` are 1-D, sum~1."""
    n = len(target)
    eps = 1e-9
    t = np.clip(target, eps, None); t = t / t.sum()
    p = np.clip(probs, eps, None);  p = p / p.sum()
    u = np.ones(n) / n

    bt = int(np.argmax(target))
    order_p = np.argsort(p)[::-1]
    top1 = int(order_p[0] == bt)
    topk_recall = int(bt in set(order_p[:topk]))

    # cluster recall: of the target's top-m actions, what fraction are in policy top-k
    tgt_top = set(np.argsort(target)[::-1][:min(cluster_m, n)])
    pol_top = set(order_p[:min(topk, n)])
    cluster = len(tgt_top & pol_top) / len(tgt_top)

    kl_p = float(np.sum(t * np.log(t / p)))
    kl_u = float(np.sum(t * np.log(t / u)))
    h = float(-np.sum(t * np.log(t)))
    h_norm = h / math.log(n) if n > 1 else 0.0

    srt = np.sort(target)[::-1]
    near = int(len(srt) > 1 and srt[0] < near_ratio * srt[1])
    return dict(top1=top1, topk=topk_recall, cluster=cluster,
                kl_p=kl_p, kl_u=kl_u, h=h, h_norm=h_norm, near=near, n=n)


def aggregate(rows, topk=3):
    if not rows:
        return None
    keys = ['top1', 'topk', 'cluster', 'kl_p', 'kl_u', 'h', 'h_norm', 'near']
    agg = {k: float(np.mean([r[k] for r in rows])) for k in keys}
    agg['n_decisions'] = len(rows)
    agg['mean_actions'] = float(np.mean([r['n'] for r in rows]))
    agg['kl_improvement'] = agg['kl_u'] - agg['kl_p']  # >0 => policy beats uniform
    agg['topk_k'] = topk
    return agg


def report(agg):
    if agg is None:
        print("no decisions evaluated"); return
    print(f"\n=== POLICY-FIT METRICS  (n_decisions={agg['n_decisions']}, "
          f"mean_actions={agg['mean_actions']:.1f}) ===")
    print(f"  KL(target||policy)   = {agg['kl_p']:.4f}")
    print(f"  KL(target||uniform)  = {agg['kl_u']:.4f}")
    print(f"  KL improvement       = {agg['kl_improvement']:+.4f}   "
          f"(>0 => policy beats uniform)")
    print(f"  top1_agree           = {agg['top1']:.3f}   (legacy; misleading)")
    print(f"  top{agg['topk_k']}_recall          = {agg['topk']:.3f}")
    print(f"  cluster_recall       = {agg['cluster']:.3f}   "
          f"(target top-m captured by policy top-k)")
    print(f"  target H (nats)      = {agg['h']:.3f}")
    print(f"  target H_norm        = {agg['h_norm']:.3f}   (1.0 = uniform)")
    print(f"  frac near-tie@top    = {agg['near']:.3f}   (top within 1.25x of 2nd)")
    # one-line verdict
    if agg['kl_improvement'] > 0.02:
        print("  -> policy MEANINGFULLY beats uniform on the target.")
    elif agg['kl_improvement'] > 0:
        print("  -> policy marginally beats uniform.")
    else:
        print("  -> policy NO better than uniform.")


# --------------------------------------------------------------------------- #
#  Decision sources
# --------------------------------------------------------------------------- #
def iter_logged_decisions(log_dir, max_decisions=None, min_actions=3):
    files = sorted(glob.glob(os.path.join(log_dir, '**', 'decisions_*.json'),
                            recursive=True))
    n = 0
    for fp in files:
        try:
            data = json.load(open(fp))
        except Exception:
            continue
        decs = data if isinstance(data, list) else [data]
        for d in decs:
            vis = d.get('visits', [])
            if len(vis) < min_actions:
                continue
            yield d
            n += 1
            if max_decisions and n >= max_decisions:
                return


def iter_sim_decisions(value_path, policy_path, games=4, mcts=100, tv=1200,
                       seed0=7000, min_actions=3,
                       away=('orc', 'skaven', 'dwarf', 'wood-elf')):
    sys.path.insert(0, os.path.join(os.getcwd(), 'engine/build'))
    sys.path.insert(0, os.path.join(os.getcwd(), 'python'))
    import bb_engine
    for i in range(games):
        hr = bb_engine.get_developed_roster('human', tv)
        ar = bb_engine.get_developed_roster(away[i % len(away)], tv)
        g = bb_engine.simulate_game_logged(
            hr, ar, 'macro_mcts', 'macro_mcts', seed=seed0 + i,
            weights_path=value_path, policy_weights_path=policy_path,
            epsilon=0.1, mcts_iterations=mcts, policy_blend=0.0, vf_blend=0.0,
            dirichlet_alpha=0.0, exploration_c=1.0)
        for d in g.get_policy_decisions():
            if len(d.get('visits', [])) < min_actions:
                continue
            yield d


def evaluate(decisions, policy, topk=3, temperature=1.0):
    rows = []
    for d in decisions:
        vis = d['visits']
        tgt = np.array([float(v['visit_fraction']) for v in vis], float)
        if tgt.sum() <= 0:
            continue
        tgt = tgt / tgt.sum()
        amat = np.array([np.asarray(v['action_features'], float) for v in vis])
        sf = np.asarray(d['state_features'], float)
        p = policy.probs(sf, amat, temperature=temperature)
        rows.append(decision_metrics(tgt, p, topk=topk))
    return aggregate(rows, topk=topk)


# --------------------------------------------------------------------------- #
#  CLI
# --------------------------------------------------------------------------- #
def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd')

    # default (fit) options on the top-level parser too
    for p in (ap,):
        p.add_argument('--source', choices=['logs', 'sim'], default='logs')
        p.add_argument('--log-dir', default='training_logs')
        p.add_argument('--value', default='weights_best.json')
        p.add_argument('--policy', default='weights_policy.json')
        p.add_argument('--max-decisions', type=int, default=4000)
        p.add_argument('--games', type=int, default=4)
        p.add_argument('--mcts', type=int, default=100)
        p.add_argument('--topk', type=int, default=3)
        p.add_argument('--temperature', type=float, default=1.0)

    s = sub.add_parser('strength', help='playing-strength-as-prior (delegates to '
                                        'measure_C_policy_strength.py)')
    s.add_argument('--games', type=int, default=12)
    s.add_argument('--workers', type=int, default=12)
    s.add_argument('--blends', default='0.0,0.15,0.3')

    args = ap.parse_args()

    if args.cmd == 'strength':
        run_strength(args)
        return

    policy = LoadedPolicy(args.policy)
    print(f"policy: {args.policy}  kind={policy.kind}"
          + (f" hidden={policy.hidden_size}" if policy.kind == 'neural' else ""))
    if args.source == 'logs':
        print(f"source: logs ({args.log_dir}), max_decisions={args.max_decisions}")
        decs = iter_logged_decisions(args.log_dir, args.max_decisions)
    else:
        print(f"source: sim ({args.games} games, mcts={args.mcts})")
        decs = iter_sim_decisions(args.value, args.policy, games=args.games,
                                  mcts=args.mcts)
    agg = evaluate(decs, policy, topk=args.topk, temperature=args.temperature)
    report(agg)


def run_strength(args):
    """Delegate to measure_C_policy_strength.py so game-truth has ONE code path.

    NOTE (asymmetric blend): the existing C++ entry only exposes a single
    `policy_blend` applied to the macro_mcts side under test; there is no
    `away_policy_blend`. A truly asymmetric A/B (policy-prior team vs identical
    no-prior team, same engine) would need a C++ `away_policy_blend` param on
    simulate_game_logged. Current proxy: macro_mcts(+policy) vs `learning`
    opponent (value-only, never uses policy) with paired seeds. FLAGGED, not
    built (non-trivial C++ change).
    """
    import subprocess
    env = dict(os.environ)
    env['C_GAMES'] = str(args.games)
    env['C_WORKERS'] = str(args.workers)
    env['C_BLENDS'] = args.blends
    print(f"strength: delegating to measure_C_policy_strength.py "
          f"(games={args.games}, blends={args.blends})")
    print("NOTE: asymmetric away_policy_blend NOT available in C++ -> using "
          "learning opponent as fixed reference (see docstring).")
    subprocess.run([sys.executable, 'measure_C_policy_strength.py'], env=env)


if __name__ == '__main__':
    main()
