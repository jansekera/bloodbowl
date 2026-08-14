# Proč trpaslík neskóruje proti orkovi (86 TD / 750 her vs 451 proti skavenovi)

**14.08.2026 · Fable analýza.** Korpus `diag_replay_mine_20260813_big_data`
(3000 her, 750/matchup, brána OFF, HEAD e4b99ee). Definice **importovány**
z `diag_drive_failure_20260811.py` (kategorie A/B/C/D1/D2, příčiny ztrát,
tempo, odpor) a `diag_exposure_scan_20260812.py` (Board, REACH/REACH0/BLZ/
CCBAD/FB2, assists s Guardem). Očekávání pre-registrována PŘED výpočtem
(`scratchpad/orc_gap_20260814/PREREG.md`). Skripty + úplné výstupy:
`scratchpad/orc_gap_20260814/` (s1–s9, *.out). Hraniční efekty bootstrap
po HRÁCH. Snímek = začátek kola. **Kapitola §9 (oprava metodiky, doplněno
odpoledne) koriguje interpretaci §2a — čti ji spolu s §2.**

Kontrola konzistence: TD z headerů = TD z drivů (naše TD = přijímací A +
STEAL+TD): skaven 254+198=452≈451 · wood-elf 157+102=259≈260 · human
96+82=178 ✓ · orc 54+31=85≈86 (rozdíly = 9 známých TD-flag anomálií
z drives.txt, 0,4 %).

---

## 1) Kde přesně drive proti orkovi umírá

**Ne na začátku.** První držení proti orkovi je NEJDŘÍV ze všech ras
(kolo 2,25 vs 2,44 skaven, 2,93 welf; vzdálenost při 1. držení ~20,4–20,9
polí všude stejná; fumbly stejné). Hypotéza „nedostaneme se k míči" padá.

**Umírá uprostřed pole, v kontaktu, pod 2kostkovým blitzem.** Plné přijímací
drivy (≥7 kol, n=709 orc / 627 skaven):

| | orc | skaven | human | wood-elf |
|---|---|---|---|---|
| A skórovali | **6 %** | 31 % | 13 % | 20 % |
| C ztratili míč | **59 %** | 26 % | 45 % | 34 % |
| tempo s míčem (pole/kolo, všechny drivy) | **2,03** | 2,74 | 2,23 | 2,49 |
| odpor v koridoru (soupeřů) | **5,20** | 3,82 | 4,73 | 4,27 |
| ztraceno v kole / vzdál. od EZ | 6,25 / 13,4 | 5,74 / 12,3 | 6,0 / 12,8 | 5,9 / 13,4 |

Bootstrap po hrách: podíl C orc−skaven CI [0,288; 0,385] — robustní.
Příčina ztráty: „soupeřův blitz/blok srazil nosiče" 87 % orc vs 80 % skaven
(CI rozdílu [0,013; 0,131]) — **mix příčin se liší málo; liší se ČETNOST**.
Nosič je Runner ~90 % kol u všech ras (nosič není proměnná).

**Kdo sráží: Black Orc 60 %** (orc, n=971 sražení; Blitzer 25 %). Proti
skavenovi Lineman 50 % / GR 15 %. Nejpomalejší figura na hřišti (MA4)
dosahuje na našeho nosiče — protože nosič STOJÍ v jejím dosahu:

P(nosič↓ za soupeřovo kolo) podle vzdálenosti nejbližšího stojícího soupeře
na začátku jejich kola (s6, orc n=4854 kol):

| d | 1 | 2–3 | 4–6 | 7+ |
|---|---|---|---|---|
| orc | **0,511** | **0,239** | 0,072 | 0,002 |
| skaven | 0,334 | 0,155 | 0,099 | 0,029 |
| human | 0,415 | 0,184 | 0,089 | 0,015 |

