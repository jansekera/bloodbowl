# PŘEDREGISTRACE — M12/(B) KLECOVÉ KRITÉRIUM, MULTI-RASOVÁ SONDA
**Zapsáno PŘED spuštěním. Prahy platí tak, jak stojí tady.**

---

## Proč tenhle běh existuje

M12/(B) (`cageScoreForSquare` — cílové pole pohybu se vybírá i podle tvaru
klece) bylo změřeno 30.→31.08. na **dw-dw** (2400 párů): **DELTA +0,0242 ±
0,0086 SE (2,82σ)** — pomáhá, směr jistý, velikost proti prahu ±0,015 CI
obkračuje. Výsledek ležel nezapsaný měsíc (nález 08.09.).

Uživatel predikoval 26.08.: **"(B) se bojím, že vyjde, že je nebezpečné od
všech krom khemri v dešti"** — klec svazuje 4 těla, proti rychlé/přihrávkové
rase je to zátěž (`P59`: proti wood-elfovi 29 % cest k TD jen přihráním).
dw-dw je přesně ten "khemri" případ (pomalá, ground-game rasa) — **potvrzuje
predikci pro POMALÝ případ, neříká nic o rychlém.**

**Tohle je SONDA, ne rozhodná noc** — uživatel 08.09.: "pak nachystej a
spusť kratší běh na to." Cíl je směr napříč rychlostí soupeře, ne přesné
číslo pro každou rasu.

## Konfigurace

* **Mode 12** (`cageAwareAdvanceArm` (B) samotné, PROTI PLACEBU — stejné
  hledání bez kritéria, ne proti vypnutému rameni).
* **Dva NOVÉ matchupy**, dwarf (my) vždy jako nositel ramene:
  * **matchup 1, dw-we** (soupeř wood-elf — rychlý, agilní, silná přihrávka)
  * **matchup 0, dw-sk** (soupeř skaven — nejrychlejší, křehký)
* **dw-dw (matchup 2) se ZNOVU NESPOUŠTÍ** — výsledek z 30.-31.08. už
  existuje (2400 párů) a slouží jako kotva pro pomalý konec spektra.
* **PAIRS = 60 na matchup** (sonda, ne rozhodná síla).
* **Tempo změřeno 08.09.** (4 páry každý, jedno vlákno): dw-we 77,8 s/pár,
  dw-sk 89,1 s/pár. Při WORKERS=4, sekvenčně: dw-we ~19 min, dw-sk ~22 min,
  **celkem ~41 min**.
* Cage gate: mode 12 srovnává rameno (B) proti PLACEBU (totéž hledání bez
  kritéria klece), ne proti vypnutému — izoluje čistě (B), stejně jako
  30.-31.08. běh.

## Co se čte jako rozhodné

**Ne** jedna souhrnná delta přes obě rasy — přesně to by predikci nepotvrdilo
ani nevyvrátilo (stejná past jako T4.4). Rozhodné je **srovnání tří bodů
podél rychlosti soupeře**:

| matchup | rychlost soupeře | delta |
|---|---|---|
| dw-dw (hotovo) | pomalý (MA3-5, žádná agilita) | +0,0242 ± 0,0086 |
| dw-we (nové) | rychlý, agilní (MA7+, Dodge) | ? |
| dw-sk (nové) | nejrychlejší, křehký (MA7-9) | ? |

Predikce z 26.08. by podpořilo: delta u dw-we a dw-sk **blízko nule nebo
záporná**, na rozdíl od jistě kladné dw-dw. Predikci by vyvrátilo: delta
zůstává kladná i u rychlých ras.

## Předregistrované předpovědi *(strojově kontrolované, `.preds`)*

Standardní pole se čtou zvlášť pro každý matchup.

| | dw-we čekám | dw-sk čekám | proč |
|---|---|---|---|
| leak | 0 | 0 | jinak se nic nečte |
| arm_acted | ≥ 0,90 | ≥ 0,90 | rameno se v cage situacích nabízí skoro pořád (dw-dw mělo 100 %) |
| n_nonzero | 0,20–0,65 | 0,20–0,65 | široké — dw-dw vyšlo 52,4 %, nová rasa může vyjít jinak |
| delta (dvoustranná) | −0,15 až +0,10 | −0,15 až +0,10 | asymetrické směrem dolů — predikce čeká pokles/otočení, ne stejný nebo vyšší zisk jako dw-dw |
| delta_1s | polovina | polovina | totéž jednostranně |

## Pořadí čtení výsledku

1. `MOVED WITHOUT THE ARM ACTING` = 0? Ne ⇒ konec.
2. `arm acted in N/M`.
3. `n_nonzero`.
4. Delta KAŽDÉHO matchupu zvlášť.
5. **Teprve pak srovnání všech tří bodů** (dw-dw, dw-we, dw-sk) — to je
   odpověď na uživatelovu predikci z 26.08., ne jednotlivá čísla samotná.

⚠️ **60 párů na matchup je slabá síla** (SE u dw-we/dw-sk čekám v řádu 0,05-0,08
podle tempa čtyřpárového testu, kde delta vyšla ±0,11-0,22 SE). Tohle
rozhodne SMĚR, ne přesné číslo — pokud vyjde nejednoznačně, čeká na
doměření s víc páry, ne na okamžité rozhodnutí.
