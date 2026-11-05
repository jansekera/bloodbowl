# P248 — měření ve hře 5.11.2026 (74 situací)

`situace.py` (jen náš tah + odpověď soupeře, párově, K = 3): lesní elfové (lovec Wardancer AG 4 + Strip Ball +
Wrestle) × orkové / lidé / skaveni / trpaslíci, 3 hry × útok/obrana; situace = náš tah, kde se plán s P248 a bez něj
liší. **74 situací** (51 v obraně). Kód: main 64e8161b (P249 zapnuté v obou ramenech). Rozdíl = zap − vyp.

| ukazatel | vyp | zap − vyp | |
|---|---|---|---|
| lovec sražen v našem tahu | 0,063 | −0,027 ± 0,020 | −1,3σ |
| lovec sražen v tahu soupeře | 0,054 | −0,018 ± 0,013 | −1,4σ |
| soupeřů jsme srazili (cena) | 0,995 | −0,009 ± 0,051 | −0,2σ |
| náš turnover | 0,207 | −0,014 ± 0,023 | −0,6σ |
| soupeř ztratil míč | 0,014 | +0,005 | +0,6σ |
| TD soupeře | 0,059 | −0,018 ± 0,013 | −1,4σ |

Pozitivní kontrola: čítač P248 v rameni „zap“ [3871, 3474, 5326], ve „vyp“ nula.
**Čtení:** všechny směry pro P248 (lovec méně na zemi, bez ceny na sražených soupeřích), ale malý vzorek — nic
přes 1,5σ.

## Kalibrace meze 0,15 (uživatel: „čísla počítej ze situací“)

Hrozba = `blitzThreat` na lovce po našem tahu (nejlepší rána soupeře, čtená); „sražen“ = lovec na konci tahu
soupeře nestojí. Obě ramena dohromady, 416 bodů.

| vypočtená hrozba | bodů | skutečně sražen |
|---|---|---|
| 0 | 59 | 0 % |
| (0; 0,15) | 48 | 0 % |
| [0,15; 0,30) | 49 | **0 %** |
| [0,30; 0,45) | 137 | 1,5 % |
| [0,45; 0,60) | 122 | 14 % |
| ≥ 0,60 | 1 | (1/1) |

Průměr vypočtené hrozby 0,31, skutečně sražen 4,8 %. **Hrozba je horní mez** (nejhorší rána, kterou soupeř
MŮŽE zahrát), ne pravděpodobnost — soupeřova AI lovce většinou netrefí, protože hraje jinam.
Pod 0,30 nesražen ani jednou (0 ze 156, horní 95% mez ≈ 2 %).
⇒ Návrh uživateli: mez **0,15 → 0,30**. ⚠️ „Skutečně sražen“ závisí na tom, jak hraje soupeř (tady naše AI);
člověk může lovce lovit cíleně. Rozhoduje uživatel.
