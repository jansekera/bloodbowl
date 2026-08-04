"""Meta round-trip testy pro policy promotion (fix 04.08.2026).

Regrese z první promoce s BB_GATE_POLICY_BLEND=0.2 (commit 6f2c50e):
_git_push po `git reset --hard` REBUILDOVAL weights_best_meta.json od nuly a
tiše zahodil policy_blend/policy_md5 zapsané promote větví → další iterace by
frozen šampiona nechala hrát s policy_blend=0.0 místo blendu jeho promoce
(nefér měkčí gate) + spustila zbytečný frozen re-benchmark. Navíc
weights_best_policy.json nebyl v push file listu.

Testy kryjí:
- _promote_meta_write: benchmark_vf_blend přítomen (minor nález č. 2 z 03.08.
  — bez něj re-benchmark každou iteraci po promoci), policy_blend/policy_md5
  zapsány, fallback bez policy souboru nuluje blend.
- _reject_meta_write: benchmark pole aktualizována na aktuální konfiguraci,
  policy pole šampiona PŘEŽIJÍ nedotčená.
- _git_push: meta i policy snapshot přežijí reset beze změny (byte-identické),
  snapshot jde do `git add` jen když existuje.
"""
from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

import pytest

PROJECT_ROOT = str(Path(__file__).parent.parent.parent)
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)

import run_iteration as ri


@pytest.fixture
def cfg(monkeypatch):
    """Deterministická gate konfigurace nezávislá na env při importu."""
    monkeypatch.setattr(ri, 'GATE_VF_BLEND', 0.15)
    monkeypatch.setattr(ri, 'GATE_POLICY_BLEND', 0.2)
    monkeypatch.setattr(ri, 'MCTS_ITERATIONS', 100)
    monkeypatch.setattr(ri, 'TV', 1200)


# ---------------------------------------------------------------- promote meta

def test_promote_meta_complete(cfg, tmp_path):
    policy = tmp_path / 'stash_policy.json'
    policy.write_text('{"policy_weights": [1, 2, 3]}')

    meta = ri._promote_meta_write(tmp_path, 0.985, 0.99, str(policy))

    on_disk = json.loads((tmp_path / 'weights_best_meta.json').read_text())
    assert on_disk == meta
    assert meta['benchmark_vf_blend'] == 0.15
    assert meta['benchmark_mcts_iterations'] == 100
    assert meta['benchmark_win_rate'] == 0.985
    assert meta['all_time_best_benchmark'] == 0.99
    assert meta['policy_blend'] == 0.2
    assert meta['policy_md5'] == hashlib.md5(policy.read_bytes()).hexdigest()
    # promotion snapshot vznikl a odpovídá policy souboru z gate
    snap = tmp_path / 'weights_best_policy.json'
    assert snap.read_bytes() == policy.read_bytes()


def test_promote_meta_all_time_best_takes_max(cfg, tmp_path):
    policy = tmp_path / 'stash_policy.json'
    policy.write_text('{}')
    meta = ri._promote_meta_write(tmp_path, 0.995, 0.99, str(policy))
    assert meta['all_time_best_benchmark'] == 0.995


def test_promote_meta_without_policy_file_zeroes_blend(cfg, tmp_path):
    meta = ri._promote_meta_write(tmp_path, 0.985, 0.99, '')
    assert meta['policy_blend'] == 0.0
    assert 'policy_md5' not in meta
    assert not (tmp_path / 'weights_best_policy.json').exists()
    # benchmark_vf_blend musí být i v této větvi
    assert meta['benchmark_vf_blend'] == 0.15


# ----------------------------------------------------------------- reject meta

def test_reject_meta_preserves_policy_fields(cfg, tmp_path):
    (tmp_path / 'weights_best_meta.json').write_text(json.dumps({
        'benchmark_win_rate': 0.985, 'benchmark_mcts_iterations': 100,
        'benchmark_vf_blend': 0.15, 'all_time_best_benchmark': 0.99,
        'tv': 1200, 'policy_blend': 0.2, 'policy_md5': 'cafebabe'}))

    meta = ri._reject_meta_write(tmp_path, 0.98, 0.99)

    on_disk = json.loads((tmp_path / 'weights_best_meta.json').read_text())
    assert on_disk == meta
    # policy pole šampiona přežila
    assert meta['policy_blend'] == 0.2
    assert meta['policy_md5'] == 'cafebabe'
    # benchmark baseline aktualizována (re-benchmark se nesmí triggerovat znovu)
    assert meta['benchmark_win_rate'] == 0.98
    assert meta['benchmark_vf_blend'] == 0.15


def test_reject_meta_without_existing_meta(cfg, tmp_path):
    meta = ri._reject_meta_write(tmp_path, 0.97, 0.98)
    assert meta['benchmark_win_rate'] == 0.97
    assert 'policy_blend' not in meta  # před první promocí s blendem nic nevzniká


# ------------------------------------------------------------- _git_push round-trip

@pytest.fixture
def push_repo(tmp_path):
    """Soubory jako po promote/reject větvi, těsně před _git_push."""
    (tmp_path / 'weights_best.json').write_text('{"value_weights": [9]}')
    meta = {'benchmark_win_rate': 0.985, 'benchmark_mcts_iterations': 100,
            'benchmark_vf_blend': 0.15, 'all_time_best_benchmark': 0.99,
            'tv': 1200, 'policy_blend': 0.2, 'policy_md5': 'cd72ed6b'}
    (tmp_path / 'weights_best_meta.json').write_text(json.dumps(meta))
    (tmp_path / 'weights_train_best.json').write_text('{"gate": 1}')
    (tmp_path / 'weights_frozen.json').write_text('{"frozen": 1}')
    (tmp_path / 'epoch_metrics.csv').write_text('epoch,loss\n1,0.5\n')
    return tmp_path