Bootstrap po hrách (orc): P(↓|d=1) − P(↓|d≥4) CI [0,434; 0,520].
V pásmu d≤3 trávíme 52,7 % expozičních kol (skaven 50,0, human 57,7 —
**chováme se ke všem stejně**); z pásma d≤3 pochází ~90 % celkového rizika
sražení. A do kontaktu si lezeme SAMI (s7): z výchozí d≥4 náš tah zavře
vzdálenost k ST4+ o **−1,38 pole**, jejich tah jen −0,93; v 18 % „dalekých"
kol skončíme po VLASTNÍM tahu ≤3 od Black Orka.

I když nosič přežije: tempo 2,03 < 2,61 ⇒ D1+D2 dalších 34 % plných drivů.
A druhá půlka rozdílu v celkových TD: **krádeže**. Obranné drivy STEAL+TD:
skaven 18 % (198 TD), orc **4 %** (31 TD) — orkův nosič za zdí s AV9 nám
míč nedá.

## 2) Čím se ork liší od skavena — mechanismus, ne popis

**a) Cena kontaktu = kostky.** Nejlepší dostupný blitz na našeho nosiče
(BLZ, začátek soupeřova kola):

| | −2 | 1k | 2k | 3k | nedosáhne |
|---|---|---|---|---|---|
| orc | 0,0 % | 10,0 % | **68,0 %** | 0,3 % | 21,7 % |
| skaven | 4,4 % | **74,7 %** | 6,3 % | 0,0 % | 14,5 % |

P(nosič↓ | BLZ=2) ≈ 0,21–0,24 u všech ras, P(↓ | BLZ=1) ≈ 0,10–0,12 —
**konverze je všude stejná, liší se MIX kostek**. Ork má 4× ST4 (2 kostky na
ST3 Runnera bez jediné asistence), 6 těl s Guard a Block na 8 figurách.
Skaven si 2 kostky musí koupit asistencemi, které nemá. Řetěz TD je
monotónní: podíl kol s BLZ≥2 6→42→60→68 % ⇔ naše TD 451→260→178→86
(skaven→welf→human→orc). Ork není anomálie — je to **krajní bod téže
křivky**: doktrína toleruje kontakt paušálně a jeho cena roste s kostkami
soupeře.

**b) Attrition se obrací.** Stojící těla na začátku kola T8 (s3):

| matchup | my | oni | bilance |
|---|---|---|---|
| vs skaven | 7,0 | 4,1 | **+2,9** |
| vs wood-elf | 6,9 | 5,2 | +1,7 |
| vs human | 5,7 | 5,4 | +0,3 |
| vs orc | 4,9 | 6,4 | **−1,5** |

Proti skavenovi zeď rozebereme (AV break 44,8 %, 7,8 INJ/hru, 0,97 CAS) a
koridor se otevře (tempo 2,74, REACH None 14,5 %). Proti orkovi bijeme
nejméně (18,5 bloků/hru — jediný matchup, kde soupeř bije víc než my: 21,9)
do AV9 (break 24 %, 2,3 INJ, 0,27 CAS) — zeď stojí celý zápas a NÁS ubývá
(12,15 KD/hru na nás, jejich AD jen 2,4 % faces = bijí z kopce).

**c) Náš roster má proti orkovi mrtvou výbavu.** Tackle (na 10 figurách)
je proti rosteru bez Dodge bezcenné; Dauntless — náš jediný anti-ST4
nástroj — je v nabídce bloků vypnutý (viz §3). Proti skavenovi je to
naopak: Tackle žne Gutter Runnery a ST4 nepotřebujeme.

## 3) Dauntless offer-gap (doplněk zadání) — reálný bug, vedlejší kolej pro útok

Kód potvrzen: nabídka (`macro_actions.cpp getBlockDiceCount`) Dauntless
nezná, provedení (`block_handler.cpp:386`) ano. Pracovní strom už obsahuje
nezacommitovanou opravu (`dauntlessInOffer`, HEAD 4c6f1c7 `M
src/macro_actions.cpp`) — korpus (e4b99ee) je PŘED ní; nic jsem neměnil.

Měření (s4, statické snímky začátku našeho kola = **spodní odhad**, nabídky
vznikají i uvnitř tahu):

