# P253 — měření ve hře 7.11.2026 (683 situací)

Uživatel 7.11.: „spusť měření“. `situace.py … 16 1` (jen náš tah + odpověď soupeře, párově, K = 1): skaveni (262) /
lesní elfové (253) / lidé (168) × orkové / lidé / trpaslíci, 16 her × útok/obrana = 128 her. Situace = náš tah, míč
drží náš hráč a stojící spoluhráč je 1–10 polí od něj blíž k endzóně. Ramena: vše · bez (vypínač `kP253PassPricing`).
P247 (engine přehazuje i nepřesnou přihrávku) platí v obou ramenech. Kód main 97afec8a + vypínač. Běh 01:25–05:57.

| ukazatel | vše | vše − bez |
|---|---|---|
| TD náš v našem tahu | 0,124 | −0,003 ± 0,002 (−1,4σ) |
| přihrávka zahrána | 0,009 | **+0,004 ± 0,003 (+1,7σ)** · elfové +0,012 (+1,7σ) |
| míč u nás po našem tahu | 0,959 | −0,001 |
| míč u nás po tahu soupeře | 0,909 | +0,006 (+0,9σ) · lidé +0,024 (+2,0σ) |
| TD soupeře | 0,003 | 0,000 |
| náš turnover | 0,157 | −0,004 (−0,6σ) |

**Pozitivní kontrola:** čítač „nabídnuto jen díky přehozům“ 4 540 a „vyřazeno předností skillů“ 665 v rameni vše,
**0 / 0** v rameni bez ⇒ vypínač i čítač fungují; brána se v hledání mění hodně.

**Čtení.**
- **Nic se nerozbilo:** turnover, TD soupeře, míč u nás beze změny (vše pod 1,5σ kromě lidí „míč po tahu soupeře“
  +2,4 b., +2,0σ — v ~20 porovnáních čekáme jedno takové náhodou).
- **Ve hře se změní málo:** AI přihrávku zahraje zřídka (0,9 % situací s P253, 0,5 % bez); P253 ji přidá hlavně
  elfům (+1,2 b.). Na TD v našem tahu vliv neměřitelný (−0,3 b., −1,4σ).
- ⇒ **P253 ponechat zapnuté** (je správnější a nic nekazí); větší dopad by měl až tam, kde AI přihrávat chce
  (např. přihrávka do skórovací zóny) — to je jiná otázka.
