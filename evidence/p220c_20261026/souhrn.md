# P220 — měření znovu 26.–27. 10. 2026 (po sloučení `035f5af1`)

Stejná sada jako `evidence/p220b_20261021`: 72 poločasů, elfové + skaveni × trpaslík/ork/člověk,
útok i obrana, stejná semínka, každý náš tah dvakrát ze stejného stavu (maska 0 × maska 128) a po něm
jen odpověď soupeře. 472 tahů, 0 chyb. Tabulka: `souhrn_tabulka.txt`, data `mereni.pkl`.

## Pozitivní kontrola (27. 10.)

`kontrola_zastaveni.py` (24 poločasů z téže sady, jen rameno s P220, čítač `take_p220_stops` = jen
skutečně hrané tahy): **elfové 0 zastavení v 84 tazích, skaveni 2 v 72.** Ramena se v celém měření
liší jen v 18 tazích ze 472 (elfové 15, skaveni 3).

## Výsledek (na 100 našich tahů, zapnuto × vypnuto)

| | elfové | skaveni |
|---|---:|---:|
| agilní **pohybem** do kontaktu bez výjimky | 18,7 × 20,3 | 10,6 × 11,1 |
| — z toho vedle našeho nosiče (roh klece) | 12,6 × 12,2 | 5,8 × 5,8 |
| naši v kontaktu celkem | 217,9 × 220,7 (−2,8 ± 1,8) | 224,3 × 224,8 |
| turnover, blitzy, sražení soupeři | beze změny | beze změny |

## Co z toho plyne

1. **P220 po sloučení skoro nezasahuje.** Dřívější −45 % (p220b) bylo zastavování přesunů klece; ty
   P220 od review 26. 10. nesoudí (`cageManaged`), protože zastavení zahazovalo celý plán klece.
2. **Agilní hráči dál chodí do kontaktu — většinou jako rohy klece** (elfové 12,6 z 18,7). To je
   **v rozporu s rozhodnutím uživatele 21. 10.: „roh vlastní klece NENÍ výjimka“ („3 ne“).** Výjimka
   pro rohy vznikla při review 26. 10. jako technická oprava, ne rozhodnutím uživatele.
3. Náprava patří do plánovače klece (agilní hráč se nepřiřadí na roh v zóně soupeře, roh tam dostane
   jiný hráč nebo zůstane prázdný), ne do zastavení přesunu — to by plán dál rozbíjelo. Souvisí s P228
   (špinavý roh je teď v hrozbě rány na nosiče). ⏳ Čeká na rozhodnutí uživatele.