| race | kol se Slayerem | adj ST4+ | nabídnuto | GAP (s Dauntless ≥1k) | GAP/hru | z toho 2k |
|---|---|---|---|---|---|---|
| orc | 9 159 | 3 401 | 1 554 | **1 715** | **2,29** | **0** |
| human | 9 964 | 1 696 | 304 | 1 335 | 1,78 | 624 |
| wood-elf | 10 263 | 1 113 | 73 | 993 | 1,32 | 457 |
| skaven | 10 852 | 0 | — | 0 | 0 | — |

1. **Kolik bloků to bere:** proti orkovi 2,29 ztracených nabídek/hru
   (spodní odhad); Slayer v 90 % těch kol neblokoval vůbec (stojí vedle
   Black Orka a nedělá nic). Eventová kontrola: bloky Slayer→ST4+ dnes
   1839/750 her (jen přes převahu asistencí) — oprava by je ~zdvojnásobila.
2. **Cíle:** proti orkovi 100 % Black Orc +Guard+Block. GAP2k=0 — Guard
   Black Orků sráží čisté asistence, Dauntless dává 1 kostku, ne dvě.
3. **Souvisí to s umíráním drivu? PŘÍMO NE.** Jen **159 z 1715 (9 %)**
   gap situací nastává, když držíme míč (z nich 35 % do 2 polí od nosiče).
   91 % je v obraně/grindu. Oprava tedy nemíří na 59 % C — míří na
   attrition bilanci a krádeže. Říkám to rovnou: **není to odpověď na
   otázku dne, je to legitimní oprava vedle ní.**
4. **Strop:** 2,29 bloků/hru × (0,83 Dauntless × 1/3 KD) ≈ **+0,63 KD
   Black Orků/hru** (+7 % k našim 9,4 KD), × 24 % AV ≈ +0,15 INJ/hru.
   S Frenzy (druhý blok) a vnitro-tahovými případy snad ×1,5–2 ⇒ strop
   ~+1–1,5 KD/hru. Bilance těl v T8 je −1,5 ⇒ oprava ukousne část, nevrátí
   znaménko. **86→451 z toho nebude; 86→~110 možná.** Měřit po nasazení na
   obraně (steal-rate, KD na Black Orcích), ne na přijímacím A.

## 4) Hypotéza 3 (doktrína koreluje opačně) — VYVRÁCENA, ale s nálezem

Žádná kontrola proti orkovi nekoreluje opačně (s5, per-race Pearson):
REACH0×ztráta orc +0,42* / skaven +0,26*, CCBAD +0,21*/+0,15*, ESC
−0,18*/−0,06* — směr všude týž, proti orkovi SILNĚJŠÍ. Kontroly navíc
proti orkovi plníme stejně nebo líp (CCBAD 2,58 vs 2,87; REACH0 1,73 vs
1,76). **Problém není špatná kontrola, ale CHYBĚJÍCÍ kontrola:** checklist
nikde neměří vzdálenost nosiče od nejbližšího stojícího soupeře — a to je
nejsilnější prediktor v celém dnešním měření (0,511 vs 0,035). Doktrína
„klec ≤2 pole/kolo" tempo omezuje, ale kontakt nezakazuje — pomalá klec
PŘED zdí = permanentní 2kostkový blitz na nosiče.

## 5) Co změnit v doktríně — konkrétně

1. **R-DIST (nové, priorita): nosič nesmí KONČIT náš tah ≤3 pole od
   stojícího soupeře**, pokud tím nekupuje TD v tomto/příštím kole.
   Dosah zdi je mechanicky MA4+2=6, blitzerů 8: „stát na 4–6" je proti
   orkovi 0,072, „na 7+" 0,002, „na ≤3" 0,24–0,51. Dnes končíme ≤3
   v polovině kol a z dálky si kontakt zavíráme sami (−1,38 pole/kolo
   k ST4+). Implementačně: kontrola d_min na konci tahu + zákaz kroku
   nosiče do pásma ≤3, dokud BLZ soupeře ≥2.
