# P302 krok 2 — obestoupit volný míč na pole, které soupeři zvednutí nejvíc ztíží (12. 11. 2026)

Změna (`kP302SurroundWorst`, bit 36): u obestoupení volného míče (P250b) se pole nevybírá nejbližší, ale s nejmenší
`opponentPickupChance` (hráč přesunutý na to pole); přesun se nabídne jen, když šanci soupeře proti dnešku sníží.
Měření `situace.py … 20 1`, `RAMENA=vse,bez_302`, **136 situací** (skaveni 53, lesní elfové 48, lidé 35), K = 1,
běh 02:13–06:00. Rozdíl = vše − bez_302 (± chyba průměru, σ).

| ukazatel | s krokem 2 | bez | rozdíl |
|---|---|---|---|
| **soupeř má míč (šance)** | 0,143 | 0,152 | **−0,009 ± 0,006 (−1,4σ)** |
| míč volný po našem tahu | 0,287 | 0,301 | −0,015 (−1,4σ) |
| **míč u nás po našem tahu** | 0,699 | 0,684 | **+0,015 (+1,4σ)** |
| míč u nás po tahu soupeře | 0,596 | 0,581 | +0,015 (+1,4σ) |
| míč u soupeře po našem tahu | 0,015 | 0,015 | 0 |
| turnover | 0,397 | 0,404 | −0,007 (−1,0σ) |
| TD soupeře | 0 | 0 | 0 |

Podle týmu (`souper_mic`): skaveni 0,152 / 0,162 · elfové 0,141 / 0,141 · lidé 0,132 / 0,151.
Pozitivní kontrola (čítače P302): s krokem 2 **280×** jiné pole než nejbližší, **81×** obestoupení nenabídnuto; bez 0 / 0.
Doba (`p302_cas.py`, tahy s volným míčem, skaveni × orkové, 2 opakování): s krokem 2 18,8 / 15,7 s, bez 16,0 / 13,8 s
⇒ ~15 % na tazích s volným míčem (šum opakování ~20 %); na celou hru zanedbatelné.

**Čtení.** Vše jde správným směrem (soupeř míč méně, my víc, turnover méně), jednotlivě pod 2σ; nic se nezhoršilo.
Krok 1 ukázal, že obestoupení na nejbližší pole škodí (+0,022); krok 2 to obrací (−0,009 proti nejbližšímu poli).
⇒ **ponechat.** Další krok (3): přistoupit k soupeřovým zvedačům (zóna ⇒ úhyb cestou k míči), změřit tímtéž.
