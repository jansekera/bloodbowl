# P302 krok 3 — přistoupit k soupeřovým zvedačům volného míče (12. 11. 2026)

Změna (`kP302MarkPickers`, bit 37): když míč nedosáhneme, přesune obestoupení hráče vedle soupeřova možného zvedače
(naše zóna ⇒ úhyb cestou k míči). Měření `situace.py … 20 1`, `RAMENA=vse,bez_mark` (bez_mark = jen krok 2),
**140 situací** (skaveni 58, lesní elfové 41, lidé 41), K = 1, běh 06:33–10:36. Rozdíl = vse − bez_mark (± chyba průměru).

| ukazatel | s krokem 3 | bez | rozdíl |
|---|---|---|---|
| **soupeř má míč (šance)** | 0,150 | 0,152 | **−0,002 ± 0,010 (−0,2σ)** |
| míč volný po našem tahu | 0,321 | 0,314 | +0,007 (+0,6σ) |
| míč u nás po našem tahu | 0,664 | 0,664 | 0 |
| míč u nás po tahu soupeře | 0,557 | 0,550 | +0,007 (+0,4σ) |
| míč u soupeře po našem tahu | 0,014 | 0,021 | −0,007 (−1,0σ) |
| turnover | 0,464 | 0,479 | −0,014 (−1,4σ) |
| TD soupeře | 0 | 0 | 0 |

Podle týmu (`souper_mic`, s / bez): skaveni 0,164 / 0,173 · elfové 0,085 / 0,109 · **lidé 0,195 / 0,164 (horší)**.
Pozitivní kontrola: čítač „přesun ke zvedači“ **887×** (v 97 ze 140 situací), bez kroku 3 **0×**; jiný výsledek tahu ve 120 situacích
⇒ mechanismus se zapíná a mění tahy.

**Čtení.** Na to, co má krok 3 dělat (snížit šanci soupeře na míč), **účinek nenaměřen** (−0,002 při chybě 0,010).
Ostatní ukazatele bez směru nad 1,5σ; u lidí opačný směr. Krok 3 je zapnutý a mění tahy, ale soupeřovo zvednutí nesnižuje.
⇒ **Nepřijato** (12. 11., uživatel „pokračuj bb“ po návrhu nepřijmout): složitost bez naměřeného přínosu. Kód kroku 3 je
zachovaný ve větvi **`p302-krok3-zamitnuto`** (commit 9a6e1e08, na GitHubu); main zůstává na kroku 2.
