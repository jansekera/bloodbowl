# PŘEDREGISTRACE — NOC 14.→15.08.2026: DAUNTLESS V NABÍDCE BLOKU

**Verze 3.** Verze 1 a 2 stavěly na P11 („neskórovat dřív") a **P11 je mrtvé** —
viz níž. Pořadí je od uživatele a **už se nemění**, ani podle výsledků
běžících analýz.

**Zapsáno PŘED spuštěním.** Prahy a pádové podmínky platí tak, jak stojí tady.

---

## 0. ROZPOČET

| kdy | co | |
|---|---|---|
| pá 18:00 → so 08:00 | **A/B Dauntless**, 3000 párů × 3 matchupy | 14 h |
| so 08:00 → 11:30 | korpus se zapnutým ramenem (rozklad drivů) | 3,5 h |
| so večer, ne večer | **běh 2 a 3** — fronta, ne plán | 2× 14 h |

**Víkend pojme tři A/B, ne pět.** Korpus jde ZA A/B: kdyby noc spadla, chceme
verdikt, ne popis.

### Proč 3000 párů a ne 1500
Fable spočítal z reálných řádků brány: **SD páru 0,54–0,56** ⇒ na **1 pp** chess
je potřeba **~12 000 párů**, na **2 pp ~3 000**. Na 1500 bychom viděli jen efekt
**≥ 3 pp** a strávili pondělí hádáním, co znamená „neprůkazně" — přesně jako
u brány klece. Uživatel 14.08.: *„kontrolní run má trvat dvakrát tak dlouho,
ať vyleze ze šumu — OK."*

---

## ⛔ CO SE ZMĚNILO PROTI VERZI 2

**P11 („neskórovat dřív, než je čas") je MRTVÉ.** Změřeno 14.08. na 3000 hrách:
* **72,5 % našich TD padne už teď v kolech 7–8** (48,9 % v kole 8).
* Když skórujeme dřív, **soupeř stejně neodpoví**: 0 % při 0–1 zbývajících
  kolech, 2,0 % při dvou, 8,3 % při čtyřech (n=24).
⇒ Pravidlo by mířilo na ~12 % našich TD a ušetřilo odhadem **6 gólů ve 3000
hrách**. Řádově pod šumem. **Neměřit.**

**Náš problém není, že skórujeme brzy — je, že v 70 % zápasů neskórujeme vůbec.**
(naše TD 975 · jejich 1208 · výhry 661 / remízy 1474 / prohry 865)

---

## BĚH 1 — P13: nabídka bloku ocení sílu, na kterou by Dauntless srovnal

### Nález
`getBlockDiceCount` (`macro_actions.cpp:126`) váží `Horns`, ale **Dauntless
nikdy**. Slayer ST3 vedle Black Orka ST4 se ocení jako **do kopce**, počet kostek
vyjde záporný a nabídka se zahodí — pro blok, který `block_handler.cpp:386`
při provedení srovná na rovnocenný. ⇒ **Slayerovi se blok na Black Orka nikdy
nenabídne.**

### Proč právě ork
Dauntless je nejsilnější přesně proti ST4: **d6+3 > 4 ⇒ 2+ ⇒ 83 %**; proti ST5
67 %, proti Treemanovi ST6 jen 50 %. **Ork je jediný soupeř se čtyřmi ST4
těly** — a je to zdaleka náš nejhorší matchup:

| | naše TD | jejich |
|---|---|---|
| skaven | **451** | 299 |
| wood-elf | 260 | 421 |
| human | 178 | 281 |
| **ork** | **86** | 207 |

### ⛔⛔ VADA ZADÁNÍ, NALEZENÁ PŘED SPUŠTĚNÍM (14.08. odpoledne)

**Harness nemá matchup dwarf–ork:**
```cpp
{"dwarf","skaven"}, {"dwarf","wood-elf"}, {"dwarf","dwarf"}, {"orc","skaven"}
```
A Dauntless vyskočí jen při `defST > attST` (náš Slayer je ST3):
* **dw-sk — nikdy** (skaven má max ST3)
* **dw-we — jen proti jednomu Treemanovi ST6, a to na 50 %**
* **orc-sk — nikdy** (null test)

⇒ **Běh, jak byl nachystaný, by 14 h měřil rozdíl, který nastane jen když Slayer
stojí vedle Treemana.** Změna míří na orky a ork-trpaslík se neměří.
**Sedmý výskyt vzorce dne — a tentokrát v mém vlastním zadání.**

⇒ **Nutná oprava před spuštěním:** přidat `{"dwarf","orc"}` jako matchup **4**
(append, aby se neposunuly indexy) a běžet **dw-orc · dw-we · orc-sk (null)**.

### ⚠️ A druhá, poctivá výhrada k síle testu
Fable §9.5: **strop Dauntless je 86 → ~110 TD na 750 orčích her**, tedy
~0,03 TD/hru ⇒ odhadem **+1 až 2 pp chess**. To je **na hraně nebo pod
rozlišením** i při 3000 párech (2 pp ≈ 3k párů).
⇒ **Očekávat NEROZHODNUTO v chess je legitimní výsledek, ne selhání.**
Rozklad drivů a attrition per rameno tady nejsou doplněk — jsou **hlavní**
odečet. A na krádežovém rameni dává Dauntless **nulu** (jejich nosič je ST3).

### Rameno
`MCTSConfig::dauntlessInOffer`, **default OFF** (produkce beze změny), harness
**mode 4**. Rameno platí pro **obě strany** — není to doktrína, kterou zkoušíme
na trpaslících, je to filtr, který neviděl dovednost, kterou resolver už ctí;
na jedné straně by to srovnávalo dva různé enginy, ne dvě ramena.

### Metrika a práh *(pre-registrováno)*
* **Primární: párová delta chess na `dw-sk` a `dw-we`, 3000 párů.**
* **PROŠLO:** obě trpasličí ramena ≥ 0 a aspoň jedno **≥ +0,02**.
* **ZAMÍTNUTO:** kterékoli trpasličí rameno **≤ −0,02**.
* Mezi tím **NEROZHODNUTO** — zapíše se jako neúspěch, ne jako naděje.
* ⚠️ `orc-sk` je **kontrolní matchup bez trpaslíka**: tam čekám **nulu**.
  Kdyby se hnul, rameno dělá něco jiného, než si myslíme.

### ⭐ POVINNÁ DRUHÁ METRIKA: MĚNA DRIVŮ *(nové ve verzi 3)*
Chess pod ~2 pp neuvidí ani 3000 párů. Fable 14.08.: compliance kontrol nemá
převod na výsledek, **měna drivů ano**:

> **Δchess ≈ 0,42 × Δ(drivy/hru)** · +0,05 drivu na hru = **+2,1 pp**

Z korpusu se zapnutým ramenem se proto **povinně** zapíše, proti baseline
`night_big_20260813/`:
1. **podíl kategorií A / B / C / D1 / D2**, celkem i **per matchup** *(ork je ta
   otázka)*;
2. **příčiny ztrát v kategorii C** — zvlášť „soupeřův blitz/blok srazil nosiče"
   (dnes 84,5 %);
3. **bloky na kolo a jejich cíle** — vzrostl podíl bloků na ST4+?
4. **DEAD/hru na obou stranách** — attrition je vedlejší kanál této změny.

### Předregistrované předpovědi *(ať se to nedá číst zpětně)*
| | čekám |
|---|---|
| bloky na kolo (K33) | **nahoru**, nejvíc proti orkovi — to je mechanismus |
| kategorie C proti orkovi | **dolů** z 59 % |
| chess `dw-sk`, `dw-we` | **nahoru nebo nula**; skaven má ST2/ST3, efekt tam má být menší než u orka |
| chess `orc-sk` | **nula** — kontrola |
| K9a tempo | beze změny nebo mírně dolů *(bít víc stojí pohyb)* |
| REACH0 | beze změny |

### Falzifikátor
Když **bloky vzrostou a chess se nehne**, je to **potřetí** (po bráně klece
a balíku G) a otevřená otázka č. 1 se povyšuje nad všechnu další doktrinální
práci. ⚠️ Ale pozor na její dnešní odpověď: **bloky/kolo jsou jeden ze dvou
prediktorů, které kontrolu proveditelnosti PŘEŽILY** (+2,8σ), takže tady je
šance na skutečný převod vyšší než u brány, která platila tempem.

---

## ⛔ PRAVIDLO SEKVENCE *(uživatel 14.08.)*

> *„Jsem pro měření zvlášť — samostatně Dauntless a po vyhodnocení později
> samostatně Wrestle."*

* **Žádné rameno nesmí obsahovat dvě změny naráz.** Ani tehdy, když spolu
  věcně souvisejí (Dauntless + Wrestle tvoří balík proti Black Orkovi — měří se
  přesto zvlášť, jinak nepůjde říct, která polovina zabrala).
* **Sobotní běh se NEPŘEDURČUJE.** Dauntless doběhne v sobotu ráno →
  **vyhodnotí se** → teprve pak se vybírá další. Fronta níž je zásobník
  kandidátů, **ne plán**.
* ⚠️ Zejména **P17 Wrestle není přislíbený na sobotu** — pořadí, které jsem
  psal odpoledne, tímhle padá.

## ZÁSOBNÍK KANDIDÁTŮ *(ne fronta, ne plán — vybírá se AŽ po vyhodnocení)*
* **P17 Wrestle** — opravuje kanál, který skaven používá v 51 % kol, a odemyká
  balík proti Black Orkům. ⚠️ Po opravě **klesne náš náskok proti skavenovi**,
  protože část našich 451 TD je artefakt toho, že ho soupeř neumí použít.
* **P2+P9c** — priorita bloku na pollutera; strop 39,4 % polluterů.
* **blitz roh vs. zeď** — jediné, co Fable nerozhodl a observačně nejde.
* **P10a** — sražení nosiče se musí vyplatit; rozpočet tří těl.
* ⛔ **NE: T5.14 Mighty Blow** — uživatel 14.08. odsunul k přestavbě rosterů
  (*„s MB si budeme pak hrát při změnách rosterů"*); dnes je dopad malý
  (Treeman + Ogre). Oprava je hotová včetně testů, čeká na větvi.

---

## ZNÁMÁ OMEZENÍ, KTERÁ VÝSLEDEK NESMÍ PŘEBÍT
* **Strop dokonalé útočné změny je +37,5 pp**, ale strop brány klece byl jen
  +3,9 pp. **Před každým dalším ramenem počítat strop napřed.**
* **Naše kontroly jsou z velké části selekční artefakt** (Fable 14.08.): K9a
  tempo ve stihnutelných drivech jen +1,7σ, čistota rohů **0σ ve všech koších**.
  Přežily jen **bloky/kolo (+2,8σ)** a **REACH0 jako počet (−2,7σ)**.
* **Soupeřova AI nehraje proti našim slabinám cíleně** ⇒ naměřená četnost chyb
  je podlaha, ne strop.
* **Snímek je začátek kola.**
* Sdílený limit pass/hand-off (P7) dělá hbité rasy slabšími, než jsou.