def _fake_git(root: Path, calls: list):
    """Mock subprocess.run: `git reset --hard` simuluje clobber working tree
    na committed (zastaralou) verzi — přesně scénář reálného bugu."""
    def run(cmd, cwd=None, capture_output=None):
        calls.append(list(cmd))
        if cmd[:2] == ['git', 'reset']:
            (root / 'weights_best_meta.json').write_text('{"benchmark_win_rate": 0.5}')
            (root / 'weights_best.json').write_text('{"committed": 1}')
            snap = root / 'weights_best_policy.json'
            if snap.exists():
                snap.unlink()

        class R:
            returncode = 0
            stderr = b''
        return R()
    return run


def test_git_push_meta_and_snapshot_survive_reset(push_repo, monkeypatch):
    root = push_repo
    (root / 'weights_best_policy.json').write_text('{"policy_weights": [7]}')
    meta_before = (root / 'weights_best_meta.json').read_bytes()
    snap_before = (root / 'weights_best_policy.json').read_bytes()

    calls = []
    monkeypatch.setattr(ri.subprocess, 'run', _fake_git(root, calls))
    ri._git_push(root, True, root / 'weights_frozen.json',
                 root / 'weights_train_best.json', 0.985, 0.558, 'promoted')

    # meta round-trip: byte-identické, policy pole nezahazovat
    assert (root / 'weights_best_meta.json').read_bytes() == meta_before
    assert (root / 'weights_best_policy.json').read_bytes() == snap_before
    # promote → best = gate data (přežije reset)
    assert (root / 'weights_best.json').read_text() == '{"gate": 1}'
    add = next(c for c in calls if c[:3] == ['git', 'add', '-f'])
    assert 'weights_best_policy.json' in add
    assert 'weights_best_meta.json' in add


def test_git_push_reject_restores_frozen(push_repo, monkeypatch):
    root = push_repo
    calls = []
    monkeypatch.setattr(ri.subprocess, 'run', _fake_git(root, calls))
    ri._git_push(root, False, root / 'weights_frozen.json',
                 root / 'weights_train_best.json', 0.985, 0.47, 'rejected')
    assert (root / 'weights_best.json').read_text() == '{"frozen": 1}'


def test_git_push_without_snapshot_omits_it_from_add(push_repo, monkeypatch):
    root = push_repo
    calls = []
    monkeypatch.setattr(ri.subprocess, 'run', _fake_git(root, calls))
    ri._git_push(root, True, root / 'weights_frozen.json',
                 root / 'weights_train_best.json', 0.985, 0.558, 'promoted')
    add = next(c for c in calls if c[:3] == ['git', 'add', '-f'])
    assert 'weights_best_policy.json' not in add
    assert not (root / 'weights_best_policy.json').exists()


# ------------------------------------------------- policy stash backup (04.08.)

def _policy_head(tag='x'):
    return {'policy_type': 'mlp', 'policy_hidden_size': 8,
            'policy_W1': [1.0], 'policy_b1': [0.5],
            'policy_W2': [[0.1]], 'policy_b2': [0.2],
            'policy_temperature': 1.0, 'policy_tag': tag}


def test_stash_creates_timestamped_backup(tmp_path):
    az = tmp_path / 'weights_az_train.json'
    az.write_text(json.dumps({'type': 'alphazero_neural', **_policy_head()}))
    stash = tmp_path / 'weights_policy.json'
    ri._stash_policy(az, stash)
    assert stash.exists()
    backups = list((tmp_path / 'policy_backups').glob('weights_policy_*.json'))
    assert len(backups) == 1
    assert json.loads(backups[0].read_text()) == json.loads(stash.read_text())


def test_carry_over_restores_missing_stash_from_backup(tmp_path):
    # Incident 2026-08-04: stash zmizel po iteraci -> carry-over se musí sám
    # obnovit z nejnovější zálohy, jinak se akumulace přes rejecty tiše přeruší.
    bdir = tmp_path / 'policy_backups'
    bdir.mkdir()
    (bdir / 'weights_policy_20260803_120000_aaaa.json').write_text(
        json.dumps({k: v for k, v in _policy_head('old').items() if k != 'policy_tag'}))
    newer = {k: v for k, v in _policy_head('new').items() if k != 'policy_tag'}
    newer['policy_b1'] = [9.9]
    (bdir / 'weights_policy_20260804_130000_bbbb.json').write_text(json.dumps(newer))
    best = tmp_path / 'weights_best.json'
    best.write_text(json.dumps({'type': 'neural', 'hidden_size': 4, 'n_features': 2,
                                'W1': [0.0], 'b1': [0.0], 'W2': [[0.0]], 'b2': [0.0]}))
    az = tmp_path / 'weights_az_train.json'
    stash = tmp_path / 'weights_policy.json'

    ri._carry_over_policy(az, best, stash)

    assert stash.exists(), 'stash must be self-healed from the newest backup'
    assert json.loads(stash.read_text())['policy_b1'] == [9.9]
    assert 'policy_W1' in json.loads(az.read_text())


def test_backup_retention_keeps_newest_30(tmp_path):
    az = tmp_path / 'weights_az_train.json'
    stash = tmp_path / 'weights_policy.json'
    bdir = tmp_path / 'policy_backups'
    bdir.mkdir()
    for i in range(35):
        (bdir / f'weights_policy_20260701_{i:06d}_old.json').write_text('{}')
    az.write_text(json.dumps({'type': 'alphazero_neural', **_policy_head()}))
    ri._stash_policy(az, stash)
    backups = sorted(bdir.glob('weights_policy_*.json'))
    assert len(backups) == 30
    assert backups[-1].name.endswith('.json') and '2026' in backups[-1].name
