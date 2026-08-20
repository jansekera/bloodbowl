# STAV KLECE PŘED OPRAVOU P45 (20.08.2026)

⚠️ **Referenční snímek starého enginu.** Po **P45** *(ležící nesmí vzít Block
akci; vstávání na 4+ při MA < 3)* se **akční ekonomika změní**, takže tahle
čísla **přestanou platit** — nebudou špatná, budou **z jiného enginu**.
⇒ **Proti tomuhle se bude číst, co P45 udělala.**

**Korpus** `corpus_baseline_20260819_data`, 3 000 her, engine `ea6f0a51`.
**Kontroly** po čtyřech dnešních opravách *(TD se nevyhazuje · ztráta míče =
nesplněno · tisk počítadel · rozpad slepeného počítadla)*.

## 1. Výsledek — tvůj měřák

| | | |
|---|---:|---|
| **držení skončilo TD** | **13,3 %** | 933 z 6 991 |
| **zápasů s aspoň jedním TD** | **31,6 %** | ⇒ v 68,4 % nedáme nic |
| ⌀ TD na zápas | **0,337** | a **1,07** na zápas, kde jsme skórovali ⇒ **když dáme, dáme právě jednou** |
| držení skončilo **ztrátou** | 48,9 % | z toho **70,1 % blitz na nosiče**, 16,5 % blok |
| držení skončilo **koncem půle** | 37,8 % | |

## 2. Plnění pravidla klece

| | | n |
|---|---:|---:|
| ⛔ **PRAVIDLO** *(4 rohy ∧ všechny čisté ∧ nosič bez dalších sousedů)* | **2,7 %** | 24 754 |
| K29⭐ plná čistá klec — ⚠️ **jen 2 klauzule ze 3** | 12,1 % | 16 517 |
| K29 žádný roh není markovaný | 80,3 % | 16 517 |
| **kol s míčem, kdy klec vůbec NESTOJÍ** | **8 237** | z 24 754 |
| obsazených rohů ⌀ | **2,13 / 4** | z toho čistých **1,90** |

⭐ **Hlavní čtení:** těch 2,7 % **není o čistotě rohů** — klec **z větší části
neexistuje**. Nestojí ve **třetině kol s míčem**, a když stojí, má **2,13
rohu ze čtyř**. *(Splnitelné je přitom 95,6 % — [[project_bloodbowl_carrier_square_rule_20260819]].)*

## 3. Zákaz a rozvrh

| | | n |
|---|---:|---:|
| **K38** nosič nekončí u **stojícího** *(zákaz)* | 87,7 % | 24 754 |
| **K38b** …ani u **ležícího** *(výstraha)* | 69,7 % | 21 706 |
| **K9a** rozvrhová podlaha | 27,2 % | 18 977 |
| **K9c SÓLO** | 34,3 % | 9 859 |
| **K9c KLEC** | 33,7 % | 7 739 |
| **K9c VÝBĚH** | **65,7 %** | 1 379 |

## 4. Postup

| | |
|---|---:|
| rovně vpřed **zablokováno** | 61,2 % kol s míčem |
| …a **do boku volno** *(uhnutí by pomohlo)* | **26,7 %** všech kol |
| ⌀ kol na TD | 4,08 *(medián 4, max **7, nikdy 8**)* |

## ⚠️ Meze snímku

* **konverze** *(část 1)* je z `diag_drive_len_20260820.py` a
  `diag_possession_end_20260820.py`, **ne z kontrol** — do `diag_rules_checks`
  nikdy nevstoupila. ⇒ **Přidat**, ať se stav klece nemusí skládat ze tří skriptů.
* rozpad **podle rasy soupeře** chybí — skript `diag_where_lost` má **vadné
  čtení jména týmu** *(u všech ras vyjde „Lineman")*.
* ⚠️ Otevřený rozpor: „ztráta v našem kole" vychází z kontrol na ~9/100 her,
  z rozpadu konců držení na ~1/100. **Neověřeno, že měří totéž.**
