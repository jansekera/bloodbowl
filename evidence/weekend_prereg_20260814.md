# PŘEDREGISTRACE VÍKENDOVÉHO BĚHU — 14.–17.08.2026

**Verze 2** (14.08. dopoledne). Přepsáno po nálezech P0.7, P9, P10a — pořadí
běhů se proti verzi 1 změnilo a P2 spadla z první pozice na třetí.

**Zapsáno PŘED spuštěním.** Prahy, směry a pádové podmínky platí tak, jak stojí
tady; kdo je bude chtít v pondělí měnit, mění je proti zapsanému textu.

> **Proč to píšu.** A/B brány klece (13.08., 1500 párů) vyšlo neprůkazně
> a strávili jsme den dohadováním, co to znamená — protože předem nebylo
> napsané, co by znamenalo „prošlo". A visí nad tím **otevřená otázka č. 1**:
> brána zlepšila skoro všechny procesní kontroly a chess se nehnul.
> ⇒ **Kontroly nejsou důkaz. Rozhoduje chess.**

---

## 0. ROZPOČET A POJISTKY

* Kapacita pá 18:00 → po 07:00 ≈ **61 h**; jedno A/B (1500 párů × 3 matchupy)
  = **7 h** ⇒ 5–6 běhů včetně analýz.
* **Šumové dno ±5,3 pp na 400 párech** ⇒ SE ≈ 0,026; na 1500 párech ≈ 0,0134.
  SE se stejně počítá **ze vzorku**.
* Harness je **deterministický** ⇒ srovnání párové hra po hře.
* Doběhová podmínka **musí grepovat řetězec, který skript opravdu tiskne**.

## 0.1 CO MUSÍ BÝT HOTOVO, NEŽ BĚH ZAČNE

| | | stav |
|---|---|---|
| **P9a rozšířené** *(tři případy dominance, viz níže)* | ⏳ dnes, build po doběhu sběru |
| P10a | jeden člen v listové evaluaci | ⏳ dnes |
| — | ověření hand-offu doběhlo a nic nezhoršilo | ⏳ ~11:00 |
| — | testy zelené, commitnuto, pushnuto | ⏳ |

⚠️ **Nedodělaná změna do nočního A/B nepatří** — poučení z brány klece, která
se měřila s vypnutou polovinou. Co není hotové v pátek 18:00, se přeskakuje.

---

## MIMO A/B — P9a: tři případy, kde bereme striktně horší pole

**Nepotřebuje A/B, je to dominance, ne kompromis.** Ověřuje se na kontrolách.

1. **Odsun nosiče do endzony, kterou útočí** — 8 darovaných TD ve 3000 hrách.
2. **Řetěz přes naše tělo, když vedle je pole se soupeřem** — řetěz do soupeře
   posune **dva jejich** a nás nestojí nic.
3. **Řetěz přes náš roh klece, když vedle je řadové tělo** — roh je dražší
   (P0.7: vyklizený roh stojí **6–9 pp** držení míče).

**Pádová podmínka:** darované TD **na nulu**, rozebrané vlastní rohy dolů,
ostatní kontroly beze změny. Kdyby se hnulo chess kterýmkoli směrem o víc než
šum, je to podezřelé — tyhle tři případy jsou příliš vzácné na velký efekt.

---

## BĚH 0 — P11: neskórovat dřív, než je čas *(přidáno 14.08., NEJSILNĚJŠÍ kandidát)*

