# P5 — HAND-OFF: STROP SPOČÍTÁN PŘED BĚHEM (19.08.2026)

První měření na korpusu, který **má opravu exportu** `HAND_OFF` (`c943e8b8`).
`corpus_baseline_20260819_data`, 3 000 her, 48 000 našich kol.
Skript `diag_handoff_choice_20260819.py`.

## Hand-off žije, jak se čekalo

**151 zahraných hand-offů = 0,050 na zápas** *(PASS 46 = 0,015)*.
Replikuje číslo ze 17.08. (130 / 0,043) na čerstvém korpusu ⇒ **nula ze 13.08.
byla vada exportu a je definitivně vyřízená.**

## ⛔ Ale strop opravy je 0,15 kola na zápas

| | kol | podíl |
|---|---:|---:|
| kol s nosičem | 19 988 | |
| **nosič JE Runner** | 16 689 | **83,5 %** |
| nosič NENÍ Runner | 3 299 | 16,5 % |

A z těch 3 299 kol, kdy nese někdo jiný:

| | kol | podíl |
|---|---:|---:|
| **Runner STOJÍ VEDLE** *(situace, na kterou P5 míří)* | **492** | **14,9 %** |
| Runner není vedle, ale dojde (do MA) | 1 128 | 34,2 % |
| žádný volný Runner na dosah | 1 679 | 50,9 % |

**V té situaci předáváme v 7,9 % (39 z 492).** ⇒ Dokonalá oprava volby
přesměruje **453 kol za 3 000 her = 0,15 kola na zápas.**

⭐ **To je řádově P8 (0,056) a P10a (0,23) — obojí ZAMÍTNUTO stropem.**

## Proč to není důvod P5 zahodit úplně

* **Nese to víc než běžné kolo.** Runner veze **3,41 pole/kolo**, Longbeard
  **1,50** (11.08.) ⇒ jedno přesměrované kolo mění tempo, ne jen jednu akci.
  Strop v KOLECH podceňuje hodnotu; strop v POLÍCH by byl ~0,15 × 1,9 ≈ 0,29
  pole na zápas, což je pořád málo proti Δx 14,4σ.
* **Větší množina je jinde.** *„Runner není vedle, ale dojde"* je **34,2 %
  (0,376 kola/zápas)** — tam ale nejde o volbu hand-offu, ale o to, že Runner
  **nestojí, kde má**. To je úloha o pozici, ne o předání.

## ⇒ Návrh rozhodnutí

**P5 jako oprava VOLBY hand-offu: ZAMÍTNOUT stropem**, stejně jako P8 a P10a.
**Otevřít místo ní otázku o POZICI Runnera** *(proč nestojí vedle nosiče, když
nosičem není)* — ta má 2,5× větší množinu a spadá pod tutéž chybějící dimenzi
**KAM** jako P9 · P34 · P35 · P38.

## ⭐ Vedlejší nález: O8 se povedla

Nosičem je Runner v **83,5 %** kol. Oprava rozestavení z 11.08. (`41c3570`,
*„proč Longbeard bere míč = ROZESTAVENÍ"*) tedy **drží** — problém „nese špatné
tělo" je dnes 16,5 %, ne většina.
