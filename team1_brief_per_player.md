# Team 1 Brief — Per-player features

## Kontext a cíl

Aktuální architektura má 70 features, které agregují 22 hráčů do průměrů a frakcí.
Model nevidí individuální hráče — neví, který konkrétní hráč má Wrestle + AG4 + je 3 čtverce od nosiče.

**Cíl tohoto briefu:** navrhnout per-player feature vektor, novou architekturu sítě a tréninkovou strategii.

Odhadovaný rozsah: ~492 features (22 hráčů × ~20 features/hráč + globální kontext).

> ⚠️ **Verze pravidel: Blood Bowl 2016 (LRB6/CRP)**
> Engine implementuje BB2016 pravidla — NE BB2020/2025. Webový výzkum v tomto briefu čerpal z různých edic.
> **Před implementací každé feature ověřit mechaniku přímo v C++ engine kódu**, ne v online zdrojích.
> Konkrétní oblasti kde se pravidla mezi edicemi liší: pass range výpočet, intercept modifikátory, některé skill efekty.

> **TODO fronta (doplněno od uživatele 2026-07-15, nízká priorita, "možná později"):**
> Dohledat existující publikované formace/taktické guide z tabletop komunity (kickoff
> deployment, race-specific playbooky) na webu a využít je jako doplněk k ručně
> objevovaným vzorcům z replay analýzy — místo vymýšlení všeho od nuly. Stejné pravidlo
> jako výše platí i tady: webový zdroj ověřit proti BB2016/tomuto enginu, ne slepě převzít
> (rozdíly mezi edicemi, případné odchylky enginu od pravidel jako Guard u foulů).

> 🚨 **VYSOKÁ PRIORITA (doplněno od uživatele 2026-07-16): 150-her replay korpus
> (`diag_perplayer_grounding_data/main/`) je z PŘED hasActed fixem.**
> Korpus vygenerován 2026-07-15, fix (viz `evidence/fable_hasacted_fix_20260715.md`)
> přišel až 2026-07-16. Systematická kontrola situačního survey
> (`evidence/fable_perplayer_replay_grounding_20260715.md`, sekce "Situation survey")
> ukázala, že **7 z 12** kurátorovaných příkladových situací obsahuje vzorec bonusové
> nelegální aktivace (hráč jedná -> jiní jednají -> stejný hráč jedná znovu) -- 4/4 ručně
> ověřené jsou GENUINNÍ, ne artefakt analýzy. Několik "GOOD decision"/"chyba" verdiktů v
> tomhle briefu (zejména situace 1, 3, 9, 10) je tedy postaveno na bonusových akcích, ne
> na skutečném jednorázovém rozhodnutí AI. **Než se z tohohle korpusu dál těží nálezy nebo
> než se cokoliv z něj bere jako finální podklad, korpus by se měl přegenerovat na
> opraveném enginu.** Cross-cutting vzorec "AI preferuje kontakt/boj před strategickou
> pozicí" (viz Okruh 4 níže, situace 4/5/6/7) je jiný typ rozhodování (volba makra, ne
> bonusová aktivace) a pravděpodobně přežije přegenerování, ale mělo by se to ověřit
> znovu na čistých datech, ne předpokládat bez kontroly.

---

## Okruh 1: Role hráčů a archetypes

### Otázky pro Team 1
- Má smysl řadit sloty ve feature vektoru podle **role** (slot 0 = nosič, 1-4 = cage guards, 5-8 = blitzeři...)?
- Nebo je lepší **kanonické pořadí podle vzdálenosti k míči** (slot 0 = nosič, sloty 1-10 = moji hráči od nejblíže, sloty 11-21 = soupeřovi hráči od nejblíže)?
- Jak ošetřit hráče mimo hřiště (KO, injured, dead) — nulové sloty nebo pevný počet aktivních?

### Standardní pozice v Blood Bowl (z pravidel)

| Pozice | Typická role | Klíčové vlastnosti |
|--------|-------------|-------------------|
| **Thrower** | Nosič míče, dirigent útoku | Pass skill, mění na passing play když potřeba |
| **Blitzer** | Útočník, path-clearer, safety, cage guard | Mobilní hitter — Guard, MightyBlow, Tackle, Frenzy |
| **Runner/Catcher** | Příjemce přihrávky, rychlý nosič | Vysoké MA a AG, Catch, Dodge |
| **Lineman** | Kanonenfutter, tie-up, ofenzivní linie | Bloky, zdržuje soupeře, obětovatelný |
| **Blocker** | Defenzivní kotva fyzických týmů | Guard, Stand Firm — drží poziční výhody |
| **Crowdsurfer** | Patrola sideline, narrow the pitch | Blízko okraje, hrozba crowd-surfem |

### Herní kontext (doplnit od uživatele)
- Souhlasí uživatel s výše uvedenými rolemi, nebo má jiné pojmenování?
- Které role jsou pro AI rozhodování nejkritičtější z jeho pohledu?

> **Doplněno od uživatele (2026-06-09):**

### Kickoff deployment — hloubka pro deep kick (doplněno od uživatele 2026-07-15)

Konkrétní nález z replay analýzy (`evidence/fable_perplayer_replay_grounding_20260715.md`,
situace 1): AI nasazuje na kickoff celý tým těsně u LOS (Thrower i Catcheři na x=11-12,
přímo na středové čáře), nikdo není odsazený hlouběji do vlastní poloviny. Když se kop
rozptýlí dál od LOS, nikdo není bezpečně vzadu, aby ho v klidu sebral — místo toho musí
někdo z LOS shluku doběhnout, sebrat míč a hned se rozhodovat pod tlakem.

**Přesný mechanismus (ověřeno v surových datech tahu, ne jen z obrázku):** Blitzer doběhl
celou svou MA (7 polí) až do rohu (0,4) a sebral míč — tím vyčerpal pohyb. Byl tam ale
BEZPEČNÝ (celý Orc shluk 13+ polí daleko, mimo dosah i s GFI). Bezpečná volba (ukončit tah
s míčem v ruce) byla plně k dispozici zdarma. Místo toho AI zahrála přihrávku bez Pass
skillu na ~6 polí, ta selhala → TURNOVER hned v prvním tahu.

**PŘEHODNOCENO (doplněno od uživatele 2026-07-15, po objevu hasActed bugu — viz
`project_bloodbowl_hasacted_bug_investigation_20260715` v paměti):** tahle konkrétní
přihrávka byla s velkou pravděpodobností **projev nelegální bonusové aktivace**, ne
čistý příklad špatné kalkulace rizika. id11 dokončil svou aktivaci (Move+Pickup) a
pak ho enginový bug (`hasActed` se po úspěšném Move nikdy nenastaví) umožnil znovu
aktivovat pro Pass, PO TOM co už jednali id9/id10/id4/id2 — v opraveném enginu by tah
po jejich akcích prostě skončil, id11 by se k žádné další akci nedostal. **Není to tedy
čistý důkaz "plochá hodnotová funkce nerozlišuje bezpečné držení od risku"** — jedna ze
srovnávaných "možností" byla nelegální, takže srovnání samo je kontaminované. Situace 8
(double-GFI gamble) zůstává validním příkladem risk-taking chování — ten je o riziku
UVNITŘ jedné aktivace (neúspěšný GFI), ne o cross-aktivační reaktivaci, jiný mechanismus,
bug se ho netýká. Po opravě hasActed bugu stojí za to tuhle situaci 1 přeměřit znovu —
možná zmizí úplně, protože ta volba přihrávky už nebude enginem vůbec nabízena.

**Princip:** Thrower + aspoň 1 Lineman (a případně 1 Blitzer) by měli zůstat hlouběji ve
vlastní polovině při nasazení na kickoff, ne všichni namačkaní na LOS — jako pojistka pro
deep kick. Tohle je jiný typ deploymentu než útočná formace níže (ta je pro fázi ÚTOKU,
tohle je pro fázi PŘÍJMU výkopu) — obě fáze vyžadují jiné rozestavění a neměly by se plést.

**Pořadí zotavení míče po kopu (doplněno od uživatele 2026-07-15):** nejde jen o TO, kdo
je vzadu, ale i o POŘADÍ, kým míč zotavit:
1. Nejdřív Lineman (případně dva) — dojde/dojdou k místu dopadu a postaví se VEDLE míče,
   ne přímo na něj — poskytnou trochu jistoty/krytí, obětovatelní hráči jdou první
2. Teprve pak Thrower (má SureHands — spolehlivější pickup, `attemptRoll` s reroll na
   sebrání) skutečně sebere míč, už v relativně krytější situaci

Tohle je přesný opak toho, co udělala AI v situaci 1 — poslala rovnou svého nejcennějšího
dostupného hráče (Blitzera bez SureHands) samotného přes celé hřiště, bez jakéhokoli
krytí napřed.

**Vyvážený dodatek — Orc udělal částečně správnou věc:** stejná situace, druhá strana —
Orc (kopící tým) držel oba Blitzery vzadu (x=19, blízko vlastní endzóny) místo aby je hned
nahnal k LOS. Obecně rozumný princip (nekomitovat specialisty brzo). V TÉTO konkrétní hře
to ale bylo tak trochu k ničemu — akce se odehrála kolem x=0-6, tedy 13+ polí od Orc
Blitzerů, takže do ní stejně nemohli zasáhnout. Spíš rozumný výchozí vzorec než důkaz
promyšlené reakce na konkrétní hrozbu.

### Útočná formace z pohledu hráče

| Slot | Počet | Role | Pozice na hřišti |
|------|-------|------|-----------------|
| Ball carrier | 1 | Nosič míče | Střed cage |
| Cage corner | 4 | Ochrana nosiče | Diagonály kolem nosiče |
| Blitzer | 1–2 | Čistí cestu, útočí na soupeřovy bloky | Před cage, side |
| Catcher/receiver | 2–4 | Čeká na handoff nebo pass | Blízko soupeřovy endzóny, volný |

**Nosič míče potřebuje Block (doplněno od uživatele):**
- Thrower i Catcher jako nosič by měli mít **Block** — jinak jsou zranitelní při blitzu soupeře
- Block na nosiči = Both Down nehodí nosiče na zem → míč zůstává

