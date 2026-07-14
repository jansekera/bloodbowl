# Post-mortem metriky `pre_td_ramp` + verdikt pro mc_td_mix Stage 2 (Fable 5, 2026-07-14)

**TL;DR: Metrika `pre_td_ramp` je z konstrukce slepá — měří selekci stavů podle
výsledku hry, ne within-game signál. A hůř: nový offline kontrolovaný A/B test
ukazuje, že mc_td_mix při α=0.7 (i při α=0.1) produkuje prakticky identickou
value hlavu jako čisté mc_return. Stage 2 jako validace mc_td_mix = STOP.**

Všechna čísla reprodukovatelná: `python3 diag_td_mix_target_diff.py` (offline,
bez enginu; čte `stage1_mc_td_mix_backup_20260713/replay_buffer.pkl` — 10 000
transitions = 624 epizod, + weight snapshoty). Kód metriky:
`python/blood_bowl/evaluate.py:14` (`pre_td_value_ramp`), targety:
`rewards.py:34/56`, `trainer.py:243/300`, `replay_buffer.py:81-118`.

## 0. Vstupní data (oba běhy, stejný init hlavy)

`run_iteration.py:383` dělá `copy(weights_best.json → weights_az_train.json)` —
**oba běhy startovaly ze STEJNÉ, už natrénované hlavy** (champion z 30.06.,
benchmark 91.5 %), ne od nuly. Očekávání „ramp půjde z ~0 do kladných hodnot"
bylo fakticky vadné už v zadání Stage 1: nikdo nezměřil ramp staré hlavy, a ta
už byla na **+0.56** (viz §2).

| běh | metoda | pre_td_ramp per-epoch | průměr |
|---|---|---|---|
| Stage 1 (13.07.) | mc_td_mix α=0.7 | +0.54 … +0.74 (16 epoch) | **+0.655** |
| Null (14.07.) | α=1.0 ≡ mc_return | +0.41 … +0.76 (15 epoch v logu) | **+0.662** |

Rozdíl průměrů −0.007 (null dokonce nominálně výš, po vyloučení warmup epochy 1
ještě víc). Žádný trend, žádná separace. Metrika nediskriminuje.

## 1. Proč je ramp kladný i pod čistým mc_return — rozhodnutí mezi hypotézami

`pre_td_value_ramp` = mean V(s) na stavech 1–3 tahy před vlastním TD minus
mean V(s) **na všech ostatních stavech všech her** (evaluate.py:46-50 — jeden
společný `other_vals` pool přes všechny hry i perspektivy). To je klíč.

### (a) „G_t s Lever-B rewardy sám rampuje" — PRAVDA, ale jinak, než se čekalo

Ramp samotného MC targetu (`mc_return` z bufferu) na identické selekci stavů:

```
mean G | pre-TD stavy              = +0.925
mean G | vše ostatní               = −0.239   → TARGET ramp = +1.164
  z toho: ostatní stavy TÝCHŽ epizod s vlastním TD = +0.854
          stavy epizod BEZ vlastního TD            = −0.725
  → within-episode složka (Lever-B bump γ^k·0.2)   = pouze +0.072
```

Target tedy rampuje **+1.16**, ale jen **+0.07** z toho je skutečný
within-game Lever-B příspěvek. Zbylých ~**+1.09 je outcome conditioning**:
pre-TD stavy z definice existují jen ve hrách, kde jsme skórovali, a tam
terminal_value (výhra +1.0 / remíza ≥ −0.35 / prohra ≥ −0.88) broadcastem
(γ^k, medián délky epizody 16 → γ^15 ≈ 0.86) zvedá VŠECHNY stavy epizody na
~+0.85, zatímco bezgólové hry (63 % stavů) sedí na ~−0.72 (remíza −0.5 /
prohra −1.0 broadcast). Čili ano — target rampuje i pod čistým mc_return,
ale primárně selekcí epizod, ne Lever-B.

