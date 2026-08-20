# JAK KONČÍ NAŠE DRŽENÍ MÍČE (20.08.2026, 3 000 her)

Uživatel 20.08.: *„umíme to popsat lépe? ztráta míče v důsledku blitz
ballcarriera?"* — **hypotéza potvrzena, a je to největší jednotlivá příčina
v celém korpusu.**

Skript `diag_possession_end_20260820.py`. Snímek `logs[i]` je začátek kola i;
držíme-li na i a na i+1 už ne, ztráta nastala **během kola i** ⇒ příčina je
v jeho událostech a `active_team` říká, čí to bylo kolo.

⚠️ **K32: v logu žádná událost `BLITZ` NEEXISTUJE, jen `BLOCK`.**
Rekonstrukce (18.08.): **blok, jehož útočník s nosičem na začátku kola
NESOUSEDIL, je blitz** — blok sousedství vyžaduje, blitz je ten, kdo k němu
teprve dojde. *(Nosič se v jejich kole nehýbe, takže záměna nehrozí.)*

## Konce držení — 6 991 držení

| | | |
|---|---:|---:|
| **ZTRÁTA MÍČE** | 3 417 | **48,9 %** |
| **DOŠLA KOLA** *(drželi jsme na konci půle)* | 2 641 | **37,8 %** |
| **TD** | 933 | **13,3 %** |

## ⭐⭐⭐ Rozpad ZTRÁT — 3 417

| příčina | | |
|---|---:|---:|
| ⛔⛔ **nosič SRAŽEN — BLITZ** *(došel k němu)* | **2 396** | **70,1 %** |
| **nosič SRAŽEN — BLOK** *(už u něj stál)* | 564 | 16,5 % |
| jiné *(nesražen, žádný neúspěšný hod)* | 197 | 5,8 % |
| neúspěšný DODGE nosiče | 126 | 3,7 % |
| neúspěšný GFI nosiče | 84 | 2,5 % |
| sražen v NAŠEM kole *(blok / jiné)* | 32 | 0,9 % |
| neúspěšný CATCH / PASS | 18 | 0,5 % |

## Co z toho plyne

⭐ **86,6 % ztrát je sražený nosič soupeřem** — a **blitz je 4,2× častější
než blok**. Vlastní chyba nosiče *(dodge + GFI + náš blok)* je dohromady
**7,1 %**. **Míč neztrácíme hloupostí. Sundají nám ho.**

⭐ **Na úrovni držení:** blitz nás připraví o míč ve **34,3 %** všech držení
*(2 396 / 6 991)* — tedy **skoro tak často, jak často nám dojdou kola
(37,8 %), a 2,6× častěji, než skórujeme (13,3 %)**.

⇒ ⭐⭐⭐ **Držení míče má dva konce a skórování je až třetí:**
**sundají nás blitzem** nebo **dojdou kola**.

## ⇒ Tím se mění naléhavost tří otevřených položek

1. **Pravidlo klece přestává být akademické.** Klec existuje **přesně proti
   tomuhle** — nosič bez volného souseda blitznout nejde bez ceny. **Plníme
   ho ve 2,7 % kol**, ačkoli je splnitelné v 95,6 %
   ([[project_bloodbowl_cage_rule_20260819]]).
2. **P37 povyšuje.** `carrierIsBlitzable` **nezná GFI**
   (`macro_actions.cpp:1162`): v **6,3 %** kol říká BEZPEČNO a soupeř
   dosáhne. Byl to „menší nález, nízká priorita" — jenže ta funkce rozhoduje,
   jestli si nosič nechá pohyb v záloze, a **blitz na nosiče je hlavní
   příčina ztráty**.
3. **Blitz jako rozpočet platí i OBRÁCENĚ.** Spec ČÁST 14 říká, že blitz se
   kupuje DOSAH. **Soupeř to přesně tak dělá** — a utrácí ho na našeho
   nosiče 2 396×.

⚠️ **Co tohle měření NEŘÍKÁ:** jestli by klec ty blitze zdražila natolik, aby
se nekonaly. Je to **popis příčiny, ne důkaz, že lék funguje** — proti tomu
stojí, že brána klece 18.08. **škodila** (−3,7σ).

Viz [[project_bloodbowl_drive_length_20260820]] · [[project_bloodbowl_where_lost_20260820]]
