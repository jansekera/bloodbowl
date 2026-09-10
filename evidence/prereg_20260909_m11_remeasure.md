# PŘEDREGISTRACE — M11(a) PŘEMĚŘENÍ PO BFS (09.09.2026)

⚑ **Není to A/B noc, je to JEDNORAMENNÝ DIAGNOSTICKÝ SBĚR.** Neprochází bránou
`colab_night_chunked.py`, ale disciplína `evidence/PREREG_CHECKLIST.md` platí:
mechanismus první, predikce dopředu, sanity-test před čtením.

## Otázka

`M11(a)` *(„vlastní zeď před nosičem", 149/149 z 26.08.)* je v `task_queue.md`
vedená jako **⏰ přeměřit, ne odškrtnout**. Od té doby se změnily **dvě** věci,
které na to můžou sahat:
* **08.09.** nasazená ramena `M12/(A) sideFree` + `(C) noResign`
* **09.09.** `movePlayerToward` přepnutý na BFS `nextStepToward` *(`75f9df43`,
  do produkce `4877198a`)*

⛔ **Tenhle běh je NEUMÍ ROZLIŠIT** a nebude to tvrdit. Ptá se jen na to, na co
se ledger ptá: **je ta vada pořád živá?** Na to jednoramenný sběr stačí —
atribuce by potřebovala párové A/B, a ta se tu nedělá.

## MECHANISMUS: co se měří a proč právě to

Ne win rate. Dvě čísla, obě přímý následek toho, co se změnilo:

| # | veličina | baseline 19.08. *(3000 her)* |
|---|---|---|
| **M1** | podíl **volných nosičů, kteří STÁLI** | **29,2 %** *(3573 / 12253)* |
| **M2** | mezi stojícími: pole **přímo vpřed obsazené NAŠÍM** tělem | **0,50** *(z 0,51 obsazených ⇒ 98 % překážek je našich)* |

**Proč M1:** `(C) noResign` cílí přímo na *„ADVANCE rezignuje, ač je volno vedle
přímky"* — 21 % kol podle `diag_c_fallback_20260826.py`. Když funguje, podíl
stojících nosičů musí klesnout.
**Proč M2:** to je doslova formulace `M11` a `C4` v `celotah_situace.md`
*(„kolikrát je pole před nosičem obsazené naším tělem")*.

## PREDIKCE — zapsáno PŘED během

1. **M1 klesne** pod 29,2 %. Očekávám **22-27 %**; pokles pod 20 % by znamenal,
   že (C) bere víc, než na kolik byla měřená *(21 % kol, a ne všechna končila
   stáním nosiče)*.
2. **M2 klesne** z 0,50. Očekávám **0,35-0,45**. ⚠️ Nečekám pád k nule: BFS
   umí nosiče **obejít** vlastní tělo, ale **nezabrání tomu, aby tam to tělo
   stálo** — na to je potřeba pořadí aktivací, tedy CELOTAH *(`C4`)*.
3. **Podíl „z obsazených jsou naši" zůstane vysoký (>90 %)** i po opravě.
   Tohle je predikce, která odliší dvě vysvětlení: když nosič obchází, mění se
   **jak často mu překážka vadí**, ne **čí ta překážka je**.

⛔ **Co by predikci VYVRÁTILO:** M1 i M2 beze změny *(±2 %)* ⇒ vada je živá,
obě nasazené změny na ni nesahají, a patří do KLECE tak, jak ji tam uživatel
09.09. přeřadil.

## SANITY-TEST *(oba PŘED čtením výsledku)*

1. ✅ **Měřidlo reprodukuje baseline** — `diag_m11_free_carrier_20260827.py`
   pustěn 09.09. na `corpus_baseline_20260819_data`: 29,2 % / 0,50 / 0,51.
   Bez tohohle kroku by nové číslo nešlo interpretovat *(neví se, jestli se
   liší kód, nebo skript)*.
2. ⏰ **Otisk enginu** — `ENGINE_HEAD` čerstvého sběru musí sedět na dnešní
   `cf8634e8`+. Když ne, sbíralo se jiným enginem, než o kterém se mluví (P22).

## Zadání běhu

* **1000 her**, produkční nastavení, žádné rameno *(`run_corpus_baseline.sh`)*
* tempo změřeno rychlotestem: **20 her / 3 min 17 s** *(~7 jader)* ⇒ **~2,8 h**
* `n` u M2: ~1200 stojících tahů ⇒ SE ≈ 0,014 ⇒ pokles o 0,05 je ~3,5 σ

⚠️ **Rychlotest odhalil, že sběr byl v tomhle prostředí ROZBITÝ** — `python3`
je 3.10, ale binding `bb_engine` je `cpython-38` ⇒ workery padaly na
`ModuleNotFoundError`. Opraveno v `run_corpus_baseline.sh` *(`PY=python3.8`
+ pojistka, která spadne PŘI STARTU, a `cd "$ROOT"`, protože collector si
vkládá cesty relativně)*. Noční A/B harness tím netrpěl — jede přes
zkompilovanou binárku, ne přes binding.

---

# VÝSLEDEK (10.09.2026) — 1000 her, engine `cf8634e8`

## SANITY-TESTY: oba prošly PŘED čtením

1. ✅ měřidlo reprodukovalo baseline *(29,2 % / 0,50 na korpusu 19.08.)*
2. ✅ `ENGINE_HEAD` = `cf8634e8` — sedí na dnešní engine
3. ✅ **tahů na hru identicky 16,00** v obou korpusech ⇒ rozdíly nejsou
   artefakt délky drivů

## Předpovědi proti skutečnosti

| # | veličina | 19.08. | 09.09. | predikce | |
|---|---|---|---|---|---|
| **M1** | volný nosič **STÁL** | 29,2 % | **18,7 %** | 22-27 % | ⛔ **MIMO** — klesl VÍC |
| **M2** | u stojícího je vpřed **NÁŠ** | 50,1 % | **27,5 %** | 0,35-0,45 | ⛔ **MIMO** — klesl VÍC |
| **M3** | z obsazených jsou **naši** | 98,9 % | **96,7 %** | > 90 % | ✅ **TREFA** |

n = 3573 vs 534 stojících ⇒ M1 i M2 jsou **~10 σ+**, ne šum.

⭐ **Obě „mimo" jsou mimo ve prospěch opravy** — vada ustoupila víc, než jsem
čekal. ⚠️ Ale běh **neumí říct, čím** *(ramena (A)/(C) z 08.09. vs BFS z 09.09.
vs cokoliv dalšího za ten měsíc)* — a netvrdí to.

## ⭐⭐⭐ NÁLEZ, KTERÝ PRŮMĚRY SKRÝVALY — „149/149" NIKDY NEZNAMENALO, CO SE Z NÍ ČTE

Doplněné měřidlo *(`diag_m11_denominator_20260910.py`, tiskne i ZBYTEK)*
rozpadlo pole **přímo vpřed** u stojícího volného nosiče na tři možnosti:

| co je vpřed | 19.08. | 09.09. |
|---|---|---|
| **NÁŠ** hráč | 50,1 % | 27,5 % |
| **JEJICH** hráč | 0,5 % | 0,9 % |
| **PRÁZDNÉ** | **49,4 %** | **71,5 %** |

⇒ ⛔ **Už v baseline měla POLOVINA stojících volných nosičů před sebou PRÁZDNO.**
`M11`/`C4` *(„149/149 vlastními")* tedy nikdy neříkala *„nosič je zablokovaný,
kdykoliv stojí"* — říkala jen, že **KDYŽ je zablokovaný, je to naše tělo**.
A to platí dál *(96,7 %)*.

⇒ ⭐⭐ **Po opravě je dominantní zbytek JINÁ otázka:** v **71,5 %** případů
nosič stojí, **ačkoliv má pole vpřed volné**. To není „vlastní zeď", to je
buď stall doktrína *(nepřekračuj čáru dřív, než musíš — `P27`/`M10`)*, nebo
volba MCTS. ⏰ **Tam se má `M11` posunout, ne odškrtnout.**

## ⚠️ POSUN JMENOVATELE — ČÍSLO, KTERÉ SE NESMÍ ČÍST JAKO VERDIKT

| | 19.08. | 09.09. |
|---|---|---|
| nemáme míč | 58,4 % | 61,9 % |
| nosič **markovaný** *(v cizí TZ)* | 16,1 % | **20,3 %** |
| nosič **volný** | 25,5 % | **17,8 %** |
| **TD na hru** | **0,369** | **0,274** |

⛔⛔ **TOHLE NENÍ NÁLEZ O M11 A NESMÍ SE TAK ČÍST.** Je to srovnání **napříč
časem** na **pohyblivé bázi**: mezi 19.08. a 09.09. je měsíc změn na **obou**
stranách *(P9c, M1-M5, M13, B5, L2, ramena (A)/(C), M14b, W-GFI…)* a korpus je
jiný. Přiřadit ten pokles TD čemukoliv konkrétnímu **z tohoto běhu nelze** —
právě na to jsou párová A/B *([[feedback_moving_baseline_only_paired_ab]])*.
⇒ Zapsáno jako **OTÁZKA**, ne jako výsledek: *stojí za to změřit TD/hru párově?*