2. **Tolerance kontaktu podle kostek, ne paušálně:** kontakt je přijatelný
   při BLZ≤1 (skaven: 0,124), ne při BLZ=2 (0,236). BLZ už počítáme
   (exposure scan) — udělat z něj vstup rozhodnutí, ne jen diagnostiku.
3. **Dauntless fix zacommitovat a změřit** (steal-rate + KD na ST4,
   obranná polovina hry). Blitz-cesta používá týž `getBlockDiceCount`
   (macro_actions.cpp:228,436) — oprava pokrývá i blitz nabídku.
4. **Cíle bloků proti orkovi:** bez Dauntlesse nemířit na ST4; bít ST3
   (Blitzer/Lineman/Thrower) a polluter rohů (per dnešní dirty-corner
   analýza). Attrition proti AV9 nevyhrajeme — cíl bití je INICIATIVA
   a čistota rohů, ne casualty.

Kauzální výhrada k R-DIST: P(↓|d) je korelační; blízkost je endogenní
(pod tlakem couváme hůř). Směr „kontakt→sražení" je ale mechanicky nutný
(blitz vyžaduje sousedství; 87 % ztrát jde přes blok) a s7 ukazuje, že
vzdálenost zavíráme převážně my. Rozhodne A/B s bránou R-DIST — efekt na
per-kolo riziku je tak velký (0,43–0,52), že vyleze i na stovkách kol;
na TD úrovni platí šumové dno ±5,3 pp/400 párů.

## 6) Je to opravitelné, nebo vlastnost matchupu?

Poctivě: **obojí, zhruba půl na půl.**
* **Vlastnost matchupu:** ST4+Guard+Block+AV9 zeď proti MA4/MA5 rosteru
  znamená, že (a) krádežové TD nebudou (198 skavení TD se proti orkovi
  nekoná z principu — to je 44 % skavení bilance), (b) attrition
  nevyhrajeme, (c) 2 kostky na nosiče jsou vždy na dosah. Parita se
  skavenem (451) je NEDOSAŽITELNÁ.
* **Naše chyba:** stejná blízkost soupeři, jakou si dovolujeme proti
  všem, stojí proti orkovi dvojnásobek — a my ji aktivně vyrábíme
  (s7). Human má skoro orčí profil kostek (BLZ≥2 60 %) a dává 178;
  drive proti orkovi umírá stejným mechanismem, jen o krok dřív.
  Kdyby R-DIST srazil čas v pásmu ≤3 na polovinu, per-kolo riziko klesne
  řádově o třetinu až polovinu — realistický cíl je pásmo **150–250
  TD/750**, tedy human-až-welf úroveň. Víc je neprokázané.

## 7) Co jsem NEZMĚŘIL a proč (povinné)

* **Kauzální efekt R-DIST** — vyžaduje A/B běh s bránou; dnes zákaz
  spouštění. Observačně neodliším „stojíme blízko, protože musíme" od
  „protože doktrína chce"; s7 je nejblíž, ale není to experiment.
* **Vnitro-tahové Dauntless příležitosti** — s4 měří jen snímky začátku
  kola (spodní odhad); plný účet chce replay pozic uvnitř tahu.
* **Zda by MCTS nabídnuté 1kostkové bloky bral** — priory/expanze neměřím
  z replayů; rozhodne až A/B po commitu opravy.
* **Rozklad P(↓|BLZ=2) na skilly** (Mighty Blow vs Block vs rerolly) —
  konverze vyšla mezi rasami podobná, takže by nezměnil závěr; nechal
  jsem to být.
* **Chování orka v zrcadle** (hraje náš engine za orka jinak proti nám než
  my proti němu?) — mimo dnešní otázku, ale symetrie by ověřila, kolik
  z rozdílu je roster vs doktrína: orc s touž doktrínou proti trpaslíkovi
  skóruje 207 (2,4× víc než my proti němu).
* **9 TD-flag anomálií** (0,4 %) — pod rozlišením všech efektů výše.

## 8) Pre-registrace vs výsledek

