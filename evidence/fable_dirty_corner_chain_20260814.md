# Špinavé rohy: blok → čistota (P0.1) a řetěz zámků (P0.5) + rozpočet blitzu (C)

**14.08.2026 · Fable analýza.** Korpus `diag_replay_mine_20260813_big_data`
(3000 her, trpaslík vs human/orc/skaven/wood-elf, brána klece OFF, HEAD
e4b99ee, 44 177 našich kol). Definice rohů/špinavosti/zámků/Δx **importovány**
z `diag_rules_checks_20260812.py` a `diag_exposure_scan_20260812.py` (jediná
definice, dvě použití). Očekávání pre-registrována PŘED výpočtem
(`scratchpad/dirty_corner_chain_20260814/PREREG.md`); část C dopsána do
pre-registrace před spuštěním výpočtu C. Skripty + úplné výstupy:
`scratchpad/dirty_corner_chain_20260814/` (extract.py, analyze.py,
analysis_output.txt, supplement.txt, bootstrap.txt).

Značení: kolo N = naše kolo (S = snímek začátku, E = konce); N+1 = naše
další kolo (S2 = začátek, E2 = konec); mezi nimi kolo soupeře. „Polluter" =
soupeřův stojící hráč sousedící s obsazeným rohem klece (ten, kdo roh špiní).
σ u hraničních efektů ověřeno bootstrapem po HRÁCH (kola v téže hře nejsou
nezávislá; hlavní efekty 10–20σ to nemění, u 2–4σ efektů uvádím clusterovou
verzi).

---

## 1) Verdikt A (P0.1): ANO — ale jen ADRESNĚ. Obecné bití rohy nečistí.

### A1: sražení pollutera čistí roh (adresný test, rozhodovací bod = začátek N+1)

Kola s ≥1 polluterem na začátku N+1, n = 3864 (z toho polluter blokován
2972, blok jen jinam 386, žádný blok 506):

| výsledek konce N+1 | polluter blokován | blok jinam | žádný blok | hit−none |
|---|---|---|---|---|
| špinavé rohy | **0,27** | 0,76 | 1,00 | **−0,73 (−22,9σ)** |
| čisté rohy | **1,41** | 1,08 | 1,11 | +0,30 (+6,1σ) |
| Δx nosiče v N+1 | **+1,62** | +1,33 | +0,76 | +0,86 (+9,7σ) |

Mechanismus je vidět přímo: blokovaný polluter je na konci N+1 na zemi
v **72,8 %** a špiní dál jen v **7,8 %**; neblokovaný špiní dál v **63,8 %**
(na zemi 3,3 %). Efekt drží ve všech pásmech hustoty (OPP3 0–2: −15,5σ,
3–4: −15,5σ, 5+: −8,6σ) — není to artefakt „kde je málo soupeřů, je čisto".

### A2: obecné bloky v N čistotu v N+1 NEZVEDAJÍ — jde opačným směrem

| vztah (kola s klecí, n=13 501) | surové | po kontrole (OPP3, REACH0, kolo) |
|---|---|---|
| bloky_N → čisté rohy N+1 | −6,4σ | **−7,5σ** (cluster −7,1σ) |
| bloky_N → špinavé rohy N+1 | +2,2σ | +0,1σ |
| bloky_N → Δx nosiče N+1 | +2,8σ | **+6,1σ** |

I s kontrolami rozšířenými o filled_N a dirty_N zůstává bloky→čisté_N+1
−4,5σ. **Pre-registrace A2 (kladný efekt ≥2σ) VYVRÁCENA s opačným
znaménkem.** Drive-level nález „bloky/kolo +2,7σ k TD" tedy NEjde přes
rohy — bloky pomáhají tempu (+6,1σ na Δx), ale čistotu rohů vyrábí jen
blok mířený na pollutera.

**Co z toho plyne pro prioritu blitzu: zelenou dostává pravidlo „udeř toho,
kdo špiní roh", NE pravidlo „přidej bloků". A podle části C má být nástrojem
především BLOK zdarma, blitz jen jako záloha (viz §3).**