### Proč je to nejsilnější, co dnes máme
*(uživatel: „kdyby Runner s míčem utekl a skóroval dříve, je to také špatně,
protože soupeř dostane čas na re-TD"; a „ujet víc polí není špatně — ale je
špatně skórovat dříve".)*

Engine **skóruje, jakmile může**: `SCORE` má prior 100, nejvyšší ze všech
maker; `+0,4` za „safe walk-in" a `+0,8` v posledním kole nemají podmínku na
zbývající kola soupeře. **Zdržovací člen neexistuje.**

⇒ A náš plán je **1:0**, ne 2:1 — trpaslík neskóruje v 65–82 % zápasů
a nejčastější výsledek je 0:0. **Když už skórujeme, je ten jediný TD celý
zápas.** Dát ho v kole 5 místo v kole 8 mění jistou výhru na minci.

⚠️ **Míří to přímo na chess, ne na procesní kontrolu** — a to je přesně to,
co otevřená otázka č. 1 od změny žádá.

### Rameno
`SCORE` a odměny za dosažitelnou endzonu dostanou podmínku na **kola zbývající
soupeři**. Držet míč před čárou a překročit ji co nejpozději. Baseline: dnešní
chování.

### Riziko, které se musí napsat předem
**Odložený TD může být žádný TD.** Kdo čeká, může přijít o míč a skončit 0:0
místo 1:0. Rameno proto **nesmí** být „nikdy neskóruj dřív", ale „neskóruj
dřív, **když míč držíš bezpečně**". Když vyjde záporně, je to poctivý výsledek
a doktrína záporné rezervy dostane první skutečný protidůkaz od 10.08.

### Metrika a práh
* Primární: **párová delta chess na `dw-we` a `dw-sk`, 1500 párů**, prahy jako
  u ostatních běhů (≥ +0,03 prošlo · ≤ −0,03 zamítnuto · mezi tím nerozhodnuto).
* Doplňkově zapsat: **kolo, ve kterém padl náš TD**, a **kolik kol zbylo
  soupeři** — před i po. Když se rozdělení nehne, rameno se nechytlo.
* A **podíl drivů, které skončily 0:0 místo TD** — to je ta cena.

---

## BĚH 1 — P10a: sražení nosiče se musí vyplatit

### Proč postoupil na první místo
Všechny tři členy, které listová evaluace o soupeřově nosiči zná, odměňují
**stání vedle něj** a visí na `ball.isHeld` — sražením zmizí naráz
(−0,24 markování, −0,10 lajna, −0,12 contain). Nastoupí místo nich
`heuristic -= 0.1  // loose ball is bad` (`macro_mcts.cpp:762`), **který
nerozlišuje „upustili jsme ho" od „právě jsme ho soupeři vyrazili z ruky".**

Bilance členů, které se mění (soupeřův nosič, 3 naše TZ, AG4):

| | před sražením | po | Δ |
|---|---|---|---|
| uprostřed hřiště | +0,13 | −0,02 | **−0,15** |
| u lajny (y=2) | +0,23 | −0,02 | **−0,25** |
| může skórovat | −0,31 | −0,02 | +0,29 ✅ |

⇒ **Čím blíž je soupeř skórování, tím víc heuristika blok chce; uprostřed
hřiště se mu brání.** A S7 boxing-in = **32,4 % našich kol** je právě ten střed.

**Proč před P2:** P2 staví novou doktrínu nad evaluací, která úspěch té
doktríny hodnotí záporně. Opravit tohle je levnější a míří to **pod** ni.

### Rameno — ⚠️ PŘEPSÁNO 14.08. po uživatelově námitce

Původní znění bylo *„volný míč se přestane hodnotit jako paušálně špatný"*.
**To by vyrobilo pravidlo „bij nosiče vždycky", a to je proti Wardancerovi
špatně.** Uživatel: *„zkontroluj před blitz Wardancera na balon, že máš
v záloze druhého pro pickup a třetího pro zablokování cesty k uzmutému
balonu."*

⇒ **Vyražený míč je zisk jen tehdy, když ho posbíráme.** Jinak jen vyrobíme
volný míč uprostřed hřiště a dáme ho rychlejšímu týmu — a trpaslík je
v souboji o volný míč nejhorší možná rasa (MA4, AG2 u většiny těl).
⇒ `loose ball is bad` **není nesmysl, je to správné pravidlo se špatnou
podmínkou**: platí, když scramble prohrajeme, neplatí, když ho vyhrajeme.

**Rameno tedy zní: sražení nosiče se odmění tehdy, když scramble vyhrajeme** —
a to je **rozpočet tří těl**: ① kdo srazí · ② kdo sebere · ③ kdo zavře cestu.

⭐ **Pravidlo je bezpodmínečné; rasa soupeře o něm nerozhoduje.** Uživatel je
řekl dvakrát a pokaždé stejně — u Longbearda proti Gutter Runnerovi jako
součást zadání (*„a navíc jsou kolem naši"*), u Wardancera jako kontrolu.
Rychlost soupeře neurčuje, **jestli** pravidlo platí, jen **jak těsně se ta
trojice počítá**: proti Wardancerovi musí být třetí tělo blíž a cesta
zavřenější, protože je na míči dřív.
Dnešní člen se ptá jen na **našeho** nejbližšího (`nearestDist`, max +0,08)
a nikdy na to, **kdo je blíž — my, nebo oni**; tělo zavírající cestu
nemodeluje vůbec.

Baseline: dnešní chování.

⚠️ Tím se běh 1 dotýká **tří těl na jednu akci** a spadá pod zákaz *„nesmíme
otevřít prostor jinde"*. Rozpočet je proto součástí ramene, ne jeho vedlejší
efekt: když třetí tělo není volné, pravidlo se **nespustí**.

### Metrika a práh *(pre-registrováno)*
* Primární: **párová delta chess na `dw-we` a `dw-sk`, 1500 párů.**
* **PROŠLO:** obě trpasličí ramena ≥ 0 a aspoň jedno **≥ +0,03 (≈ +2,2 SE)**.
* **ZAMÍTNUTO:** kterékoli trpasličí rameno **≤ −0,03**.
* Mezi tím **NEROZHODNUTO** — zapíše se jako neúspěch, ne jako naděje.

### Předregistrované předpovědi kontrol
| | čekám |
|---|---|
| bloky na nosiče | **nahoru** — to je mechanismus |
| K33 bloky celkem | ±2 pp; velký růst = rameno bije všechno, ne nosiče |
| **volných míčů získaných NÁMI / vyrobených** | **nahoru** — tohle je ta podmínka; když roste jen jmenovatel, rameno vyrábí scrambly a prohrává je |
| DEAD/hru soupeře | nahoru nebo beze změny |
| K9a tempo | beze změny |
| ztráta míče soupeřem | **nahoru** |

⚠️ Je to **listová evaluace**, hledání ji může přebít. Malý nebo nulový efekt
proto **nevyvrací mechanismus**, jen říká, že search si poradil sám.

---

## BĚH 2 — blitz: roh, nebo zeď? *(Fable NEROZHODL)*

Observačně je rozdíl nula (Δx v N+1 po fázích +1,36/+2,07/+3,13 vs
+1,26/+2,16/+3,35), ale srovnání nese **selekci** — kola, kdy engine blitzuje
roh, nejsou náhodná. **Odpoví jen A/B.**

* Rameno: blitz nemíří na roh, když polluter jde srazit **blokem zdarma**;
  míří na zeď kupředu.
* Opora: **45,5 %** dnešních blitzů na roh padlo v kolech, kde blok zdarma šel;
  blitz na roh stojí **~0,7 pole** tempa (−6,4σ) a nekupuje nic měřitelného.
* Práh: **stejný jako běh 1.**

---

## BĚH 3 — P2+P9c: čistit roh blokem, a tím správným *(spadlo z prvního místa)*

### Pořadí povinností *(přepsáno 14.08. podle uživatele)*
1. **Udeř na pollutera** — v **72,8 %** ho kostky složí a ležící roh nešpiní
   (`threatens()` vrací true jen pro stojícího). Tohle je hlavní páka.
2. **Když zůstane stát (27,2 %), pošli ho PRYČ od rohu** — jediná páka, kterou
   tam máme, a je zadarmo. *Sražení je kostka, směr je volba; frekvencí se to
   poměřovat nedá.*
3. **Nikdy ne přes vlastní roh** (P0.7) · **do soupeře radši než do nás** (P9a).

### Strop účinku — poctivě, PŘEDEM
Pravidlo lze použít na **39,4 %** polluterů (P9c, n=5089) — ne na 61,1 %, jak
vycházelo z hrubšího kritéria. **Nečekat velký efekt; kdyby vyšel, je to
podezřelé.**

### Předregistrované předpovědi kontrol
| | čekám |
|---|---|
| K29 čisté rohy | **nahoru** |
| K33 bloky | ±2 pp; růst = rameno dělá něco jiného |
| K34 REACH0=0 | nahoru nebo beze změny; **pokles = odsuny tlačí soupeře k nosiči** |
| K31 idle těla | dolů |
| K9a tempo | beze změny; pokles = platíme rohy tempem |

### ⛔ Zákaz, který platí nad celým během *(uživatel 14.08.)*
*„Musí být situace nachystaná — nesmíme se hnát za jedním cílem a otevřít
prostor jinde."* Kritérium se čte nad **současnou** deskou, ne nad tou, kam
bychom někoho došli. **Dosažitelnost není povinnost.**

---

## FALZIFIKÁTOR NAD CELÝM VÍKENDEM

Když se kontroly zlepší a chess se nehne, je to **třetí případ** (po bráně
klece a balíku G) a **otevřená otázka č. 1 se povyšuje nad všechnu další
doktrinální práci.** Pak přestává být obhajitelné vyrábět pravidla podle
kontrol, o kterých nevíme, že k něčemu jsou.

## CO SE V PONDĚLÍ ZAPÍŠE BEZ OHLEDU NA VÝSLEDEK

1. Verdikt proti prahu **napsanému výše**, ne proti dojmu.
2. Pohyb všech kontrol proti předpovědi — **včetně těch, co nevyšly.**
3. Při NEROZHODNUTO: kolik párů by bylo potřeba, a jestli se to vyplatí.
4. Do trvalé knihy `evidence/task_queue.md`, se stavem a commitem.

## ZNÁMÁ OMEZENÍ, KTERÁ VÝSLEDEK NESMÍ PŘEBÍT

* **Soupeřova AI nehraje proti našim slabinám cíleně** ⇒ naměřená četnost chyb
  je **podlaha, ne strop**.
* **Snímek je začátek kola** ⇒ P9c je horní mez proveditelnosti, ne záruka.
* **44,2 % odsunových polí je obsazených** ⇒ odsun často řetězí a hýbe i našimi
  těly.
* **Klastrovaná pozorování** u P0.7 ⇒ směr, ne σ.
* Sdílený limit pass/hand-off (P7) dělá hbité rasy slabšími, než jsou.
