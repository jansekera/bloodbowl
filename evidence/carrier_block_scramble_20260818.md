# BLOK NA SOUPEŘOVA NOSIČE: KOLIK MÁME BLÍZKO VOLNÝCH TĚL (18.08.2026)

Zadal uživatel 18.08.: *„ještě k tomu blocku na nosiče — zkusit dopočítat,
kolik dalších volných hráčů máme pak blízko."*
Nástroj `diag_carrier_block_scramble_20260818.py`, korpus `corpus_baseline_20260817_data`
(3 000 her, otisk enginu `5e5ab352`), snímek **začátku našeho kola**,
vzdálenost Chebyshev k **poli nosiče** — odtud se míč po sražení rozptyluje.

## Kolik je vůbec příležitostí

| | |
|---|---|
| našich kol | 48 000 |
| z toho soupeř drží míč | 20 565 |
| **blok na nosiče JE k dispozici** | **3 733 = 18,2 %** *(7,8 % všech našich kol, ~1,24 na zápas)* |
| kandidátů na úder, když jde | 1,51 |

## ⭐⭐⭐ Kolik máme u pole nosiče těl — a kolik jich má soupeř

*(bez toho, kdo by udeřil; „volný" = stojící a nesousedící se stojícím soupeřem)*

| do | našich stojících | z toho **VOLNÝCH** | **soupeřových volných** |
|---|---|---|---|
| 1 | 0,51 | 0,27 | **1,23** |
| 2 | 1,60 | **0,92** | **2,26** |
| 3 | 2,63 | 1,69 | 2,96 |

**Rozložení našich volných těl do 2 polí:**
**0 volných v 45,7 %** · 1 v 29,9 % · 2 v 14,9 % · 3 v 6,4 % · 4+ v 3,1 %

**Kdo má u pole nosiče převahu (volná těla do 2):**
my 19,3 % · shoda 18,4 % · **soupeř 62,3 %**

## ⭐ Co z toho plyne pro P10

Uživatelova otázka ze 14.08. zněla: *„když je vedle našeho Longbearda možnost
block na GR s míčem a navíc jsou kolem naši — co může být lepšího?"*
Odpověď na první půlku platí. **Druhá půlka („a navíc jsou kolem naši") ale
většinou NENASTÁVÁ:** v **45,7 %** případů nemáme u pole nosiče **ani jedno**
volné tělo, a v **62,3 %** jich má soupeř víc než my.

⇒ **P10a se nesmí formulovat jako „preferuj blok na nosiče".** Sražení nosiče
míč **uvolní** — a uvolní ho do prostoru, kde je soupeř ve 3 z 5 případů silnější.
Správný tvar je **podmínka na lokální přesilu**, ne priorita akce:
*udeř na nosiče, když u jeho pole máme aspoň tolik volných těl co soupeř* —
což je dnes **37,7 %** příležitostí (my > nebo shoda).

⚠️ **Poctivá výhrada, a je důležitá.** Měřeno na **začátku kola, před pohybem**.
Během kola se dá k nosiči nejdřív dojít a udeřit až potom, takže 0,92 je
**DOLNÍ ODHAD** toho, co bychom u scramble mít mohli. To ale doktrínu nemění,
jen ji přeformuluje z výběru akce na **pořadí akcí**:
⭐ **blok na nosiče se má PŘIPRAVIT, ne popadnout** — napřed přivést těla,
udeřit až nakonec. Přesně proto je to úkol pro **R4 „tělo bez úkolu"**
(2,14 idle těl na kolo, 94,7 % někam dosáhne), ne pro filtr nabídky.

⇒ **Zbývá dopočítat, o kolik se ta bilance zlepší, když se těla nejdřív svedou**
— tedy totéž číslo na snímku KONCE našeho kola, plus kolik těl na pole nosiče
vůbec dosáhne. Bez toho neznáme strop P10a.
