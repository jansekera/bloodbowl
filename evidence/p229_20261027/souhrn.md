# P228 + P229 — měření 27. 10. 2026 (worktree `p228-spinavy-roh`, stav 5b9409a5)

48 poločasů útoku (8 dvojic × 6: trpaslík, člověk, ork, skaven proti orkům, lidem, trpaslíkům a
lesním elfům), soupeř kope. Každý náš tah s míčem třikrát ze stejného stavu a semínka:
**vše** (P228 + P229), **bez P229**, **bez obou** (= main). Po každém rameni jen odpověď soupeře.
297 tahů s míčem, 0 chyb. Tabulka `souhrn_tabulka.txt`, data `mereni.pkl`, skript `mereni.py`.

## P228 — špinavý roh v hrozbě rány na nosiče (vše × bez obou, na 100 tahů)

| | s P228 | bez | rozdíl ± SE |
|---|---:|---:|---:|
| rohy sražené v odpovědi soupeře | 13,8 | 25,9 | −12,1 ± 2,6 |
| naši sražení celkem | 82,8 | 92,9 | −10,1 ± 3,8 |
| turnover v našem tahu | 15,2 | 18,5 | −3,4 ± 1,3 |
| postup nosiče (pole) | 209 | 260 | −50 ± 11 |
| nosič sražen / míč ztracen | 3,4 / 3,7 | 3,4 / 3,7 | 0 ± 0,7 |
| TD | 7,1 | 6,7 | +0,3 ± 0,6 |
| čas CPU na tah | 12,2 s | 8,9 s | +37 % |

Dělá, co má: soupeř srazí o polovinu méně rohů, méně turnoverů. Cena je pomalejší postup
(nejvíc skaveni −113 a lidé −79 polí na 100 tahů) a o 37 % víc času. Ztráty míče ani TD se nemění.

## P229 — prolomení zdi (vše × bez P229)

**Zasáhlo jen ve 2 tazích z 297** (pozitivní kontrola: v obou ladicí výpis `P229 zeď před klecí`,
jinak jsou ramena shodná). Oba tahy stály postup:
- trpaslík × lesní elf (hra 3, tah 6): prolom 1, postup 1 místo 3;
- ork × člověk (hra 2, tah 12): černý ork ST 4 blokoval člověka ST 3 **na jednu kostku bez Block**
  (křídlo vedle zdi asistovat nemohlo). Rameno bez P229 skórovalo — ale to byl volný tah hledání
  v předposledním kole s míčem v bezpečí (hrozba 0,05) a TD přes GFI; podle zdržování (P175/P180)
  se tu TD hrát nemá, vadou je jen blok na jednu kostku.

## Rozhodnutí uživatele po měření (27. 10.)

1. „block na 1 kostku bez block se nemá dít“ — rána při prolamování jen na 2+ kostky, nebo na
   jednu s Block (blok rohu i blitz).
2. „bezpečný krok vpřed má přednost před prolomením — ale záleží o kolik polí“ — bezpečný plán klece
   o 2+ pole (`kWallBreakSafeStep`, návrh, čeká na potvrzení) prolomení přebíjí.
3. „záleží kolikátý byl tah — zdržování před TD s GFI, pokud je bezpečné“ — potvrzuje, že TD v tahu
   ork × člověk správně nebylo.

Měření běželo ještě bez bodů 1 a 2.
