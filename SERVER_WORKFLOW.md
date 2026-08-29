> ## ⛔⛔ HISTORICKÝ ZÁZNAM — SERVER UŽ NENÍ (29.08.2026)
>
> `jan@linuxs` (192.168.40.143) je **nedostupný natrvalo** (ověřeno 29.08.:
> `connect ... port 22: Connection timed out`). Postup níž **není návod
> k použití**, je to záznam, jak se to dělalo.
>
> ⭐ **Jedna věc z něj ale platí dál a jinde se nikde nepíše:** limit sezení
> Google Colabu je podle zkušenosti tohoto projektu **8 hodin** — a to je
> číslo, podle kterého se dnes krájí noc na kusy
> (`colab_night_preflight.py`, `colab_night_chunked.py`).
>
> Co po serveru zbylo: záloha domovského adresáře je
> `/home/jenda/zal/claude/` (stav 26.08.), korpus `bb-data` a noční snapshoty
> jen tam.

# Server Workflow — Blood Bowl AI Training

Tento dokument popisuje kompletní postup pro spuštění AlphaZero tréninku na vzdáleném serveru přes SSH. Bez časového limitu Colabu (8–10h) lze pustit 10–20 iterací najednou.

---

## Předpoklady

### Na laptopu
- SSH klíč `jansekera` = `~/.ssh/id_ed25519_jansekera` (přidán do GitHub)
- SSH alias `github-jansekera` nakonfigurován v `~/.ssh/config`
- Server dostupný jako `ssh server` (alias v `~/.ssh/config` → port 2222)
- Repo naklonované lokálně s fungujícím buildem

### Na serveru
- Ubuntu 22.04+ (nebo jiný Linux s apt-get)
- SSH přístup z laptopu přes port 2222 (`ssh server`)
- Klíč `jansekera` zatím chybí — přidáme ho (viz krok 1b)

---

## 1. První spuštění — setup serveru

### 1a. Připoj se na server
```bash
# Alias "server" je nakonfigurován v ~/.ssh/config na laptopu
ssh server
# = ssh jsekera@192.168.40.200 -p 2222
```

### 1b. Přenes klíč `jansekera` na server (jednorázově)

Na **laptopu**:
```bash
# Zkopíruj privátní i veřejný klíč na server
scp ~/.ssh/id_ed25519_jansekera server:~/.ssh/
scp ~/.ssh/id_ed25519_jansekera.pub server:~/.ssh/
```

Na **serveru**:
```bash
chmod 600 ~/.ssh/id_ed25519_jansekera
chmod 644 ~/.ssh/id_ed25519_jansekera.pub
```

Přidej SSH config alias na serveru (`~/.ssh/config`):
```
Host github-jansekera
    HostName github.com
    User git
    Port 22
    IdentityFile ~/.ssh/id_ed25519_jansekera
```

Test:
```bash
ssh -T github-jansekera
# Očekávaný výstup: Hi jansekera! You've successfully authenticated...
```

Tím bude `git push` fungovat, protože remote URL repo používá alias `github-jansekera`:
```
git@github-jansekera:jansekera/bloodbowl.git
```

**Pokud SSH klíč ještě není na serveru:** Prozatím použij `--no-push` a přenes weights ručně (viz sekce 5).

### 1c. Naklonuj repo a sestav engine

```bash
# Klonování — používáme SSH alias github-jansekera (stejně jako na laptopu)
git clone git@github-jansekera:jansekera/bloodbowl.git ~/bloodbowl

# Setup: nainstaluje deps, sestaví C++ engine, ověří
cd ~/bloodbowl
bash setup.sh
```

Setup trvá cca 3–5 minut. Na konci uvidíš:
```
C++ testy: [  PASSED  ] 395 tests
bb_engine import: OK (Human)
=== Setup complete! ===
```

---

## 2. Příprava před každým tréninkem

### 2a. Aktualizuj repo (dostaneš nejnovější weights z Colabu/laptopu)
```bash
cd ~/bloodbowl
source venv/bin/activate
git pull origin main
```

Ověř, že `weights_best.json` je aktuální:
```bash
python3 -c "
import json
with open('weights_best_meta.json') as f:
    m = json.load(f)
print(f'Benchmark: {m[\"benchmark_win_rate\"]:.1%}, MCTS: {m[\"benchmark_mcts_iterations\"]}')
"
```

### 2b. (Volitelně) Uprav parametry
Parametry jsou na začátku `run_iteration.py`:
```python
EPOCHS = 10
GAMES_PER_EPOCH = 40
MCTS_ITERATIONS = 100
...
```

Na výkonném serveru lze zvýšit:
- `GAMES_PER_EPOCH = 60` — více dat na epochu, stabilnější trénink (~8.5h na iteraci)
- `MCTS_ITERATIONS = 200` — hlubší prohledávání, ~2× pomalejší
- `EPOCHS = 15` — delší trénink (~7.5h na iteraci s GAMES=40)

Na slabším serveru nech výchozí (EPOCHS=10, GAMES=40, MCTS=100 ≈ 5–6h/iterace).

