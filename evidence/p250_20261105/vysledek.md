# P250 — měření ve hře 5.11.2026 (160 situací)

`situace.py` (jen náš tah + odpověď soupeře, párově, K = 2): **náš tým skaveni (61) / lesní elfové (60) / lidé (39)**
× orkové / lidé / trpaslíci, 3 hry × útok/obrana. Situace = náš tah v obraně, soupeř drží míč, na nosiče někdo
dosáhne (stejné jako P249e/f). Kód main 3e325fb3. Běh 15:11–19:10 (po restartu notebooku znovu od začátku).
Rozdíl = vše − rameno bez části (± chyba průměru, σ).

| ukazatel | vše | bez a (zvedač) | bez b (obestoupit) | bez c (odtlačit) | bez P250 |
|---|---|---|---|---|---|
| míč u nás po našem tahu | 0,062 | +0,003 (+0,3) | +0,006 (+0,6) | 0,000 | +0,006 (+0,6) |
| míč u nás po tahu soupeře | 0,047 | +0,006 (+0,6) | +0,003 (+0,4) | 0,000 | +0,006 (+0,6) |
| soupeř ztratil míč v našem tahu | 0,138 | +0,006 | +0,003 | 0,000 | +0,013 (+1,2) |
| TD soupeře | 0,091 | +0,003 | 0,000 | 0,000 | +0,006 (+1,4) |
| náš turnover | 0,528 | +0,019 (+1,0) · elfové +0,042 (+1,3) | +0,003 | −0,003 | 0,000 |

**Pozitivní kontrola** (čítače `take_p250`, součet za 320 tahů ramene; čítají i během hledání MCTS, ne jen zahrané tahy):

| rameno | a zvedač změněn | b obestoupení nabídnuto | c odtlačen na míč |
|---|---|---|---|
| vše | 8 945 | 1 909 | 97 |
| bez a | **0** | 1 968 | 39 |
| bez b | 9 087 | **0** | 118 |
| bez c | 8 750 | 1 857 | **0** |
| bez | **0** | **0** | **0** |

⇒ vypínače i čítače fungují (nula přesně tam, kde je část vypnutá).

**Kde mechanismus vůbec mohl zasáhnout:** soupeř ztratil míč v našem tahu jen v **37 ze 160** situací. V nich
(podmínka je až po zásahu ⇒ jen orientačně):

| v 37 situacích | vše | bez a | bez b | bez c | bez |
|---|---|---|---|---|---|
| míč u nás po našem tahu | 0,270 | 0,230 | 0,203 | 0,270 | 0,216 |
| míč u nás po tahu soupeře | 0,189 | 0,162 | 0,149 | 0,189 | 0,162 |
| turnover | 0,446 | 0,432 | 0,446 | 0,459 | 0,446 |

**Čtení.**
- **Nic se nerozbilo:** turnover, TD soupeře ani pokusy o nosiče se proti „bez P250“ neliší (vše pod 1,5σ).
- **Účel (míč u nás) zatím nezměřen, ne vyvrácen:** P250 působí jen na volný míč, a ten v této sadě vznikl
  jen ve 37 situacích ⇒ celkový rozdíl +0,006 je pod rozlišením. V těch 37 je míč u nás po našem tahu
  27,0 % proti 21,6 % bez P250 (+5,4 bodu), ale podmíněně a na malém vzorku.
- **c (odtlačit na míč) v zahraných tazích nic nezměnilo:** rozdíl vše − bez c je přesně 0 ve všech ukazatelích
  (při stejném semínku) — čítač 97 je z hledání, do zahraného tahu se odtlačení nedostalo.
- **a u elfů turnover +4,2 bodu (+1,3σ)** — pod hranicí, hlídat v dalším měření.

**Další krok (návrh):** měřit tam, kde P250 působí — situace, kdy **na začátku našeho tahu leží míč volně**
(po výkopu, po srážce nosiče v tahu soupeře) a dosáhne na něj náš hráč. Metriky stejné (míč u nás po našem tahu /
po tahu soupeře, turnover), ramena stejná.
