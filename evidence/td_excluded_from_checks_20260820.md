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
