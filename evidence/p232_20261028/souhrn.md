# P232 v běžných hrách — souhrn (28.10.2026)

Co P232 dělá (uživatel 27.–28.10.: „donutit se pohnout kupředu s míčem i přes riziko a to riziko popsat … cílem je
dojít a dát TD“):
- krok vpřed se hraje, i když je riskantní, pokud není riskantnější než stát;
- hranice rizika je podle času: 0,15 s časovou rezervou, 0,30 bez ní;
- hrozba na nosiče platí i po tahu: přesuny, které by ji zvedly, se nehrají.

Měřeno párově: tentýž stav i semínko, jen náš tah + jedna odpověď soupeře. Zmrazená knihovna `zmrazeno_p232` (commit
407527c6). Bylo 8 dvojic × 6 her, soupeř kope. Výsledek je 311 tahů s míčem (`tabulka.txt`).

| na 100 tahů s míčem (posun = pole za tah) | vše | bez P232 | rozdíl ± SE |
|---|---:|---:|---:|
| ramena se liší (pozitivní kontrola) | | | **26 z 311 tahů** |
| posun míče vpřed | 2,50 | 2,48 | +0,03 ± 0,03 |
| tahy jen do strany (řádek) | 1,31 | 1,97 | −0,66 ± 0,46 |
| nosič sražen v odpovědi | 1,93 | 1,29 | +0,64 ± 0,45 |
| míč ztracen v odpovědi | 2,62 | 1,97 | +0,66 ± 0,46 |
| turnover | 8,68 | 9,00 | −0,32 ± 0,56 |
| TD | 9,97 | 9,97 | 0 |
| čas na tah (s) | 29,9 | 27,6 | +2,4 ± 0,6 |

Čtení:
- P232 změnil rozhodnutí v 8 % tahů s míčem, a to hlavně u orků (15 ze 119). U trpaslíků ve 4 ze 115 tahů.
- Dělá, co má: místo posunu do strany jde vpřed. Posun vpřed je ale malý, +0,03 pole za tah, na hranici šumu.
- Cena je přijaté riziko. Nosič byl sražen asi o 2 tahy z 311 víc (+0,64 ± 0,45 na 100, tedy 1,4 SE, není
  průkazné). To odpovídá záměru: riziko do hranice se přijímá.
- „Hrozba platí i po tahu“ samotná (rameno `bez_drzeni`) změnila 22 tahů. Na posun, sražení ani ztrátu míče vliv nemá.
- Čas: +8 % na tah.
- TD v měřeném tahu se neliší (9,97 na 100 v obou ramenech).

Závěr: mechanika ve hře funguje (pozitivní kontrola = 26 změněných tahů) a míří správným směrem. Účinek je malý,
protože situace „bezpečný krok vpřed není“ je v běžných hrách vzácná. Nechat zapnuté.