**Konkrétní potvrzení z replaye (doplněno od uživatele 2026-07-16, ze situace 4, tah 3):**
Skaven nosič id20 (**GutterRunner, ST2/AG4/MA9, Dodge+SureFeet, BEZ Block skillu**)
doběhl pohybem přímo vedle stojícího Orc **Lineman** (ST3, žádné skilly, nemusel se
ani hnout) — bez Block skillu na nosiči stačil obyčejný neoznačkovaný Lineman: blok,
knockdown, armor break, **turnover**. Srovnání ve stejném tahu: jiný Skaven (id18,
Blitzer+Guard, **MÁ Block**) aktivně zaútočil na Orc hráče a "odveta" byla jen slabý
push — Block skill přesně tenhle rozdíl dělá. Potvrzuje principy výše: (1) nosič bez
Block je zranitelný i vůči triviálnímu soupeři, ne jen vůči blitzu, (2) nosič nesmí
skončit pohyb vedle stojícího soupeře zbytečně (viz i Skaven pohybový princip u
situace 4 níže) — tady šlo o dvojité zbytečné riziko na tom nejdůležitějším hráči
(nosiči), ne na vedlejším hráči.

**Rasová závislost útočné formace (doplněno od uživatele 2026-06-09):**

| Rasa | Receiver taktika | Dosah přihrávky | Poznámka |
|------|-----------------|-----------------|----------|
| **Wood Elf** | Catcher čeká u endzóny | Long pass (10+ čtverců) | AG4, Catch → pass je primární zbraň. Konkrétní mechanika viz Okruh 4 "Vytvoření volného receivera blitzem" — u Wood Elf by tohle mělo být standardní, vysoce prioritní součást hry, ne okrajová taktika. Replay data (15.07.) ale ukázala, že AI tohle u Wood Elf nevyužívá o nic víc než u ras bez pass hry vůbec (kapitalizace 3.1 % vs Dwarf 1.6 %, Orc 0.0 %) — mezera je rasa-nezávislá. |
| **Dark/High Elf** | Catcher blíže, kratší pass | Short/quick pass | Spolehlivější než wood elf ale podobný styl |
| **Orc** | Kombinace short pass + handoff v jednom tahu | Short pass → handoff | Překoná vzdálenost bez vysokého AG; funguje i proti pomalým týmům (Khemri) |
| **Dwarf** | Cage grind, žádní catchers | Handoff maximálně | MA4-5, bez AG → žádný pass |
| **Skaven** | Rychlý runner bez cage | Handoff | MA7-9, prostě doběhne |

**Situační závislost:** Orci a jiné "bashery" normálně hrají cage grind, ale proti Khemri (extrémně pomalí, ST4+ všude) musí taky vyslat receivera dopředu — Khemri nedostihnou, ale je zbytečné čekat 8 tahů.

**Skaven nosič — kam NESMÍ skončit pohyb (doplněno od uživatele 2026-07-16, ze situace 4):**
Ve sledovaném driveu (`g0001.json.gz`, poločas 2 tah 3) Skaven nosič id20 doběhl pohybem
rovnou vedle stojícího Orc obránce (id1, který se nemusel ani hnout) — Orc pak zdarma
zablokoval, srazil nosiče a vynutil turnover. Zásadní princip pro Skaven (a obecně rychlé
agilní nosiče, které se spoléhají na to, že se soupeři vyhnou kontaktu, ne na Block skill):
**po pohybu nesmí nosič skončit v poli sousedícím se stojícím soupeřem** (dává mu zadarmo
blitz) **ani blízko okraje hřiště** (ztracený míč po knockdownu tam má menší prostor
odskákat zpátky do hry — viz i throw-in nález výše, kde odražený míč skončil mimo hřiště
a musel se vracet throw-inem). **Implikace pro per-player/pohybové features:** cílová
pozice pohybu nosiče by měla penalizovat (1) sousedství se stojícím soupeřem bez Block
skillu, (2) blízkost k okraji hřiště (sideline i vlastní/soupeřova endzóna), ne jen
maximalizovat postup směrem k endzóně.

**Konkrétní potvrzení z replaye — Orc short pass+handoff kombinace se vůbec nepřipravuje
(doplněno od uživatele 2026-07-16, ze situace 4):** ve hře `g0001.json.gz`, poločas 2 (8
tahů/stranu), domácí tah 6 (zbývají jen 2), se žádný stojící Orc hráč za celý poločas
nedostal dál než na x=15 ze soupeřovy endzóny na x=25 — nikdo tedy není připravený jako cíl
téhle kombinace, přestože se blíží konec poločasu. Stejný kořen jako `is_free_receiver`
deployment nález ze situace 1, jen pozdě v poločase, kde už na nápravu není čas.

**Existující mechanismus na tempo postupu (`carrierStallAwareSteps`) se v téhle situaci
vůbec nespustil -- kořen je macro-generation, ne chybějící logika (doplněno od uživatele
2026-07-16, VYSOKÁ PRIORITA):** `engine/src/macro_actions.cpp:841-866` má už hotovou a
dobře navrženou funkci `carrierStallAwareSteps()` -- počítá přesně kolik polí má nosič
tenhle tah urazit, aby dorazil do endzóny v posledním tahu poločasu (`turnsRemaining`,
`distToEndzone`), a `carrierIsBlitzable()` přepne na full-sprint, když je nosič v
bezprostředním ohrožení. **Tohle je přesně ten mechanismus, co by měl řešit náš problém
(situace 4) -- ale nepoužil se**, protože se pro nosiče (id3) v tazích 4-6 vůbec
nevybralo ADVANCE makro (tah 4 dostal místo toho BLOCK, tahy 5-6 nedostal nic
postupového).

**OPRAVA/ZPŘESNĚNÍ (2026-07-16, po hlubší kontrole kódu -- předchozí verze tohoto zápisu
byla příliš neurčitá a jeden bod byl vyloženě špatně, viz níže):**

1. **Generování ADVANCE makra funguje správně, není to bug.**
   `engine/src/macro_actions.cpp:272-278` -- ADVANCE se vygeneruje jako kandidát VŽDY,
   když `dist > maxReach` (nosič nemůže doskočit do endzóny tenhle tah). V tahu 4 to
   platilo (dist~25 vs maxReach~7-8), takže ADVANCE byl mezi kandidáty -- generování
   samo o sobě NENÍ ten problém.
