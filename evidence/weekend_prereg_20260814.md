# PŘEDREGISTRACE VÍKENDOVÉHO BĚHU — 14.–17.08.2026

**Zapsáno PŘED spuštěním.** Účel je jediný: aby se výsledek nedal číst zpětně.
Prahy, směry a pádové podmínky platí tak, jak stojí tady; kdo je bude chtít
v pondělí měnit, mění je proti zapsanému textu.

> **Proč to vůbec píšu.** A/B brány klece (13.08., 1500 párů) vyšlo neprůkazně
> a strávili jsme den dohadováním, co to znamená — protože předem nebylo
> napsané, co by znamenalo „prošlo". A visí nad tím
> **otevřená otázka č. 1**: brána zlepšila skoro všechny procesní kontroly
> a chess se nehnul. ⇒ **Kontroly nejsou důkaz. Rozhoduje chess.**

---

## 0. ROZPOČET A POJISTKY

* Kapacita: pá 18:00 → po 07:00 ≈ **61 h**. Jedno A/B (1500 párů × 3 matchupy)
  = **7 h** (měřeno 13.08.). ⇒ vejde se 5–6 běhů včetně analýz.
* **Šumové dno: ±5,3 pp na 400 párech** (null-test 12.08.) ⇒ SE ≈ 0,026.
  Na 1500 párech SE ≈ 0,0134. **SE se stejně počítá ze vzorku, ne z přepočtu.**
* Harness je **deterministický** (ověřeno bit-identicky 13.08.) ⇒ srovnání je
  párové hra po hře, ne dvě nezávislé sady.
* Každý běh: lockfile + marker + kontrola běžícího interpretu. Doběhová
  podmínka **musí grepovat řetězec, který skript opravdu tiskne** — na tuhle
  past jsme dnes narazili (`BIG_PARTIAL` u hotového korpusu).

## 0.1 CO MUSÍ BÝT HOTOVO, NEŽ BĚH ZAČNE

| | | stav |
|---|---|---|
| P9a | zákaz odsunu nosiče do endzony, kterou útočí | ⏳ dnes |
| P2+P9c | blok na pollutera + výběr blokujícího podle geometrie odsunu | ⏳ dnes |
| — | ověření hand-offu na kontrolách doběhlo a nic nezhoršilo | ⏳ ~11:00 |
| — | testy zelené, změny commitnuté a pushnuté | ⏳ |

⚠️ **Když P2+P9c do pátku 18:00 nebude hotové a otestované, běh 1 se NESPOUŠTÍ**
a jede se rovnou běh 2. Nedodělaná změna do nočního A/B nepatří — to je
poučení z brány klece, která se měřila s vypnutou polovinou.

---

## BĚH 1 — P2+P9c: čistit roh blokem, a tím správným *(hlavní)*

### Hypotéza
Sražení hráče, který špiní roh klece, **cíleným blokem** (ne blitzem, ne víc
bitím obecně) zvýší podíl čistých rohů a tím dojezd drivů.

### Mechanismus a proč čekáme, že to zabere
* Adresný úder na pollutera: špinavé rohy v N+1 **0,27 vs 1,00** (−22,9σ,
  n=3864), Δx nosiče **+1,62 vs +0,76** (+9,7σ) — Fable 14.08.
* Špinavý roh nechaný přes soupeřovo kolo = **ztracené tělo**: 94,3 %
  nedostupné na začátku N+1.
* ⚠️ **Obecné bloky čistotu ZHORŠUJÍ** (−4,5σ) ⇒ rameno **nesmí** zvyšovat
  celkový počet bloků, jen je přesměrovat.

### Rameno
Kandidát: při volbě bloku dostane přednost polluter, u kterého existuje
volné stojící tělo, z jehož pozice aspoň jedno ze tří odsunových polí
pollutera **odklidí od rohu** a **nepřiblíží k nosiči**. Blokující se vybírá
podle téhle geometrie, ne podle dostupnosti. Baseline: dnešní chování.

### Strop účinku — poctivě, PŘEDEM
Pravidlo se dá použít jen na **39,4 %** polluterů (P9c, 3000 her, n=5089).
Ne na 61,1 %, jak vycházelo z Fableho hrubšího kritéria — rozdíl **21,7 pp**
jsou případy, kdy udeřit lze, ale odsun pollutera od rohu neodklidí.
⇒ **Nečekat velký efekt.** Kdyby vyšel velký, je to podezřelé, ne radost.

