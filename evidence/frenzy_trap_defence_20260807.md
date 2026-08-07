# Obrana proti frenzy trapu — plán pro Rat Ogra (bez Blocku)

Zadání uživatele 07.08.: skavení Rat Ogre se přidá **bez Blocku** (aby se
vešel na TV 1300) a musí si „dát pozor na frenzy trap a vždy mít nachystané
3 kostky na block". Tento dokument = rešerše (open web) + dopočitatelný
plán obrany. Souvisí: fronta 14b/14c (roster revize), 7c (asistent před
blokem), 7/7b (kostky z pole, kde se skutečně bloká).

## 0) ⚠️ ROZSAH (upřesnění uživatele 07.08.) — tenhle dokument je o AKCI
Název svádí, ale obsah je z drtivé většiny **rozhodování o konkrétním
bloku** (spočítat oba bloky, jít/nejít, poskládat 3 kostky) = fronta
bod **6b**, dělá se hned.
**Poziční OBRANA před pastí** — nenechat se do ní vůbec navést: kam
Frenzy hráče postavit, aby ho soupeř neměl čím nalákat (volná těla
u cíle, Side Step v dosahu, konfigurace kde je každý blok past) — je
**samostatná, odložená vrstva** (fronta bod 12b, rodina probed exposure).
V tomhle dokumentu je jen naznačená.

## 1) Co říká komunita (rešerše 07.08.)
* **Mechanika pasti:** Frenzy nutí druhý blok, když první skončil Pushed
  nebo Defender Stumbles a oba pořád stojí a sousedí. Druhý blok už se
  hází z JINÉ pozice → **soupeř může mít na druhý blok asistenty, které
  u prvního neměl**. „Můžeš mít výhodný první blok a druhý se ti obrátí."
* **Jak past staví soupeř:** (a) nechá poblíž nemarkovaného hráče, který
  po pushi doasistuje; (b) postaví více těl tak, aby follow-up dostal
  Frenzy hráče do base-to-base s několika soupeři; (c) **Side Step** —
  obránce si sám vybere, kam ho odstrčíš, takže tě vede tam, kam chce
  (do pasti, k autu, nebo pryč od akce).
* **Sideline je dvousečná:** stát 1-2 pole od čáry proti Frenzy hráči je
  nebezpečné (chain push + surf) — pro NÁS je to naopak zbraň.
* **Doporučení komunity:** Block/Wrestle mít DŘÍV než Frenzy (my vědomě
  nemáme); před blokem analyzovat asistence pro OBA bloky; blok vynechat,
  když druhý vypadá špatně; Frenzy dávat na silné hráče.

## 2) Co z toho engine už umí (ověřeno v kódu 07.08.)
* `block_handler.cpp:559-577` — povinný druhý blok, správně podmíněný
  (oba stojí, sousedí); u blitzu navíc kontroluje zbytek pohybu a GFI
  strop, takže bez pohybu se druhý blok nehází. Odpovídá pravidlům.
* `block_handler.cpp:82-87` — **SideStep: obránce si vybírá pole**, tedy
  past je v našem enginu reálná (wood-elf má Wardancera se Side Step!).
* `block_handler.cpp:516` — **Fend** brání follow-upu.
* `feature_extractor.cpp:328-343` — featura **[61] frenzy_trap_risk**
  (moji Frenzy hráči sousedící s 2+ soupeři). Síť tedy vstup MÁ.
* ⇒ **Chybí jen rozhodovací vrstva:** plánovač ani generace maker
  o druhém bloku neví (BLOCK se generuje pouze podle `dice >= 2`
  spočítaných z aktuálních pozic).

## 3) Plán obrany — dopočitatelný, bez kostek předem

