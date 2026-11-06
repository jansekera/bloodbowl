# P250c — ověření části a (zvedač podle celé akce) u elfů a skavenů, 6.11.2026 (231 situací)

Uživatel 6.11.: c NEodstraňovat; ověřit a — *„zvednutí míče i u souseda nejvíce pomáhá elfům - pak skavenům - v tomto
vidím správnou odezvu“* ⇒ očekávání: a pomáhá nejvíc elfům, pak skavenům.

`situace.py … 40 2` s `NASI=woodelf,skaven RAMENA=vse,bez_a` (jen náš tah + odpověď soupeře, párově, **K = 2**):
lesní elfové (86) / skaveni (145) × orkové / lidé / trpaslíci, 40 her × útok/obrana = 480 her. Situace jako P250b
(volný míč na začátku našeho tahu, ne tah 0). Kód main 824c497e. Běh 15:18–23:37.

| ukazatel | vše | vše − bez a |
|---|---|---|
| míč u nás po našem tahu | 0,719 | **+0,026 ± 0,016 (+1,6σ)** |
| míč u nás po tahu soupeře | 0,528 | +0,017 ± 0,016 (+1,1σ) |
| míč u soupeře po našem tahu | 0,006 | −0,006 (−1,7σ) |
| TD soupeře | 0,004 | −0,002 |
| náš turnover | 0,435 | −0,009 (−0,5σ) |

Po týmech (míč u nás po našem tahu / po tahu soupeře):
- **lesní elfové (86): +0,052 ± 0,031 (+1,7σ) / +0,052 ± 0,031 (+1,7σ)**; míč u soupeře −0,017 (−1,8σ)
- **skaveni (145): +0,010 ± 0,017 (+0,6σ) / −0,003**

Spojeno s P250b (váženo 1/σ², stejné situace a ramena vše × bez a):
- **elfové: +0,077 ± 0,028 (+2,8σ)** (P250b +0,189 ± 0,065 na 37 situacích bylo z části náhoda — teď menší, ale potvrzené)
- **skaveni: +0,006 ± 0,015 (+0,4σ)**

Pozitivní kontrola: čítač a = 19 626 v rameni vše, **0** v bez a.

**Čtení.**
- **Pořadí sedí s očekáváním uživatele:** a pomáhá nejvíc elfům (≈ +8 bodů míče u nás po našem tahu, spojeně +2,8σ),
  u skavenů skoro nic (+0,6 b.).
- **Nic se nerozbilo:** turnover beze změny (−0,9 b.), TD soupeře beze změny.
- ⇒ **a ponechat zapnuté.** (c zůstává podle rozhodnutí uživatele.)
