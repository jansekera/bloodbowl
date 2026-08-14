# Brief pro tým: Rozchodit správně plné AlphaZero na Blood Bowl projektu

**Datum:** 2026-06-15
**Stav:** příprava zadání — čeká na doplnění od uživatele před spuštěním týmu
**Kontext:** [[project-bloodbowl]], navazuje na `team1_results_opus.md` (per-player features — ODLOŽENO ve prospěch tohoto)

---

## Proč tento brief (rozhodnutí)

Model stagnuje na ~86–87 % (TV1200), ~75 % her končí remízou (Nash equilibrium),
head-to-head vs frozen ~50 %. Dosud zvažovaný krok byl **per-player features (70→492)**.
**Přehodnoceno:** nejdřív musíme rozjet **plné AlphaZero**, jinak budeme stagnovat
i s per-player features — přidaná kapacita modelu nepomůže, pokud se model neučí
ze searche (chybí policy-improvement operátor).

## Klíčové zjištění (ověřit, ale silná indicie)

**Celá AlphaZero policy infrastruktura je POSTAVENÁ, ale leží ladem — vypnutá dvěma vypínači:**

1. **`run_iteration.py:230` `--policy-lr=0`** → `training_loop.py:94`
   `use_policy = policy_lr > 0 and mcts_iterations > 0` → **False** → `policy_trainer`
   se vůbec neinstancuje. Policy síť se NEtrénuje.
2. **`--policy-blend` se v `run_iteration.py` nepředává** → default `0.0` (`train_cli.py:74`)
   → v MCTS guard `config_.policy && config_.policyBlend > 0.0f`
   (`engine/src/macro_mcts.cpp:188,312`) nikdy nesplněn → search bere **heuristické
   priory, ne naučenou policy**. I kdyby se policy trénovala, search ji ignoruje.

**Co reálně běží:** value-guided MCTS s heuristickými priory + reward shaping
(`--training-method=mc_shaped`). NE AlphaZero.

**Hotové, ale nepoužité díly AZ:**
- `python/blood_bowl/policy_trainer.py` — cross-entropy na MCTS visit distribucích
  (`train_on_decisions`), linear i `NeuralPolicyTrainer`, `save/load_combined_weights`.
- PUCT s policy priory: `engine/src/mcts.cpp:22` (doslova "AlphaZero PUCT"),
  priory z policy sítě `mcts.cpp:204-243`, progressive widening podle priorů.
- Emise visit counts z C++: `cpp_runner.py:84,354` (`visit_fraction` per rozhodnutí).
- Dirichlet noise na root (`macro_mcts.cpp:88-99`, `dirichletWeight`/`dirichletAlpha`).
- **Imitation warmup** (`training_loop.py:440`, `train_cli.py:77 --imitation-epochs`):
  natrénovat policy na heuristické MCTS visity (policyBlend=0), pak blend rozjet.
  Klasický bootstrap — taky postavený a nepoužitý.

## Centrální otázka pro tým

**Jak správně nakonfigurovat a rozchodit plné AlphaZero na tomto projektu?**
Konkrétně: najít KAŽDÉ spící/špatně nastavené místo AZ smyčky, určit **minimální
správnou konfiguraci** k zapnutí, identifikovat rizika a předložit **ověřený
step-by-step bring-up plán** včetně toho, jak poznat, že se model reálně učí.

AZ smyčka, kterou ověřujeme: self-play vyrobí `(stav, π=visit-counts, z=výsledek)`
→ trénuj policy na π (cross-entropy) + value na z → použij novou síť jako prior
v MCTS → opakuj. Najít, kde je to rozbité/vypnuté, a minimální správnou konfiguraci.

## Dekompozice na agenty (oblasti)

### Agent 1 — Policy training & data path
- Co přesně dělá `policy-lr > 0`? Round-trip combined weights C++↔Python
  (`save/load_combined_weights`, `bb_module.cpp` pybind, `weights_az_train.json` formát).
- Teče visit-count signál správně? `cpp_runner.py` decisions → `training_loop.py:430-446`.
- `PolicyTrainer` (linear) vs `NeuralPolicyTrainer` — který pro tento projekt a proč.
- **Past (z team1):** `policy_network.cpp:44` hardcode `float hidden[64]` + `min(H,64)`;
  `features.py` parity vs C++ (`NUM_FEATURES=70` na ~12 místech). Ověřit, že policy
  vstup (state 70 + action 15 = 85) je konzistentní C++↔Python.

### Agent 2 — MCTS prior consumption (search side)
- Jak priory vstupují do PUCT (`mcts.cpp` micro i `macro_mcts.cpp` makro — OBA mají
  policy path, ověřit který se v běhu používá).
- `policyBlend` schedule: AZ chce postupně 0→~1? Nebo fixní? Interakce s progressive
  widening (`maxChildren`) a dirichlet noise (jen self-play root, ne benchmark).
- Self-play vs benchmark vs gating: kde má být policy zapnutá a kde ne (explorace vs
  čistá síla). Aktuální `policy_blend=0.0` ve VŠECH cestách (`cpp_runner.py:131`).

### Agent 3 — Value target & reward shaping
- `mc_shaped` (MC návrat + shaping) vs AZ čisté `z` (±1 výsledek). Má se shaping
  pro AZ vypnout/omezit? Interakce s remízovým equilibriem (shaping může remízy betonovat).
- `td_lambda` jako alternativa — kdy.
- Shaping features (idx 12,15,35,40,42,59,63 — viz [[project-bloodbowl]]) vs čistý AZ.

### Agent 4 — End-to-end bring-up plán & detekce učení
- **Minimální správná konfigurace** k zapnutí obou vypínačů (konkrétní flagy
  `run_iteration.py` + `train_cli`), pořadí kroků.
- Bootstrap: použít imitation warmup (`--imitation-epochs`)? Warm-start z aktuálních
  value weights?
- **Jak poznat, že se to UČÍ** (ne jen běží): policy loss klesá, shoda prior↔visit
  roste, remízovost klesá, decisive head-to-head roste. Jaké metriky logovat.
- Rollback safety + jak to sedí na gating (právě opraveno na decisive-only,
  `run_iteration.py`) a na pozdější per-player.

## Provozní omezení (důležité)

- **Styl:** jedna změna naráz, ověřit (2–3 iterace), pak další. Viz [[feedback-implementation-style]].
- **Server po pull musí rebuildnout engine** (`cd engine/build && make -j$(nproc)`),
  měnit-li se C++. Bez pushe se změny v tréninku neprojeví. Viz [[feedback-commit-before-training]].
- Hardware: 12 CPU, MCTS=100, EPOCHS=16, GAMES_PER_EPOCH=40, hidden_size=64.
- **Lekce z team1:** Sonnet brief měl chyby, Opus je proti kódu opravil. Tým musí
  KAŽDÉ tvrzení ověřit proti aktuálnímu kódu, ne věřit tomuto briefu.

## Očekávaný deliverable

`team_alphazero_results.md`: ověřený stav AZ smyčky (co je rozbité/vypnuté, file:line),
minimální správná konfigurace, rizika, **step-by-step bring-up plán** seřazený podle
ceny/přínosu, a sada metrik pro detekci, že se model reálně učí.

## K DOPLNĚNÍ od uživatele (před spuštěním)

- [ ] Souhlas se scope / oblastmi agentů
- [ ] Model agentů (team1 = Opus)
- [ ] Případná další omezení / preference (čistý AZ vs ponechat část shapingu?)