### A. OCENIT CELOU SEKVENCI, NE PRVNÍ BLOK (jádro)
Pro každý blok Frenzy hráče F na cíl D spočítat:
1. `dice₁` = kostky teď (dnešní výpočet);
2. **výčet polí, kam může D odletět** — `getPushbackSquares` vrací až 3;
3. pro každé takové pole: pozice F po follow-upu (F vstoupí na původní
   pole D) a `dice₂` = `getBlockDiceCount` **z NOVÝCH pozic** — tedy
   s asistencemi, které tam reálně budou;
4. **kdo vybírá:** normálně útočník (bereme MAX přes pole), ale proti
   **Side Step** vybírá obránce ⇒ tam brát **MIN** (nejhorší případ);
5. **riziko celé sekvence** (bez Blocku jsou špatné 2 stěny ze 6):
   `P(průšvih) = p₁ + (1−p₁)·P(druhý blok nastane)·p₂`,
   kde `pᵢ = (2/6)^n` při n kostkách pro nás, resp. `1−(4/6)^n` proti nám.
   Orientačně bez Blocku: 3 kostky = 3,7 %, 2 = 11 %, 1 = 33 %,
   1 proti = 56 %, 2 proti = 80 %.

### B. TVRDÁ PRAVIDLA PRO BIG GUYE BEZ BLOCKU
1. **Práh kostek se řídí BLOCKEM, ne rolí** (upřesněno 07.08.):
   * **bez Blocku** (Rat Ogre): 1 kostka = 33 %, 2 = 11 %, **3 = 3,7 %**
     ⇒ neblokovat pod 3 kostky. 3 kostky proti ST3 = ST5 + **2 asistenti**
     (7 vs 3) ⇒ přímý odběratel bodu 7c.
   * **s Blockem** (trpasličí Slayeři, Blitzeři): 1 kostka = 17 %,
     **2 kostky = 2,8 %** ⇒ stačí 2, tedy JEDEN asistent (např. Slayer ST3
     vs elf ST3: 1 kostka → s asistentem 4 vs 3 = 2 kostky).
   Formulovat přes skill, ne přes jméno: „blokuj od prahu, kde riziko
   klesne pod ~4 %".
   * **VÝJIMKA — BIG GUY VŽDY 3 KOSTKY** (uživatel 07.08.), i s Blockem.
     Důvody: **Loner** (block_handler.cpp:343-346 — týmový reroll se
     spotřebuje a teprve pak se hází 4+; při neúspěchu je pryč bez efektu,
     takže si big guy blok fakticky neopraví), **aktivační riziko**
     nega-traitů, a **cena kusu** (sražený big guy = nejdražší tělo na
     zemi a magnet na foul). Rat Ogre je z big guyů nejcitlivější:
     bez Blocku 2 kostky = 11 %, a Frenzy to hází dvakrát.
   * ⚑ **PRÁH ALE NENÍ KONSTANTA** (uživatel 07.08.): plyne z toho, co
     jednotlivé stěny znamenají pro KONKRÉTNÍ dvojici. Protipříklady:
     (a) **Gutter Runner s Wrestle + Strip Ball na nosiče bez Sure Hands
     smí i do 2 kostek PROTI** — Wrestle = oba na zem bez turnoveru
     (block_handler.cpp:383-399), Strip Ball shodí míč při pushi
     (:501-511) ⇒ jediná špatná stěna je Attacker Down, tedy ~31 %
     turnover vs ~69 % uvolněný míč; (b) **trpaslík s Blockem smí do
     1 kostky** (17 %, AV9 + Thick Skull, odsun markera je pro grind
     cenný). ⇒ Prahy jsou jen časté případy obecné formule
     „hodnota stěn × rozdělení kostek vs cena turnoveru TEĎ".
2. **Zakázat blok, jehož nejhorší druhý blok je „kostky proti nám"**
   (`dice₂ ≤ −1`) — to je definice pasti.
3. **Proti Side Step cíli požadovat rezervu** — obránce si vybere pole,
   takže se počítá nejhorší varianta, ne průměr.
