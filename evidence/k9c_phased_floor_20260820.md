# T0.1 — K9 PŘEPSANÁ PO FÁZÍCH (20.08.2026)

Zadání uživatele 18.08.: *„jsem pro přepsat celé — minimálně na fáze — a ty
kroky asi měřit zvlášť"*, a *„je zbytečné uvedení konstanty někam, kde by
stačilo — nesmí se rozpadnout, ale má jet co nejrychleji"*.

## Co K9c je

Fáze se klasifikuje **ze začátku kola** *(podlaha je požadavek NA to kolo, ne
na to, kam jsme došli — táž past, kvůli které K9a bere `need` z `carS`)*:

| fáze | jak se pozná | mechanický strop |
|---|---|---|
| **VÝBĚH** | na endzonu se dá dojít už teď (`dist ≤ MA + 2`) | MA nosiče |
| **KLEC** | klec stojí (≥ 2 rohy, táž mez jako K29) | **MA nejpomalejšího rohu**, shora omezené nosičem |
| **SÓLO** | ani jedno | MA nosiče |

**Podlaha: `need_fáze = min(need_rovnoměrný, strop_fáze)`** — dlužíš svůj
rovnoměrný podíl, ale nikdy víc, než co fáze mechanicky dovolí.

⭐ **Žádná laděná konstanta.** `+2` u výběhu jsou dva GFI, tedy **pravidlo**;
`≥2 rohy` je mez, na které už stojí K29; strop klece je **funkce těl**
(MA nejpomalejšího rohu), ne číslo. ⇒ [[feedback_implement_the_rule_not_the_outcome]]

⭐ **Jediná definice:** `phase_floor()` v `diag_rules_checks_20260812.py`
volá kontrola **i** σ-tabulka. Kopie by znamenala, že metr měří jinou
veličinu než kontrola a nikdo se to nedozví *(táž lekce jako `corridorResistance`)*.

## Výsledek na 3 000 hrách (17 728 kol)

| | splněno | n | ⌀ rezerva |
|---|---:|---:|---:|
| K9a *(stará, rovnoměrná)* | 23,7 % | 17 728 | −2,96 |
| K9c **SÓLO** | 34,6 % | 9 679 | −1,83 |
| K9c **KLEC** | 33,8 % | 7 635 | −1,77 |
| K9c **VÝBĚH** | ⛔ **2,9 %** | 414 | ⛔ **−3,38** |
| **K9a žádala mechanicky NEMOŽNÉ** | **29,2 %** | 17 728 | +1,12 |

## ⛔ σ-tabulka: PŘEPIS JAKO KONTROLA NEUSPĚL

*(3 000 her, 5 031 drivů, 835 se skórováním)*

| veličina | σ |
|---|---:|
| **K9c_rezerva** *(číslo)* | **+23,1** |
| **K9a_žádala_nemožné** | **−21,6** |
| K9a_splněno *(ano/ne)* | **20,8** |
| **K9c_splněno** *(ano/ne)* | **16,3** |
| Δx | 14,4 |

⚠️ **Poctivé čtení:** srovnávat 23,1σ (spojitá) s 20,8σ (binární) je podvod
na sobě samém — spojitá veličina dostane vyšší σ skoro vždy.
**Férové je stejný tvar proti stejnému tvaru: K9c 16,3σ proti K9a 20,8σ.**

⇒ **Nová podlaha je jako KONTROLA slabší prediktor než stará.**

⭐ **A důvod stojí hned vedle:** *„K9a žádala nemožné"* má samo **−21,6σ**.
Být ve stavu, kdy fáze na požadované tempo nestačí, **je samo o sobě silná
zpráva, že drive nedopadne.** Když to nová podlaha odpustí, **zahodí signál**.

⇒ ⭐⭐⭐ **Rovnoměrná podlaha je lepší prediktor PRÁVĚ PROTO, že je
nespravedlivá.** Slepuje *„nechtěli jsme"* a *„nemohli jsme"* — a to druhé je
taky špatná zpráva.

⇒ **Pravidlo a metr nemusí mít týž tvar** *(už to stojí v komentáři u `Check.num`)*:
**K9a je dobrý METR a špatný PŘÍKAZ; K9c je správný PŘÍKAZ a horší METR.**
⇒ **Nechat obojí.** K9a zůstává v σ-tabulce, K9c je to, co říkáme enginu.

## ⭐⭐⭐ NEČEKANÝ NÁLEZ: VÝBĚH JE 2,9 %

Ve fázi, kdy je endzona **na dosah**, splníme podlahu ve **2,9 %** kol
(n = 414) a průměrně nám chybí **3,38 pole** — nejhorší ze všech tří fází
o řád. Replikuje ze vzorku (100 her: 5,3 %, n = 19).

⚠️ **Poctivá výhrada:** v POSLEDNÍM kole půle je `need_fáze = min(dist, MA)`,
tedy „sprintuj naplno", takže nízký podíl je zčásti dán konstrukcí. **Ale
průměrný schodek −3,38 pole tím vysvětlený není** — když je endzona na
dosah, hýbeme se o tři a půl pole míň, než umíme.

⇒ Napojuje se přímo na to, že **v 68,4 % zápasů nedáme ani jeden TD**
([[project_bloodbowl_where_lost_20260820]]) a na **P39** *(nosič se
neaktivuje)*. ⛔ **Fáze VÝBĚH nebyla dosud NIKDY samostatně měřena.**

⚠️ A druhá věc, kterou to boří: **SÓLO 34,6 % a KLEC 33,8 % jsou prakticky
totéž.** Předpoklad za T0.1 zněl *„rovnoměrná podlaha trestá klec"* — klec
ale proti sólu nezaostává. **Zaostává VÝBĚH.**

## Kde to je

`phase_floor()` + kontroly `K9c_solo` · `K9c_cage` · `K9c_run` · `K9x`
v `diag_rules_checks_20260812.py`; napojení do σ-tabulky
v `diag_drive_predictors_20260813.py`.
