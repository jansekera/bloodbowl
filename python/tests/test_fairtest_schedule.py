"""Engine-free tests for the F0 fairtest schedule + paired per-race evaluation.

Pattern per evidence/fable_dwarf_playstyle_gap_20260803.md F0 and the
league-pool test suite (spec parsing / deterministic scheduling, no engine).
"""
from collections import Counter

import pytest

from blood_bowl.fairtest_schedule import (
    DEFAULT_RACES,
    chess_score,
    matchup_for_seed,
    per_race_paired,
    recommended_n_pairs,
    schedule,
)


# ── schedule ────────────────────────────────────────────────────────────────

class TestSchedule:
    def test_deterministic(self):
        assert schedule(100) == schedule(100)
        assert matchup_for_seed(37) == matchup_for_seed(37)

    def test_one_block_covers_every_ordered_pair_exactly_once(self):
        n = len(DEFAULT_RACES)
        block = schedule(n * n)
        counts = Counter(block)
        assert len(counts) == n * n
        assert all(c == 1 for c in counts.values())

    def test_mirrors_included(self):
        n = len(DEFAULT_RACES)
        block = set(schedule(n * n))
        for race in DEFAULT_RACES:
            assert (race, race) in block, f"mirror {race} missing"

    def test_period5_gap_closed_dwarf_meets_everyone(self):
        # The old period-5 cycle gave dwarf ONLY skaven and wood-elf
        # (dwarf report §3a). Round-robin must give dwarf all 5 races.
        n = len(DEFAULT_RACES)
        opponents = {b for a, b in schedule(n * n) if a == "dwarf"}
        opponents |= {a for a, b in schedule(n * n) if b == "dwarf"}
        assert opponents == set(DEFAULT_RACES)

    def test_balanced_home_away_over_full_blocks(self):
        n = len(DEFAULT_RACES)
        games = schedule(3 * n * n)  # 3 full blocks
        home = Counter(a for a, _ in games)
        away = Counter(b for _, b in games)
        assert set(home.values()) == {3 * n}
        assert set(away.values()) == {3 * n}
        # both orientations of every unordered non-mirror pair, equally often
        for a in DEFAULT_RACES:
            for b in DEFAULT_RACES:
                assert games.count((a, b)) == 3

    def test_custom_race_list(self):
        races = ("a", "b", "c")
        counts = Counter(schedule(9, races))
        assert len(counts) == 9
        assert all(c == 1 for c in counts.values())

    def test_input_validation(self):
        with pytest.raises(ValueError):
            matchup_for_seed(0, ())
        with pytest.raises(ValueError):
            matchup_for_seed(-1)

    def test_recommended_n_pairs_rounds_up_to_block(self):
        assert recommended_n_pairs(800) == 800    # 32 * 25
        assert recommended_n_pairs(790) == 800
        assert recommended_n_pairs(801) == 825
        assert recommended_n_pairs(0) == 25
        assert recommended_n_pairs(4, ("a", "b")) == 4


# ── paired per-race evaluation ──────────────────────────────────────────────

def _pair(seed_idx, ra, rb, cand_as_ra, base_as_rb, base_as_ra, cand_as_rb):
    """Build a side-swapped pair of result rows for matchup (ra, rb).

    Orientation 1 (cand home): candidate plays ra scoring cand_as_ra,
    baseline plays rb scoring base_as_rb. Orientation 2 (cand away):
    baseline plays ra scoring base_as_ra, candidate plays rb scoring
    cand_as_rb.
    """
    return [
        {"seed_idx": seed_idx, "cand_home": True, "race_h": ra, "race_a": rb,
         "cand": cand_as_ra, "base": base_as_rb},
        {"seed_idx": seed_idx, "cand_home": False, "race_h": ra, "race_a": rb,
         "cand": cand_as_rb, "base": base_as_ra},
    ]


class TestChessScore:
    def test_values(self):
        assert chess_score(2, 1) == 1.0
        assert chess_score(1, 2) == 0.0
        assert chess_score(1, 1) == 0.5


