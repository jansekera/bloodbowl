"""Korpus se ZAPNUTOU bránou klece (noc 13.→14.08.2026).

Proč vzniká: v korpusu z 11.08. je `plán: NOT_CONSULTED` ve 100 % kol, protože
`cageAdvance` je v produkci vypnutá. Dokud se nezapne, **nelze spočítat K9b**
(kvóta je funkcí odporu, a `resistance` je všude 0), `paceAch` se loguje jako
0.0, a hranice S2/S3/S4 v rozložení situací se podle volby konstanty hýbe mezi
6 a 354 koly. Tři nezávislé měřicí díry, jedna příčina.

⚑ POTŘEBUJEME HO BEZ OHLEDU NA VÝSLEDEK A/B `run_gate_20260813.sh`.
   To A/B se ptá „hraje se s bránou lépe?". Tenhle korpus se ptá „co brána
   vlastně dělá?" — a na to potřebujeme odpověď i tehdy (zvlášť tehdy), když
   A/B vyjde neprůkazně.

⚑ KONFIGURACE JE ZÁMĚRNĚ TOTOŽNÁ s korpusem `diag_replay_mine_20260811b_data`
   — tytéž seedy, tíž soupeři, totéž pořadí orientací, TV1200, MCTS-100,
   vf_blend=0.0, tytéž váhy. **Jediný rozdíl je zapnutá brána na NAŠÍ straně.**
   ⇒ Srovnání je párové hra po hře, ne dvě nezávislé sady. Rozdíl v K9a, K29,
   K31 a K36 mezi korpusy je pak přímo dopadem brány na chování, ne šum
   mezi vzorky. NEMĚNIT ani jednu z konstant níž.

⚑ Brána se zapíná té straně, kde stojí trpaslík — `cage_advance` je per-side
   a hry se střídají doma/venku.

Usage:
    python3 diag_replay_mine_20260813_gate.py collect [n=120]
"""
import gzip
import json
import os
import sys
import time
from multiprocessing import Pool
from pathlib import Path

# Vše až po DATA_ROOT musí zůstat shodné s diag_replay_mine_20260811b.py,
# jinak přestane platit párovost korpusů.
OPPONENTS = ["skaven", "wood-elf", "orc", "human"]
W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV, VF_BLEND, MCTS = 1200, 0.0, 100
WORKERS = int(os.environ.get("WORKERS", "10"))

# Parametrizace přes prostředí, aby existovala JEDNA implementace sběru místo
# třetí kopie téhož souboru. Defaulty reprodukují gate korpus z 13.08. beze
# změny; velký noční korpus si přepíná bránu, adresář i seed.
#   CAGE_GATE=1  brána zapnutá na NAŠÍ straně (default, gate korpus)
#   CAGE_GATE=0  produkční stav -- A/B brány 13.08. NEPROŠLO, takže korpus,
#                který má popisovat hranou hru, ji má vypnutou
CAGE_GATE = os.environ.get("CAGE_GATE", "1") != "0"
#   DAUNTLESS=1  nabídka bloku oceňuje sílu, na kterou by Dauntless srovnal
#                (P13, 14.08.). Default 0 = produkční stav. Platí pro OBĚ strany:
#                není to naše doktrína, je to filtr, který neviděl dovednost,
#                kterou resolver už ctí — na jedné straně by to srovnávalo dva
#                různé enginy, ne dvě ramena.
DAUNTLESS = os.environ.get("DAUNTLESS", "0") != "0"
BASE_SEED = int(os.environ.get("SEED_BASE", "20260811"))
DATA_ROOT = Path(os.environ.get(
    "DATA_ROOT", "diag_replay_mine_20260813_gate_data"))


def _add_engine_to_path() -> None:
    """bb_engine je zkompilovaná .so v engine/build, ne nainstalovaný balík.
    Stejně jako `diag_utils.py:69-70`; ve workeru zvlášť, aby to platilo i při
    startovací metodě `spawn` (fork by cestu zdědil, spawn ne)."""
    for p in ("engine/build", "python"):
        if p not in sys.path:
            sys.path.insert(0, p)


def _game_worker(args: tuple) -> dict:
    seed, idx, out_path = args
    os.nice(19)
    _add_engine_to_path()
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
        # jediný rozdíl proti 20260811b: brána na trpasličí straně
        cage_advance=CAGE_GATE and dwarf_home,
        away_cage_advance=CAGE_GATE and not dwarf_home,
        dauntless_in_offer=DAUNTLESS,
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
    with Pool(WORKERS) as pool:
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