### Metrika a práh *(pre-registrováno)*
* **Primární: párová delta chess na `dw-we` a `dw-sk`, 1500 párů.**
* **PROŠLO:** obě trpasličí ramena ≥ 0 a aspoň jedno **≥ +0,03 (≈ +2,2 SE)**.
* **ZAMÍTNUTO:** kterékoli trpasličí rameno **≤ −0,03**.
* Mezi tím: **NEROZHODNUTO** — a to se zapíše jako neúspěch, ne jako naděje.

### Předregistrované předpovědi kontrol *(ať se to nedá číst zpětně)*
| | čekám |
|---|---|
| K29 čisté rohy | **nahoru** — to je mechanismus |
| K33 bloky na kolo | **beze změny** ±2 pp; růst = rameno dělá něco jiného, než má |
| K34 REACH0=0 | **nahoru nebo beze změny**; pokles = odsuny tlačí soupeře k nosiči ⇒ P9c nefunguje |
| K31 idle těla | **dolů** — těla dostávají úkol |
| K9a tempo | beze změny; pokles = platíme rohy tempem |

### Falzifikátor
Když K29 stoupne a chess se nehne, je to **třetí případ** téhož vzorce
(brána klece, balík G) a **otevřená otázka č. 1 se povyšuje nad všechnu
další doktrinální práci.** Pak už není obhajitelné vyrábět další pravidla
podle kontrol, o kterých nevíme, že k něčemu jsou.

---

## BĚH 2 — blitz: roh, nebo zeď? *(otázka, kterou Fable NEROZHODL)*

Observačně je rozdíl nula: Δx v N+1 po blitzi na roh a do zdi je po fázích
prakticky totožný (+1,36/+2,07/+3,13 vs +1,26/+2,16/+3,35). Ale srovnání nese
**selekci** — kola, kdy engine blitzuje roh, nejsou náhodná. **Odpoví jen A/B.**

* Rameno: blitz nikdy nemíří na roh, když polluter jde srazit blokem zdarma.
  Míří na zeď kupředu.
* Opora: **45,5 %** dnešních blitzů na roh padlo v kolech, kde blok zdarma šel;
  blitz na roh stojí **~0,7 pole** tempa v témž kole (−6,4σ) a nekupuje nic
  měřitelného potom.
* Práh: **stejný jako běh 1.**
* ⚠️ **Běh 2 je částečně vnořený do běhu 1.** Když poběží 1 první a projde,
  musí být 2 postavený **nad** ramenem 1, ne nad baseline — jinak se měří
  dvakrát totéž. Když 1 neprojde, jede 2 nad baseline.

---

## BĚH 3 — P9a samostatně *(jen když zbude čas)*

Zákaz odsunu nosiče do endzony, kterou útočí. **Nepotřebuje A/B** — je to
striktní dominance, ne kompromis, a 8 darovaných TD ve 3000 hrách je dost.
Ověřuje se na kontrolách: darované TD musí klesnout na **0**. Sem se dává jen
tehdy, kdy by stroj jinak stál.

---

## CO SE V PONDĚLÍ ZAPÍŠE BEZ OHLEDU NA VÝSLEDEK

1. Verdikt proti prahu **napsanému výše**, ne proti dojmu.
2. Pohyb všech pěti kontrol proti předpovědi — **včetně těch, co nevyšly.**
3. Kdyby vyšlo NEROZHODNUTO: **kolik párů by bylo potřeba** na změřený efekt,
   a jestli se to vůbec vyplatí.
4. Do trvalé knihy `evidence/task_queue.md`, se stavem a commitem.

## ZNÁMÁ OMEZENÍ, KTERÁ VÝSLEDEK NESMÍ PŘEBÍT

* **Soupeřova AI nehraje proti našim slabinám cíleně** ⇒ naměřená četnost chyb
  je **podlaha, ne strop**.
* **Snímek je začátek kola** — pořadí akcí uvnitř kola může vzor zničit dřív;
  P9c je proto **horní mez proveditelnosti**, ne záruka.
* **44,2 % odsunových polí je obsazených** ⇒ odsun často řetězí a hýbe
  i našimi těly. Do doktríny to zatím zapracované NENÍ.
* Sdílený limit pass/hand-off (P7) dělá hbité rasy slabšími, než jsou.