| # | očekávání | výsledek |
|---|---|---|
| 1 | podíl „blitz srazil" vyšší vs orc | ✓ slabě (87 vs 80 %, CI [0,01;0,13]) — hlavní rozdíl je četnost C, ne mix |
| 2 | BLZ≥2 častěji vs orc, skaven 1k | ✓✓ 68 % vs 6 % — pozor, interpretaci koriguje §9.1 |
| 3 | REACH0 nejistý směr | REACH0 se mezi orc/skaven neliší (1,73/1,76) — není to páka |
| 4 | bilance stojících ≥1,5 těla T8 | ✓ rozdíl bilancí orc vs skaven 4,4 těla (−1,5 vs +2,9) |
| 5 | naše AD vyšší vs orc, jejich DD/DS vyšší | ✓ (AD 4,7 % nejhorší; jejich AD 2,4 % nejlepší) |
| 6 | 1. držení stejné; ztráta DŘÍV a dál | ✓/✗ — držení dokonce nejdřív; ztráta POZDĚJI (6,25 vs 5,74), jen dál od EZ |
| 7 | tempo nejnižší vs orc | ✓ 2,03 |
| 8 | kontroly neplnitelné vs orc, ne opačné | ✗ obojí špatně: plníme je stejně a korelují stejným směrem (silněji) — chybí kontrola VZDÁLENOSTI |
| 9 | primární (a) kostky + (b) attrition, ne pozdní míč | ✓ + doplněk: polovina rozdílu TD je v krádežích (198 vs 31), které jsem v prereg neměl |

## 9) OPRAVA METODIKY (námitka uživatele 14.08.): hrozba přes všechny kanály

**Námitka:** „BLZ≥2 porovnává hrušky s jablky — měří silový kanál, kterým
hrozí jen ork, a ignoruje dovednostní (Wrestle, Strip Ball), kterým hrozí
skaven." Oprávněná. Postaven model hrozby na kostku bloku přes všechny
kanály (s8; q_down = DD+DS, +BD u útočníka s Wrestle bez Blocku
[block_handler.cpp:494, oba k zemi BEZ brnění], q_strip = PUSH×2 u Strip
Ballu proti nosiči bez Sure Hands [block_handler.cpp:627 negaci potvrzuje];
dosah/kostky/asistence beze změny z exposure scanu; skilly z roster.cpp).
THREAT_raw = bez odečtu Sure Hands, THREAT_net = s ním. Očekávání
pre-registrována před plným během (PREREG.md, addendum 10–13). Výstupy:
s8_threat_channels.out, s9_mirror_decomp.out.

### 9.1 Původní řetěz se ROZPADL — říkám to rovnou

| race | THREAT_net | THREAT_raw | P(nosič↓)emp | naše TD |
|---|---|---|---|---|
| human | 0,450 | 0,488 | 0,162 | 178 |
| orc | 0,424 | 0,455 | 0,173 | 86 |
| skaven | **0,400** | 0,441 | **0,125** | 451 |
| wood-elf | 0,413 | 0,512 | 0,151 | 260 |

Pravidlová hrozba na NAŠEHO nosiče je napříč rasami skoro stejná (skaven
dokonce nejnižší, ale jen o 0,05) a **při stejné hrozbě je konverze rasově
nezávislá** (pásmo THREAT 0,15–0,35: 0,090/0,095/0,105/0,090; n=451–2745).
Realizované riziko se liší jen 1,38× (0,125→0,173) — to přes skládání
a tempo unese kus rozdílu v C, ale ne 5,2× v TD. **Formulace „ork nás
ohrožuje víc" byla špatné pojmenování příčiny.** Řetěz „BLZ≥2 ⇔ TD" z §2a
tím padá jako VYSVĚTLENÍ; přežívá jen jako popis toho, kudy realizovaná
hrozba chodí. (Vzdálenostní žebříček 0,511/0,239/0,072/0,002, attrition
a R-DIST z §1–§5 platí dál — kontakt je drahý u všech ras.)

### 9.2 Skavenova hrozba je neutralizovaná — dvěma RŮZNÝMI mechanismy