4. **Asistenti musí pokrývat i pole PO PUSHI**, ne jen současné pole
   obránce — jinak nám na druhý blok „vyprchají". Umístění asistentů
   proto vybírat tak, aby sousedila i s kandidátními push poli
   (dopočitatelné: průnik okolí `D` a okolí `push(D)`).
5. **Preferovat push k postranní čáře** — surf sekvenci ukončí ve prospěch
   (obránec ven, žádný druhý blok). Frenzy se tím z rizika stává zbraň;
   napojení na L-pin doktrínu (fronta bod 12).
6. **Nechat rezervu pohybu u blitzu** — engine druhý blok bez pohybu
   nehází (správně), ale to znamená useknutou sekvenci; rozhodovat
   vědomě, ne náhodou.
7. **Wild Animal (po opravě 0bcf849): 1/6, že akce nevyjde vůbec** —
   patří do ceny každého Ogrova manévru, ne jen kostky bloku.

### C. CO POUŽÍT, NE STAVĚT ZNOVU
* featura [61] frenzy_trap_risk — vstup pro síť už existuje;
* `getPushbackSquares`, `getBlockDiceCount`, `countAssists` — vše hotové;
* sideline penalizace v `scoreMoveAction` (macro_actions.cpp:81).
Nové je jen **složení**: dvoublokový výpočet + jeho zapojení do generace
BLOCK maker (dnes gate `dice >= 2` z aktuálních pozic) a do výběru
blitzera (bod 7).

## 4) Testovatelnost (per-člen, dle požadavku uživatele)
1. past: cíl s nemarkovaným kamarádem vedle push pole → sekvence
   vyhodnocena jako zakázaná, i když `dice₁ = 3`;
2. Side Step cíl → bere se nejhorší push pole;
3. asistent umístěný tak, že po pushi vypadne → nižší hodnocení než
   asistent pokrývající obě pole;
4. push k čáře → sekvence oceněna výš (surf ukončí);
5. blitz bez pohybu → druhý blok se nekoná, ocenění to ví.

## Zdroje
- https://bbtactics.com/frenzy/
- https://www.goonhammer.com/blood-bowl-the-ultimate-guide-to-frenzy/
- https://steamcommunity.com/sharedfiles/filedetails/?id=827210711

---

## 5) OVĚŘENÍ PŘED IMPLEMENTACÍ (07.08.) — dvě zjištění, jedno mění plán

### ✅ Výběr push pole je v enginu DETERMINISTICKÝ, ne „útočník si vybere"
`block_handler.cpp:82-100`: default je **`chosenIdx = 0`** — první prázdné,
jinak první dostupné. Volbu mění jen dvě dovednosti: **SideStep** (vybírá
obránce, pole nejdál od útočníka) a **Grab** (útočník, pole nejblíž čáře,
jen mimo blitz).
⇒ **Korekce plánu:** dnes NEMÁME „bereme MAX přes push pole" — engine
odstrčí, kam padne pořadí sousedů. Plánovač tedy musí počítat s tím, KTERÉ
pole engine skutečně zvolí (deterministické, dopočitatelné), ne s ideálem.
Dvě cesty, obě legitimní, rozhodnout při implementaci:
 (a) modelovat realitu (číst tutéž logiku volby) — levné, konzistentní;
 (b) doplnit volbu push pole útočníkem dle pravidel (CRP: útočník vybírá
     z dostupných polí) — pravidlově správnější, ale je to změna chování
     enginu = samostatná měřená položka, ne součást tohoto plánu.
**Pozn.: (b) je pravděpodobně další rules-parity nález** — v pravidlech si
útočník vybírá vždy; u nás jen s Grabem. Zapsat do skupiny parity oprav.

