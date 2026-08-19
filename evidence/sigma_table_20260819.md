# σ-TABULKA 19.08.2026 — nový korpus, dvě nové veličiny

Korpus `corpus_baseline_20260819_data`, **3 000 her**, engine `ea6f0a51`,
**5 031 plných drivů** (≥7 našich kol), z toho **835 se skórováním**.
Surový výstup `evidence/sigma_table_20260819.txt`.

| veličina | TD drivy | bez TD | σ | 18.08. | |
|---|---:|---:|---:|---:|---|
| K9a_splněno | 0,437 | 0,210 | **+20,8σ** | +20,7σ | ✅ |
| REACH0_počet | 1,390 | 2,119 | **−16,7σ** | −16,7σ | ✅ |
| Δx | 2,739 | 2,065 | **+14,4σ** | +13,9σ | ✅ |
| bloků/kolo | 1,715 | 1,524 | **+11,0σ** | +10,4σ | ✅ |
| K35_fb2 | 0,871 | 0,791 | **+10,6σ** | +11,6σ | ✅ |
| ⭐ **odpor_koridoru** | 0,889 | 1,176 | **−9,6σ** | — | 🆕 |
| K34_reach0 | 0,456 | 0,355 | **+9,2σ** | +9,3σ | ✅ |
| rohů_ŠPINAVÝCH | 0,124 | 0,173 | **−6,2σ** | −6,8σ | ✅ |
| K29_čisté | 0,820 | 0,770 | **+4,8σ** | +5,1σ | ✅ |
| K33_blok (ano/ne) | 0,761 | 0,774 | **−2,5σ** | −2,5σ | ✅ |
| rohů_všech | 1,261 | 1,302 | −1,5σ | −2,1σ | ✅ |
| rohů_ČISTÝCH | 1,136 | 1,129 | +0,3σ | −0,2σ | ✅ |
| ⭐ **PRAVIDLO_klece (K29⭐⭐)** | 0,022 | 0,022 | **−0,2σ** | — | 🆕 |

## Co je nového

### ⭐ `odpor_koridoru` **−9,6σ** — první měření veličiny, která existovala jen v enginu

K9b vytáhla `corridorResistance()` mimo bránu 18.08., ale **žádný korpus ji do
dneška nevezl**, takže se nikdy neptalo, jestli něco předpovídá. Předpovídá:
drivy se skórováním mají v koridoru ⌀ **0,89** odporu, drivy bez TD **1,18**.
Řadí se hned za pětici nejsilnějších a je to **jediná nová veličina za měsíc**,
která tam vstoupila zdola.
⚠️ Sentinel `-1` *(nosič mimo hru)* se **nepřičítá** — jinak by se „nepočítalo se"
vydávalo za nulový odpor.

### ⭐ `PRAVIDLO_klece` **−0,2σ** — replikuje Fableho nulu, a je to vyhladovělý metr

Fable naměřil 19.08. na **starém** korpusu 0,0σ; tady vychází **−0,2σ**
nezávisle. Není to verdikt o pravidle: **korpus ho hraje ve 2,2 % kol**
(0,022 v obou sloupcích), takže tabulka měří korpus, který pravidlo NEHRAJE.
⏰ **Přeměřit po noci P38** — teprve korpus, který pravidlo hraje, ho umí ocenit.

## Replikace

**Všech jedenáct veličin z 18.08. replikovalo**, žádná nezměnila znaménko ani
řád. Pravidlo z 18.08. *(replikuje vše s |σ| ≥ 3 a nic pod tím)* platí i tady:
jediné dvě veličiny pod 3σ — `rohů_všech` a `rohů_ČISTÝCH` — se opět hýbou
kolem nuly a `rohů_ČISTÝCH` **znovu obrátilo znaménko** (−0,2 → +0,3).

## ⛔ Vada aparátu opravená cestou

Hlavička tiskla **„korpus bez otisku — viz P22"** u korpusu, který otisk **má**:
hledala `ENGINE_HEAD` nad datovým adresářem, jenže otisk leží v **sourozenci
bez přípony `_data`**. Tedy přesně ta vada, kterou si ten skript sám vynucuje —
*původ čísla musí cestovat s číslem*. Opraveno před zápisem výsledku.
