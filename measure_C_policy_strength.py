"""C-primárně: pomáhá policy jako prior REÁLNÉ herní síle?
Náš hráč = macro_mcts (value=weights_best, policy=weights_policy) s různým policy_blend.
Soupeř = learning (value-based, NEPOUŽÍVÁ policy) — pevná reference.
SPÁROVANÉ seedy napříč blend úrovněmi → jediný rozdíl je policy prior.
Pravda = výsledky her, žádné top1/visit metriky (imunní vůči problému s klecí).

Parametry přes env:
  C_GAMES   (default 12)  počet her na blend úroveň (na každou rasu rovnoměrně)
  C_WORKERS (default 12)  paralelní procesy
  C_BLENDS  (default "0.0,0.3")  čárkou oddělené úrovně policy_blend
Dlouhý test: C_GAMES=120 venv/bin/python3 measure_C_policy_strength.py
"""
import os, sys
sys.path.insert(0, 'engine/build')
sys.path.insert(0, 'python')
from multiprocessing import Pool

GAMES   = int(os.environ.get('C_GAMES', 12))
WORKERS = int(os.environ.get('C_WORKERS', 12))
BLENDS  = [float(x) for x in os.environ.get('C_BLENDS', '0.0,0.3').split(',')]
AWAY    = ['orc', 'skaven', 'dwarf', 'wood-elf']
MCTS, TV = 100, 1200
VALUE  = 'weights_best.json'
POLICY = 'weights_policy.json'

def worker(args):
    seed, away_race, blend = args
    import bb_engine
    hr = bb_engine.get_developed_roster('human', TV)
    ar = bb_engine.get_developed_roster(away_race, TV)
    g = bb_engine.simulate_game_logged(
        hr, ar, 'macro_mcts', 'learning',
        seed=seed, weights_path=VALUE, policy_weights_path=POLICY,
        epsilon=0.1, mcts_iterations=MCTS, policy_blend=blend, vf_blend=0.0,
    )
    r = g.result
    return (blend, r.home_score, r.away_score)

def main():
    tasks = []
    for blend in BLENDS:
        for i in range(GAMES):
            seed = 5000 + i           # SPÁROVÁNO napříč blendy
            away = AWAY[i % len(AWAY)]
            tasks.append((seed, away, blend))
    print(f'C-primárně: {len(BLENDS)} blendů × {GAMES} her = {len(tasks)} her, '
          f'{WORKERS} workers, soupeř=learning, MCTS={MCTS}', flush=True)

    with Pool(WORKERS, initializer=lambda: sys.path.insert(0, 'engine/build')) as p:
        results = p.map(worker, tasks)

    print('\n=========== C-PRIMÁRNĚ: HERNÍ SÍLA POLICY JAKO PRIOR ===========')
    print(f"{'blend':>6} {'W':>4} {'D':>4} {'L':>4} {'win%':>7} {'scoreΔ':>8}")
    base = None
    for blend in BLENDS:
        rs = [(h, a) for (b, h, a) in results if b == blend]
        w = sum(1 for h, a in rs if h > a)
        d = sum(1 for h, a in rs if h == a)
        l = sum(1 for h, a in rs if h < a)
        n = len(rs)
        winr = 100 * (w + 0.5 * d) / n
        sd = sum(h - a for h, a in rs) / n
        if base is None:
            base = winr
        print(f"{blend:>6} {w:>4} {d:>4} {l:>4} {winr:>6.1f}% {sd:>+8.2f}")
    print('\nINTERPRETACE: vyšší blend → vyšší win% / scoreΔ = policy jako prior POMÁHÁ (náš fix).')
    print('              plochý/horší s blendem = policy jako prior nepomáhá → per-player/Tým 1.')

if __name__ == '__main__':
    main()
