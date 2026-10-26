# P220 — měření 21. 10. 2026 (maska 128 zapnuto × vypnuto)

Pravidlo: agilní hráč (AG4+, nebo AG3 + Dodge) nekončí **přesun** v tackle zóně soupeře. Výjimky: (a) markuje nosiče
soupeře, (b) je asistencí bloku nebo blitzu, který v tomto tahu ještě může přijít, (c) blokuje/blitzuje sám.
Kód je v `nextWalkStep` (macro_actions.cpp), maska `kP220AgileNoContact` = 128.

Sada je stejná jako v diagnostice P220: 72 poločasů, elfové + skaveni × trpaslík/ork/člověk, útok i obrana,
stejná semínka. Každý náš tah se hraje dvakrát ze stejného stavu a se stejným semínkem a po každém se odehraje
jedna odpověď soupeře. Dál se nehraje. Výsledek: 466 tahů, 0 chyb. Skript je `mereni.py`, vyhodnocení
`vyhodnot.py mereni.pkl`, celá tabulka v `souhrn_tabulka.txt`. Rozdíl = zapnuto − vypnuto, ± = SE párově, σ = σ párových rozdílů.

## Hlavní čísla (na 100 našich tahů)

| | elfové zap. | vyp. | rozdíl ± SE (σ) | skaveni zap. | vyp. | rozdíl ± SE (σ) |
|---|---:|---:|---:|---:|---:|---:|
| agilní „jen došel“ (kategorie diag.py) | 41,5 | 45,0 | −3,5 ± 2,4 (36,1) | 18,1 | 21,9 | −3,8 ± 1,5 (23,2) |
| — z toho bez pohybu (vstal na místě / předal, přihrál) | 34,1 | 31,4 | +2,6 ± 1,5 (22,8) | 12,2 | 12,2 | 0,0 ± 0,6 (9,2) |
| — **s pohybem** (= dopočet) | **7,4** | **13,6** | ≈ −6,1 | **5,9** | **9,7** | ≈ −3,8 |
| — z toho vedle našeho nosiče (roh klece) | 5,7 | 7,9 | −2,2 ± 1,3 (19,7) | 2,1 | 4,2 | −2,1 ± 0,9 (14,4) |
| „jen došel“ sražen v odpovědi | 12,2 | 14,4 | −2,2 ± 1,1 (17,4) | 5,5 | 7,6 | −2,1 ± 0,9 (14,4) |
| agilní v kontaktu sraženi v odpovědi | 53,7 | 55,0 | −1,3 ± 2,1 (31,7) | 13,9 | 16,5 | −2,5 ± 1,0 (15,7) |
| naši v kontaktu celkem | 206,6 | 209,2 | −2,6 ± 2,9 (43,9) | 249,8 | 253,6 | −3,8 ± 2,3 (34,8) |
| naši sraženi v odpovědi (všichni) | 75,5 | 76,9 | −1,3 ± 2,3 (34,4) | 88,2 | 89,0 | −0,8 ± 1,0 (15,9) |
| turnover našeho tahu | 17,9 | 17,5 | +0,4 ± 0,4 (6,6) | 24,9 | 25,3 | −0,4 ± 0,4 (6,5) |
| soupeři sraženi naším tahem | 78,6 | 79,5 | −0,9 ± 1,2 (18,7) | 72,2 | 73,8 | −1,7 ± 0,8 (12,9) |
| výjimka nosič | 9,2 | 8,7 | +0,4 ± 0,4 | 8,4 | 8,4 | 0 |
| výjimka asistence | 11,4 | 11,8 | −0,4 ± 1,0 | 3,0 | 3,4 | −0,4 ± 0,4 |
| výjimka blok (vlastní blok/blitz) | 74,2 | 75,1 | −0,9 ± 1,2 | 12,2 | 12,7 | −0,4 ± 0,4 |
| blitzy | 83,4 | 83,8 | −0,4 ± 0,4 | 86,1 | 86,1 | 0 |
| blitz → sražen / posun | 27,5 / 22,3 | 27,9 / 22,7 | −0,4 / −0,4 | 28,7 / 28,3 | 29,1 / 28,3 | −0,4 / 0 |
| blitz → padl blitzující / bez účinku / oba leží | 11,4 / 17,5 / 4,8 | 11,4 / 17,0 / 4,8 | 0 / +0,4 / 0 | 9,3 / 12,7 / 2,1 | 9,3 / 12,2 / 2,5 | 0 / +0,4 / −0,4 |

## Co z toho plyne

1. **Pravidlo zabírá na to, na co míří.** Agilní hráč, který se pohnul a skončil v kontaktu bez výjimky: elfové
   13,6 → 7,4, skaveni 9,7 → 5,9 na 100 tahů (zhruba −45 %). V odpovědi soupeře je sražených „jen došel“ o 2,1–2,2
   na 100 tahů méně. U skavenů klesá i počet agilních hráčů v kontaktu sražených v odpovědi (−2,5 ± 1,0).
2. **Výjimky fungují dál.** Nosič, asistence, vlastní blok i blitzy se nezměnily (rozdíly do 1 SE). Turnovery
   se nemění.
3. **Většina toho, co diag.py vede jako „jen došel“, je bez pohybu:** hráč v kontaktu vstal na místě, nebo předal
   či přihrál, a nepohnul se. Elfové 34 ze 42, skaveni 12 z 18. Tohle pravidlo P220 neřeší (není to pohyb do
   kontaktu) a diag.py to počítá do porušení. Měřítko „jen došel“ z 20. 10. (elfové 48, skaveni 23) tedy
   případy pohybu do kontaktu výrazně nadsazuje.
4. Zbytek s pohybem (5,9–7,4 na 100 tahů) pravidlo nepokrývá. Pravděpodobný původ (neověřeno): chůze s míčem
   (zvednutí, ADVANCE, SCORE, předávka) nebo soupeř přitlačený blokem k našemu hráči. Část (2–6) stojí vedle
   našeho nosiče a vypadá jako roh klece.
5. Soupeři sražení naším tahem: −0,9 / −1,7 na 100 tahů (u skavenů −1,7 ± 0,8, tedy na hranici 2 SE).
   Možná ubylo asistencí, které pravidlo už nepustí. Neověřeno.
