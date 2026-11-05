# P249 — měření ve hře 4.–5.11.2026 (noc, 210 situací)

`situace.py` (jen náš tah + odpověď soupeře, párově po situacích, K = 2): obrana lidé / orkové / trpaslíci × útok
orkové / lidé / skaveni, 4 hry × útok/obrana. Situace = náš tah v obraně, míč drží soupeř a na nosiče někdo z našich
dosáhne (≤ MA + 2) — **bez** výběru podle rozdílu plánů (c a d působí až při vyhodnocení bloku). **210 situací**
(lidé 78, orkové 71, trpaslíci 61). Rozdíl = vše − rameno bez dané části (± chyba průměru, v závorce σ).

| ukazatel | vše | celé P249 (vše − bez) | B4 | b (blok 1 kostka / 2 proti) | c (přehoz) | d (odsun) |
|---|---|---|---|---|---|---|
| blitz na nosiče | 0,148 | **+0,081 ± 0,019 (+4,4)** | **+0,083 (+4,6)** | −0,002 | −0,012 (−1,3) | +0,005 |
| blok na nosiče | 0,062 | +0,010 (+0,8) | +0,010 (+1,4) | +0,017 (+1,5) | −0,002 | +0,005 |
| soupeř ztratil míč **v našem tahu** | 0,093 | **+0,029 ± 0,012 (+2,5)** | +0,017 (+1,8) | +0,002 | +0,010 (+1,0) | +0,005 |
| soupeř ztratil míč celkem (náš + jeho tah) | 0,131 | **+0,000 ± 0,013 (0,0)** | +0,005 | 0,000 | −0,010 (−0,9) | −0,002 |
| nosič postoupil v jeho tahu (sloupce) | 0,910 | +0,043 (+0,6) | +0,019 | +0,026 (+1,2) | −0,062 (−0,9) | +0,033 (+0,9) |
| TD soupeře v jeho tahu | 0,057 | −0,002 (−0,4) | −0,007 (−1,0) | 0,000 | +0,005 (+1,4) | −0,007 (−1,0) |
| naši sražení v jeho tahu | 0,960 | −0,045 (−1,7) | −0,026 (−1,1) | −0,026 (−1,3) | **−0,045 (−2,0)** | +0,014 |
| náš turnover | 0,179 | **+0,043 ± 0,013 (+3,3)** | **+0,038 (+3,1)** | +0,012 (+1,9) | +0,017 (+1,3) | −0,014 (−1,3) |

**Pozitivní kontrola:** čítač každé části je v rameni bez ní nulový a jinde zabírá (B4 [2196, 16598, 1848] ·
b/c/d [862, 3171, 1080] v rameni „vše“; čítače zahrnují i simulace MCTS).

**Čtení.**
- **Mechanismus B4 funguje:** blitz na nosiče +8,3 bodu (+4,6σ) a soupeř ztratí míč **v našem tahu** o 2,9 bodu
  častěji (+2,5σ).
- ⚠️ **Celkem ale soupeř míč neztrácí častěji (0,0):** bez P249 ho stejně často ztratí **sám ve svém tahu**
  (vše: náš tah 0,093 + jeho 0,038; bez: 0,064 + 0,067). Míč, který vyrazíme dřív, by v polovině případů pustil sám.
- **Cena: náš turnover +4,3 bodu (+3,3σ)**, skoro celý z B4 (+3,8). TD soupeře beze změny (báze 5,7 %).
- **b** (blok na nosiče na 1 kostku / 2 proti): bloku na nosiče +1,7 bodu, turnover +1,2 (+1,9σ), jinak nic.
- **c** (přehoz): naši sražení v tahu soupeře −0,045 (−2,0σ) — příčina neověřená; jinak nic jistého.
- **d** (odsun k dalšímu blokujícímu): nic měřitelného (zasahuje, ale vzácně rozhoduje).
- Po rasách (celé P249): lidé míč −3,8 bodu, trpaslíci +4,1, orkové +0,7 — malé vzorky, opačné směry.
⇒ Návrh uživateli 5.11.; rozhoduje uživatel.
