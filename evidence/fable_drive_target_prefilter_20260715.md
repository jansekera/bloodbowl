# Drive-level value target (c1) — offline pre-filter: **REFUTED**

**Datum:** 2026-07-15 · **Autor:** Fable 5 (offline agent)
**Zadání:** `proposals_value_signal_roadmap_20260714.md` §2.4 — nejlevnější falzifikace ve frontě.
**Nástroj:** `diag_drive_target_diff.py` (ponechán v repu vedle `diag_td_mix_target_diff.py`, znovupoužitelný).
**Data:** živý `/home/jan/claude/bloodbowl/replay_buffer.pkl` (10 000 transitions, 625 epizod, snapshot 2026-07-14 08:44 — pre-H2-fix, per roadmap §2.3 pro divergenční verdikt OK).
**Metodika:** §E — init `weights_best.json`, lr=3e-4, 3 průchody, seed 20260714, identické shuffled pořadí ve všech ramenech; referenční rameno = čistý G target (bit-exact α=1.0 rameno z td_mix skriptu).

## Rekonstrukce drivů

Hranice = `reward_step≠0` (TD mezi stavem i a i+1 → drive končí ve stavu i), konec epizody = `is_terminal`. Census: **1295 drivů** (316 own-TD, 354 opp-TD, 625 vyšumělých). Poznámka: hranice poločasu bez TD v bufferu vidět není → H1 bezgólový drive splývá s prvním H2 drivem téhož druhu (přiznaná limitace, na divergenční verdikt bez vlivu).

## Referenční hodnoty

| veličina | hodnota |
|---|---|
| within-ep std labelu G | 0.0276 (potvrzuje post-mortem 0.028) |
| init hlava: within-ep V std / ctrl ramp / MSE | 0.2567 / −0.1430 / 0.2380 |
| **ref rameno (target=G)**: within-ep V std / ctrl ramp / MSE | **0.2343 / −0.0579 / 0.2106** |

## Grid (λ×D×d0), metriky proti ref rameni

Prahy §1.2: mean|ΔV|>0.1 · corr<0.99 · within-ep V std ↑ · MSE zhoršení <10 % rel. Ramp = **outcome-controlled** (jen uvnitř own-TD epizod; uncontrolled varianta záměrně nereportována).

| λ | D | d0 | ep-std **T** (label) | mean\|ΔV\| | corr | ep-std V | MSE vs G (rel) | ctrl ramp |
|---|---|---|---|---|---|---|---|---|
| 0.7 | 0.4 | 0.0 | 0.071 | 0.078 ✗ | 0.996 ✗ | 0.227 ✗ | +5.3 % ✓ | −0.081 |
| 0.7 | 0.4 | 0.1 | 0.070 | 0.072 ✗ | 0.996 ✗ | 0.227 ✗ | +4.4 % ✓ | −0.079 |
| 0.7 | 0.6 | 0.0 | 0.077 | 0.062 ✗ | 0.998 ✗ | 0.228 ✗ | +3.7 % ✓ | −0.069 |
| 0.7 | 0.6 | 0.1 | 0.076 | 0.058 ✗ | 0.998 ✗ | 0.228 ✗ | +3.1 % ✓ | −0.067 |
| 0.5 | 0.4 | 0.0 | 0.107 | 0.140 ✓ | 0.986 ✓ | 0.220 ✗ | +14.8 % ✗ | −0.101 |
| 0.5 | 0.4 | 0.1 | 0.105 | 0.134 ✓ | 0.987 ✓ | 0.220 ✗ | +13.1 % ✗ | −0.097 |
| 0.5 | 0.6 | 0.0 | 0.118 | 0.112 ✓ | 0.992 ✗ | 0.221 ✗ | +10.2 % ✗ | −0.078 |
| 0.5 | 0.6 | 0.1 | 0.115 | 0.104 ✓ | 0.993 ✗ | 0.221 ✗ | +8.5 % ✓ | −0.075 |
| 0.3 | 0.4 | 0.0 | 0.144 | 0.211 ✓ | 0.968 ✓ | 0.210 ✗ | +31.1 % ✗ | −0.123 |
| 0.3 | 0.4 | 0.1 | 0.141 | 0.203 ✓ | 0.968 ✓ | 0.211 ✗ | +28.1 % ✗ | −0.116 |
| 0.3 | 0.6 | 0.0 | 0.159 | 0.167 ✓ | 0.983 ✓ | 0.212 ✗ | +20.7 % ✗ | −0.090 |
| 0.3 | 0.6 | 0.1 | 0.156 | 0.157 ✓ | 0.984 ✓ | 0.212 ✗ | +17.6 % ✗ | −0.085 |

