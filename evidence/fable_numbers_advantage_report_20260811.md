# Početní převaha → skóre: co říkají data (11.08.2026)

Nástroj: `diag_numbers_advantage_20260811.py` · hlavní čísla z korpusu
`diag_replay_mine_20260811b_data/` (120 her, dnešní build, COLLECT_DONE),
srovnání proti `diag_replay_mine_20260811_data/` (build 08:16).
Zadání bylo v průběhu přesměrováno: cíl je rámec **1:0** (vlastní přijímací
drive + obrana), krádež je příležitost, ne plán. Bod 5 proto vykazuji jen
jako rozdělení, těžištěm je otázka *„pomáhá jejich oslabení našemu drivu?"*

**Definice předem** (žádné zpětné prahy): převaha = rozdíl hráčů na hřišti
(hráči mimo jsou z pole odstranění, on-pitch = délka pole) na snímku začátku
kola; „převaha ≥ 2" vs „vyrovnáno" = |rozdíl| ≤ 1; vzdálenosti k endzone po
ose x; koridor odporu = stojící soupeři mezi nosičem a endzone v pásu y±2;
blitz = BLOCK, jemuž v témže kole předcházel MOVE téhož hráče.

---

## 1. Vzniká převaha včas? — Vzniká POZDĚ a NEPŘEŽIJE kickoff

Průběžný rozdíl (my − soupeř) na hřišti, korpus b:

| kolo | on-pitch | stojící | podíl kol s převahou ≥2 |
|---|---|---|---|
| 1 | −0,04 | −0,51 | 0 % |
| 4 | +0,23 | −0,15 | 7 % |
| 8 | +0,42 | +0,90 | 15 % |
| **9 (start 2. půle)** | **+0,09** | **−0,41** | **4 %** |
| 12 | +0,40 | +0,29 | 14 % |
| 16 | +0,81 | +1,94 | 27 % |

* Převaha roste **uvnitř půle** a na jejím konci je největší — a **o poločase
  spadne zpět k nule** (T8→T9: +0,42→+0,09). Totéž v malém dělá každý kickoff
  po TD: chybějící se vracejí (viz §7).
* První kolo s převahou ≥ 2: **medián 7,5**; **66 ze 120 her stav převahy ≥ 2
  nikdy nezažije**. Celkem hrajeme v převaze ≥ 2 jen **13 % svých kol**.

**Odpověď: je co proměňovat jen v úzkém okně na konci půle.** Převaha není
trvalý stav zápasu — je to přechodný stav ocasu půle. Tím padá představa
„hrajeme většinu zápasu proti osmi": v tomto korpusu neplatí (rozpor
s tabulkou G viz §8).

## 2. Verdikty ke třem předem zapsaným hypotézám

**a) Uživatel: „umlácený tým se přestane prát a uteče" — ČÁSTEČNĚ POTVRZENO.**
Soupeř v naší převaze ≥ 2 (jeho kola, b): bloků 0,87 vs 1,32 za kolo (−34 %),
hráčů v kontaktu na konci jeho kola 1,16 vs 1,80, dodgů 0,44 vs 0,33 (+35 %),
hráčů v naší půlce 2,74 vs 3,19. **Kontakt opouští a dodgeuje víc.** ALE:
postup jeho nosiče se nezpomaluje (2,24 vs 1,99 pole/kolo; n=33, ve starším
korpusu 1,14 vs 1,98 — nestabilní, malé n) a nosič od nás neutíká do dálky
(vzdálenost k nejbližšímu stojícímu trpaslíkovi 2,76 vs 3,39 — je BLÍŽ,
protože ho markujeme). Extrémní podoba útěku existuje (situace S2), ale
typická není.

**b) Fable: „v převaze dál lovíme těla místo míče" — VYVRÁCENO.**
V převaze ≥ 2 jdeme po míči VÍC: blok na nosiče v 61 % kol (vs 43 %
vyrovnaně), blitz na nosiče 46 % vs 33 %, nosič markován na konci kola 50 %
vs 41 %, a míč soupeři v našem kole sebereme ve **34 % kol převahy** (vs
21 %). Cíl blitzu se v režimu převahy mění správným směrem.

**c) Uživatel (hlavní): „nesebereme míč, natož abychom došli" —
PRVNÍ STUPEŇ NEPLATÍ, DRUHÝ PLATÍ TVRDĚ.** Míč sebereme 0,92× za hru
(§5) — braní míče funguje. Ale **90 % získání je aritmeticky
nezhodnotitelných**: medián získání v T6 půle, 12,5 pole od jejich endzone,
3 zbývající naše kola; při tempu 1,73 potřebujeme ~7. Krádež tedy skoro
nikdy nemůže být zdrojem TD — což je konzistentní s rámcem 1:0 (krádež
= ukončení jejich drivu, ne začátek našeho).

K bodu 4 (obklíčení): **boxing-in se neděje.** Volná pole kolem jejich
nosiče na konci našeho kola: vyrovnaně průměr 4,2 (medián 4), v převaze ≥ 2
průměr **5,5 (medián 6)** — v převaze má nosič VOLNĚJI (zčásti mechanicky:
na hřišti je méně těl, a ta chybějící jsou jejich).