### (b) „Hlava to umí z korelace featur" — PRAVDA

V-ramp na stejných stavech bufferu, stejná definice okna:

| hlava | uncontrolled ramp | pozn. |
|---|---|---|
| init `weights_best.json` (PŘED Stage 1) | **+0.558** | stará hlava, corr(V,G)=0.81 |
| Stage 1 final (α=0.7, backup az_train) | +0.711 | |
| Stage 1 best (backup train_best) | +0.686 | |
| null best (α=1.0, root train_best 08:08) | +0.682 | |

Hlava, která nikdy neviděla mc_td_mix, měla ramp +0.56. Cross-game kalibrace
(V ≈ atenuovaný G, korelace 0.81) stačí: hlava outputuje vyšší V na stavech,
jejichž featury korelují se skórováním/vítězstvím — přesně „paradox" z
root-cause analýzy 30.06. (zdravý output spread z cross-game statistiky).

### (c) „Metrika měří stavovou selekci, ne kvalitu targetu" — PRAVDA a rozhodující

Kontrolní výpočet: **outcome-controlled ramp** = pre-TD vs ostatní stavy
POUZE UVNITŘ epizod s vlastním TD:

```
raw MC target:  +0.072
init hlava:     −0.172     stage1 hlava: −0.055     null hlava: −0.088
```

Po odfiltrování selekce epizod je V-ramp u VŠECH hlav dokonce **záporný**
(částečně konfundováno post-TD stavy, kde skóre-lead featura zvedá V — ale
žádná hlava nevykazuje kýžený within-drive růst k TD). Kladných +0.6–0.7
publikované metriky je tedy ~100 % selekce výsledku hry a ~0 % within-game
signálu. **Verdikt: platí (c) jako operativní vysvětlení, s (a)+(b) jako
mechanismy, které selekci sytí.** Metrika by ukázala +0.6 každé hlavě, která
je aspoň hrubě kalibrovaná na outcome — tj. i „ploché" hlavě z root-cause
analýzy. Nikdy nemohla oddělit starý target od nového.

## 2. Co Stage-1 data dokazují a nedokazují

**Dokazují:** implementace mc_td_mix je funkční a stabilní (w_norm_Δ ±0.001,
grad_norm ~2.3 bez trendu, mean_abs_vf ~0.6, žádná inverze) a targety obou
metod se na identických transitions skutečně liší (viz §3.1) — mix není
implementační no-op.

**Nedokazují:** jakékoli zlepšení (ani zhoršení) value hlavy. pre_td_ramp
byla na verdikt nepoužitelná od začátku.

**Nový nález nad rámec „metrika je slepá":** kontrolovaný offline A/B (§3.2)
ukazuje, že mc_td_mix je při současných featurách/rewardech **téměř no-op i na
úrovni naučené funkce**, a to pro libovolné α. Problém tedy není jen slepá
metrika — páka sama je (v této podobě) mrtvá.

## 3. Diskriminující test (offline, `diag_td_mix_target_diff.py`)

### 3.1 Bit-level rozdíl targetů na identických transitions (10 000, backup buffer)

Δ_t = (1−α)·(r_t + γ·V(s′) − G_t), α=0.7, V(s′) ze stage1 hlavy (s init hlavou
prakticky totéž):

```
mean|Δ| = 0.089;  |Δ|>0.05 na 50 % transitions;  |Δ|>0.10 na 32 %
směr: pre-TD −0.072, ostatní stavy TD-epizod −0.078, bezgólové epizody +0.059
      (bootstrap reguluje broadcast extrémy k méně extrémnímu V)
within-episode std targetu: MC 0.028 → mix 0.079 (2.8×)
```

Target se tedy liší hodně a přesně tam, kde root-cause říkal (mix má ~3×
víc within-game struktury). ALE:

### 3.2 Kontrolovaný A/B retrain — targety se liší, hlavy NE

