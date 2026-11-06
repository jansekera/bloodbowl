# P250b — měření na volném míči 5./6. 11. 2026 (155 situací)

`situace.py … 20 1` (jen náš tah + odpověď soupeře, párově, **K = 1**): **skaveni (72) / lesní elfové (37) / lidé (46)**
× orkové / lidé / trpaslíci, 20 her × útok/obrana = 320 her. Situace = **na začátku našeho tahu leží míč volně** a náš
stojící hráč na něj dosáhne (≤ MA + 2); **bez tahu 0** (po výkopu kouř 5/5 zvednuto ve všech ramenech = strop).
Kód main 3e325fb3. Běh 21:14–01:20. Rozdíl = vše − rameno bez části (± chyba průměru, σ).

| ukazatel | vše | bez a (zvedač) | bez b (obestoupit) | bez c (odtlačit) | bez P250 |
|---|---|---|---|---|---|
| míč u nás po našem tahu | 0,671 | +0,026 (+0,9) | −0,013 (−1,0) | 0,000 | +0,013 (+0,4) |
| míč u nás po tahu soupeře | 0,497 | −0,026 (−0,8) | −0,013 (−0,8) | 0,000 | −0,026 (−0,8) |
| míč u soupeře po našem tahu | 0,019 | −0,019 (−1,7) | 0,000 | 0,000 | −0,006 |
| TD soupeře | 0,006 | 0,000 | 0,000 | 0,000 | 0,000 |
| náš turnover | 0,477 | −0,019 (−0,7) | 0,000 | −0,006 | **−0,039 (−1,3)** |

Po týmech (míč u nás po našem tahu, vše − bez a / vše − bez P250):
- **lesní elfové (37): +0,189 ± 0,065 (+2,9σ) / +0,135 ± 0,069 (+2,0σ)**; po tahu soupeře +0,081 (+1,1)
- skaveni (72): −0,014 / −0,028 (pod 1σ)
- lidé (46): −0,043 / −0,022; **po tahu soupeře −0,109 ± 0,064 (−1,7σ)** u a

**Pozitivní kontrola** (čítače `take_p250`, za 155 tahů ramene): vše [7 667 · 1 251 · 68], bez a [**0** · 1 766 · 129],
bez b [8 004 · **0** · 75], bez c [7 781 · 1 286 · **0**], bez [**0 · 0 · 0**] ⇒ vypínače i čítače fungují.

**Čtení.**
- **Nic se nerozbilo:** turnover s P250 spíš méně (−3,9 b., −1,3σ), TD soupeře beze změny.
- **Celkově účel nezměřen jako zlepšení:** míč u nás po našem tahu +1,3 b. ± 2,9 — pod rozlišením (K = 1, 155 situací).
- **a (zvedač podle celé akce) pomáhá lesním elfům:** +18,9 bodu míče u nás po našem tahu (+2,9σ). ⚠️ Jeden z ~60 rozdílů
  v tabulkách ⇒ může být i náhoda; u lidí opačný náznak po tahu soupeře (−1,7σ). Ověřit cíleně (elfové, víc situací, K = 2).
- **b (obestoupit) v zahraných tazích skoro nic** (±1 situace).
- **c (odtlačit na míč) v zahraných tazích nic nezměnil ani tady** — rozdíl vše − bez c je přesně 0 u míče i TD (1 turnover);
  spolu s P250 (160 situací) **0 změn v 315 situacích**, ačkoli hledání ho zvažuje (čítač 68–129). Kandidát na odstranění
  (KISS) — rozhodne uživatel.