Kauzální výhrada k A1: „hit" vyžaduje mít čím udeřit, což koreluje s lokální
převahou těl. Stratifikace po hustotě efekt nemění a mechanická cesta
(72,8 % polluterů na zemi) je přímo pozorovaná; zbytkový confound „těla
zrovna byla po ruce" ale observačně vyloučit nejde — definitivně to rozhodne
až A/B s změněnou prioritou. Reverzní směr (čistý roh → blok) je vyloučen
konstrukcí: treatment se čte na začátku kola, outcome na konci.

## 2) Verdikt B (P0.5): řetěz platí — ale účet se platí v TĚLECH, ne přímo v tempu.

### B1: špinavý roh = ztracené tělo na příští kolo (nejtvrdší číslo analýzy)

Osud těla stojícího na špinavém rohu (konec N) na začátku N+1, n = 3680 těl:

| stav na začátku N+1 | podíl |
|---|---|
| zamčené v TZ | 49,6 % |
| na zemi | 43,5 % |
| mimo hřiště | 1,3 % |
| **volné** | **5,7 %** |

**94,3 % těl ze špinavých rohů je na začátku dalšího kola nedostupných.**
Jako ČISTÝ roh na konci N+1 slouží jen **13,8 %** z nich (400/2906).
Pre-registrace B1 (≥50 % zamčené∨na zemi) potvrzena s velkou rezervou.

### B2: řetěz do N+1 (kontroly: OPP3, REACH0, kolo, filled_N; kola s klecí)

| špinavé_N → … | surové | po kontrole |
|---|---|---|
| zamčení mimo rohy (konec N) | +14,5σ | — (tautologická část jen 0,16 z 1,25 zámků/kolo) |
| zamčená těla na začátku N+1 | +20,3σ | **+9,9σ** |
| volná těla na začátku N+1 | −37,7σ | **−21,3σ** |
| naši sražení v soupeřově kole | +13,3σ | **+4,2σ** |
| čisté rohy konec N+1 | −17,7σ | **−11,4σ** (cluster −13,0σ) |
| špinavé rohy konec N+1 | +21,5σ | **+10,9σ** |
| Δx nosiče v N+1 | −9,8σ | **−1,7σ** (cluster −1,7σ) — NEPRŮKAZNÉ |

Čtecí tabulka: Δx_N+1 = +2,04 (0 špinavých) / +1,57 (1) / +1,31 (2+).
Surová ztráta ~0,5 pole na špinavý roh existuje, ale **po kontrole na
hustotu z ní zbývá šum** — pomalost je nesená hustotou soupeřů, špinavý roh
k ní sám o sobě přidává málo. Pre-registrace B2 (Δx ≥3σ po kontrolách)
POTVRZENA JEN NAPŮL: tělová část řetězu (zámky, volná těla, rohy) drží
na 10–21σ, tempová část ne.

Mezičlánek zvlášť: zamčení_S2 → Δx_N+1 po kontrole jen −2,5σ, → čisté_N+1
+0,7σ. Řetěz tedy NENÍ „roh → zámek → pomalost", ale **„roh → tělo pryč
(zámek NEBO země) → není z čeho stavět rohy v N+1"** — a chybějící čisté
rohy se pak platí ztrátou míče (čistota rohů → TD +2,6σ z 13.08. platí dál).

### B3: splatnost po soupeřích — uživatelova hypotéza platí půlkou

P(ztráta míče v soupeřově kole) podle špinavých rohů, surově:

| soupeř | dirty=0 | dirty≥1 | Δpp | σ (surové) | po kontrole hustoty (cluster σ) |
|---|---|---|---|---|---|
| human | 10,5 % | 24,6 % | +14,1 | +8,5σ | +1,9σ |
| orc | 11,2 % | 29,0 % | +17,8 | +9,6σ | +2,4σ |
| skaven | 8,7 % | 17,4 % | +8,7 | +6,1σ | +1,6σ |
| wood-elf | 11,6 % | 21,7 % | +10,1 | +6,7σ | +1,1σ |

