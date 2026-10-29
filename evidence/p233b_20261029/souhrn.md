# P233 v běžných hrách po zúžení řetězu (29. 10. 2026)

Knihovna: zmrazená kopie commitu 43e08d71 (řetězové odtlačení jen směrem od klece).
Hry: 9 dvojic (undead|nurgle|chaos × human|skaven|wood-elf), N=6, soupeř kope, 16 tahů.
Ramena ze stejného stavu a semínka: `vse` (0) a `bez_p233` (131072). Po rameni jen odpověď soupeře.

## Výsledek
- 391 našich tahů, z toho 275 s míčem.
- Řadič došel ke zdi před klecí (P229) v **1 tahu**. P229 tam zvolil „dostavba na místě“ (běžci soupeře nejsou rychlejší), takže P233 nebyl na řadě.
- **P233 na řadě: 0 tahů, 0 ran.** Ramena se neliší v žádném tahu; všechny ukazatele mají rozdíl 0.
- Čas na tah: +0,09 ± 0,51 s (šum).

## Kontroly
- Pozitivní kontrola stopy: stopa řadiče se zapisovala (15 024 řádků v 70 procesech), včetně jednoho řádku P229 (2 výskyty = obě ramena).
- Pozitivní kontrola P233 s novým řetězem: situace S1 (`kontrola/`) — P233 dál hraje blitz hráčem 7 na (14,9) a bloky hráči 4 a 5.
- Kouř N=1 (68 tahů): také 0 tahů u zdi.

## Závěr
Dosah P233 v náhodných hrách je prakticky nulový (dřív P229 2/297, P230 0/296, teď P229 1/391, P233 0/275).
Další měření v náhodných hrách nic nepřinese; chování P233 kryjí testy (8/8 mutací) a situace S1–S5.
Zbývá ruční partie s klecí u zdi.

Soubory: `mereni.py`, `vyhodnot.py`, `beh.sh`, `tabulka.txt`. Mimo git: `zmrazeno/`, `stopa/`, `*.pkl`, `beh.log`.