* **Strip Ball (legitimní pravidlo):** stripper v dosahu nosiče syrově
  ve 26,4 % kol (skaven; welf 47,2 %), po Sure Hands **5,3 %** (welf 6,3 %)
  — **Sure Hands na Runnerovi bere ~80 % strip hrozby**. Empirie sedí:
  strip uspěl 0,05/hru, Sure Hands negace 0,11/hru. Engine správně
  (block_handler.cpp:627). ⇒ potvrzení doktríny „nosič = Runner": jeho
  Sure Hands je tiše nejcennější skill proti třem ze čtyř ras.
* **Wrestle (artefakt enginu, nová položka rodiny P13):** v modelu je 1k
  Wrestle skavenův NEJLEPŠÍ kanál v 51,1 % kol (q=3/6 na kostku, bez
  brnění, bez turnoveru). Realizace: ofenzivně 0,16 použití/hru, **49 BD
  sražení našeho nosiče za 750 her**; faces skutečných sražení: BD 0 %,
  DD+DS 100 %. Příčina v kódu: **rozhodovací vrstva Wrestle nezná** —
  `scoreFace` dá Wrestlerovi za BD skóre 1 („only att falls", omyl),
  `shouldRerollBlock` mu BD přehodí, nabídkový filtr 1k vyžaduje Block;
  `grep Wrestle` v macro_actions/action_features/policies = **0 výskytů**.
  Provedení skill umí (block_handler:494) — táž třída chyby jako Dauntless,
  na straně soupeře. (Defenzivně Wrestle firuje, 0,90/hru ve skaven hrách
  — proti NAŠIM blokům, automatika bez rozhodování; pozn.: Wrestle sráží
  bez KNOCKED_DOWN eventu, takže s1 příčina „srazil nosiče" těch 49
  případů nevidí — na závěrech nic nemění.)
* **Poctivý důsledek:** část našich 451 proti skavenovi je artefakt.
  Skaven s viděným Wrestle by měl na našeho nosiče reálný kanál v ~polovině
  kol; model dává strop (hrozba ~orc úrovně), realita bude níž (musí projít
  screen). Po opravě Wrestle náš náskok proti skavenovi KLESNE.

### 9.3 Nosný řetěz je ZRCADLOVÝ: ork si nosiče uhlídá, skaven ne

Naše kolo, jejich stojící nosič (s8 část C, s9):

| race | n | P(jejich nosič↓) | náš BLZ≥2 | STEAL+TD | naše TD |
|---|---|---|---|---|---|
| skaven | 4126 | **0,347** | **54,4 %** | 198 | 451 |
| wood-elf | 4374 | 0,266 | 43,4 % | 102 | 260 |
| human | 5157 | 0,183 | 19,0 % | 82 | 178 |
| orc | 5619 | **0,113** | **7,5 %** | 31 | 86 |

**Monotónní přes všechny čtyři rasy ve všech sloupcích.** Konverze při 2k
je všude vysoká (0,45–0,55) — když kostky máme, bereme; rozdíl je v tom,
jak často je máme. Přijímací rameno (tempo 2,03, odpor 5,20, attrition §2b)
běží přes tutéž veličinu — naše kostky na jejich TĚLA v koridoru. Jedna
proměnná („kdo má sílu kolem míče"), dvě ramena; zrcadlové je strmější
a čistší.

### 9.4 Čím si ork nosiče hlídá — struktura vs pozice (s9)

| složka | orc | skaven | příspěvek k rozdílu BLZ≥2 (46,9 pp) |
|---|---|---|---|
| **identita nosiče**: ST2 na hřišti | 0 % kol | 38,8 % kol (GR; BLZ≥2 pak 97 %) | **≈37,6 pp (~4/5)** |
| **pozice**: REACH=0 (nedosáhneme) | 27,8 % | 2,1 % | většina zbytku |
| dmin náš→jejich nosič | 5,09 | 2,52 | — |
| adjacentní blok možný | 10,1 % kol | 28,1 % | — |
| **clona**: Guard sousedé nosiče | 1,19 | 0,15 | — |
| BLZ≥2 při ST3+ a REACH>0 | 10,4 % | 27,9 % | ≈9,6 pp |
| … a Guard=0 | 13,3 % | 28,3 % | zbytek = nedostupné asistence |

⇒ **~4/5 zrcadlového rozdílu proti skavenovi je identita nosiče** (jejich
AI nechá nosit křehkého Guttera 39 % kol) — to není naše zásluha, to je
jejich slabost. Ork drží Throwera ST3+Block v 78 % kol, čtvrtinu kol mimo
náš dosah (MA5 za MA4 zdí), zbytek za Guard clonou; 1k tlak na něj
konvertuje 0,109/kolo (Block, BD bezpečné).

### 9.5 Kolik z toho vezme P13 (Dauntless) a P15 (nabídka slepá k ceně cíle)

* **P13 na steal rameno: nula.** Jejich nosič je ST3 (Thrower 78 %, ST4
  nenosí nikdy) — Dauntless proti ST3 vůbec nefiruje (podmínka defST>attST).
  Působí jen nepřímo přes zeď (koridor/attrition přijímacího ramene).
  **Strop 86→~110 z §3 platí beze změny i při čtení přes nový kanál.**
* **P15 na steal rameno: skoro nic.** Zóna „REACH>0 & BLZ<2" je vs orc
  64,7 % kol, ale z 98,2 % je to **BLZ=1 — a ty akce SE nabízejí**: blitz
  na nosiče vždy (carrier +10 ve skóre, macro_actions.cpp:447), 1k
  adjacentní blok taky (všichni naši mají Block). Skutečně nenabízený
  „do kopce" blok na nosiče je 1,8 % zóny. Limit vs orc je fyzika
  (ST3+Block nosič, Guard, dosah), ne nabídkový filtr. P15 zůstává
  relevantní jinde (cena cíle při volbě, ne generování).
* **Jediná oprava z této rodiny, která hýbe matchupovými poměry výrazně,
  je Wrestle do rozhodovací vrstvy (9.2) — a ta pomůže skavenovi proti
  nám, ne nám proti orkovi.**

### 9.6 Co se mění v závěrech §5–§6

Diagnóza příčiny se mění z „ork nás ohrožuje víc" na **„ork si svého
nosiče uhlídá (Thrower, dosah, Guard) a svého soupeře nutí hrát bez
kostek; skaven nám nosiče daruje (GR ST2, dosah 2,5)"**. Doporučení
R-DIST, tolerance kontaktu dle BLZ a Dauntless z §5 platí (přijímací
rameno). Nově navíc: (a) **Wrestle do rozhodovací vrstvy** — férovost
enginu, s vědomím, že sníží naše skóre proti skavenovi; (b) steal rameno
proti orkovi nemá levnou opravu — realistický zisk zůstává v přijímacím
rameni (§5), odhad 150–250 TD/750 se nemění, jen jeho těžiště.

### 9.7 Doplňky k „nezměřeno"

* uptake nabízených 1k blitzů na jejich nosiče v MCTS (z replayů vidím
  nabídku a výsledek, ne rozhodnutí);
* kolik by skaven reálně vytěžil z opraveného Wrestle (chce A/B);
* proč skavení AI nechává nosit GR 39 % kol (jejich doktrína — mimo rozsah).

### 9.8 Pre-registrace (addendum 10–13) vs výsledek

| # | očekávání | výsledek |
|---|---|---|
| 10 | THREAT_net nemonotónní s TD, ~0,40–0,50; welf nejnižší | ✓ nemonotónní; miss v detailu: nejnižší je skaven (0,400), welf 0,413 |
| 11 | konverze při stejné hrozbě: orc ≈ model, skaven ≪ model | ✓ (pásmo 0,15–0,35 všichni 0,090–0,105; skavenův horní pás mrtvý přes nepoužitý Wrestle) |
| 12 | Wrestle 0,1–0,2/hru ofenzivně, BD sražení ~0, strip ~0 | ✓ (0,16/hru; 49 BD z ~4900 expozic; strip 0,05/hru) |
| 13 | zrcadlo skaven ≥2,5× orc | ✓ 3,1× (0,347 vs 0,113) |