Odložená splatnost po kontrole hustoty (cluster σ u skavena):

* Δx_N+1: **skaven −0,064 (−4,3σ) — JEDINÁ rasa s průkazným tempovým
  účtem**; human −0,0σ, orc −0,3σ, wood-elf +0,5σ.
* zamčení_S2: +4,0 až +6,0σ u všech ras (univerzální).
* čisté_N+1: −3,7 až −6,9σ u všech ras (univerzální).
* naši sražení: human +3,9σ, orc +4,8σ; skaven/wood-elf ~0 (bijci dorážejí
  zamčená těla, hbití je obcházejí).

**Verdikt B3:** „skaven platí na splátky" POTVRZENO (jediná rasa, kde
špinavý roh průkazně zpomalí klec v dalším kole, a nejnižší okamžitá
splatnost). „Wood-elf si to vybere hned" VYVRÁCENO v marginální podobě:
proti wood-elfovi je base riziko ztráty vysoké VŽDY (11,6 % i s čistými
rohy) — špinavý roh k němu přidává nejméně ze všech ras (+1,1σ po kontrole).
Okamžitou splatnost za špinavý roh účtují nejvíc POMALÍ BIJCI (orc +17,8 pp,
human +14,1 pp surově): špinavý roh jim dává kontakt, který by si jinak
museli koupit blitzem. Hbitým rasám kontakt nechybí — proto u nich roh
marginálně „nestojí" tolik. ⭐ Uživatelova pointa „účet nepřijde v kole
chyby" ale POTVRZENA obecně: jediné složky účtu, které přežijí kontrolu
hustoty u všech ras, jsou ODLOŽENÉ (zámky +4–6σ, chybějící čisté rohy
−4–7σ, tělo z rohu 94 % nedostupné).

## 3) Verdikt C: konflikt s prolomením zdi z většiny mizí — čistí se BLOKEM, ne blitzem.

### C1 — nejdůležitější číslo: 61 % polluterů jde udeřit ZDARMA

Z 5089 polluterů na začátku kola (3938 kol): **61,1 %** má volného stojícího
souseda (mimo nosiče a mimo těla na rozích) ⇒ blok bez blitzu; **35,0 %**
dokonce s ≥2 kostkami. Na úrovni kol: v **69,2 %** kol s polluterem jde
aspoň jeden očistit zdarma. Po rasách 59,3–63,2 % (2k: ork jen 22,1 %,
wood-elf 43,8 % — proti orkovi je blok zdarma častěji do kopce).
Engine už dnes zdarma udeří 57,6 % polluterů v témž kole — a účinkuje to:
udeřený polluter ⇒ špinavé na konci téhož kola 0,27 vs 0,81 (−20,8σ).

### C2 — když na roh padne blitz, stojí to tempo teď a nekupuje nic potom

Rozdělení blitzu v kolech s polluterem: none 53 %, wall_fwd 17 %,
carrier_mark 14 %, **corner 12,6 %**, other 3 %. Srovnání corner vs wall_fwd
(kontroly OPP3_S, počet polluterů, kolo):

* Δx v témž kole: corner +1,80 vs wall +2,52 ⇒ **−6,4σ, ~−0,7 pole** —
  cena je reálná a okamžitá.
* Δx_N+1: −0,8σ, špinavé_N+1: +1,9σ (špatný směr), ztráta míče: −0,7σ —
  **žádný měřitelný výnos navíc proti blitzi do zdi.**
* **45,5 % blitzů na roh padlo v kolech, kde VŠICHNI polluteři měli volného
  souseda** (blok zdarma šel) — v těch kolech je blitz na roh vyhozený
  rozpočet.

### C3 — těla na to jsou: nedostatek je alokační

V 52,6 % kol s polluterem existuje ≥1 idle tělo (K31 definice; průměrně
2,14/kolo v těch kolech) a **94,7 %** z nich na pollutera DOSÁHNE (BFS,
MA+2). V 96,2 % takových kol dosáhne aspoň jedno. Sedí to na nález
„7,03 volných z 11": klec není nedostavěná z rozhodnutí, ale volná těla,
která existují, se na pollutery neposílají.

