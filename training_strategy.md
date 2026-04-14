# Training Strategy — nápady a poznatky (duben 2026)

## Aktuální stav

- `weights_best.json` = 86.7% vs random (PROMOTED duben 2026)
- Parametry: EPOCHS=15, GAMES=60, MCTS=100, LR=0.0001
- Oscilujeme mezi 80–90% — těžko překročit 90%

## Snapshoty ze 90%+ (25. března 2026)

Oba soubory z éry MCTS=50, hidden=64, kdy byl rekord **93.3%**:

| Soubor | Score diff | W2 mean abs | W1 max abs |
|---|---|---|---|
| `weights_snap_e25_90pct_+1.2.json` | +1.2 | 0.166 | 0.350 |
| `weights_snap_e25_90pct_+1.0.json` | +1.0 | 0.195 | 0.311 |

Modely jsou **výrazně odlišné** (W1 avg abs diff = 0.14) — nejde o šum.
**Doporučení: použít +1.2** (vyšší score diff = přesvědčivější výhry vs random).

Jak použít: zkopírovat do `weights_best.json` a spustit trénink.

---

## Možnosti jak dále (seřazeno od nejméně invazivní)

### A) Vrátit se k 90% snapshotu ← DOPORUČENO jako první krok
- Zkopírovat `weights_snap_e25_90pct_+1.2.json` → `weights_best.json`
- Trénovat ze silnějšího výchozího bodu (93.3% éra)
- Parametry nechat stejné (EPOCHS=15, GAMES=60, MCTS=100)
- **Riziko:** snapshot je z MCTS=50 éry, přechod na MCTS=100 může způsobit nestabilitu

### B) Snížit LR na 0.00005
- Konzervativnější aktualizace, menší oscilace
- Vhodné pokud model osciluje kvůli příliš agresivním krokům
- **Riziko:** pomalejší učení, může stagnovat

### C) Zvýšit GAMES na 80
- Ještě méně šumu na epochu (byl GAMES=40 → spike epoch 6, GAMES=60 → stabilnější)
- Trénink ~7.5h — stále v limitu
- **Riziko:** čas se blíží 8h hranici

### D) Použít weights_train_best.json místo weights_best.json
- Po každém tréninku se ukládá nejlepší epocha do `weights_train_best.json`
- Tato epocha bývá lepší než finální (model degraduje ke konci)
- Nastavit notebook aby četl `weights_train_best.json` jako výchozí bod
- **Riziko:** může způsobit nestabilitu gating logiky (frozen vs train_best)

### E) Pokračovat stejně (variance)
- Oscilace 80–90% může být jen šum; průměr je ~85%
- Každá iterace má ~50% šanci dostat se nad 90% (hrubý odhad)
- **Riziko:** ztráta času bez pokroku

---

## Pravidla pro nasazení (z reward_shaping_ideas.md)
- Vždy jen jedna změna naráz
- Benchmark ≥ 90% po 2× po sobě → teprve uvažovat o dalším experimentu
