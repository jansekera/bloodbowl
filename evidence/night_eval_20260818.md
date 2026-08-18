# VYHODNOCENÍ NOCI 17.→18.08.2026

Předregistrace: `evidence/night_prereg_20260817.md`. Čteno **v pořadí, které tam stojí.**

## 1. Běhy — oba doběhly

| běh | stav | doklad |
|---|---|---|
| A/B brána klece s CRN, `dw-we`, 8×750 = **6 000 párů** | ✅ `AB_DONE (8/8)`, `NIGHT_DONE` | `gate_crn_20260817/chain.log` |
| smoke test seedování (mode 2, 50 párů) | ✅ `+0,0000 ± 0,0000`, `n_nonzero 0` | `gate_crn_20260817/control_mode2/run.log` |
| korpus baseline 3 000 her | ✅ `COLLECT_DONE`, otisk enginu `5e5ab352` | `corpus_baseline_20260817/chain.log` |

Jediný POKUS, žádný FAIL, žádný restart. Zámek, úklid dětí i preflight ze 17.08. fungovaly.

## 2. Výsledek — v pre-registrovaném pořadí

| # | co | hodnota | verdikt |
|---|---|---|---|
| ① | `MOVED WITHOUT THE ARM ACTING` | **0** ve všech 8 shardech | ✅ delta se **smí** číst |
| ② | `arm acted` | **6000/6000 (100 %)** | ✅ plný jmenovatel |
| ③ | `n_nonzero` | **3766/6000 = 62,8 %** | ⚠️ pre-reg čekal **>80 %** — předpověď MIMO; podle vlastního pravidla se **práh NEPOSOUVÁ** |
| ④ | párová delta chess `dw-we` | **−0,0248 ± 0,0068 SE** *(−3,7σ)*, 95 % CI **[−0,038; −0,012]** | ⇒ **≤ −0,015 = BRÁNA ŠKODÍ** |

Shardy: −0,0120 −0,0267 −0,0413 −0,0507 −0,0160 −0,0060 −0,0273 −0,0180 —
**8/8 záporných.** Empirická SE mezi shardy **0,0053** < sdružená **0,0068**
⇒ **žádná overdisperze**, sloučení je legitimní.
Rameno prokazatelně jedná: `cage plans adopted 3,1/hru`.
Attrition beze změny (dwarf surv/11: 10,63 vypnuto vs 10,68 zapnuto).

⇒ **T3.1 ZAMÍTNUTO — potvrzeno, tentokrát s kontrolou.**
⛔ **Není to replikace 13.08.** Mezi běhy jsou 3 commity enginu (P13, oprava hand-offu,
odmítnutí darovaného TD). Odpověď zní *„v dnešním enginu brána škodí"*, ne
*„tehdejší zamítnutí bylo správné"*. ⚠️ Výhrada z 13.08. platí dál: **chybí jí plán
trasy, ne schopnost** (vyměnila tempo za bití a čistotu rohů) — kód se nezahazuje.

⚠️ **Pre-registrovaná předpověď „kolem nuly ⇒ NEROZHODNUTO" je falzifikována
směrem ke škodě.** To je ten mechanismus, kvůli kterému předregistrace vznikla.

## 3. Falzifikátor nástroje SPUSTIL — CRN u všudypřítomného ramene skoro nepomáhá

Předregistrace si vyžádala zapsat i to, kdyby CRN neredukovalo rozptyl.
Srovnání na **stejný počet párů** (750):

| | SE / 750 párů | SD páru |
|---|---|---|
| 13.08. **bez** CRN (`gate_measure_20260813`) | 0,0204 | 0,559 |
| 18.08. **s** CRN | 0,0191 | 0,523 |

⇒ **redukce ~6 %**, předregistrace slibovala **15–25 %**.

