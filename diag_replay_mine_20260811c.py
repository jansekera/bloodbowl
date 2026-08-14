"""Čerstvý korpus na AKTUÁLNÍM buildu, trpasličí (2026-08-11).

Proč vzniká: všechna naše behaviorální čísla (tempo 2,40 · první držení
míče v kole 4,1 · Longbeard nese ve 44-49 % kol · 39 % kol nula rohů klece)
pocházejí z korpusu z 30.07. — tedy PŘED roster fixem f7aa61c, PŘED D-vlnou 1
a PŘED dnešní opravou rozestavení (41c3570). V enginových letech je to měsíc.

⚑ KONFIGURACE JE ZÁMĚRNĚ TOTOŽNÁ s korpusem z 30.07.
   (TV1200, MCTS-100, vf_blend=0.0, tytéž váhy)
   ⇒ jediný rozdíl je ENGINE, takže se čísla dají srovnat přímo
   a položka [3] "změřit dopad opravy rozestavení" vypadne skoro zadarmo.
   NEPŘEPÍNAT na produkční vf_blend=0.15 -- rozbilo by to srovnatelnost.

⚑ KAŽDÁ HRA MÁ TRPASLÍKA. Starý korpus měl trpaslíka jen v 10 z 24 zápasů
   (~20 drivů), což je na rozklad příčin málo. Tady je trpaslík vždy na jedné
   straně a střídají se soupeři, plus obě orientace (doma i venku), aby se
   nemíchal home/away bias.

⚑ Pool(4) a nice -19: souběžně běží měření balíku G na 6 jádrech z 12.

Usage:
    python3 diag_replay_mine_20260811.py collect [n=120]
"""
import gzip
import json
import os
import sys
import time
from multiprocessing import Pool
from pathlib import Path

OPPONENTS = ["skaven", "wood-elf", "orc", "human"]
W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV, VF_BLEND, MCTS = 1200, 0.0, 100
BASE_SEED = 20270811
DATA_ROOT = Path("diag_replay_mine_20260811c_data")


def _game_worker(args: tuple) -> dict:
    seed, idx, out_path = args
    os.nice(19)
    import bb_engine
    opp = OPPONENTS[(idx // 2) % len(OPPONENTS)]
    dwarf_home = (idx % 2 == 0)          # obě orientace, ať se nemíchá bias
    ra, rb = ("dwarf", opp) if dwarf_home else (opp, "dwarf")
    hr = bb_engine.get_developed_roster(ra, TV)
    ar = bb_engine.get_developed_roster(rb, TV)
    lgr = bb_engine.simulate_game_logged(
        hr, ar, home_ai="macro_mcts", away_ai="macro_mcts",
        seed=seed, mcts_iterations=MCTS,
        weights_path=W, away_weights_path=W,
        epsilon=0.0, vf_blend=VF_BLEND,
        policy_weights_path=POLICY_PATH,
    )
    turns = lgr.get_turn_logs()
    rec = {
        "seed": seed, "home_race": ra, "away_race": rb,
        "home_score": lgr.result.home_score, "away_score": lgr.result.away_score,
        "turn_logs": turns,
    }
    with gzip.open(out_path, "wt") as f:
        json.dump(rec, f, default=lambda o: o.tolist() if hasattr(o, "tolist") else o)
    return {"seed": seed, "hs": lgr.result.home_score, "as": lgr.result.away_score,
            "races": f"{ra}/{rb}", "n_turns": len(turns)}


def cmd_collect(n: int) -> None:
    DATA_ROOT.mkdir(parents=True, exist_ok=True)
    tasks = [(BASE_SEED + i, i, str(DATA_ROOT / f"g{i:04d}.json.gz"))
             for i in range(n)]
    t0 = time.time()
    done = 0
    with Pool(4) as pool:
        for r in pool.imap_unordered(_game_worker, tasks):
            done += 1
            print(f"[{done}/{n}] seed={r['seed']} {r['races']} "
                  f"{r['hs']}-{r['as']} turns={r['n_turns']} "
                  f"({time.time()-t0:.0f}s)", flush=True)
    Path(DATA_ROOT / "COLLECT_DONE").touch()
    print(f"HOTOVO {n} her za {time.time()-t0:.0f}s", flush=True)


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "collect"
    if cmd == "collect":
        cmd_collect(int(sys.argv[2]) if len(sys.argv) > 2 else 120)
    else:
        print(__doc__)