---

## 3. Spuštění tréninku

### Jednoduchý run (1 iterace)
```bash
cd ~/bloodbowl
source venv/bin/activate
python3 run_iteration.py
```

### Více iterací za sebou (doporučeno pro server)
```bash
python3 run_iteration.py --loop 5
```
Každá iterace: freeze → self-play → gate → promote/reject → push. Spouštějí se za sebou automaticky.

### Spuštění na pozadí (odpojitelné od SSH)
```bash
# screen (jednodušší)
screen -S bloodbowl
python3 run_iteration.py --loop 10
# Odpoj: Ctrl+A, D
# Znovu připoj: screen -r bloodbowl

# nebo nohup (bez screen)
nohup python3 run_iteration.py --loop 10 > training.log 2>&1 &
echo "PID: $!"
# Sleduj průběh:
tail -f training.log
```

### Bez git push (server bez GitHub SSH klíče)
```bash
python3 run_iteration.py --loop 5 --no-push
# Weights jsou uloženy lokálně — přenes ručně (viz sekce 5)
```

---

## 4. Sledování průběhu

### Průběh tréninku (pokud běží v screenu)
```bash
screen -r bloodbowl
# Odpoj znovu: Ctrl+A, D
```

### Log z nohup
```bash
tail -f ~/bloodbowl/training.log
```

### Stav weights po dokončení
```bash
cd ~/bloodbowl
python3 -c "
import json
with open('weights_best_meta.json') as f:
    m = json.load(f)
print(f'Benchmark: {m[\"benchmark_win_rate\"]:.1%}')
"
```

---

## 5. Přenos výsledků (pokud `--no-push`)

Pokud server nemá GitHub SSH klíč, přenes weights ručně na laptop:

```bash
# Na laptopu — stáhni weights ze serveru (alias "server" v ~/.ssh/config)
rsync -av server:~/bloodbowl/weights_best.json \
           ~/bloodbowl/
rsync -av server:~/bloodbowl/weights_best_meta.json \
           ~/bloodbowl/
rsync -av "server:~/bloodbowl/weights_snap_*.json" \
           ~/bloodbowl/

# Pushni z laptopu
cd ~/clone/bloodbowl
git add weights_best.json weights_best_meta.json weights_snap_*.json
git commit -m "weights ze serveru"
git push
```

---

## 6. Aktualizace kódu na serveru

Pokud upravíš Python kód na laptopu a pushneš:
```bash
# Na serveru
cd ~/bloodbowl
git pull origin main
# Rebuild engine není potřeba pokud se C++ kód nezměnil
```

Pokud se změnil C++ engine (soubory v `engine/src/` nebo `engine/include/`):
```bash
cd ~/bloodbowl
source venv/bin/activate
cd engine/build
make -j$(nproc)
cd ../..
./engine/build/bb_tests --gtest_brief=1 2>&1 | tail -1
```

---

## 7. Tipování výkonu

Odhad času na iteraci (EPOCHS=10, GAMES=40):

| CPU                  | Čas/hra (MCTS=100) | Čas/iterace |
|----------------------|--------------------|-------------|
| Colab (T4/CPU)       | ~46s               | ~5.5h       |
| Laptop (i7/Ryzen 7)  | ~20–35s            | ~2.5–4h     |
| Server (32 jader)    | ~5–15s             | ~0.8–2h     |

> **Poznámka:** MCTS je single-threaded per hra, ale hry běží sekvenčně (jedna po druhé). Více jader nepomáhá přímo — ale rychlejší jedno jádro = rychlejší trénink.

Pro skutečnou paralelizaci by bylo potřeba upravit CPPRunner, aby spouštěl více her najednou (zatím není implementováno).

---

## 8. Typický workflow den po dni

```
Ráno:
  laptop → git pull (stáhni výsledky z noci)
  → ověř benchmark, rozhodnutí o parametrech

Přes den (server):
  server → git pull → python3 run_iteration.py --loop 5
  → běží 5 iterací (~5–10h podle HW)

Večer:
  laptop → git pull → zkontroluj výsledky
  → případně uprav parametry v run_iteration.py
  → git push → server zítra stáhne aktualizaci
```

---

## 9. Řešení problémů

### `bb_engine not found`
```bash
cd ~/bloodbowl && bash setup.sh
```

### `git push failed`
Zkontroluj SSH klíč: `ssh -T github-jansekera`  
Nebo použij `--no-push` a přenes ručně (viz sekce 5).

### `git pull` conflict (server i laptop pushly najednou)
```bash
cd ~/bloodbowl
git fetch origin
git reset --hard origin/main
# Weights na serveru jsou v weights_az_train.json — neztrácejí se
```

### Trénink se zastaví uprostřed (SSH disconnect, reboot)
```bash
# weights_az_train.json = výsledek posledního dokončeného tréninku
# weights_best.json = nejlepší doposud promoted model
# Stačí znovu spustit run_iteration.py — začne novou iteraci od weights_best.json
python3 run_iteration.py --loop N
```