### ⚠️ Follow-up je v enginu POVINNÝ pro všechny, ne jen pro Frenzy
`resolveBlock` má parametr `noFollowUp`, ale **nikdo ho nikdy nenastaví na
true** (jediné volání s ním je Fend uvnitř, :516-517). V pravidlech je
follow-up **volitelný** (kromě Frenzy, kde je povinný).
**Frenzy je JEDINÁ výjimka z volitelnosti** (upřesnění uživatele 07.08.).
⇒ Bereme tím obraně jednu možnost: nesledovat push a zůstat stát. Pro
Rat Ogra a trpasličí Slayery to nevadí (Frenzy je nutí tak jako tak), ale
pro VŠECHNY ostatní hráče je to odchylka — a zrovna trpaslíci by nesledování využili (zůstat ve
formaci klece místo vytažení z ní). **Samostatný parity nález**, souvisí
s doktrínou klece (roh nesmí být vytažen follow-upem — obava u Frenzy
slayerů z 07.08.).

### Co z toho plyne pro matematiku
Sekvenční výpočet zůstává platný, jen se zjednodušuje: **push pole není
množina k optimalizaci, ale jedna dopočitatelná hodnota** (resp. dvě
větve: SideStep = obránce, jinak deterministicky). Riziko sekvence tedy:
`p₁ + (1−p₁)·[druhý blok nastane]·p₂`, kde druhý blok nastane, právě když
po pushi oba stojí a sousedí (u blitzu navíc zbývá pohyb/GFI).


---

# 6) RIZIKA A BENEFITY — SPOČÍTÁNO (07.08., na žádost uživatele)

## Obecný tvar (jediný vzorec, zbytek jsou dosazení)
Kostka bloku má 6 stěn: 1× Attacker Down, 1× Both Down, 2× Push,
1× Defender Stumbles, 1× Defender Down. Označme **b = kolik stěn je pro
TUHLE dvojici špatných**. Pak:
* **vybíráme my** (n kostek pro nás): `riziko = (b/6)^n`
* **vybírá soupeř** (n kostek proti nám): `riziko = 1 − ((6−b)/6)^n`

| b | 1 kostka | 2 pro nás | 3 pro nás | 2 proti | 3 proti |
|---|---|---|---|---|---|
| **1** (s Blockem) | 16,7 % | **2,8 %** | 0,5 % | 30,6 % | 42,1 % |
| **2** (bez Blocku) | 33,3 % | 11,1 % | **3,7 %** | 55,6 % | 70,4 % |

Prahy „2 kostky s Blockem / 3 bez něj" = jen řádky téhle tabulky pod ~4 %.

## Případ A: Gutter Runner (Wrestle + Strip Ball) vs nosič BEZ Sure Hands
Rozbor stěn **pro tuhle dvojici** (proto b = 1, ne 2):
* Attacker Down — jediná špatná (GR na zemi, turnover);
* **Both Down — Wrestle: oba na zem, bez zbroje a BEZ turnoveru**
  (block_handler.cpp:383-399) + nosič leží ⇒ **míč padá**;
* Push ×2 — **Strip Ball shodí míč** (Sure Hands by to zrušil, :501-511);
* Defender Stumbles / Defender Down — nosič dolů ⇒ míč padá.

**Dvě kostky PROTI (soupeř vybírá):**
| výsledek | pravděpodobnost |
|---|---|
| turnover (Attacker Down) | **30,6 %** |
| Both Down — míč padá, ale náš GR leží | 25,0 % |
| míč padá, GR STOJÍ | 44,4 % |
| **míč uvolněn celkem** | **69,4 %** |

⇒ Skaven kupuje **69% šanci sebrat míč** za 31% riziko konce tahu. Pro tým,
jehož celá hra je míč, dobrý obchod — a proto je 2 kostky PROTI legitimní
volba, ne chyba. (S 1 kostkou by to bylo 16,7 % / 83,3 %.)

