# P245 — měření ve hře 4.11.2026 (1. běh: obě části dohromady)

`situace.py` (jen náš tah + odpověď soupeře, párově po situacích, K = 4): obrana lidé / orkové / trpaslíci × útok
orkové / lidé / skaveni, 4 hry × útok/obrana. Situace = náš tah v obraně (míč drží soupeř), kde se plán s P245 a bez
něj liší. **133 situací.** Rozdíl = s P245 − bez (± chyba průměru).

| ukazatel | bez P245 | rozdíl | |
|---|---|---|---|
| šance nosiče na TD v jeho tahu (výhled) | 0,014 | −0,002 ± 0,001 | −1,3σ |
| nosič od zóny po cestě s rizikem ≤ 1/3 (sloupce) | 10,26 | **+0,34 ± 0,22** | +1,6σ (trpaslíci **+0,89, +3,1σ**) |
| nosič postoupil v tahu soupeře (sloupce) | 0,61 | **−0,25 ± 0,12** | −2,1σ |
| TD soupeře v jeho tahu | 0,030 | +0,000 ± 0,005 | 0 |
| blitz na nosiče v našem tahu | 0,120 | **−0,073 ± 0,019** | −3,9σ |
| soupeř ztratil míč (náš tah + jeho) | 0,113 | **−0,024 ± 0,012** | −2,0σ ⚠️ |
| náš turnover | 0,158 | +0,008 ± 0,011 | +0,7σ (lidé +0,038, +1,9σ) |

**Čtení.** Zavazení dělá, co má: nosič v tahu soupeře postoupí méně (−0,25 sloupce) a jeho bezpečný doběh je dál
od zóny. Brána na blitz nosiče dělá, co má (méně blitzů na nosiče), ale **cena: soupeř o 2,4 bodu méně často ztratí
míč** — a přínos, kvůli kterému brána je (méně turnoverů, méně našich sražených), v tomhle běhu vidět není (turnover
beze změny; sražení našich se neměřilo). TD soupeře beze změny (báze 3 %, malé n).
⇒ **2. běh** (spuštěn 4.11.): obě části zvlášť (vše / bez brány / bez zavazení / bez obou), navíc naši sražení v tahu
soupeře; P248 ve všech ramenech vypnuto.

## 2. běh 4.11.2026: obě části zvlášť (133 situací, tytéž; P248 vypnuto)

Ramena: **vše** · **bez brány** (= jen zavazení) · **bez zavazení** (= jen brána) · **bez** (nic). Párově, ± chyba průměru.
Rameno „vše − bez“ vyšlo **přesně jako v 1. běhu** (stejná semínka ⇒ měřidlo je opakovatelné).

| ukazatel | bez | jen zavazení − bez | jen brána − bez | brána navíc k zavazení (vše − bez brány) |
|---|---|---|---|---|
| nosič od zóny, bezpečný doběh (sloupce) | 10,26 | **+0,78 ± 0,13 (+6,1σ)** | −0,42 ± 0,18 (−2,4σ) | **−0,44 ± 0,18 (−2,5σ)** |
| nosič postoupil v tahu soupeře | 0,61 | **−0,29 ± 0,12 (−2,4σ)** | −0,06 ± 0,05 | +0,06 ± 0,07 |
| TD soupeře v jeho tahu | 0,032 | −0,008 ± 0,004 (−2,0σ) | +0,004 ± 0,005 | +0,008 ± 0,005 (+1,6σ) |
| blitz na nosiče v našem tahu | 0,120 | +0,009 ± 0,006 | **−0,073 ± 0,019** | **−0,083 ± 0,020 (−4,2σ)** |
| soupeř ztratil míč | 0,113 | −0,002 ± 0,008 | −0,017 ± 0,010 (−1,6σ) | **−0,023 ± 0,010 (−2,3σ)** |
| náš turnover | 0,158 | +0,019 ± 0,012 (+1,6σ; lidé +0,038) | +0,008 ± 0,008 | −0,011 ± 0,009 (−1,3σ) |
| naši sražení v tahu soupeře | 0,915 | −0,015 ± 0,026 | −0,023 ± 0,020 | −0,028 ± 0,021 (−1,3σ) |

**Čtení.**
- **Zavazení funguje samo:** bezpečný doběh nosiče +0,78 sloupce (+6σ), postup v jeho tahu −0,29, TD soupeře −0,8 bodu.
  Cena: turnover +1,9 bodu (+1,6σ), u lidí +3,8.
- **Brána ubírá:** blitz na nosiče −8 bodů, a s ním **soupeř méně často ztratí míč (−2,3 bodu, −2,3σ)** a nosič má
  **bezpečný doběh o 0,44 sloupce blíž** (blitz nosiče i bez sražení zdržuje). Přínos, kvůli kterému brána je:
  naši sražení −0,03 hráče (−1,3σ), turnover −1,1 bodu (−1,3σ) — oboje malé a nejisté.
- ⇒ Návrh uživateli 4.11.: **zavazení nechat, bránu vypnout** (nebo zmírnit). Rozhoduje uživatel.