Stejný init (weights_best), stejné pořadí dat (fixní seed), 3 průchody
bufferem (~30 k updateů, srovnatelné s reálným během), lr=3e-4, jediný rozdíl α:

| α | mean&#124;V_α−V_1.0&#124; | corr(V_α,V_1.0) | pre-TD ramp | within-ep V std | MSE vs G |
|---|---|---|---|---|---|
| 1.0 | 0 (ref) | 1.0000 | +0.726 | 0.243 | 0.226 |
| 0.7 | 0.012 | 0.9998 | +0.719 | 0.242 | 0.225 |
| 0.5 | 0.022 | 0.9993 | +0.710 | 0.241 | 0.224 |
| 0.3 | 0.034 | 0.9984 | +0.698 | 0.240 | 0.223 |
| 0.1 | 0.048 | 0.9966 | +0.681 | 0.238 | 0.224 |

I při α=0.1 (90 % bootstrap!) je výsledná hlava korelovaná 0.997 s čistým
mc_return a mean|ΔV|=0.048 při mean|V|=0.62. Ramp s klesajícím α mírně KLESÁ,
within-ep V std se nemění. **Proč (strukturálně):** bootstrap člen r+γV(s′) je
sebereferenční — V je naučené ze stejného G na stejných featurách, takže
r+γV(s′) ≈ V(s) ≈ to, co už hlava predikuje → gradient mixu ≈ gradient MC.
Navíc within-episode V std hlavy (0.24) je ~9× větší než within-episode std MC
targetu (0.028): within-game strukturu hlavě dodávají FEATURY (generalizace
napříč stavy), ne per-state labely. Přemíchání estimátoru téhož rewardu
nepřidá žádnou novou informaci — informační bottleneck je reward/featury, ne
tvar targetu. (Caveat: A/B je replay-only, bez full-log větve, a startuje z
konvergované hlavy — ale přesně tak startuje i produkce, viz §0; shoda
makro-metrik obou reálných běhů je s tímto závěrem plně konzistentní.)

## 4. Verdikt a doporučení pro Stage 2

**STAGE 2 (vf_blend=0.15 + N=150 gate jako validace mc_td_mix): STOP.**
Treatment a control se na úrovni hlavy liší o mean|ΔV|=0.012 (corr 0.9998).
Při šumovém dně gate ±8–11 pp nemá takový rozdíl žádnou šanci na detekci —
běh by z konstrukce nemohl nic prokázat, jen spálit ~5 h + gate.

Doporučení:
1. **Odpojit vf_blend bring-up od mc_td_mix.** Zapnutí vf_blend=0.15 je
   nezávislý krok roadmapy (syntéza 07-09, krok 2 / AZ bring-up) a má se
   ověřit jako vlastní jednovariablový experiment se stávající mc_return
   hlavou — na volbě α nezáleží (hlavy jsou zaměnitelné).
2. **`diag_td_mix_target_diff.py` §E ponechat jako levný pre-filtr** pro
   jakýkoli budoucí value-target návrh: kandidátní target si musí NEJDŘÍV
   offline vydělat netriviální divergenci hlavy (např. mean|ΔV| > ~0.1 při
   nezhoršené MSE), než dostane trénovací běh. Ramp-metriku v této podobě
   nepoužívat na verdikty; pokud ji zachovat v logu, pak jedině
   outcome-controlled variantu (within-TD-episode kontrast, §1c), s vědomím
   post-TD konfoundu.
3. **Pokud má within-game value signál někdy vzniknout,** musí přijít z nového
   informačního kanálu, ne z re-mixu G: (i) bohatší featury (per-player plán),
   (ii) bootstrap z externího evaluátoru, který ví něco, co G neví (např.
   heuristický leaf eval enginu jako teacher), (iii) jiný reward. To je ale
   nová položka do fronty, ne úprava mc_td_mix.

Drobnost pod šumem (nezakládat na tom nic): stage1 hlava má nejméně záporný
outcome-controlled ramp (−0.055 vs −0.17 init / −0.088 null).