### C4 — kdy roh, kdy zeď: NEROZHODNUTO

Po fázích (kola 1–3 / 4–6 / 7–8) jsou Δx_N+1 po blitzi corner a wall
prakticky totožné (+1,36/+2,07/+3,13 vs +1,26/+2,16/+3,35; n=37–286).
Žádná fáze, kde by blitz na roh měřitelně vyhrával, nenalezena — práh
nevymýšlím. Observační srovnání navíc nese selekci (kola, kdy engine
blitzuje roh, nejsou náhodná); skutečnou odpověď dá jen A/B.

## 4) Čísla do doktríny

1. **Priorita: polluter s volným stojícím sousedem ⇒ BLOK na něj, před
   obecným mlácením.** Pokrývá 61 % polluterů (35 % s ≥2k); blokovaný
   polluter je v 72,8 % na zemi a roh přestane špinit v 92 %; neblokovaný
   špiní dál v 64 %.
2. **Blitz na roh jen jako záloha:** když polluter volného souseda NEMÁ
   (39 % případů) a současně blitz nepotřebuje nosič. Blitz na roh stojí
   ~0,7 pole tempa v témž kole a proti blitzi do zdi nekupuje nic
   měřitelného; 45,5 % dnešních blitzů na roh je plýtvání (blok zdarma šel).