## Případ B: trpaslík s Block + Tackle, JEDNA kostka, proti elfovi s Dodge
| výsledek | pravděpodobnost |
|---|---|
| turnover (Attacker Down) | **16,7 %** |
| **srazí elfa** (Both Down díky Blocku + Stumbles díky Tackle + Def Down) | **50,0 %** |
| odstrčí (2× Push) | 33,3 % |

⇒ Za 17 % riziko kupuje 50% sražení markera a v dalších 33 % aspoň odsun.
Náklad pádu je u trpaslíka nejnižší v lize (AV9 + Thick Skull) a cena
turnoveru závisí na okamžiku tahu — po dokončení bezkostkové práce je
téměř nulová. **Proto smí do jednokostkových bloků.**
Pozn.: bez Tackle by Stumbles proti Dodge byl jen push → sražení 33 %,
ne 50 %. Tackle na rozích klece se tím vyplácí i mimo klec.

## Co z toho pro implementaci
Nepočítat „kolik mám kostek", ale **ocenit stěny pro konkrétní dvojici**
(Block, Wrestle, Tackle, Strip Ball, Sure Hands obránce, drží míč?),
zvážit rozdělením podle počtu kostek a toho, kdo vybírá, a porovnat
s **cenou turnoveru v daném okamžiku tahu**. Obě tabulky výše jsou
zároveň akceptační testy.


## 7) BENEFIT PROTI RIZIKU — druhá strana rovnice (07.08.)

Prahy zbroje/zranění ověřeny v `injury.cpp`: zbroj prolomena při
`2d6 > AV`; zranění 2-7 omráčen, 8-9 KO, 10+ zraněn; **Thick Skull
zachrání před KO na 4+** (zůstane jen omráčen).

**Cena jednoho sražení podle toho, KDO leží:**
| tělo | prolomená zbroj | odejde ze hry (KO+) | z toho zranění |
|---|---|---|---|
| elf / skaven AV7 | 41,7 % | **17,4 %** | 6,9 % |
| trpaslík AV9 + Thick Skull | 16,7 % | **4,9 %** | 2,8 % |

### Trpaslík, 1 kostka, vs elf s Dodge — attrition bilance
| | |
|---|---|
| srazí elfa | 50,0 % (Block + Tackle ruší Dodge) |
| **elf odejde ze hry** | **8,7 %** |
| trpaslík padne | 16,7 % |
| **trpaslík odejde ze hry** | **0,8 %** |
| **poměr výměny** | **11 : 1 pro nás** |
| skutečná cena | 16,7 % turnover (ne zranění) |

⇒ **Tohle je celý smysl trpasličí hry:** i jednokostkový blok je
attrition směna 11:1, protože AV9 + Thick Skull dělá z jeho pádu
skoro neškodnou událost. Jediná reálná cena je turnover — a ten se
řídí okamžikem tahu (risk-last ⇒ blokovat po dobrané bezkostkové práci).
⚠️ Zesiluje to bod 16 (mrtví se vracejí): dnes se těch 8,7 % po
touchdownu smaže, takže se výměna 11:1 v datech vůbec neprojeví.

### Gutter Runner (Wrestle + Strip Ball), 2 kostky PROTI
| | |
|---|---|
| **míč uvolněn** | **69,4 %** |
| turnover | 30,6 % |
| **GR odejde ze hry** | **5,3 %** |

⇒ Benefit není attrition, ale **míč**. A Wrestle riziko sráží dvakrát:
u Both Down se **nehází na zbroj vůbec** (oba jen položeni), takže
jediná cesta ke zranění je Attacker Down. Skaven tedy kupuje 69% šanci
na míč za 31% konec tahu a 5% ztrátu hráče.

### Souhrn: jiný tým, jiná měna
* **trpaslík** platí turnoverem a kupuje **attrition** (11:1);
* **skaven** platí turnoverem i hráčem a kupuje **míč** (69 %).
Proto nemá smysl jeden práh kostek pro oba — práh je jen důsledek toho,
co si daný tým za dané riziko kupuje.