**Žádná kombinace neprošla všemi prahy.** Struktura selhání je komplementární past:
- λ=0.7: MSE OK, ale hlava se od reference prakticky nehne (ΔV 0.06–0.08, corr ≥0.996) — málo drive složky = žádná divergence.
- λ≤0.5: divergence ano (ΔV 0.10–0.21, corr až 0.968), ale MSE vs G se rozpadá (+10 až +31 %, přesný mc_return_shaped 89→80 vzor) a hlavně…
- **within-episode V std NEVZROSTLA ANI JEDNOU** — všech 12 kombinací ji naopak snižuje (0.210–0.228 vs ref 0.234), přestože label sám within-game strukturu prokazatelně nese (ep-std T 0.07–0.16 = 2.5–5.8× G). Controlled ramp je ve všech ramenech zápornější než ref (−0.067 až −0.123 vs −0.058).

## Interpretace — přesně diagnostický případ roadmapy §2.3 (poslední odstavec)

Target novou informaci **má** (drive outcome v labelech zvedl within-ep std labelu řádově, jak §2.1 predikoval), ale lineární hlava na 73 featurách ji **neumí reprezentovat**: vysoké mean|ΔV| se realizuje jako mezi-epizodní přeškálování (mean|V| klesá 0.62→0.42–0.57), ne jako within-game struktura. Nuance: *relativní* within-ep struktura (ep-std/mean|V|) mírně roste (0.38→0.50 při λ=0.3), tj. informace se do hlavy tlačí, ale jen za cenu smrštění a rozbité outcome kalibrace — absolutní práh §1.2 to neplní a MSE guard to zabíjí souběžně.

**Závěr per §2.3/§7:** binding constraint = **kapacita featur, ne tvar targetu**. Tento výsledek je druhý nezávislý doklad téže věci (první: post-mortem — V std 0.24 z featur vs label std 0.028).

## Doporučení

1. **c1 `drive_mix` NEIMPLEMENTOVAT** (žádné Python wiring, žádný trénink). Zápis zrcadlí mc_td_mix REFUTED vzor; ušetřen celý implementační+tréninkový cyklus za cenu 48 s CPU.
2. **Posílit prioritu kanálu (a) per-player featury** — Fáze A (Python-only snapshot persist + ridge) přesně kvantifikuje, kolik kapacity chybí; roadmap §7 větev „ΔV vysoké, ep-std stojí" explicitně ukazuje sem.
3. Kanál (b1) teacher zůstává pořadí #2 beze změny (piggyback na H2-fix rebuild); jeho pre-filtr navíc rovnou změří featurovou kapacitu na H (fit featury→H, §3.1 bod 1) — tj. (a) a (b) testy se z tohoto nálezu vzájemně posilují.
4. Pokud by se c1 někdy vracelo, má smysl **až po** rozšíření featur (kanál a) — target sám o sobě informaci nese, jen ji dnešní hlava nemá kam dát.

*Skript: `diag_drive_target_diff.py` · surový výstup běhu: scratchpad `drive_target_run1.log` (nepřežije session; kompletní čísla jsou v tabulce výše). Nic necommitováno.*