3. **Špinavý roh nechaný přes soupeřovo kolo = ztracené tělo:** 94,3 %
   nedostupné na začátku N+1 (50 % zámek, 43 % země), jen 13,8 % poslouží
   jako čistý roh v N+1. Tohle číslo patří do ceny pozice (spec „co pozice
   dává soupeři" — dimenze, kterou audit spec hlásí jako chybějící).
4. **Rasový modifikátor:** proti orkovi/humanovi špinavý roh zdražuje
   okamžitou ztrátu míče nejvíc (surově +17,8/+14,1 pp; kontrolovaně ale jen
   ~2σ — hraniční); proti skavenovi je jediný průkazný tempový účet
   (−4,3σ Δx v N+1). Proti wood-elfovi roh marginálně skoro nic nemění
   (+1,1σ) — tam rozhoduje base expozice (REACH0), ne rohy.
5. **Idle těla na pollutery:** 2,14 idle těl/kolo v polovině kol
   s polluterem, 94,7 % dosáhne — pravidlo R4 („tělo bez úkolu") má mít
   „dojdi k polluterovi / postav se na asistenci bloku na něj" vysoko
   v repertoáru úkolů.
6. **NEdoporučuje se** zvedat obecný počet bloků kvůli rohům: bloky_N →
   čisté_N+1 je −4,5σ i po plných kontrolách. Bloky pomáhají tempu
   (+6,1σ), čistotu rohů vyrábí jen adresný úder.

## 5) Co jsem NEZMĚŘIL a proč (povinná sekce)

* **Pořadí akcí uvnitř kola.** Snímek je začátek kola; nevím, zda blok na
  pollutera padl před pohybem nosiče, nebo po něm. Závěry A1/C2 stojí na
  stavech začátek→konec kola; vnitřní sekvence by mohla část efektu
  přeskládat (např. roh je „čistý" jen proto, že se klec posunula jinam).
  Bez per-akčního stavového logu neměřitelné.
* **TD v N+1 jako výnos bití pollutera.** První verze tabulky měla artefakt
  (výběr na platný konec N+1 vylučuje TD kola — nulová množina, přesně vada
  z auditu 13.08.). Po opravě: hit 0,61 % vs none 2,10 % TD — ale směr je
  zamlžený reverzní kauzalitou (kolo, kdy se sprintuje na TD, bloky
  nepotřebuje). NEROZHODNUTO, nepoužívat.
* **Counterfactual blitzu** („co by týž blitz vynesl jinde"). Z replayí
  observačně nejde; C2 je srovnání selekcí zatížených skupin s kontrolami,
  ne experiment. Rozhodne jen A/B se změněnou prioritou.
* **Riziko bloku na pollutera** (both-down/skull řetěz, cena rerollu).
  C1 dělí jen podle dostupných kostek (≥2k ano/ne), ne podle plného
  rizikového účtu.
* **„Zeď" jako objekt.** wall_fwd je proxy (cíl blitzu před nosičem mimo
  jeho okolí); skutečné „prolomení zdi" (otevření koridoru) nemá v nástrojích
  definici, takže konkurence roh-vs-zeď je měřená jen přes tuhle proxy.
* **Blitz bez pohybu** (vyhlášený z kontaktu) je od bloku v eventech
  nerozlišitelný — klasifikuje se jako blok zdarma; podíl blitzů tím mírně
  podhodnocuji.
* **Dlouhodobý attrition výnos** bití polluterů (CAS/KO přes drive) — mimo
  rozsah, navazuje na balík G.

## 6) Pre-registrace vs výsledek — co vyšlo a co mě překvapilo

| očekávání (PREREG.md) | výsledek |
|---|---|
| A1: adresné bití čistí roh, ≥15 pp, ≥3σ | ✅ mnohem víc (−22,9σ; „přestal špinit" 92 % vs 36 %) |
| A2: obecné bloky → čisté_N+1 kladné ≥2σ | ❌ **VYVRÁCENO s opačným znaménkem** (−7,5σ) |
| B1: tělo z rohu ≥50 % zamčené/na zemi | ✅ 94,3 % |
| B2: Δx penalta −0,3 až −0,5 pole, ≥3σ po kontrolách | ⚠️ surově −0,5 pole sedí, ale po kontrole hustoty −1,7σ ⇒ NEPRŮKAZNÉ; tělová větev (zámky/rohy/volná těla) 10–21σ ✅ |
| B3: wood-elf platí hned nejvíc, skaven na splátky | ⚠️ skaven-splátky ✅ (−4,3σ tempo, jediná rasa); wood-elf-hned ❌ (marginálně nejmenší, +1,1σ; okamžitě účtují ork/human) |
| B4: efekt rohů nezmizí po kontrole hustoty | ✅ pro těla a rohy; ❌ pro tempo |
| C1: 50–70 % polluterů čistitelných zdarma | ✅ 61,1 % |
| C2: blitz na roh <15 % kol; Δx cena <0,5 pole | ✅ 12,6 %; ⚠️ cena vyšší: ~0,7 pole |
| C3: ≥⅓ idle těl dosáhne | ✅ 94,7 % — víc, než jsem čekal |

**Největší překvapení:** (1) obecné bloky čistotu rohů v dalším kole
ZHORŠUJÍ — „bít víc" a „bít ty pravé" jsou empiricky různé doktríny;
(2) špinavý roh je marginálně nejdražší proti pomalým bijcům, ne proti
elfům — hbitým rasám dává roh málo, protože kontakt na nosiče si umějí
vzít i bez něj; (3) tempová část účtu za špinavý roh je skoro celá
vysvětlená hustotou — skutečný, na hustotě nezávislý účet je v TĚLECH.

## 7) Metodické poznámky

* Jmenovatele a n u všech tabulek; prázdné množiny hlášeny jako artefakt
  (TD řádek A1), ne jako výsledek.
* Bez binarizace prediktorů: špinavé rohy, bloky, zámky, REACH0 jako počty;
  koše jen pro čtení.
* σ z výběru (Welch / Fisherova z); u efektů 2–4σ navíc bootstrap po hrách
  (400 resamplů) — kola v téže hře nejsou nezávislá; per-race okamžitá
  splatnost tím spadla na 1,1–2,4σ a podle toho je i formulovaná.
* Korpus je z jednoho HEAD (e4b99ee, brána OFF) — čísla popisují chování
  DNEŠNÍ AI; po změně priority blitzu/bloku je nutné přeměřit (A/B).
