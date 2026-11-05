# P249e/f — měření ve hře 5.11.2026 (169 situací)

`situace.py` (jen náš tah + odpověď soupeře, párově, K = 2): **náš tým skaveni (61) / lesní elfové (66) / lidé (42)**
× orkové / lidé / trpaslíci, 3 hry × útok/obrana. Situace = náš tah v obraně, soupeř drží míč, na nosiče někdo
dosáhne. Kód main db8999f3. Rozdíl = vše − rameno bez části (± chyba průměru, σ).

| ukazatel | vše | bez e (pořadí, záloha) | bez f (strop úhybu) |
|---|---|---|---|
| blitz na nosiče | 0,716 | +0,009 (+1,1) | **+0,533 ± 0,036 (+14,9)** |
| soupeř ztratil míč v našem tahu | 0,130 | −0,009 | **+0,062 ± 0,015 (+4,2)** |
| míč u nás po našem tahu | 0,050 | 0,000 | +0,012 (+1,6) |
| míč u nás po tahu soupeře | 0,018 | −0,003 | +0,006 (+1,4) |
| TD soupeře | 0,115 | −0,003 | −0,003 (−0,2) · elfové **−0,045 (−1,8)** · lidé **+0,048 (+2,1)** |
| náš turnover | 0,580 | +0,012 | **+0,284 ± 0,034 (+8,3)** |

Pozitivní kontrola: čítač e [3] = 196 v „vše“, 0 v „bez_e“; čítač f [4] = 71 134 v „vše“, 0 v „bez_f“.
Pořadí (rameno vše): před blitzem na nosiče průměrně **11,9 maker**, po něm **1,8** (blitz už přichází pozdě).

**Kdy AI pokus zahraje** (rameno vše; šance a riziko nejlepšího blitzu na nosiče, bez stropu):

| šance srazit / vzít míč | pokusů | zahráno |
|---|---|---|
| < 0,1 | 236 | 72 % |
| 0,1–0,3 | 66 | 73 % |
| 0,3–0,5 | 14 | 64 % |
| ≥ 0,5 | 20 | 95 % |

| riziko neúspěchu | bodů | blitz s P249f | bez P249f |
|---|---|---|---|
| < 1/6 | 8 | 62 % | 62 % |
| 1/6–1/3 | 32 | 78 % | 72 % |
| 1/3–1/2 | 4 | 75 % | 50 % |
| **≥ 1/2** | **292** | **72 %** | **11 %** |

Se šancí ≥ 0,3 AI nezkusila nic jen v **1 situaci** (lesní elfové × lidé, hra 0, tah 7; riziko 0,11, šance 0,31).

**Čtení.**
- **P249f dělá, co uživatel chtěl:** skoro všechny pokusy o nosiče mají riziko nad 1/2 (292 z 336) a dřív je strop
  zahodil (11 %); teď 72 %. Soupeř ztratí míč v našem tahu o 6,2 bodu častěji (+4,2σ).
- **Turnover +28 bodů** — cena, kterou uživatel bere („člověk to zkusí“). Blitz přichází po přípravě (11,9 maker před,
  1,8 po), takže turnover už většinu tahu nestojí. TD soupeře celkem beze změny; elfové −4,5, lidé +4,8 (±2σ, malé vzorky).
- **P249e** mění málo: MCTS blitz na nosiče sám nechává na konec, odklad zasáhl jen 196× v hledání.
- ⚠️ **Slabé místo je A18 — zvednout a udržet:** míč vyrazíme ve 13 % situací, ale **po našem tahu ho máme jen v 5 %**
  a **po tahu soupeře v 1,8 %**. Shodit umíme; zvednout a ochránit ne. ⇒ další krok: rozebrat situace „míč na zemi
  po blitzu“ — kdo z nás k němu dosáhl, proč nezvedl, kdo ho zvedl po nás.
