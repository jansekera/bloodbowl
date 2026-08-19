# T0.1 KROK A — TEMPO PO FÁZÍCH (19.08.2026)

Uživatel 18.08. zakázal psát podlahu K9 jako konstantu: *„podlaha fáze klec se
nepíše jako 2 pole/kolo — pravidlo zní, že klec jede tak rychle, jak rychle se
dokáže znovu složit, strop je funkce volných těl a cíl je maximum, ne ten strop."*
⇒ **Podlaha se musí z něčeho odvodit, a to něco je tohle měření.**
`corpus_baseline_20260819_data`, 3 000 her, skript `diag_phase_pace_20260819.py`.

Jmenovatel: **17 728** našich kol, kdy nosič drží míč na začátku **i** konci kola
*(kola se ztrátou nebo výměnou nosiče se nepočítají — jinak by se ztráta míče
vydávala za nulové tempo)*.

## Fáze existují a mají různé tempo

| fáze | kol | podíl | **Δx/kolo** | odpor koridoru | volných těl |
|---|---:|---:|---:|---:|---:|
| **SÓLO** | 8 274 | 46,7 % | **2,26** | 1,18 | 7,26 |
| **KLEC** | 7 111 | 40,1 % | **1,93** | 1,41 | 7,82 |
| **VÝBĚH** | 2 343 | 13,2 % | **2,85** | 0,00 | 8,24 |

*(Klasifikace, první shoda vyhrává: **VÝBĚH** = odpor koridoru 0 a žádný stojící
soupeř na nosiče nedosáhne ani přes GFI · **KLEC** = aspoň dvě naše těla na
diagonálách nosiče · **SÓLO** = zbytek.)*

⇒ **Rovnoměrná podlaha trestá klec systematicky.** Dnešní K9 porovnává všechny
tři fáze proti témuž `ceil(vzdálenost / zbývající kola)`, ale klec vydá
**o 0,33 pole/kolo míň než sólo a o 0,92 míň než výběh**. Za 8 kol je to
**2,6 až 7,4 pole** rozdílu — a K9a je s 20,8σ nejsilnější prediktor tabulky,
takže se tím systematicky křiví to, podle čeho se řadí fronta.

## ⭐ Tempo klece JE funkce volných těl — a je monotónní

| volných těl | kol | **Δx/kolo** | odpor | rohů |
|---:|---:|---:|---:|---:|
| 3 | 54 | **1,33** | 1,81 | 2,26 |
| 4 | 142 | 1,65 | 1,68 | 2,32 |
| 5 | 321 | 1,80 | 1,72 | 2,46 |
| 6 | 761 | 1,98 | 1,66 | 2,58 |
| 7 | 1 399 | 1,86 | 1,54 | 2,72 |
| 8 | 1 757 | 1,88 | 1,49 | 2,81 |
| 9 | 2 656 | **2,02** | 1,14 | 2,90 |

Od tří těl k devíti roste tempo **1,33 → 2,02** *(+52 %)* a rohů **2,26 → 2,90**.
⇒ **Uživatelovo pravidlo je v datech vidět:** klec jede tak rychle, jak rychle
se dokáže složit, a schopnost složit se je počet volných těl.

⚠️ **Je to zamotané s odporem** — s počtem těl klesá i odpor koridoru
(1,81 → 1,14), takže část toho růstu nedělá klec, ale volnější hřiště.
Rozplést to jde jedině dvourozměrně (těla × odpor), na což je 54 kol
v nejnižším koši málo.

## Co zbývá rozhodnout, než se K9 přepíše

Podlaha nesmí být konstanta, ale **taky nesmí být průměr z téhož korpusu** —
to by bylo kruhové: polovina kol by prošla konstrukcí. A uživatel řekl
**„cíl je maximum, ne ten strop"**, takže referencí nemá být to, co vydáváme
běžně, ale to, co jde vydat.
⇒ Tvar reference je **rozhodnutí, ne detail**, a je zapsané jako otevřené.

---

# ⛔ KOREKCE TÉHOŽ DNE — kapacita se nesmí číst z kol, kde se pravidlo nehraje

*(uživatel 19.08.: „tady předpokládám záplavu starých čísel neaplikovatelných
na poslední rozvrh")*

Námitka sedí a **obrací závěr výše**. Tempa v kroku A jsou z korpusu, kde se
pravidlo klece plní ve **2,7 %** kol a klec má ⌀ 1,3 rohu. Zapsat je jako
KAPACITU znamená **povýšit dnešní neschopnost na strop**.

Δx nosiče podle toho, jak dobře klec stojí *(3 000 her)*:

| řez | kol | ⌀ Δx | medián | p75 |
|---|---:|---:|---:|---:|
| všechna kola s nosičem | 17 728 | 2,20 | 1,0 | 4,0 |
| ≥2 rohy | 7 703 | **1,98** | 1,0 | 4,0 |
| 4 rohy | 1 744 | 2,14 | 1,0 | 4,0 |
| 4 rohy, všechny čisté | 1 263 | 2,30 | 2,0 | 4,0 |
| ⭐ **CELÉ PRAVIDLO (K29⭐⭐)** | **377** | **2,55** | 3,0 | 5,0 |

⇒ **Správně postavená klec je RYCHLEJŠÍ, ne pomalejší.** Monotónně:
1,98 → 2,14 → 2,30 → **2,55**, tedy blízko výběhu (2,85).

⇒ ⛔ **Závěr kroku A („rovnoměrná podlaha trestá klec") NEPLATÍ.** Podlaha
netrestá klec, trestá **naši** klec. Fáze „klec" není pomalá fáze; pomalá je
rozbitá klec, a to je vada, ne vlastnost.

⚠️ **Korelace, ne kauzalita:** kola, kde pravidlo drží, můžou být kola, kde je
soupeř daleko — což umožnilo obojí naráz, čistou klec i rychlý postup.

## Co to dělá s T0.1

* **Kapacita fáze KLEC se z tohoto korpusu spolehlivě změřit NEDÁ.** Nejlepší
  dostupná dolní mez je **2,55** ze 377 kol, kde pravidlo drží — ne 1,93.
* ⏰ **Přeměřit po noci P38.** Teprve korpus, který pravidlo hraje, dá kapacitu,
  na které smí rozvrh stát. Táž podmínka jako u σ pravidla (0,0 = vyhladovělý metr).
* **Zpětný rozvrh od 8. kola zůstává správný tvar** *(uživatel 19.08.)* —
  mění se jen to, čím se plní: kapacitou pravidlem postavené klece, ne historií.
* ⛔ **Percentil jako kapacita byl špatná náhrada.** S průměrem vyšlo 95,8 % kol
  jako „nesplnitelný rozvrh", s p75 40,7 %, s p90 19,6 % — číslo řídil volený
  percentil, ne hra. Percentil jen zakrýval to, že chybí kapacita z pravidla.
