# ⛔⛔ KONTROLY VYHAZOVALY KOLA, VE KTERÝCH JSME SKÓROVALI (20.08.2026)

## Co se stalo

`diag_rules_checks_20260812.py` měl hned na začátku smyčky:

```python
if S.get("touchdown") or E["half"] != S["half"]:
    st["vyřazeno (TD/poločas mezi snímky)"] += 1
    continue
```

⇒ **Kolo, které skončilo touchdownem, se nedostalo do ŽÁDNÉ kontroly** —
ani do K9a, K29, K31, K33, K34, K35, K38.

**Filtr má legitimní půlku:** snímek `E` je až **po výkopu a přestavení**,
takže **pozice těl** z něj číst nejde. ⛔ **Jenže rozvrhové kontroly žádné
pozice nepotřebují** — stačí jim začátek kola a fakt, že se skórovalo.
A právě jim to vyhazovalo **PRÁVĚ TA KOLA, VE KTERÝCH JSME USPĚLI.**

## Dopad — celý korpus, 3 000 her

| | před opravou | **po opravě** | n |
|---|---:|---:|---:|
| **K9c VÝBĚH** | ⛔ **2,9 %** | ⭐ **69,3 %** | 1 308 |
| K9c SÓLO | 34,6 % | 34,8 % | 9 710 |
| K9c KLEC | 33,8 % | 34,0 % | 7 662 |
| **K9a** *(nejsilnější prediktor, 20,8σ)* | 23,7 % | **27,6 %** | 18 680 |

## ⛔ P41 SE TÍM STAHUJE

Nález *„fáze VÝBĚH má 2,9 %, o řád nejhorší ze tří"* byl **artefakt
kontroly**. Skutečnost je opačná: **VÝBĚH je NEJLEPŠÍ fáze, a to dvojnásobně**
*(69,3 % proti ~34 %)*.

**Proč to vyšlo tak nízko:** kontrola vyžadovala **téhož nosiče držícího míč
na konci kola**, a **TD ho ze snímku odstraní**. Na vzorku: z 232 kol fáze
VÝBĚH jich **139 skončilo TD** a kontrola počítala jen **79 zbylých** — tedy
měřila *„z kol, kdy jsme byli u endzony a NESKÓROVALI, jak často jsme
postoupili dost"*, což je skoro tautologicky nízké.

## Jak se to našlo

⚠️ **Ne kontrolou, ale sporem dvou vlastních měření.** Nový skript
`diag_runout_phase_20260820.py` *(rozpad podle `turns_left`)* dal **68,7 %**
proti 2,9 % z T0.1 — **rozdíl 24×**. Teprve to donutilo srovnat jmenovatele.

⇒ ⭐ **Dvě nezávislá měření téže veličiny jsou levnější než jedno pečlivé.**
Kdyby vzniklo jen jedno, P41 by se stavěla dál.

⚠️ **A druhá polovina poučení:** ten nový skript byl **taky vadný**, jen
opačně — bral **každé zmizení nosiče jako TD**, což platí pro 139 ze 153
*(zbylých 14 byly ztráty míče)*. **Obě měření byla špatně; správnou hodnotu
dalo teprve jejich srovnání.**

## Oprava

TD větev se vyhodnocuje **PŘED** plošným skipem: rozvrhové kontroly
*(K9a, K9c, K9x)* dostanou **splněno**, poziční *(K29, K38)* se přeskočí,
protože pro ně `E` opravdu použitelné není.

---

# AUDIT VÝSTUPŮ ZE SMYČKY (20.08. večer) — další tři nálezy

Po opravě TD jsem prošel **všech 27 míst**, kde smyčka kontrol dělá `continue`
nebo `skip()`, a ptal se u každého: **jakou TŘÍDU kol to vyhazuje?**

## 1. ⛔ Moje ranní oprava udělala kontrolu ASYMETRICKOU

Kola s TD se od rána odklánějí dřív ⇒ ve větvi `if car is None` zbyla
**právě ta, kde jsme míč ZTRATILI** — a všechna se `skip()`ovala.

⇒ **Úspěch se počítal jako splněno, neúspěch se vyhazoval** ⇒ kontrola byla
**systematicky optimistická**. **Horší než původní stav**, kdy se vyhazovalo
obojí *(vadně, ale symetricky)*.

**Opraveno:** ztráta míče = **maximální selhání rozvrhu**, ne chybějící data
⇒ rozvrhové kontroly dostanou **nesplněno**; poziční *(K29/K38)* se skipují
dál, protože bez nosiče je jejich predikát opravdu nedefinovaný.

| | před | po |
|---|---:|---:|
| K9a | 29,5 % | **28,9 %** |
| VÝBĚH | 67,2 % | **63,4 %** |

## 2. ⛔ Tiskl se PEVNÝ SEZNAM počítadel

Když se počítadlo přejmenuje, vytiskne se u starého názvu **nula** — a nula
vypadá jako *„tohle se nikdy neděje"*, ne jako *„tohle už se nepočítá"*.
Přesně tak se dnes tiskly `vyřazeno (TD/poločas)` a `bez míče na konci kola`
jako **0**, ačkoli obojí nastalo stovkykrát.

**Opraveno:** tiskne se, **co se opravdu počítalo**, a nulová počítadla se
vypíšou zvlášť jako nulová.

## 3. ⛔ Jedno počítadlo slepovalo DVĚ různé věci

`bez míče na konci kola` hlásilo **587 z 1 469** kol jako ztrátu. Slepovalo
totiž **obranná kola, kdy jsme míč nikdy neměli** *(578)* se **skutečnými
ztrátami** *(9)*.

**Opraveno** rozpadem na tři: obrana *(N/A)* · ztráta po doběhnutí rozvrhu
*(N/A)* · **skutečná ztráta** *(nesplněno)*.

## ⚠️ OTEVŘENÝ ROZPOR — nedořešeno

„Ztráta míče v našem kole" vychází zde na **~9 na 100 her**, kdežto ranní
rozpad konců držení *(`diag_possession_end_20260820.py`)* dal *„sražen
v NAŠEM kole 0,9 % z 3 417"*, tedy **~1 na 100 her**. **Nejspíš neměří
totéž** *(ranní verze vyžaduje událost `KNOCKED_DOWN` u nosiče, tahle jen
to, že na konci kola nedržíme)* — ale **ověřeno to není**.
⇒ **Zapsáno jako otevřené, ne dopočítáno narychlo** *(20.08. se třikrát
ukázalo, že tvrzení předběhlo ověření)*.
