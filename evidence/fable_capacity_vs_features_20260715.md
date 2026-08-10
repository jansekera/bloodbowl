# Capacity vs. features probe — verdikt: **FEATURES jsou strop** (kapacita within-game strukturu nezachrání)

**Datum:** 2026-07-15 · **Autor:** Fable 5 (offline agent)
**Otázka:** Je REFUTED drive-target výsledek (viz `fable_drive_target_prefilter_20260715.md`) strop **featur**, nebo jen strop **lineární kapacity**? Tj. dokázal by nelineární model na TÝCHŽ 73 featurách zachytit within-episode strukturu drive-labelu?
**Nástroj:** `diag_capacity_probe.py` (ponechán v repu, seed jako volitelný argv). Data: tentýž `replay_buffer.pkl` (10 000 transitions, 625 epizod). Žádný nový sběr dat, žádný C++, nic v produkci.

## Setup

- **Target T** = drive-level target v bodě **λ=0.5, D=0.6, d0=0.1** — jediný grid-point včerejšího testu, který prošel divergencí (mean|ΔV|=0.104>0.1) i MSE guardem (+8.5%<10 %) a padl jen na ep-std V, tj. přesně na testované ose.
- **Split po epizodách** (80/20, žádný leak transitions přes split), metriky jen na held-out epizodách. Dva seedy (20260715, 7) — vzor identický.
- **Modely na identických standardizovaných 73 featurách:** RidgeCV (lineární), HistGradientBoosting (500 iter cap, early-stop; skutečná kapacita — train R²≈0.90), MLP 128-64.
- Klíčové metriky: held-out MSE/R² vs T; **ep-std predikcí**; **within-ep R²/corr** (pred i label centrované per epizoda — přímý test within-game struktury); **between-ep R²** (R² na epizodních průměrech — atribuce zisků).

## Výsledky (seed 20260715; seed 7 v závorce, kde se liší podstatně)

| model (target T) | R² test | ep-std pred | within-ep R² (corr) | between-ep R² |
|---|---|---|---|---|
| label T sám | — | **0.117** | — | — |
| linear RidgeCV | +0.561 (0.523) | 0.179 | −2.26 (r −0.09) | +0.703 |
| GBT | **+0.645** (0.619) | 0.198 | −2.05 (r **+0.16/+0.19**) | **+0.780** |
| MLP 128-64 | +0.563 (0.538) | **0.239** | −3.79 (r −0.05) | +0.781 |

Sanity: 0 % duplicitních sousedních feature-vektorů, unique-rows/ep = 1.000 — stavy nejsou *literálně* totožné body; jejich rozdíly ale nenesou predikovatelnou informaci o drive-výsledku.

## Čtení

1. **GBT reálně zlepšuje held-out R² o +0.08–0.10** proti lineáru — ale dekompozice ukazuje, že **celý zisk je between-episode** (0.70→0.78 na epizodních průměrech): nelineární interakce featur lépe kalibrují, *která epizoda jak dopadne*. To je reálná, ale malá rezerva kapacity — a **není to ta informace, o kterou jde**.
2. **Within-episode struktura zůstává nezachycena každou třídou modelů**: within-ep R² hluboce záporné u všech (lineár −2.3, GBT −2.1, MLP −3.8); nejlepší within-ep korelace (GBT r≈+0.16–0.19) vysvětluje ~3 % within-ep variance labelu. Label přitom nese ep-std 0.117 — ~4× G. Přesně replikuje včerejší nález, teď i pro vysokokapacitní třídu.
3. **Confound z §zadání potvrzen na MLP**: nejvyšší ep-std predikcí (0.239, blízko „hezkého" čísla) při **nulovém** zisku held-out R² (+0.002) a nejhorším within-ep R² (−3.8) = čistá inflace variance šumem, ne signál. Ep-std predikcí bez R²/within-ep opory se nesmí číst jako úspěch — platí i zpětně pro interpretaci jakýchkoli budoucích běhů.
4. Poznámka ke srovnatelnosti: absolutní ep-std čísla zde (konvergovaný fit, held-out) nesedí 1:1 na včerejší SGD-3-pass in-sample čísla (0.210–0.228 vs ref 0.234) — bridge check (in-sample ridge ep-std 0.179) potvrzuje, že rozdíl je footing, ne rozpor. Verdikt stojí na lineár-vs-nelineár **gapu na identickém footingu**.

## Verdikt

**Strop je v reprezentaci (73 featur), ne v kapacitě modelu.** Nelineární model s ~volnou kapacitou (train R² 0.90) na týchž featurách nezachytí within-episode/drive strukturu o nic víc než lineár; jeho jediný zisk je malé between-episode dokalibrování.

**Doporučení:**
- **Neinvestovat teď do neural value head** — dokud se nerozšíří featury (kanál a, per-player), větší model nemá co separovat; navíc by hrozila přesně MLP-pastička z bodu 3 (vypadající-jako-signál variance).
- **Kanál (a) per-player featury zůstává prioritou #1** — třetí nezávislý doklad téže věci (post-mortem V-std vs G-std; drive-target REFUTED; tento probe).
- Až featury porostou, tento skript je hotový levný re-test: pokud with-new-features within-ep R² lineáru zůstane záporné, ale GBT ne, *pak* je čas na nelineární hlavu. Between-ep zisk GBT (+0.08 R²) si zapamatovat jako malou, reálnou, ale nyní nerozhodující rezervu.

*Skript: `diag_capacity_probe.py` · sklearn 1.9.0 doinstalováno do venv (jen diagnostická závislost, repo nedotčeno) · nic necommitováno.*