## 3. ⭐ Hlavní otázka: pomáhá jejich oslabení NAŠEMU přijímacímu drivu?

**Na startu drivu oslabení prakticky neexistuje**: ze 172 našich přijímacích
drivů jich 160 začíná proti plným jedenácti (kickoff soupeře doplní — KO se
vracejí, o poločase téměř všichni; trvale venku zůstávají jen CAS a vyloučení,
ověřeno: 97/100 CAS zůstává mimo, EJECTED 128/128). Skupina „soupeř na startu
o 3+ dole" je **prázdná**. Proto měřím oslabení jako **maximum chybějících
soupeřů BĚHEM drivu**; proti konfoundu délky drivu beru jen drivy začínající
v T1–T2 půle (plná délka):

| max soupeřů dole během drivu | drivů | TD % | tempo nosiče | odpor v koridoru | ztráta míče | kol |
|---|---|---|---|---|---|---|
| 0 | 31 | **6 %** | 2,16 | 4,72 | 71 % | 7,1 |
| 1 | 37 | 11 % | 2,17 | 4,72 | 70 % | 7,2 |
| 2 | 28 | 18 % | 2,28 | 4,26 | 64 % | 7,7 |
| **3+** | **24** | **46 %** | **3,00** | **3,53** | **50 %** | 7,6 |

**Odpověď: ANO, dramaticky.** Když je soupeř během našeho drivu o 3+ dole,
TD dáváme v 46 % (proti 6 % u plné jedenáctky), tempo se z 2,2 zvedne na
3,00 — téměř na potřebných 3,14 — a odpor v koridoru klesne z 4,7 na 3,5.
**Konverze funguje; nefunguje to, že se k ní skoro nedostaneme:**

1. **Stav „3+ dole" nastane jen ve 24 ze 120 plnodélkových přijímacích
   drivů (20 %)** — a protože se buduje mlácením uvnitř téhož drivu, přijde
   pozdě v drivu.
2. **I při 3+ dole polovina drivů ztratí míč.** Přes všechny přijímací
   drivy je ztráta 56 %; příčiny: **soupeřův blok/blitz na našeho nosiče 56×**
   (62 % ztrát), **naše vlastní přihrávka/chytání 21×** (AG2 trpaslíci házejí
   a nechytají!), nezvládnutý pickup 8×, dodge nosiče 2×. Dalších 12 %
   drivů míč vůbec nezvedne.

**Co tedy drive drží:** ne odpor soupeře (ten s oslabením klesá a my ho
umíme umlátit), ale **(a) nechráněný nosič** — jediný soupeřův dodge-through
blitz zabíjí drive i proti třem chybějícím (situace S1), a **(b) tempo
2,2 pod potřebou 3,14**, dokud soupeř není výrazně dole. Pro plán 1:0 to
znamená: ochrana nosiče (obsazené rohy, ne jen zóny) a rychlejší mlácení
NA ZAČÁTKU vlastního drivu jsou páky; krádež a koncovková převaha nejsou.

## 4. Bod 5 (zúženo dle pokynu): rozdělení získání míče

110 získání / 120 her (0,92 na hru). Kolo v půli: medián **T6**
(histogram T3:14 T4:17 T5:22 T6:21 T7:12 T8:23 — hromadí se na konci půle).
Vzdálenost k jejich endzone: medián **12,5 pole** (5–9: 25×, 10–14: 44×,
15–19: 30×, 20+: 8×). Zbývající naše kola: medián 3. **Aritmeticky
nezhodnotitelných (dist/1,73 > zbývající kola): 90 %.** Dál to nerozvíjím.

## 5. Tři konkrétní situace

### S1 — drive umře dva kroky od endzone (g0027, wood-elf vs dwarf, konečné 0:0)
H2T7, elfové **o 3 dole** (hrají v 8), náš přijímací drive od H2T1. Runner
+Block (id 16, MA6) donesl míč na **(2,11) — dvě pole od endzone x=0**.
Jenže stojí tam SÁM: nejbližší stojící trpaslík Longbeard +Guard na (6,11),
Blitzer na (5,10); rohy kolem nosiče prázdné.

```
      01234567890123456789012345   dwarf útočí na x=0, R=nosič
  y 7 .........+x...............
  y 8 .......w...xx.............   w = Wardancer (7,8)
  y 9 ........t+.+.+............
  y10 ....lB.++.................
  y11 ..R..xL.........x.........   R = Runner s míčem (2,11)
```

Wardancer z (7,8): 3× MOVE, dodge na (3,10), **jednokostkový blitz na
nosiče** — push + KNOCKED_DOWN, míč se kutálí, elfí Lineman ho sebere a
odnáší na (4,8). Zápas skončí 0:0. Tři chybějící elfové nepomohli, protože
**jediné, co drive potřeboval, byla obsazená pole kolem nosiče** — přesně
doktrína rohů, která se tu nekonala.