2. **`carrierStallAwareSteps()` je správně navržený, taky není bug** (viz výše).
3. **CHYBA V MÉM PŘEDCHOZÍM ZÁPISU:** tvrdil jsem, že "musí postoupit celá klec, ne jen
   nosič" je chybějící mechanismus -- **není, už existuje** od 2026-06-25
   (`macro_mcts.cpp:513-531`, komentář "search-side #2: advance the whole CAGE, not just
   the carrier"): heuristika v `MacroMCTSSearch::simulate()` odměňuje postup ESKORTY
   (spoluhráčů do 4 polí od nosiče) směrem k endzóně, přesně proto, aby "klec postoupila,
   nosič ji následuje bezpečně". Navíc jsou tam i stall-pacing bonus (`idealDist =
   turnsLeft * ma`) a urgency bonus pro poslední 2 tahy (`macro_mcts.cpp:543-563`) --
   všechno už implementované a AKTIVNÍ (nezávislé na vf_blend, "added post-vf-blend
   so vf_blend never dilutes it").
4. **Existuje i statický žebříček priority maker** (`greedyMacroRank()`,
   `macro_mcts.cpp:34-51`) kde ADVANCE=50 > BLOCK=15 -- přesně to pořadí, co bychom
   chtěli. ALE tenhle mechanismus (`greedyLookaheadBonus`) je zapojený jen když
   `config_.leafLookahead == true`, a to je **`false` ve výchozím nastavení**
   (`engine/include/bb/mcts.h:27`) -- tedy v analyzované hře (i skoro jistě ve všech
   trénovacích bězích) **neaktivní**.

**VYŘEŠENO (2026-07-16, Fable diagnostika s reálným zkompilovaným enginem, 400 seedů) --
tři sbíhající se mechanismy, ne jeden bug.** Plná zpráva:
`evidence/fable_advance_vs_block_diagnostic_20260716.md`. Diagnostika reprodukovala
skutečnou volbu hry (BLOCK 72.5 % běhů) i chování v tahu 5 (kontrafaktuál bez markera
na nosiči pořád nevede k postupu, přesně jako reálná hra).

1. **Risk-pricing je zde dominantní:** nosič byl označkovaný (Gutter Runner vedle),
   takže ADVANCE vyžaduje dodge 4+ -- naměřeno 23,3 % turnoverů. One-ply EV: BLOCK
   +0,809 (skoro bez rizika, Block skill) vs. ADVANCE +0,756. Break-even neúspěšnost
   dodge, při které by ADVANCE vyhrálo, je jen **8,7 %** -- žádný neskillovaný dodge
   se tam za současných vah nikdy nevejde, takže označkovaný nosič prakticky nikdy
   nepostoupí, kdekoliv na hřišti.
2. **Free-block EV je vždy dostupný konkurent:** bezrizikový 2-dice blok má leaf
   hodnotu (+0,017 až +0,03) zhruba rovnou **5 polím postupu nosiče** (naměřený
   gradient jen ~+0,010/pole) -- "něco srazit" vyrovná nebo porazí "postoupit" skoro
   pokaždé, nezávisle na konkrétním stavu.
3. **Asymetrie prior floor (stejný mechanismus jako CAGE-floor fix z 07-03, nikdy
   nerozšířený na ADVANCE):** `expand()` dává BLOCK/CAGE floor ~0,093-0,12 bez
   podmínek, ADVANCE floor nemá (jen když tým prohrává o 2+, tady Orc vedl). I v
   kontrafaktuálu, kde je nosič plně neoznačkovaný a ADVANCE má objektivně nejlepší
   one-ply Q ze všech ~20 maker s nulovým rizikem, ho plné search vybere jen v 0,8 %
   případů -- alokace visitů je hladová po prioru, ne řízená hodnotou.

Potvrzeno jako NE příčina, přestože "aktivní": cage-advance bonus se spouští, ale je
skoro plochý na vlastní polovině; stall-pacing bonus se spouští na MAXIMU (+0,100)
přesně když nosič stojí v maximální vzdálenosti od endzóny (idealDist = turnsLeft×MA =
dist) -- funguje podle kódu, ale samotný design bere "nehnul jsem se, pořád přesně na
plánu" jako strop odměny, což ruší ~40 % tlaku na postup přesně v režimu hluboko na
vlastní polovině, kde je pohyb nejvíc potřeba. Natrénovaná value funkce nehraje roli
(vf_blend=0 v tomhle batchi).

**Tři konkrétní opravné páky (zatím jen potvrzeno+změřeno, neimplementováno):**
(a) risk-aware cenění postupu nosiče / explicitní "nosič je označkovaný" penalizační
term, (b) ADVANCE prior floor symetrický s existujícím BLOCK/CAGE floorem, (c)
přetvarovat `stallPacing`, aby "na plánu" přestalo být stropem odměny v maximální
vzdálenosti od endzóny.

**Priorita v samotném rozhodování -- POTVRZENO diagnostikou (2026-07-16):** heuristika
váhy počítá "správně" podle svého vlastního zadání (search maximalizuje zadanou
hodnotu), ale to zadání samo je špatně kalibrované -- BLOCK vyhrává nad ADVANCE právě
proto, že postup s míčem je oceněný příliš nízko (viz 3 mechanismy výše) vůči
bezrizikovému bloku. Tohle přesně potvrzuje uživatelův bod: ADVANCE by měl v
rozhodování vyhrávat nad "nosič zůstane stát a zablokuje souseda po ruce" -- oprava je
(a)+(b)+(c) výše, ne předpoklad k ověření.

**ZOBECNĚNÍ -- stejný vzorec potvrzen i na obranné straně, u jiné rasy (doplněno od
uživatele 2026-07-16, ze situace 5):** situace 5 (`g0007.json.gz`, Skaven vs Dwarf) ukázala
zrcadlový případ: Dwarf nechal rvačku na LOS (tahy 1-3, série bloků/kazualit/faulů)
pohltit VŠECHNY hráče místo udržení pokrytí šířky hřiště, soupeřův nosič unikl bokem a
doběhl nekontrolovaně do endzóny -- žádný trpaslík nebyl v dosahu, na nápravu bylo pozdě.
**Situace 4 (Orc, útok) + situace 5 (Dwarf, obrana) = stejný kořenový vzorec: AI dává
příležitostnému kontaktu/boji vyšší váhu než širší strategické pozici** -- ať je to
postup s míčem (útok) nebo udržení obranného pokrytí (obrana), ať je to Orc nebo Dwarf.
Není to rasa-specifická ani útok/obrana-specifická věc, ale obecná vlastnost
rozhodovacího mechanismu (search/heuristika preferuje bezprostředně viditelný/levný na
vyhodnocení kontakt před vzdálenější strategickou hodnotou). **Mělo by se to prošetřovat
společně** s `carrierStallAwareSteps`/macro-generation záhadou výše, ne jako dvě
oddělené věci -- diagnostický nástroj navržený tam (porovnání heuristického skóre
zvoleného BLOCK vs. alternativy) by měl pokrýt i tenhle obranný případ.

**Třetí instance stejného vzorce, jiná příčina -- honění akce místo rvačky (doplněno od
uživatele 2026-07-16, ze situace 6):** situace 6 (`g0009.json.gz`, Wood Elf vs Human)
ukázala další variantu: Human obrana se v tazích 2-3 celá stáhla za akcí (x=14-25,
y=3-9), čímž nechala **celý horní pruh hřiště (y=0-2) bez obrany**. Wood Elf to
systematicky vyzvěděl (4 podpůrní hráči poslaní přes y=1-2 v tahu 3) a v tahu 4 tudy
nosič proběhl nekontrolovaně do endzóny. Tady příčinou kolapsu pokrytí nebyla rvačka
(situace 5) ani ignorovaný postup (situace 4), ale **honění míče/akce vlastní obranou**
-- třetí odlišný mechanismus vedoucí ke stejnému výsledku (pokrytí zkolabuje do jednoho
pásu, soupeř to najde a využije). **Uživatel: tohle platí stejně i pro lidského hráče/
trenéra** -- obrana by se nikdy neměla nechat celá stáhnout za akcí; hlídat, aby vždy
zůstalo pokrytí přes celou šířku hřiště, protože soupeř mezery aktivně vyhledává.

**Čtvrtá instance -- PICKUP vs BLOCK, zpřesněno na "dokončí polovinu sekvence" (doplněno
od uživatele 2026-07-16, ze situace 7):** situace 7 (`g0003.json.gz`, Dwarf vs Wood Elf) --
volný míč ležel 2 celé tahy neošetřený. Přesný přepočet: pole s míčem mělo **2 nepřátelské
tackle zóny** (ne "volně dostupný pickup", jak zněla první verze) -- přímý pokus o pickup
by nesl -2 modifikátor. Oba týmy ale **správně provedly první půlku chytré sekvence**:
každý tým ve svém tahu zablokoval jednoho ze dvou markerů na míči (away `BLOCK id18→id6`,
home `BLOCK id3→id18` o tah později), čímž snížil TZ na míči z 2 na 1 -- přesně princip
"nejdřív odsunout soupeře, pak sebrat s nižší penaltou". **Ani jeden tým ale sekvenci
nedokončil skutečným pickupem** se sníženou penaltou, oba jen pokračovali něčím jiným.
**Čtvrtý mechanismus stejného kořenového vzorce** (situace 4: ADVANCE vs BLOCK; situace
5: obranné pokrytí vs rvačka; situace 6: obranné pokrytí vs honění akce; situace 7:
PICKUP vs BLOCK) -- tady konkrétně ve variantě "umí polovinu správné vícekrokové
sekvence, nedotáhne ji do konce", ne prostá lhostejnost k volnému míči. Přímo souvisí i
s 07-14 mining nálezem (40.8 % hranic tahů v nil-nil hrách má míč na zemi).

**I bashery (Orc) by měly posílat receiver-kandidáta dopředu, ne jen bojovat o pozici
(doplněno od uživatele 2026-07-16):** i když Orc normálně hraje cage grind bez pasové
hry (viz tabulka výše), princip "poslat aspoň jednoho hráče dopředu jako receiver
option" by měl platit i pro ně -- viz nález výše (`is_free_receiver`/situace 1: nikdo
posunutý dopředu od tahu 1) a Orc short-pass+handoff kombinace, co se v datech vůbec
nevyužívá. I bashery by z posunutého receivera profitovaly (rychlejší skóre, když se
naskytne příležitost), ne jen agility rasy jako Wood Elf.

**Promarněná příležitost byla už dřív, ne jen "pozdě si uvědomili" (doplněno od
uživatele 2026-07-16):** už v tahu 4 (5 tahů ještě zbývalo: 4-8) bylo skórování s
Orc MA5-6 reálně dosažitelné (x=8-12 → x=25 zvládnutelné cca za 3 tahy i s GFI) --
místo rozjetí drivu ale Orc celý tah 4 strávil jen bojem o pozici u vlastní
endzóny (bloky/protiblity, viz níže). Selhání tedy není otázka nedostatku času do
konce poločasu, ale toho, že se drive k soupeřově endzóně vůbec nezačal
rozjíždět, i když k tomu byl prostor.

**Cage/formace u hranice hřiště — u které endzóny záleží (doplněno od uživatele 2026-07-16,
ze situace 4):** sideline cage (formace natlačená k okraji hřiště) dává smysl jen když tlačí
směrem k **soupeřově (skórovací) endzóně** — omezuje soupeři úhly útoku na nosiče těsně před
skórováním. Ve situaci 4 (`g0001.json.gz`, Orc doma vs Skaven, poločas 2 tah 6, stav 1:0 pro
Orc) je ale formace natlačená k **vlastní** endzóně (x=0 — Orc skóroval v poločase 1 na
x=25, takže x=0 je jejich vlastní gólová čára, ne soupeřova). To je **špatná výchozí situace
sama o sobě**, ne promyšlená taktika: cage/formace u vlastní branky nemá tu výhodu (soupeř
už tak útočí jen z jedné strany, protože je za nosičem out-of-bounds/endzóna), ale ZATO
nedává nosiči žádný prostor uhnout, pokud se formace prolomí — kombinace "u vlastní endzóny"
+ "u okraje hřiště" nosiče doslova zahání do rohu bez únikových čtverců. Obzvlášť rizikové
proti Skavenu (vysoké MA, Dodge, dokážou rychle obklíčit i omezené úhly). Se stavem 1:0 pro
Orc není důvod takhle riskovat — bezpečnější by bylo držet nosiče blíž středu hřiště, kde má
víc únikových směrů. **Implikace pro per-player/formation features:** rozlišovat "cage u
vlastní endzóny" od "cage u soupeřovy endzóny" — ne jen "cage u okraje hřiště" jako jednu
kategorii, protože taktická hodnota je opačná.

**Implikace pro per-player features:**
- Model musí vidět: je daný hráč v diagonále nosiče? (= cage corner slot)
- Model musí vidět: je hráč v dosahu soupeřovy endzóny s míčem v ruce? (= scoring threat)
- Model musí vidět: je receiver volný (0 TZ) a blízko endzóny? (= pass/handoff target)
- Pass range per hráč: MA+AG kombinace určuje reálný dosah přihrávky
- Blitzer identifikace: hráč bez míče s dostatečným MA + Wrestle/Block + není engagovaný

---

## Okruh 2: Skill interakce — klíčové pro per-player features

### Blok a kontakt

| Situace | Relevantní skills | Taktická implikace |
|---------|------------------|-------------------|
| Blitz na nosiče s Block | **Wrestle** > Block | Both Down hodí oba na zem → míč volný bez ohledu na Block nosiče |
| Nosič s Block a bez Wrestle útočníka | Block chrání nosiče | Push nebo Defender Down, ale ne Both Down |
| Blitz + Strip Ball | **Strip Ball** + Wrestle | Strip Ball shodí míč při push-back, Wrestle při Both Down → 83% šance dostat míč z Block nosiče jedním blokem |
| Nosič s Sure Hands | **Sure Hands** kontruje Strip Ball | Sure Hands přímo zruší Strip Ball efekt |
| Agilní hráč v tackle zóně | **Tackle** ruší Dodge | Tackle neguje Dodge při opouštění TZ i při Stumble výsledku |

### Poziční a asistenční interakce

| Skill | Efekt | Taktická implikace pro AI |
|-------|-------|--------------------------|
| **Guard** | Přidává asistenci bez ohledu na počet soupeřů v dosahu | Jeden Guard hráč zásadně mění bilanci bloků v okolí — jeho pozice je kritická |
| **Frenzy** | Nutí follow-up po push-back | Past: Frenzy hráč u 2+ soupeřů dostane counter-blok — AI musí vidět tuto hrozbu |
| **Side Step** | Push-back na hráčovu volbu místo útočníkovy | Kontruje crowd-surf a chain-push — AI musí vědět, že push nemusí jít kam čeká |
| **Stand Firm** | Odmítá push-back | Drží pozici v cage — klíčové pro cage breakdown analýzu |
| **Dauntless** | Dočasné vyrovnání ST rozdílu | Umožňuje útok na silnějšího hráče — AI by měla vědět, kdo Dauntless má |

### Výpočet asistencí — pro AI jednoznačné, pro lidi matoucí (doplněno 2026-06-09)

Asistence určují počet kostek bloku. Lidé je často počítají špatně — AI může být v tomto přesnější.

**Algoritmus pro každý možný blok (útočník A útočí na obránce D):**

```
offensive_assists = počet mých hráčů P kde:
    P je adjacent k D
    AND P ≠ A
    AND (P má Guard OR P není v TZ žádného jiného soupeře)

defensive_assists = počet soupeřových hráčů Q kde:
    Q je adjacent k A
    AND (Q má Guard OR Q není v TZ žádného jiného mého hráče)

net = (A.ST + offensive_assists) − (D.ST + defensive_assists)
```

**Výsledek → počet kostek:**

| net | Kostky |
|-----|--------|
| +2 nebo více | 3 kostky, útočník vybírá výsledek |
| +1 | 2 kostky, útočník vybírá |
| 0 | 1 kostka |
| -1 | 2 kostky, **obránce** vybírá |
| -2 nebo méně | 3 kostky, obránce vybírá |

**Kde lidé chybují:**
1. Zapomenou odečíst asistenta který je v TZ jiného soupeře (kromě Guard)
2. Nezapočítají soupeřovy obranné asistenty
3. Přehlédnou Guard hráče soupeře kousek dál

**Implikace pro per-player features:**
- `block_dice_available[A→D]` = předpočítaný výsledek pro každou kombinaci útočník→obránce
- Alternativa: dát síti raw `(A.ST, D.ST, A.Guard, D.Guard, A.adj_free_count, D.adj_free_count)` a nechat ji odvodit
- Doporučení Team 1: explicitně předpočítat `net_st_for_block` per hráč jako scalar — nejdůležitější signal pro rozhodnutí "blokovat nebo ne"

> ⚠️ **BB2016 ověřit:** algoritmus asistencí výše je konzistentní mezi edicemi pro blokování. Rozdíly:
> - Guard na **fouly**: v BB2020 Guard pomáhá i při foulování — v BB2016 ověřit v engine kódu
> - Mighty Blow + Claw: v BB2020 platí jen když skilled hráč **útočí** (ne brání) — ověřit v BB2016

### Casualty kombinace

| Combo | Efekt |
|-------|-------|
| **Mighty Blow + Claw** | Claw: armor break na 8+ bez ohledu na AV; Mighty Blow: +1 k armor/injury → nejsmrtelnější kombinace v hře |
| **Mighty Blow + Dirty Player** | Efektivní při foulování — kumulativní bonus |

### Big Guy skilly (BoneHead/Loner/WildAnimal) — riziko aktivace vs. pasivní hodnota (doplněno od uživatele 2026-07-15)

Nález ze situace 1 (`evidence/fable_perplayer_replay_grounding_20260715.md`): Human Ogre
(ST5, MightyBlow, Block, ale i **BoneHead**) otevírá hru blokem. Samotný blok byl bezpečný
(ST5 + MightyBlow + Block skill = nízké riziko výsledku) — ale **aktivace Ogra sama o sobě**
nese riziko: BoneHead vyžaduje kontrolu (confusion check) při KAŽDÉ aktivaci, nezávisle na
tom, co pak dělá. V pozorovaném tahu kontrola prošla, ale nemusí vždy.

**Princip:** Big Guy s BoneHead/Loner/WildAnimal by se měl aktivovat jen když je to opravdu
nutné pro to, co se ten tah chce udělat — ne jako vedlejší efekt (např. "uvolnit mu cestu,
ať kolem něj projdou ostatní" NENÍ dost silný důvod riskovat confusion check). **Pasivní
hodnota beze aktivace:** Big Guy stojící na místě pořád vytváří tackle zónu a funguje jako
"blokovací zeď" pro soupeře — tuhle hodnotu dostanete zdarma, bez rizika, jen tím, že ho
necháte stát. Aktivovat ho stojí za riziko jen když je konkrétní odměna (silný blok, klíčový
pohyb) dost velká, aby vyvážila šanci na confusion.

**Implikace pro per-player features:** `has_bighuy_skill` (BoneHead/Loner/WildAnimal) per
hráč + informace, jestli byl tenhle tah aktivován — model by měl umět rozlišit "aktivuji ho,
protože musím" od "aktivuji ho navíc, i když bych nemusel".

**Propojení s kickoff deploymentem (doplněno od uživatele 2026-07-15):** pokud má Big Guy
sloužit jako pasivní "zeď", měla by ho tam **sestava na kickoff rovnou postavit** — ne ho
tam muset tahem 1 dotahovat/aktivovat (přesně to, co se stalo v situaci 1: Ogre byl aktivován
hned na začátku, i když jeho hodnota jako blokovací zdi by fungovala i beze změny pozice).
Patří to ke stejnému principu jako "Kickoff deployment — hloubka pro deep kick" výše
(Okruh 1): deployment by měl počítat s tím, kteří hráči budou pasivní/aktivní od první
vteřiny hry, ne to řešit až za pochodu prvním tahem.

**Doplnění: skutečný zdroj bezpečnosti bloku není Block skill, ale attacker-choice dice
(doplněno od uživatele 2026-07-16).** Výše uvedená situace 1 popisovala blok jako bezpečný
částečně díky Block skillu Ogra, ale to je nepřesné zdůvodnění. I bez Block skillu je Ogrův
blok proti slabším soupeřům (typicky Orkům) bezpečný, pokud je díky ST5 + assistům
vyhodnocen jako **3-dice (nebo obecně attacker-choice)** — na attacker-choice kostkách si
útočník může vždy odmítnout "Both Down" a vybrat jiný výsledek, takže riziko srážky Ogra
samotného je fakticky nulové bez ohledu na to, jestli Block skill má. Block skill se stává
nutným až tam, kde je blok **defender-choice** (soupeř má vyšší efektivní sílu) nebo
1-die (žádná bezpečná volba). **Princip pro aktivaci Big Guye:** než ho aktivovat na blok,
ověřit, že assist matematika dává attacker-choice — ne spoléhat na to, že Block skill
riziko pokryje, a ne se spokojit s tím, že "měl Block, takže to bylo OK" jako v situaci 1.
Konkrétní empirický check (rozložení počtu kostek u všech bloků iniciovaných Big Guyem/Ogrem
bez Block skillu napříč 150 hrami) **zatím neproběhl** — TODO, pokud bude potřeba ověřit,
jak často k tomu v praxi dochází.

**Doplnění: preferovat konkrétně 3 kostky, ne jen "attacker-choice" obecně (doplněno od
uživatele 2026-07-16).** Když blokuje Ogre (nebo jiný Big Guy), cílit na **3-dice**, ne se
spokojit s marginálním 2-dice attacker-favor. Důvod navazuje na BoneHead/Loner riziko výše:
protože každá aktivace Big Guye stojí confusion-check riziko, když už se riskuje, vyplatí se
tu aktivaci maximálně zajistit extra assistem navíc (2-dice → 3-dice) místo úspory pohybu.
**Princip pro per-player/macro selection:** při výběru bloku pro Big Guye preferovat tu
variantu (cíl/pořadí aktivace), která dosáhne 3-dice přes dostupné assisty, i za cenu
menšího pohybu navíc k zajištění assistu.

### Pohybové skills

| Skill | Efekt | Relevance |
|-------|-------|-----------|
| **Sprint + Sure Feet** | +1 rush + re-roll rushe | Rychlý hráč dostane míč/skóruje z větší vzdálenosti než MA napovídá |
| **Jump Up** | Vstávání zdarma, blok vleže | Padlý hráč s Jump Up je stále hrozba |
| **Leap** | Přeskočení hráče | Průnik přes obrannou linii bez potřeby dodge |
| **Catch** | Re-roll chycení | Zvyšuje spolehlivost přihrávkové hry |
| **Pass** | Re-roll přihrávky | Přihrávková strategie je viabilní jen s Pass |
| **Pass Block** | Pohyb až 3 čtverce před intercept rollem | Viz sekce níže — rozšiřuje intercept zónu |

### Passing mechanics — BB2016 (LRB6), potvrzeno

**AG tabulka (thrower):**

| Vzdálenost | Mod hodu | Dosah (čtverce) |
|-----------|----------|-----------------|
| Quick Pass | +1 | 1–3 |
| Short Pass | 0 | 4–6 |
| Long Pass | −1 | 7–10 |
| Long Bomb | −2 | 11+ |
| Per enemy TZ na throweru | −1 každá | — |

Základní hod = AG tabulka (AG3 → 4+, AG4 → 3+, atd.). Výsledný hod = základní + modifikátory.

**Catching modifiers:**
- Accurate pass nebo hand-off: +1
- Scattered/bouncing: +0
- Per enemy TZ na catcheru: −1 každá

**Správný postup pro přihrávkovou sekvenci (doplněno od uživatele 2026-07-16):** než
přihrát, vyčistit tackle zóny od soupeřů **na OBOU koncích** — throwerovi i receiverovi
(příjemci) — ne jen jednomu z nich. Oba mají svůj vlastní "−1 per enemy TZ" postih (viz
tabulky výše), takže odblokování jen throwera nebo jen receivera pořád nechává tu druhou
penaltu aktivní. Teprve po vyčištění obou stran má smysl skutečně hodit. **Implikace pro
`has_clear_pass_lane`/multi-step sekvence:** feature/plán by měl explicitně sledovat
enemy TZ count na throweru I na receiveru zvlášť, ne jen jednu souhrnnou "je cesta
volná" hodnotu.

**Interception — BB2016:**
- Základní modifikátor: **−2** (AG test)
- Per enemy TZ na **interceptoru**: **−1 každá**
- Kdo může interceptovat: hráč pod Range Rulerem + blíže throweru i targetu než jsou od sebe navzájem + má TZ (není prone/stunned)
- BB2016: **pouze intercept** (žádný "Deflect" — to přišlo v BB2020)

**Pass Block skill — kritické pro `has_clear_pass_lane`:**
- Hráč s Pass Block se pohne **až 3 čtverce** v momentě kdy soupeř ohlásí pass — PŘED kontrolou intercept rollu
- Musí skončit v: pozici pro intercept, NEBO na cílovém čtverci, NEBO s TZ na throweru/catcheru
- Dodge rolly při pohybu platí normálně — soupeřovy TZ zdražují pohyb
- **Implikace pro feature:** `has_clear_pass_lane` musí uvažovat i hráče s Pass Block kteří jsou do 3 čtverců od pass lane — ne jen ty kteří jsou přímo pod ní

> ⚠️ **BB2016 vs BB2020 — passing:**
> - BB2016: přesnost závisí na **AG** (číselná hodnota) — žádná samostatná PA statistika
> - BB2020: přibyla PA (Passing Ability) jako samostatný stat
> - Implementovat podle engine kódu, ne online zdrojů

---

## Okruh 3: Rasové rozdíly

### Základní charakteristiky 4 trénovaných ras

| Rasa | MA | ST | AG | Klíčové skills | Styl hry |
|------|----|----|-----|----------------|----------|
| **Human** | 6 | 3 | 3 | Block, různorodá sestava | Vyvážený, cage i passing |
| **Orc** | 5-6 | 3-4 | 2-3 | Block, MightyBlow, Tackle | Fyzická hra, cage, attrition |
| **Dwarf** | 4-5 | 3-4 | 2 | Block, Guard, Tackle, Stand Firm | Pomalý cage, neprolomitelný, žádný Dodge |
| **Skaven** | 7-9 | 2-3 | 3 | Sprint, Sure Feet, Dodge | Rush, vyhýbání kontaktu, nehrají cage |
| **Wood Elf** | 7-8 | 2-3 | 4 | Dodge, Catch, Pass, Leap | Agility, passing, žádný kontakt |

### Otázky pro Team 1
- Má smysl dát rasový kontext jako explicitní feature (one-hot encoding rasy soupeře)?
- Nebo jsou rasové rozdíly dostatečně zachyceny přes per-player stats (MA, ST, AG, AV) a skills?

### Princip: důležitost featury je rasově podmíněná (doplněno od uživatele 2026-07-15)

Kandidátní featury nemají napříč rasami stejnou váhu — u Wood Elf je passing/
mobilita **primární zbraní** (viz tabulka výše), u Dwarf/Orc je to okrajové
nebo irelevantní (Dwarf nemá vůbec pass). Důsledek pro validaci: agregátní
míra "jak často se situace X vyskytuje" přes všech 5 ras může zaretušovat, že
featura je u JEDNÉ rasy zásadní a u zbytku nepodstatná. Konkrétní příklad:
"relativní mobilní výhoda spoluhráče" (viz revidovaná `is_free_receiver`
níže) — u Wood Elf by měla být jak častější (vysoký MA/AG roster), tak
klíčovější pro herní úspěch, než u ostatních ras. Validaci kandidátů proto
rozpadat i podle rasového matchupu, ne jen poolovaně — a explicitně sledovat,
jestli AI selhává STEJNĚ napříč rasami i tam, kde by daná featura měla být
rasa-specificky zásadní (to by byl silnější nález než poolované číslo samo).
- Jak modelovat rasové skill tendence, které se projeví až v průběhu sezóny (levely, skills pick)?

### Taktické styly podle typu rasy (z taktických zdrojů)

| Typ | Rasy | Styl | Co AI musí umět jinak |
|-----|------|------|-----------------------|
| **Slow plodding cage** | Dwarf, Orc, Nurgle | Pomalu posouvat cage, 8 tahů na TD | Rozpoznat neprolomitelný cage → defenzivní screen |
| **Fast agile cage** | Dark Elf, High Elf, Skaven | Rychlý pohyb, passing play jako záloha | Reagovat na rychlou změnu strategie |
| **Running/hybrid** | Human, Undead, Norse | Mix fyzické hry a flexibility | Čtení soupeřova plánu, adaptace |

### Závěr: race jako explicitní feature?

**Ne — s per-player features se rasová identita vyřeší implicitně.**

Od tahu 1 jsou viditelné MA, ST, AG, AV a skills každého hráče. Model uvidí:
- 11 soupeřových hráčů s MA=4-5, Guard, Stand Firm → pozná dwarfe
- 11 soupeřových hráčů s MA=7-9, Dodge → pozná skaveny

`RosterSpeed` (SLOW/MIXED/FAST) je v game state a mohl by být přidán jako pomocný feature, ale není kritický pokud máme per-player MA.

**Otázka pro Team 1:** Stojí za to přidat `opp_roster_speed` (1 feature, 3 hodnoty) jako explicitní zkratku pro rychlejší konvergenci, nebo to per-player stats pokryjí dostatečně?

### Bimodální rasy jako kritický validační case (doplněno od uživatele 2026-07-15)

**Příklad: Lizardmen (6× Saurus ST6/AG1/MA6 + 6× Skink ST2/AG3/MA8).** Dnešní
agregátní featury (průměry přes 22 hráčů) jsou lossy hlavně pro **vyvážené**
rozdělení statů v týmu (Human apod.) — u bimodálního rozdělení jako Lizardmen
je průměrování obzvlášť škodlivé: průměr ST(Saurus, Skink) ≈ 4 nepopisuje ani
jeden typ hráče. Per-player featury by tohle měly opravit implicitně (stejná
logika jako rozpoznání rasy soupeře z per-player statů, viz výše) — síť by
viděla `ST6/AG1` (cage corner/blocker profil) vs `ST2/AG3/MA8` (nosič/receiver,
vyhýbat se kontaktu) přímo na úrovni jednotlivých slotů, bez explicitní rasové
nálepky. `net_st_for_block` by tu byl obzvlášť diagnostický — otázka "kdo dá
blok a s kolika kostkami" je u bimodálního týmu mnohem ostřejší než u
vyrovnaného.

**Důležitá podmínka:** tenhle přínos je zatím jen principiální — Lizardmen
(nebo jiná výrazně bimodální rasa) NENÍ v dnešním trénovaném poolu ras
(cyklus period-5 v gatingu: Human/Orc/Dwarf/Skaven/Wood Elf). Bez přidání
bimodální rasy do self-play rotace by per-player featury tenhle case nikdy
neviděly natrénovat, a validace (Fáze A i pozdější B/C benchmark) by tenhle
přínos vůbec nezachytila.

**Akční položka pro validaci per-player plánu:** zvážit přidání Lizardmen
(nebo podobně bimodální rasy) do tréninkového/gating poolu jako součást
Fáze A/B validace — je to zásadní stress-test toho, jestli per-player
featury skutečně řeší to, co mají řešit, ne jen okrajové vylepšení.

**Explicitně ODLOŽENO (doplněno od uživatele 2026-07-15): Stunty týmy
(Halfling, Goblin — Really Stupid, secret weapons, jiné injury-roll
modifikátory) a Vampire (Bloodlust/thrall hunger mechanika)** se do
tréninkového poolu NEPŘIDÁVAJÍ, dokud model nezvládne dobře tradičnější
týmy (Human/Orc/Dwarf/Skaven/Wood Elf + bimodální Lizardmen test výše).
Důvod: tyhle rasy mají vlastní herní mechaniky nad rámec statů/skillů,
které by v rané fázi učení přidávaly šum navíc, ne čistý per-player
stres-test. Pořadí: tradiční rasy + Lizardmen nejdřív → stunty/Vampire
až jako samostatná pozdější fáze (vlastní feature/reward úvahy).

### Obranná formace z pohledu hráče (doplněno od uživatele 2026-06-09)

| Slot | Počet | Role | Požadované skills | Cíl |
|------|-------|------|-------------------|-----|
| **Ball blitzer** | 1 | Útočí na nosiče soupeře | **Wrestle** (+ Tackle proti Dodge nosičům) | Shodit nosiče → míč volný |
| **Ball catcher** | 1 | Zvedne míč po úspěšném blitzu | **Sure Hands** + volný (0 TZ) | Získat possession |
| **Defensive blocker** | 2–3 | Váže soupeřovy cage corners | Block, Guard | Brání reformování cage |
| **Elven wall** | dle situace | Screen mezi cage a mou endzónou | Dodge (přežití), mobilita | Blokuje přímou cestu k TD |
| **Killer blitzer** | 1 | Útočí na jiné hráče než nosiče | **Mighty Blow + Claw** | KO/injury → přesila na hřišti |
| **Fouler** | 1 | Fouluje padlé hráče soupeře | Dirty Player | Injury bez armor rollu |

**Klíčová obranná sekvence:**
1. Ball blitzer s Wrestle dosáhne na nosiče (MA >= dist, nebo dodge přes TZ)
2. Both Down → oba padají, míč volný
3. Ball catcher (volný, SureHands) v dosahu míče → pickup
4. Defensive blockers drží soupeřovy hráče engagované → brání counter-útoku
5. Killer blitzer mezitím útočí na jiné hráče → attrition výhoda do dalších tahů

**Implikace pro per-player features — obrana:**
- Ball blitzer: Wrestle=1 + může dosáhnout na nosiče (BFS flood-fill, ne Chebyshev)
- Ball catcher: SureHands=1 + tz_count=0 + vzdálenost k nosiči ≤ MA
- Killer blitzer: MightyBlow=1 + Claw=1 + target s nízkým AV nebo bez Block
- Fouler: v dosahu padlého/stunned soupeřova hráče
- Elven wall: hráč s Dodge mezi soupeřovým cage a mou endzónou, bez přímého kontaktu

**Foulovat vzácným skill-holderem je chyba -- kdo fouluje, na tom záleží (doplněno od
uživatele 2026-07-16, ze situace 9):** konkrétní příklad z replaye -- Orc **Thrower**
(Block+SureHands+**Pass**, jediný/hlavní přihrávač týmu) provedl foul na obyčejného
Linemana, místo aby tu akci udělal nahraditelný hráč. Riziko foulu (vyloučení, pokud
sudí chytí) je přijatelné u nahraditelného Linemana, ale u vzácného specialisty
(Thrower, Blitzer se vzácnou skill kombinací, Big Guy) je cena mnohem vyšší -- ztráta
takového hráče na zbytek zápasu bolí nesrovnatelně víc. **Implikace pro per-player
features:** `foul_candidate` výběr by měl explicitně penalizovat hráče se vzácnými/
nenahraditelnými skilly (Pass, Sure Hands na throweru, jakýkoliv Big Guy) jako
kandidáty na foul. **Pravidlo: vždy foulovat nejlevnějším/nejnahraditelnějším hráčem v
dosahu, nikdy cenným nebo unikátním** (specialista se vzácnou skill kombinací, Big
Guy) -- ne prvním, kdo je v dosahu padlého soupeře. Navazuje na už zapsaný "FOUL
overuse" nález (07-14 mining, situace 3) -- nejde jen o TO, že se fouluje moc často,
ale i o TO, kterým hráčem se riskuje.

**Sekvenční poznámka (doplněno od uživatele 2026-07-16): tohle patří AŽ DO per-player
práce, ne před ni.** Ověřeno v kódu -- `macro_actions.cpp:492-509` (FOUL generování) už
generuje kandidáta pro KAŽDÉHO způsobilého fouléra zvlášť (ne jen prvního nalezeného),
takže výběr MEZI kandidáty je čistě na hodnotové funkci -- a ta dnes vidí jen agregátní
70-dim featury, ne "tenhle konkrétní kandidát je můj jediný Pass-skill hráč". Bez
per-player rozlišení konkrétních skillů na konkrétních hráčích se tohle nedá spolehlivě
opravit jako izolovaná záplata. Validovat spolu s bimodální rasou (Saurus vs. Skink u
Lizardmen, viz [[project_bloodbowl_bimodal_race_case_20260715]]) -- foul-target výběr
je přesně ten typ úkolu, kde bimodální roster ukáže, jestli per-player featury fungují
i na netriviálním případu.

**`carrier_can_be_blitzed` — prioritní feature (doplněno 2026-06-09):**
- Nejdůležitější signal pro hodnotovou funkci: může soupeř příští tah dosáhnout na mého nosiče?
- Pro **nosiče**: kritické — nosič s blitzable=1 je v bezprostředním nebezpečí, V(s) musí penalizovat
- Pro **ostatní hráče**: nižší priorita — non-carrier hráči jsou většinou blitzable stejně, signal je méně rozlišující
- Implementace: `carrier_blitzable = any(can_reach_carrier[j] for j in opponents)` — využívá stejný BFS flood-fill
- Již existuje jako agregovaný feature 63 (`carrier_blitzable`) — v per-player verzi se stane explicitní součástí carrier slotu

**Ohrožení klece je asymetrické podle mobility skillů, ne agregátní počet (doplněno od
uživatele 2026-07-16, ze situace 11):** konkrétní příklad ukázal, že i když soupeř může
příští tah obklíčit celou klec (7/9 hráčů v dosahu), skutečné riziko je **jen pro
eskorty bez Dodge/Leap** (Lineman, Thrower) — nosič a eskorty s Dodge+Leap mají silný
únik i z plného obklíčení. **Implikace:** per-player "je tenhle hráč v ohrožení" signál
by měl vážit Dodge/Leap na TOM KONKRÉTNÍM hráči, ne se spokojit s agregátní "kolik
soupeřů dosáhne na klec" hodnotou — riziko klece jako celku je nerovnoměrně rozložené
mezi jejími členy.

**Výjimka — schování rychlých hráčů (doplněno 2026-06-09):**
- Rychlí hráči s vysokým MA (Skaven Gutter Runner MA9, Dodge) se záměrně umisťují mimo dosah soupeře
- Cíl: zůstat "neviditelný" (blitzable=0, 0 TZ, daleko od kontaktu) → sprint na volný míč nebo endzónu kdy soupeř nemůže reagovat
- **Platí hlavně při sestavení obranné formace** na začátku každé půle — v průběhu hry taktika mizí
- Implikace: `blitzable` pro vysoké MA hráče s Dodge je relevantní signal i mimo carrier slot (opak běžného pravidla)
- Otázka pro Team 1: stojí za to přidat `is_fast_runner` flag (MA≥7 + Dodge) jako součást per-player slotu?

---

## Okruh 4: Multi-step taktické sekvence

### Známé sekvence (z diskuze)

**Blitz na nosiče → pickup:**
1. Identifikuj útočníka s Wrestle + dostatečnou AG (pro dodge přes TZ) + dosah (MA)
2. Proveď blitz → Both Down → oba padají, míč volný
3. Identifikuj volného hráče s SureHands v dosahu míče
4. Pickup + bezpečný přenos

**Cage breakdown:**
1. Identifikuj nejslabší roh cage (nejnižší ST, bez Guard)
2. Blitz guard v rohu (nejlépe 2-dice)
3. Druhý hráč útočí na nosiče (teď bez plné cage ochrany)

**Crowd surf:**
1. Identifikuj soupeřova hráče u sideline (y=0 nebo y=14) bez Side Step
2. Blitz nebo block s push směrem ven
3. Hráč jde off-pitch → možná KO/injury bez armor rollu

**Vytvoření volného receivera blitzem (doplněno od uživatele 2026-07-15):**
1. Prosaď co nejvíc kandidátů na receivera (Catch/AG, ne nosič) přes obrannou linii dopředu — cíl je nastřádat víc potenciálních příjemců, ne jen jednoho
2. Najdi mezi nimi hráče s **přesně 1** soupeřem v tackle zóně (ne 0 — ten už je volný a nic dělat netřeba; ne 2+ — tam by blitz na jednoho pořád nechal druhou TZ)
3. Blitzni TOHOTO jediného hlídače pryč (push/knock down)
4. Receiver je teď 0 TZ = plně volný, ideální cíl pro handoff/pass v tomtéž nebo příštím tahu

Přímo navazuje na dnešní `is_free_receiver` revizi (relativní mobilní výhoda) a na `enemy_tz_count` per hráč (už v Opusově Q6 layoutu) — krok 2 je přesně dotaz "kdo má enemy_tz_count == 1", tedy levný filtr přes existující plánovanou featuru. Kandidát na test proti replay datům: děje se tohle v datech vůbec, nebo AI single-covered receivery nevyužívá stejně jako nevyužívá plně volné?

**Proražení hradby s vědomou obětí asistentů (doplněno od uživatele 2026-07-15):**
1. Situace: musíš prorazit soupeřovu obrannou hradbu (screen/blok hráčů), ale tvůj útočník na to sám ST-bilancí nestačí (je slabší)
2. Přiveď 2 pomocníky jako offensive assists (viz asistenční algoritmus, Okruh 2) — zvednou net-ST natolik, že blitz přes hradbu má šanci
3. **Vědomá oběť:** oba pomocníci se tím odhalí/exponují (ztrácí svou dosavadní krycí pozici, stanou se zranitelní příští tah)
4. Cíl proražení NENÍ ten jeden blok samotný — je to otevření cesty, kterou pak využijí VLASTNÍ receiveři (typicky Wood Elf) k postupu

**Doplnění — blok nemusí vyjít čistě (doplněno od uživatele 2026-07-15):** blitz na hradbu nemusí cíl srazit — výsledek může být jen Push (cíl zůstane stát, jen se posune), a i když sražen, hradbu často drží další hráči vedle. V obou případech musí receiver-kandidáti PROJÍT hradbou přes dodge (opustit tackle zónu zbylých obránců), ne jen využít prázdné místo — dodge je tedy standardní součást téhle sekvence, ne výjimka. Hráči s **Dodge skillem** mají tenhle dodge spolehlivý (podle uživatele cca 2+) — což je přesně proč tahle celá sekvence sedí na Wood Elf (mají Dodge jako klíčový skill, viz Okruh 3) mnohem líp než na rasách bez Dodge, kde by receiver-kandidát měl reálnou šanci dodge nezvládnout a upadnout uprostřed pokusu o průchod.

**OPRAVA (doplněno od uživatele 2026-07-15, koriguje předchozí odstavec):** "dvojí využití — pomocníci se rovnou stanou receivery" bylo mylné zjednodušení a je to v přímém rozporu s "půlkou cesty jako pastí" níže. Aby hráč poskytl asistenci, musí být ADJACENT k blokovanému soupeři — z definice tedy skončí V tackle zóně, ne mimo ni. Nejsou tedy "volní receiveři po proražení", jsou přesně v té rizikové pozici (exponovaní, ne free), a soupeřův mezitímní tah je může srazit dřív, než by se z nich receiver mohl stát. Aby se z pomocníka skutečně stal receiver, musel by v NĚJAKÉM DALŠÍM tahu ještě navíc vyjít z TZ (dodge nebo soupeřovo odstranění) — to není automatické pokračování stejné sekvence, je to samostatný, nejistý krok navíc.

**Celkové vyhodnocení (revidováno):** tah (blitz s asistencí přes hradbu) je pořád hodnotný kvůli samotnému proražení cesty pro JINÉ, už volné receivery — ale spoléhat na to, že se z asistentů samotných stanou receiveři, je optimistické zjednodušení, ne spolehlivý mechanismus. Cenu (exponovaní pomocníci) je třeba počítat jako reálné riziko, ne jako "bezplatný bonus".

Klíčové pro hodnotovou funkci: vyžaduje to spojit krátkodobě rizikový stav (2 exponovaní pomocníci, reálné riziko, ne bezplatný bonus) s dlouhodobým ziskem (otevřená cesta pro JINÉ, už volné receivery) — vícekrokový credit-assignment problém, stejná rodina jako nil_nil/cage-breakdown/receiver-deployment výše. Bez fungujícího TD(λ) nebo jinak propagovaného creditu model nemá jak spojit "poslal jsem 2 hráče do rizika" s "o pár tahů později skóroval receiver, co s tímhle blokem vůbec přímo nesouvisel".

### Univerzální princip: nikdo nechce dostat block (od uživatele)

**Platí pro všechny hráče a všechny rasy:**
- Hráč který skončí tah adjacent k soupeřovu silnému hráči → příští tah dostane block
- Pohyb "přijít k soupeři a nechat se praštit" = špatný tah bez ohledu na rasu

**Silné týmy (Dwarf, Orc, Chaos) řeší obranu přesilou:**
- Přivedou asistenty → 2-dice bloky na soupeřovy hráče → shazují na zem
- Soupeř **prone nebo stunned nemůže blokovat** — musí nejdřív vstát (stojí celý pohyb)
- Attrition: více soupeřů na zemi = volnější pohyb, méně TZ, méně counter-bloků
- **Výjimka: Jump Up** — hráč s Jump Up vstane zdarma a může hned blokovat → prone hráč s Jump Up je stále hrozba, nelze ho ignorovat

**Implikace pro per-player features — poziční riziko:**
- Pro každého hráče: kolik soupeřových hráčů ho může příští tah blokovat (a s kolika kostkami)?
- Hráč adjacent k soupeři s ST výhodou = vysoké poziční riziko
- "Bezpečný pohyb" = dostat se do cíle BEZ zbytečného adjacentního kontaktu se silným soupeřem
- Prone/stunned soupeř = neohrožuje (nemůže blokovat) — **výjimka: Jump Up** (vstane zdarma → blokuje)
- Toto je jeden z nejdůležitějších signálů pro hodnotovou funkci — V(s) musí oceňovat bezpečné pozice

**Půlka cesty jako past — timing receiver-deploymentu (doplněno od uživatele 2026-07-15):**
Navazuje na receiver-deployment nález (Okruh 5 / dnešní replay grounding) — nejde jen o TO, jestli se hráč pošle dopředu, ale JAK DALEKO. Pokud receiver-kandidát doběhne jen částečně a skončí tah pořád v tackle zóně soupeře, přijde na řadu SOUPEŘŮV tah dřív, než AI stihne v příštím vlastním tahu vyhodnotit "teď je volný, přihraj mu" — soupeř ho mezitím srazí. **Částečný postup je tedy hůř než žádný pohyb** (exponuje hráče bez realizace zisku), zatímco pohyb až do 0 TZ přežije do dalšího tahu. Feature `enemy_tz_count` po dokončení pohybu tedy musí být striktně 0, ne jen "nižší než předtím" — binární podmínka, ne škála.

### Otázky pro Team 1
- Jak zakódovat "volný hráč v dosahu míče po blitzu" jako feature? (vyžaduje lookahead)
- Jsou tyto sekvence zachytitelné hodnotovou funkcí V(s), nebo potřebujeme action features?
- Jak explicitně zakódovat "poziční riziko" každého hráče (počet soupeřů co ho mohou blokovat a s jakou ST výhodou)?

### Trpasličí L-pozice / sideline pin (doplněno od uživatele 2026-06-09)

Dwarfové formují vlastní hráče do tvaru L přičemž jednu stranu L tvoří sideline. Soupeřův nosič (nebo klíčový hráč) je chain pushem přesunut k sideline a pak obklíčen — sideline + 2 dwarfové tvoří L past.

```
SIDELINE (y=0 nebo y=14)
─────────────────────────
  D   N   .
  D   .   .

  D = dwarf hráč tvořící L
  N = soupeřův nosič pinnutý k sideline
```

**Sekvence L-pin:**
1. Guard hráč poskytuje asistenci → 2-dice block na soupeřova hráče
2. Chain push posune soupeře k sideline (y=0 nebo y=14)
3. Dwarf follow-up + druhý dwarf zaujme pozici → L uzavřeno
4. Postupné utahování dalšími tahy — více dwarfů se přidává do L
5. Výsledek: nosič potřebuje 3+ dodges pro únik, může házet jen 1-dice bloky; jeho tým je příliš daleko

**Proč funguje specificky pro dwarfe:**
- **Guard** masivně přítomný → 2-dice bloky i bez početní výhody pro vytvoření pinu
- **Stand Firm** → dwarfové z L nemůžou být vyblokováni zpět
- **Tackle** → nosič s Dodge nemůže snadno vyskočit
- Nízké MA nevadí — L je pozicová taktika, ne sprint

**Rozdíl od cage:**
- Cage chrání vlastního nosiče (defenzivní formace)
- L-pozice obklíčuje soupeřova hráče u sideline (ofenzivní/defenzivní past)

**Implikace pro per-player features:**
- Je soupeřův hráč u sideline (y=0/14)? → pinnable
- Kolik mých hráčů je adjacentních k soupeřovu nosiči u sideline?
- Počet escape routes nosiče (volné čtverce kam může jít) → čím méně, tím lepší L
- Guard hráči v dosahu → dostupné asistence pro udržení L

---

### Další sekvence z taktických zdrojů

**Cage advance (pomalý tým):**
1. Drž cage — 4 hráče na diagonálách kolem nosiče
2. Blitz/block soupeřovy hráče kteří blokují postup
3. Posuň cage o 1-2 čtverce dopředu
4. Opakuj — cíl: 8 tahů, 1 TD

**Wide receiver drive:**
1. Blitzer vyčistí obrannou linii (2-dice block)
2. Runner/Catcher se pohne po okraji hřiště (edge run)
3. Hand-off nebo pass na hráče blíže endzóně
4. Score

**Defenzivní screen:**
1. Rozmísti hráče mezi cage soupeře a svou endzónu
2. Neber kontakt — 1-2 čtverce mezera
3. Nutí soupeře dodgovat přes screen → vyšší šance turnoveru

**Lineman sacrifice (ofenzivní linie):**
1. Postav linemen na LOS
2. Po kickoffu jejich blocky zdržují soupeře
3. Zbytek týmu se formuje do cage za nimi

### Herní kontext (doplnit od uživatele)
- Jaké další 2-3 krokové sekvence jsou časté a důležité ze zkušenosti?
- Které z výše uvedených AI hraje nejhůře?

> **TODO: doplnit od uživatele + po replay analýze**

---

## Okruh 5: Situace kde AI dnes chybuje

### Metoda identifikace
Replay viewer (`python -m blood_bowl.replay_viewer --verbose --save=replay.json`) umožňuje přehrát hru tah po tahu s ASCII hřištěm. Analýza chyb proběhne po doběhnutí aktuálního tréninku.

### Známé systémové problémy
- **nil_nil rate ~44%** — pasivní hra, Nash equilibrium bez gólu; model se nenaučil prolomit soupeřovu cage
- **Cage breakdown** — uživatel identifikoval jako problematické (bez replay nelze detailně)
- **Pickup loose ball** — uživatel identifikoval jako problematické (bez replay nelze detailně)

### Otázky pro Team 1
- Jak per-player features pomohou s nil_nil problémem konkrétně?
- Cage breakdown vyžaduje vidět: ST každého cage guardu, Guard skill, vzdálenost mých blitzerů. Co dalšího?

> **TODO: doplnit po replay analýze**

---

## Technické otázky pro Team 1 — s odpověďmi

### ML/RL Architect

**Q1: Slot ordering**
> **Odpověď: Distance-based.** Slot 0 = ball carrier (nuly pokud nikdo). Sloty 1–10 = moji hráči seřazeni Chebyshev vzdáleností k míči. Sloty 11–21 = soupeřovi hráči seřazeni Chebyshev vzdáleností k míči. Off-pitch hráči: fixní slot zachován, `is_on_pitch=0` + zbytek nuly. Role-based ordering vyžaduje herní heuristiky které budou špatně v edge cases — distance ordering je deterministický a stabilní.

**Q2: Architektura pro ~492 vstupů**
> **Odpověď: 492→256→128→1**, sdílená první vrstva pro value head i policy head. Transformer NEpoužívat — MCTS volá hodnot. funkci ~400×/tah (~3.2M evaluací/epoch), transformer by byl 5× pomalejší. První vrstva 256 (ne 128) — vstupní prostor má husté cross-player interakce, menší první vrstva vytváří informační bottleneck.

**Q3: Warm start**
> **Odpověď: Náhodná inicializace, NE přenos vah.** W1 mapuje různé vstupní prostory — přenos by injektoval falešné prior. Výjimka: inicializovat output bias na `mean(old_output_weights)` — ušetří ~3–5 epoch kalibrace. LR schedule: 0.001 (epoch 1–3) → 0.0003 (4–10) → cosine decay na 0.0001.

**Q4: Interaction features**
> **Odpověď: 3 explicitní, zbytek implicitní.** Explicitně: `can_blitz_carrier[j]` per soupeř, `in_carrier_diagonal[i]` per můj hráč, `dist_to_ball` per hráč. Tyto tři kódují dvě nejdůležitější taktická rozhodnutí a jsou trivially computatable. Komplexní interakce (Wrestle vs Block combos, Guard assist chains) — nechat síť odvodit z raw per-player stats.

**Q5: nil_nil mechanika (bonus)**
> **Odpověď: nil_nil klesne z ~44% na ~10–15%, ale neeliminuje se.** Per-player features opravují informační asymetrii (síť teď vidí ST=2 cage corner bez Guardu). Zbytek (~10–15%) jsou skutečné poziční stalemates — draw penalty stále potřeba jako ortogonální fix reward asymetrie.

---

### C++ Engine Analyst

**Q1: Skills enum**
> **Odpověď: Všechny skills jsou v enumu** (`enums.h`, SKILL_COUNT=74, `std::bitset<128>`). Mylně považované za chybějící — všechny přítomné: Wrestle=36, Tackle=11, StripBall=9, JumpUp=29, Pass=6, Catch=1, Sprint=30, SureFeet=12, SideStep=7, StandFirm=8, Dauntless=18, Leap=42. Mezera je pouze v `feature_extractor.cpp` který je nepoužívá. `p.hasSkill(Wrestle)` funguje okamžitě bez změn v C++.

**Q2: BFS flood-fill pro `can_reach_carrier`**

Chebyshev aproximace (`MA - TZ_count >= dist`) je nedostatečná — hráč může obejít TZ šikmo vzad a stejně doběhnout, Chebyshev to nezachytí.

```
reachable_safe(player) = BFS z player.pos,
                          max_depth = player.MA,
                          blocked    = čtverce v soupeřově TZ

can_reach_carrier[j] = carrier.pos ∈ reachable_safe(player_j)
```

**Výjimka pro blitz:** cílový čtverec smí být v TZ — hráč tam vstupuje záměrně.

**Výkonnost:** 26×15 = 390 čtverců × 22 hráčů = ~8 600 operací → pod 1 ms. Stejný flood-fill pokryje `can_reach_loose_ball`, `can_score_this_turn`, `escape_routes_count`.

**`can_reach_loose_ball` musí nést kontestovanost, ne jen dosažitelnost (doplněno od uživatele 2026-07-15):**

Nález ze situace 3 replay walkthroughu (`evidence/fable_perplayer_replay_grounding_20260715.md`):
statistika "foul místo pickupu, když je volný míč do 2 polí od stojícího spoluhráče" (231
instancí/150 her) neřeší, jestli byl ten pickup reálně bezpečný. Konkrétní příklad: míč
obklopený 3 nepřátelskými tackle zónami, nejbližší hráč AG2 (Dwarf) — modifikátor -3 dělá
pickup prakticky nemožný, takže volba foulu místo pickupu byla pravděpodobně SPRÁVNÁ, ne bug.

**Dva parametry, co featura/analýza musí nést, ne jen binární dosažitelnost:**
1. **Počet nepřátelských TZ na poli s míčem** (kontestovanost) — 0 TZ je jiná situace než 3 TZ
2. **AG útočícího hráče** (a jestli má Sure Hands) — i **plně volný** pickup (0 TZ) není bez
   rizika; podle uživatele: "váhal bych s pickupem i kdyby byl míč volný" — báze úspěchu podle
   AG pořád existuje, riziko turnoveru z neúspěšného pickupu je reálné i bez TZ.

Bez těchhle dvou parametrů je "foul-near-loose-ball" statistika nespolehlivá jako důkaz chyby —
nutno rozlišit kontestovaný/rizikový pickup (foul může být správná volba) od skutečně volného,
nízkorizikového pickupu (tam by foul byl pravděpodobně chyba). Aplikuje se stejně na
`can_reach_loose_ball` jako featuru — měla by kódovat míru rizika, ne jen ano/ne.

**Zjemnění `carrier_blitzable` — rizikově vážená dosažitelnost (doplněno od uživatele 2026-07-15):**

Replay grounding (15.07., `evidence/fable_perplayer_replay_grounding_20260715.md`) potvrdil BFS-vs-Chebyshev mezeru měřením, v obou směrech:
- **Přehnané varování (hlavní, 22.6 % defender-to-move snímků):** Chebyshev řekne "blitzable", ale žádná bezpečná (TZ-vyhýbající se) cesta neexistuje — chování AI to potvrzuje (útok jen 3.5 % vs 62.1 % u skutečného ohrožení).
- **Přehlédnuté ohrožení (vzácné, 0.7 %, 12 případů):** Chebyshev řekne "bezpečné", ale BFS najde cestu, kterou Chebyshev minul.

**Navržený postup (fázovaně, ne najednou):**
1. **Teď (Fáze A/binární verze, už otestováno):** `carrier_blitzable` = existuje bezpečná (0-dodge) cesta — ano/ne. Tohle už ukázalo silný, chováním potvrzený signál.
2. **Později (Fáze B, C++ `canReachSquare()` dle Q2 výše — struktura `{reachable, reachableRisky, minCost, dodgeCount}` už počítala s touhle distinkcí):** rozšířit na rizikově váženou verzi — cesta vyžadující dodge(y) počítá jako "blitzable" jen pokud je SDRUŽENÁ pravděpodobnost úspěchu všech dodge rollů na cestě > 50 % (ne izolovaná pravděpodobnost jednoho hodu — dva dodge po 70 % dají dohromady ~49 %, tedy POD prahem). Platí pro obě chyby (přehnané i přehlédnuté varování), ne jen pro tu častější.
3. Práh 50 % je návrh, ne dogma — validovat proti tomu, jestli to lépe koreluje s pozorovaným chováním AI/soupeře, stejně jako se dnes validoval binární case.

**Q3: KO/injured hráči**
> **Odpověď: Fixní slot dle roster indexu** (1–22) + `on_pitch` flag jako první feature v slotu (1.0 = STANDING/PRONE/STUNNED, 0.0 = KO/INJURED/DEAD/OFF_PITCH) + zero-fill zbytku. Lepší než pure zero-fill — síť rozliší "slot prázdný" od "hráč na (0,0) s MA=0". `PlayerState` enum v enginu: STANDING, PRONE, STUNNED, KO, INJURED, DEAD, EJECTED, OFF_PITCH.

**Implementační náročnost: 2–3 dny.**

**`NUM_FEATURES` — jeden bod změny, žádný ruční resize (ověřeno 2026-06-09):**
- Konstanta definována na jednom místě: `feature_extractor.h:8` — `constexpr int NUM_FEATURES = 70`
- `StateLog` (`game_simulator.h:39`) drží `float features[NUM_FEATURES]` — automaticky poroste
- Všechna ostatní místa (`mcts.cpp` 3×, `macro_mcts.cpp` 2×, `policies.cpp`) taktéž — žádný ruční resize
- Stačí změnit `70` → nová hodnota a překompilovat
- **Jedna věc ověřit:** `POLICY_INPUT_SIZE = NUM_FEATURES + NUM_ACTION_FEATURES` (`policy_network.h:11`) — automaticky poroste; ověřit že Python strana načítá síť se správnou velikostí vstupní vrstvy

---

### Training Loop Expert

**Q1: Cold start hyperparametry**
> **Odpověď:** LR zvýšit na **0.0005** pro první 4–8 loopu (pak zpět na 0.0003). `BM_FLOOR=0.50` a `ANTI_REGRESSION=0.48` pro loopy 1–4 (jinak čerstvá síť neprojde gate). `GAMES_PER_EPOCH=80` pro prvních 6 loopu, pak zpět na 40.

**Q2: Konvergence**
> **Odpověď: 8–15 loopu** než model spolehlivě překoná random hráče na >60%. Při 80 her/epoch (~18h/loop): **4–9 týdnů**. Loopy 1–5 budou vykazovat near-random benchmark scores — nil_nil maskuje učení, očekávat to.

**Q3: TD(λ)**
> **Odpověď: Implementovaný ale neaktivní** — pipeline používá `mc_shaped` (`trainer.py` řádky 183–225, 466–530). Pro cold start doporučeno přepnout na `td_lambda` s **lambda=0.9** — propaguje credit ~9 tahů zpět, přímo adresuje nil_nil problém. Lambda se nemění s velikostí sítě.

**Q4: GAMES_PER_EPOCH scale-up**
> **Odpověď: 40→80 pro cold start** (prvních 6 loopu). Snižuje per-epoch variance z ±15% na ±11%. Diminishing returns nad ~120 her/epoch.

**Kritický objev:** Skutečný počet benchmark her = **200** (ne 400 — `BENCHMARK_MATCHES=400` se dělí napůl). SD=±3.5%, ANTI_REGRESSION=0.51 je 0.3 SD → statisticky nezachytitelný šum. Pro smysluplný gate potřeba **≥600 her**.

---

### Game Domain Expert

**Q1: Priorita skills**

| Priorita | Skills |
|----------|--------|
| **MUST HAVE** | Block, Dodge, Guard, Wrestle, Sure Hands |
| **HIGH** | Tackle, Strip Ball, Stand Firm, Side Step, Mighty Blow |
| **NICE TO HAVE** | Claw, Frenzy, Dauntless, Pass, Catch |
| **LOW** | JumpUp, Sprint, SureFeet, Leap |

Top 5 zdůvodnění: Block=carrier ochrana, Dodge=escape cost, Guard=assist bilance, Wrestle=Both Down trigger, SureHands=ball recovery. Bottom: JumpUp/Sprint/SureFeet jsou niche situační, Leap příliš specifický.

**Q2: Slot ordering**
> **Odpověď: Hybrid** — slot 0 = carrier (nebo nejbližší k míči pokud nikdo nemá míč), slot 1 = ball blitz kandidát (nejbližší soupeř s Wrestle/StripBall), zbytek dle vzdálenosti k míči. Pure distance ordering je jednodušší a konzistentní — AI se naučí že hráči na slotech 2–5 v clusteru kolem nosiče *jsou* cage.

**Q3: Ball scatter**
> **Odpověď: Stačí vědět "blitzable + SureHands collector přítomen".** Přidat `nearest_sure_hands_dist_to_carrier` jako skalár. Pre-computed scatter heatmap přidává ~8 values ale AI nemůže na fine-grained scatter reagovat.

**Chybějící kritické features (identifikováno Domain Expertem):**
- `can_be_blitzed_by_opponent` — per hráč, prioritní pro carrier slot
- `assist_count_for_block` → `net_st_for_block` per možný blok (viz sekce Asistence výše)
- `is_in_opponent_tz` — tz_count říká kolik TZ, ale ne čích
- `adjacent_to_sideline` — kritické pro L-pozici a crowd surf
- `has_clear_pass_lane` — bez toho AI systematicky běží místo hází

**Crowd-surf i o pole dál od kraje -- vícekroková sekvence (doplněno od uživatele
2026-07-16, ze situace 12):** crowd-surf nefunguje jen na hráče, co už stojí přímo na
kraji hřiště -- cíl **1 pole od kraje** lze taky vysurfovat vícekrokovou sekvencí:
nejdřív ho zatlačit o pole blíž ke kraji (push jako vedlejší efekt bloku, případně
zopakovaný blok/Frenzy follow-up), pak druhým blokem/pokračováním dotlačit přes hranici.
Vyžaduje to přípravu předem (blokující hráč musí být na správné straně, aby push
směřoval ke kraji, ne od něj). **Implikace pro per-player/multi-step features:**
`adjacent_to_sideline` by se mělo rozšířit na "vzdálenost od kraje ≤ 2" s ohledem na
směr možného pushe (odkud útočník útočí), ne jen binární "je na kraji ano/ne" -- rozšiřuje
to okno crowd-surf hrozby i na obranu (viz níže).

**Obrana proti vysurfování je stejně důležitá jako útok (doplněno od uživatele
2026-07-16):** vzhledem k výše uvedenému rozšíření hrozby na "≤2 pole od kraje" by se
per-player riziko mělo počítat i pro VLASTNÍ hráče (ne jen jako útočná příležitost proti
soupeři) -- vlastní hráč (zejména nosič nebo cenný specialista) by neměl zbytečně končit
pohyb blízko kraje hřiště, pokud tam nemá Stand Firm/Side Step jako pojistku (viz Side
Step řádek výše -- kontruje crowd-surf tím, že push jde na volbu bránícího se hráče, ne
útočníka).

**Pozor na SOUPEŘŮV Frenzy -- posune cíl o 2 pole jedinou akcí (doplněno od uživatele
2026-07-16).** Frenzy (`block_handler.cpp:510-511`) vynucuje povinný druhý blok, pokud po
prvním push/Both Down zůstanou útočník a cíl stojící a sousedící -- to znamená, že
**jeden Frenzy hráč může cíl odtlačit o 2 pole jedinou aktivací** (první push, pak
povinný follow-up push), ne postupně přes dva samostatné bloky/tahy jak jsem psal výše.
Konkrétní příklad z rosteru: **Skaven Rat Ogre** (`roster.cpp:69-70`, MA6/ST5/AG2,
**Frenzy**+MightyBlow+WildAnimal+PrehensileTail) -- Big Guy se silou i Frenzy dohromady,
přesně ten typ hráče, co dokáže vlastního hráče stojícího 2 pole od kraje samostatně
dostrkat ven. **Existující feature 61 (`frenzy_trap_risk`,
`feature_extractor.cpp:328-343`) tohle NEŘEŠÍ** -- měří jen riziko pro MÉ VLASTNÍ Frenzy
hráče (že se po follow-upu sám přetáhne mezi 2+ soupeře), ne hrozbu OD soupeřova Frenzy
na moje hráče. **Implikace pro per-player features:** potřeba nová featura --
"stojím do 2 polí od kraje A soupeř s Frenzy na mě dosáhne" -- odlišná od existující 61.
**Obrana:** Stand Firm (odmítne push úplně) nebo Side Step (vybere si směr pushe sám,
místo útočníkovy volby) na hráčích blízko kraje proti známým Frenzy soupeřům -- bez
jednoho z těch dvou skillů je hráč do 2 polí od kraje v reálném ohrožení jedním jediným
blokem.

---

## Stav doplnění

- [x] Skill interakce (z webu + diskuze)
- [x] Rasové charakteristiky (základní)
- [x] Technické otázky pro Team 1 — **zodpovězeno všemi 4 specialisty**
- [x] Role hráčů od uživatele
- [x] Obranná formace od uživatele
- [x] BFS pathfinding pro can_reach
- [x] Asistence — algoritmus
- [x] BB2016 varování
- [ ] Multi-step sekvence — doplnit od uživatele (okruh 4 TODO)
- [ ] Analýza chyb AI — čeká na replay (okruh 5)
