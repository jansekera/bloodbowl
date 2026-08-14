# Nil-nil fix plán (Team 1 konzultace, 2026-06-01)

## Diagnóza

Nil-nil rate stabilně 40–50% — ani jeden tým nepickuje míč. Příčina:

- Všechny carrier-conditional featury ([12], [40], [42], [63]) jsou `0.0` dokud míč leží na zemi
- Shaping signal pro pickup decision byl fakticky nulový
- Heuristika v `simulate()` má pickup bonus jen **+0.08** — příliš slabý
- **Featura [14] `ballOnGround`** = 1.0 existuje v C++ kódu, ale nebyla v shaping weights

Pokles benchmarku 85% → 81.5% je statisticky nevýznamný (p=0.184, n=400) — čistý šum.

---

## Krok A — implementováno 2026-06-01

**Soubor:** `python/blood_bowl/trainer.py` — `DEFAULT_SHAPING_WEIGHTS`

Změny:
- `(14, -0.8)` — `ball_on_ground`: penalizace za míč ležící na zemi; feature již existovala v C++, jen chyběla váha. Shaping: zvednutí míče = +0.8 bonus (Φ klesne z -0.8 na 0).
- `(67, +0.8)` — `loose_ball_proximity`: incentiva přibližovat se k míči i bez pickupu; feature je aktivní i když míč leží (defaultuje na 0.5, ne 0).
- `(12, +1.2)` — `i_have_ball`: zvýšeno z +0.5. Při 50% pickup úspěšnosti byl expected value pickup bonus jen +0.247 — nedostatečné pro překonání risk aversion.

**Ověření:** 16-epoch run, sledovat `nil_nil_rate` v `score_log.csv`. Cíl: pokles pod 30%.  
Power analysis: pokles ≥7pp je detekovatelný s 80% jistotou po 16 epochách.

---

## Krok B — čeká na ověření kroku A

**Podmínka:** spustit po 16-epoch runu; implementovat pouze pokud nil_nil_rate stále >30%.

**Soubor:** `python/blood_bowl/trainer.py` — metoda `_get_reward` v obou třídách (LinearTrainer i NeuralTrainer)

Změna:
```python
# před:
if winner is None:
    return 0.0
# po:
if winner is None:
    return -0.3   # draw penalty: rozbíjí Nash equilibrium defenzivní hry
```

**Zdůvodnění:** Draw penalty -0.3 rozbíjí symetrii — defenzivní strategie přestane být neutrální (EV=0) a stane se mírně zápornou. RL Expert: při ~40% win rate má útočná strategie EV = 0.4×1 + 0.3×(-0.3) + 0.3×(-1) ≈ +0.01, tj. mírně lepší než draw-hunting.

---

## Gating metrika (navrženo, zatím neimplementováno)

Training Loop Expert doporučuje composite score místo samotného chess:

```
composite = 0.6 × chess_score + 0.4 × (1 - nil_nil_rate) > 0.55
```

Bez tohoto model projde gatingem i s 50% nil_nil rate pokud chess > 50%.