### S2 — útěk v extrému: Gutter Runner v rohu (g0009, skaven vs dwarf 1:1)
H1T5, skaveni o 2 dole (9 na hřišti). Jejich nosič Gutter Runner +Sure Feet
(MA9) stojí s míčem na **(1,2) — zalezlý v rohu vlastní půlky** (skaven
útočí na x=25, tedy couvl až ke své endzone). Kolem něj **8 volných polí**.

```
      01234567890123456789012345   g = nosič GR (1,2)
  y 2 .g........................
  y 3 ......+gg.........L.......
  y 4 ....b.gll...x.............
  y 6 ............R.+...........
  y 7 ...........x.LR...........
```

Nejbližší STOJÍCÍ trpaslík je Runner na (12,6) — **11 polí daleko**; co je
blíž, leží (+ = naši ležící na 6,3 a 7,8-13,10). S MA4–6 proti MA9 tohle
nikdy nedoběhneme. Uživatelova věta „obklíčíme málo — utečou na druhou
stranu" v čisté podobě. Zároveň je vidět cena: skaven tím drive stalluje,
nepostupuje.

### S3 — mediánová krádež: aritmeticky mrtvá (g0080, dwarf vs skaven 1:0)
H2T5: náš Blitzer +Guard+Tackle (id 9) získává míč na **(13,5)** po sérii
bloků (kolem leží 3 sražení Gutter Runneři). Vzdálenost k endzone soupeře
(x=25): **12 polí**. Zbývají **4 naše kola**; potřeba 12/1,73 ≈ 7 kol.

```
      01234567890123456789012345   B(13,5) = náš nosič po krádeži
  y 3 .......L..+....x..........
  y 4 ..........RLlxx...........
  y 5 ......L....+lBg.l.........
  y 6 ............B.+..S........
```

Typické získání míče: uprostřed hřiště, uprostřed druhé půle, po umlácení —
a na TD z něj aritmetika nestačí. (Hru jsme vyhráli 1:0 — gól přišel
z regulérního přijímacího drivu, ne z téhle krádeže.)

## 6. Rozdíl mezi korpusy (nechtěné A/B dnešních oprav)

Směr je u všech ras stejný — dnešní build skóruje víc:

| soupeř | bez našeho skóre (a → b) | naše TD/hru (a → b) |
|---|---|---|
| skaven | 43 % → **20 %** | 0,63 → 0,87 |
| wood-elf | 76 % → 66 % | 0,23 → 0,33 |
| human | 86 % → 76 % | 0,13 → 0,23 |
| orc | 100 % → 90 % | 0,00 → 0,10 |

(n=30 her na rasu — jednotlivé buňky jsou v šumu, konzistentní směr přes
všechny čtyři rasy ale náhoda spíš není.) Dále: podíl kol v převaze ≥ 2
vzrostl 9,4 → 13,0 %, tempo přijímacích drivů proti skavenům 2,42 → 3,18.
Struktura problému se ale NEZMĚNILA: ztráta míče v přijímacích drivech
46 → 56 % (podíl ztrát blokem na nosiče 51 → 63 % — jednokostkové bloky
soupeře nosiče trestají dál), volná pole kolem jejich nosiče v převaze
4,35 → 5,52. Závěry §3 platí v obou korpusech.

## 7. Čeho jsem si všiml, na co ses neptal

1. **Kickoff je resetovací tlačítko převahy.** KO hráči se na kickoffu
   vracejí v 65 % (n=234) — čekal bych ~50 % při hodu 4+; o poločase se
   vrací téměř všichni (z „o 3 dole" na 0 v 11 z 12 přechodů). Stálou
   složku attrition tvoří jen CAS+EJECTED (~1,3 hráče na konci hry). Stojí
   za pravidlovou kontrolu, jestli KO návrat nehází příliš štědře.
2. **AG2 trpaslíci ztrácejí drive přihrávkou/chytáním — 21× v korpusu b**
   (23 % všech ztrát v přijímacích drivech). To je vlastní gól: druhá
   největší příčina ztrát je něco, co vůbec nemusíme dělat.
3. **Rozpor s tabulkou G ze zadání:** tam soupeř na konci průměr 2,82
   chybějících a 54 % her s 3+; v tomto korpusu (poslední snímek hry)
   průměr 1,29 a 19 % (korpus a: 1,22 / 15 %). Nevysvětluji — jiný korpus
   (4800 her) nebo jiný okamžik/metodika měření. Kdyby tabulka G počítala
   stav uprostřed drivu, sedělo by to s §1.
4. **Datové drobnosti:** 82 logů (2,1 %) má dva hráče na jednom poli
   (např. g0027 idx 29: náš Troll Slayer i elfí Treeman na (8,9));
   po TD v T8 vzniká artefaktový log „T9" (ošetřeno, do statistik nevstupuje).
5. **Skaven je jediná rasa, kterou už dnes porážíme skrz přijímací drive**
   (TD 27 % drivů, tempo 3,18) — právě proti němu attrition během drivu
   funguje nejrychleji. Ork je opačný extrém (TD 9 %, tempo 2,15, bez
   skóre 90 %): tam nefunguje ani mlácení, ani tempo.