class TestPerRacePaired:
    def test_candidate_dwarf_always_wins(self):
        # 10 pairs of dwarf-skaven where the DWARF side wins iff the
        # candidate plays it: cand-as-dwarf 2:0, base-as-dwarf 0:2.
        rows = []
        for i in range(10):
            rows += _pair(i, "dwarf", "skaven",
                          cand_as_ra=2, base_as_rb=0,   # cand dwarf beats base skaven
                          base_as_ra=0, cand_as_rb=2)   # base dwarf loses to cand skaven
        out = per_race_paired(rows)
        d = out["dwarf"]["vs"]["skaven"]
        assert d["n_paired"] == 10
        assert d["cand_decisive_wr"] == 1.0
        assert d["base_decisive_wr"] == 0.0
        assert d["delta_decisive_pp"] == 100.0
        assert d["mean_delta_chess"] == 1.0
        # zero-variance perfect split -> t reported as +inf, flagged significant
        assert d["significant_95"] is False or d["t"] == float("inf")

    def test_null_effect_is_symmetric(self):
        # Whoever plays dwarf wins: candidate and baseline identical as dwarf.
        rows = []
        for i in range(8):
            rows += _pair(i, "dwarf", "wood-elf",
                          cand_as_ra=1, base_as_rb=0,
                          base_as_ra=1, cand_as_rb=0)
        out = per_race_paired(rows)
        d = out["dwarf"]["vs"]["wood-elf"]
        assert d["cand_decisive_wr"] == 1.0
        assert d["base_decisive_wr"] == 1.0
        assert d["delta_decisive_pp"] == 0.0
        assert d["mean_delta_chess"] == 0.0
        w = out["wood-elf"]["vs"]["dwarf"]
        assert w["delta_decisive_pp"] == 0.0

    def test_global_wr_confound_does_not_leak(self):
        # The §3a artifact: race X losing its matchup does NOT make the
        # candidate look bad when candidate and baseline lose equally.
        # Additionally give the candidate a real edge in a DIFFERENT matchup;
        # X's numbers must stay clean.
        rows = []
        for i in range(6):
            rows += _pair(i, "dwarf", "skaven",       # dwarf always loses
                          cand_as_ra=0, base_as_rb=1,
                          base_as_ra=0, cand_as_rb=1)
        for i in range(6, 12):
            rows += _pair(i, "human", "orc",          # cand-as-human wins, base draws
                          cand_as_ra=2, base_as_rb=0,
                          base_as_ra=1, cand_as_rb=1)
        out = per_race_paired(rows)
        assert out["dwarf"]["vs"]["skaven"]["delta_decisive_pp"] == 0.0
        assert out["human"]["vs"]["orc"]["mean_delta_chess"] == 0.5

    def test_mirror_contributes_two_samples_per_pair(self):
        rows = _pair(0, "dwarf", "dwarf",
                     cand_as_ra=1, base_as_rb=0,
                     base_as_ra=1, cand_as_rb=0)
        out = per_race_paired(rows)
        assert out["dwarf"]["vs"]["dwarf"]["n_paired"] == 2
        assert out["_meta"]["n_pairs"] == 1

    def test_paired_t_detects_small_consistent_edge(self):
        # candidate draws as dwarf where baseline loses: delta chess +0.5
        # per pair, perfectly consistent -> zero-variance +inf t is fine;
        # add one neutral pair to give it finite variance.
        rows = []
        for i in range(9):
            rows += _pair(i, "dwarf", "skaven",
                          cand_as_ra=1, base_as_rb=1,   # draw
                          base_as_ra=0, cand_as_rb=1)   # base dwarf loses
        rows += _pair(9, "dwarf", "skaven",
                      cand_as_ra=1, base_as_rb=1,
                      base_as_ra=2, cand_as_rb=1)  # base dwarf wins this one
        out = per_race_paired(rows)
        d = out["dwarf"]["vs"]["skaven"]
        assert d["mean_delta_chess"] > 0
        assert d["t"] is not None and d["t"] > 1.96
        assert d["significant_95"]

    def test_unpaired_rows_skipped_and_reported(self):
        rows = _pair(0, "orc", "skaven", 1, 0, 1, 0)
        rows.append({"seed_idx": 99, "cand_home": True, "race_h": "orc",
                     "race_a": "skaven", "cand": 3, "base": 0})  # no mirror
        out = per_race_paired(rows)
        assert out["_meta"]["n_unpaired_rows_skipped"] == 1
        assert out["orc"]["vs"]["skaven"]["n_paired"] == 1

    def test_mismatched_mirror_matchup_raises(self):
        rows = _pair(0, "orc", "skaven", 1, 0, 1, 0)
        rows[1]["race_a"] = "dwarf"
        with pytest.raises(ValueError):
            per_race_paired(rows)

    def test_overall_pools_across_matchups(self):
        rows = _pair(0, "dwarf", "skaven", 2, 0, 0, 2)
        rows += _pair(1, "dwarf", "wood-elf", 2, 0, 0, 2)
        out = per_race_paired(rows)
        assert out["dwarf"]["overall"]["n_paired"] == 2
        assert set(out["dwarf"]["vs"]) == {"skaven", "wood-elf"}

    def test_race_ordering_respects_argument(self):
        rows = _pair(0, "orc", "human", 1, 0, 1, 0)
        out = per_race_paired(rows, races=DEFAULT_RACES)
        keys = [k for k in out if k != "_meta"]
        assert keys == ["human", "orc"]