⭐ **A ví se proč, což je cennější než to číslo.** Identických párů (delta přesně 0)
vyšlo **37,2 %** — **víc**, než se čekalo. Naivně by to dalo redukci √0,628 ≈ **21 %**.
Dostali jsme 6 %, protože **páry, které se hnou, se hnou VÍC**: podmíněné SD páru je
**0,66** proti nepárové **0,56** (+18 %).

⇒ **PRAVIDLO: rozpočet párů se NESMÍ počítat z podílu identických párů.**
CRN nulové páry vyrobí, ale rozptyl přesune do zbytku; podíl identických párů dává
**horní mez** úspory, ne úsporu. Plánuje se dál podle šumového dna bez CRN
(2 pp ⇒ ~5 300 párů). U **vzácného** ramene zůstává CRN zásadní (mrtvé rameno = exaktní nula).

## 4. ⭐⭐ Co noc ukázala o KONTROLÁCH — spravili jsme spuštění, ne čtení

17.08. se opravil **spouštěč** (zámek, sirotci, `POKUS n`, `night_preflight`) a do
harnessu přibyl **per-pair leak test**. Obojí v noci fungovalo přesně jak mělo.
**Čtecí strana selhala na pěti místech naráz** — viz T2.10–T2.14 ve frontě:

1. ⛔ **ARM blok = falešný poplach.** `run_night_ab.sh:150` grepuje `^  ARM `, což je
   řádek **pouze pro mode 4** (`diag_f1_cage_advance_harness.cpp:528`). V mode 0 takový
   řádek neexistuje ⇒ do `chain.log` se vytisklo **„(harness nic netiskl — stará
   binárka?)"** přesně na místě, které předregistrace označuje za nejcennější ranní
   čtení. Ve skutečnosti test proběhl a byl **čistý 8/8**. Kdo ráno čte odshora, dojde
   k závěru, že noc je nedůvěryhodná — na základě vadného grepu.
2. ⛔ **`head -1` = jen shard 0.** I po opravě vzoru by leak v shardu 5 nikdy neprobublal.
3. ⛔ **Nic neslučuje shardy.** `chain.log` končí `NIGHT DONE` bez jediného čísla.
   6 000 párů existovalo jen jako 8× ±0,019 — **osm jednotlivě neprůkazných čísel**;
   odpověď (−0,0248 ± 0,0068, 8/8 záporných) spočítal ráno člověk. Táž rodina jako
   audit aparátu 13.08.: **snímek se vydává za stav.** A je to přesně krok, kde si
   unavené čtení vybere shard, který se hodí.
4. ⛔ **Dva různé prahy v jedné noci.** Harness tiskne natvrdo `[pre-reg: >= +0.03]`,
   předregistrace na tutéž noc říká **±0,015**. Práh se nikde strojově nevyhodnotí.
5. ⛔ **Preflight neověří, že kontrola vůbec existuje.** Hlídá mtime binárky
   i `libbb_engine.so`, ale ne to, jestli binárka `MOVED WITHOUT THE ARM ACTING` umí
   vytisknout. Kdyby neuměla, noc vypadá normálně a **verdikt stojí na kontrole, která
   neproběhla** — rodina T2.7.

Vedlejší (T2.14): běh měl `CORPUS=0`, takže **dvě ze šesti** pre-registrovaných
předpovědí (K9a tempo dolů, bloky nahoru) byly od začátku **nezodpověditelné**;
a minutá předpověď `n_nonzero` (62,8 % vs >80 %) je informace o rameni — brána sahá
na méně kol, než jsme mysleli — kterou nic nezachytává.

⇒ *Kontrola, kterou nikdo nepřečte, a výsledek, který nikdo nespočítá, se od chybějící
kontroly a chybějícího výsledku neliší.*

## 5. Co se tím odemklo

**Spouštěč P30 je splněn**: `corpus_baseline_20260817_data` (3 000 her, otisk enginu
`5e5ab352`) existuje ⇒ σ-tabulka, od 17.08. **POZASTAVENÁ**, se smí přepočítat
a teprve pak zase smí řadit frontu.
