# PLÁN ZMĚN OD 21.08.2026

Sestaveno 20.08. večer, kdy je engine **zmrazený** běžící nocí P40.
Řadí se podle **dvou cílů klece** *(spec 15.0b′)*, ne podle pořadí vzniku.

---

## ⛔ NEJDŘÍV: JEDNA ZMĚNA ZNEPLATNÍ VŠECHNO PŘEDCHOZÍ

**P45 je zásah do akční ekonomiky**, tedy do každé hry. ⇒ **Po ní neplatí
žádné dosavadní číslo:** σ-tabulka, P38 (+0,0827), noc P40, všechny stropy
z korpusu. **Nejsou špatné — jsou z jiného enginu.**

⇒ **Pořadí je proto dané a nedá se přeskládat:**

```
  1. P45 + vstávání 4+      (oprava pravidel, engine)
  2. X3 — makra s pořadím do logu   (engine, POZOROVACÍ)
  3. REBUILD + testy
  4. NOVÝ KORPUS            ← už s pořadím maker
  5. teprve pak ostatní změny, každá měřená proti NOVÉMU korpusu
```

⚠️ **X3 musí být PŘED korpusem, ne po něm** *(uživatel 20.08. chtěl „hned po
korpusu" — upraveno)*: `TurnLog` nemá makra vůbec, takže **X3 je změna
enginu**. Po korpusu by se korpus musel sbírat **znovu**.

⭐ **A batchovat X3 s P45 se SMÍ**, ačkoli dvě změny před jedním korpusem
obvykle ne: **X3 je čistě pozorovací** — jen zapisuje, co se stalo, do hry
nesahá ⇒ **nemůže zamaskovat účinek P45**.

⇒ **Co X3 rozhodne:** kolik kol zahraje **CAGE PŘED ADVANCE**. `expandCage`
kotví rohy na `carrier.position` **v okamžiku provedení**, a pořadí maker
nikdo nevynucuje ⇒ zahraje-li se CAGE dřív, **rohy se postaví kolem starého
pole**. To je kandidát na **65,9 % rozpadů klece**.

⚠️ **Sběr korpusu trvá hodiny** ⇒ pustit ho **hned po P45**, ne až po dalších
opravách. Zatímco běží, jde psát kód, ne měřit.

---

## 1. 🛡️ P45 — ležící nesmí vzít Block akci *(a dvě věci k tomu)*

**Doloženo textem obou edic 20.08.** *(CRP ř. 46 natvrdo; BB2016 přes Jump Up,
která to právo teprve uděluje — obě edice se **shodují**)*.

| # | co | kde |
|---|---|---|
| **a** | ležící **nedostane BLOCK** — postavení je pohyb, takže postavit se a udeřit jde **jen blitzem** | `rules_engine.cpp` generace akcí + `resolveStandUp` musí spotřebovat akci |
| **b** | ⭐ **Jump Up** to právo **uděluje**, ale za **hod AG +2**, a při selhání **Block akce propadne a hráč zůstane ležet** | tamtéž |
| **c** | při **MA < 3** se hází **4+** na postavení; po úspěchu **žádný další pohyb kromě GFI** | `move_handler.cpp:319` vrací dnes prostě `fail()` |

**Testy, které musí vzniknout:**
* ležící bez Jump Up **nemá v nabídce BLOCK**;
* ležící **s** Jump Up ho má, ale s **hodem**, a **neúspěch = propadlá akce**;
* **Treeman (MA 2)** se **postaví na 4+** — dnes se nepostaví **nikdy**
  *(změřeno: 911 sražení, vstal v **0**)*;
* po postavení na 4+ **nesmí dál chodit**.

**Co to změní ve hře:** dnes **dáváme prakticky Jump Up každému**, takže
soupeř umí za kolo sundat **víc rohů**, než pravidla dovolují. ⇒ Kandidát na
vysvětlení, proč se klec nevyplácí a proč **brána klece 18.08. ŠKODILA**.

---

## 2. NOVÝ KORPUS

`run_corpus_baseline.sh`, 3 000 her. **Vše ostatní na něj čeká.**
⚠️ Do jeho doběhnutí **nedělat žádné závěry z čísel** — ta stará platí pro
starý engine a nová ještě nejsou.

---

## 3. 🛡️ Ochrana nosiče *(70,1 % ztrát míče je blitz na nosiče)*

| pořadí | co | poznámka |
|---|---|---|
| **P37** | `carrierIsBlitzable` **nezná GFI** *(`macro_actions.cpp:1162`)* — v **6,3 %** kol tvrdí bezpečno, kde soupeř dosáhne | rozhoduje, **jestli si nosič nechá pohyb v záloze**; rameno + čítač, per strana |
| **P44** | **follow-up je povinný** — `noFollowUp` existuje, ale **oba volající** berou default `false` | udělat **volitelný**, rozhodovat podle toho, **zůstane-li blitzující volný** *(pak může být **roh klece**)*; ⚠️ u **Frenzy zůstává povinný** |
| **P42** | rameno k zákazu **„nosič nekončí kolo u stojícího soupeře"** | kontrola **K38 už stojí** (87,7 %); strop **3,0 % kol ≈ 0,74 ztráty míče/zápas** |

---

## 4. 🏃 Dojít co nejdál

| pořadí | co | poznámka |
|---|---|---|
| **P40** | přečíst noc *(placebo mode 7)* | ⚠️ **na STARÉM enginu** — číst hned ráno, než se P45 nasadí, ať zůstane vnitřně konzistentní s P38 |
| **P46** | **těla klece nenásledují nosiče do boku** — rameno už 2D hledá, ale **jen pro nosiče** | `expandReposition`; **26,7 % našich kol** má rovně zablokováno a do boku volno |
| **P47** | **kde je hranice „skórovat hned vs posunout klec"** | spec 15.0d dala **obě větve**, ne hranici; ⚠️ engine nezdržuje schválně ⇒ hrozí **vyhladovělá metrika** |
| **P39** | nosič se neaktivuje | P40 to možná rozhodne |

---

## 5. Provoz — co se od zítřka mění

* ⏰ **noc se spouští POZDĚ ODPOLEDNE**, po commitnutí denní práce, a
  **nejpozději hodinu před koncem**, ať zbude čas ověřit, že se rozjela
  *(`postav → otestuj → commitni → spusť → zkontroluj`)*;
* ✅ **T2.15 je hotová** ⇒ použít **`CHUNKS=40 WORKERS=8`**
  *(6 800 = 40 × 170; ⚠️ `SHARDS×4 = 32` **NEDĚLÍ**)*;
* 📅 **pátek = delší okno** ⇒ jediné legitimní využití je **VÍC PÁRŮ**
  *(2 pp → 5 300, 1 pp → 21 200)*, ne naskládání otázek.

---

## ⚠️ Co se NEDĚLÁ a proč

* **P32** *(klec jede jen rovně)* — **mrtvý kód**, plánovač se nespouští
  (`NOT_CONSULTED` 100 %) a brána byla zamítnuta. Žije jako **P46**.
* **P41** — **zamítnuto**, byl to artefakt kontroly.
* **P43** *(Gutter Runner + Jump Up)* — **odloženo** na fázi změn v rosterech.
* **obranná větev** *(T1.11 sloupce → L)* — odblokovaná, ale **stroj patří kleci**.
