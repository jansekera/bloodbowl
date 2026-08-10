# Report: Proč se policy („intuice") neučí + jak zvětšit výtěžek iterace

**Datum:** 2026-08-10/11 · **Zadání:** `evidence/fable_learning_mechanism_20260811.md`
**Diagnostika (skripty + plné výstupy):** `evidence/diag_learning_mechanism_20260811/`
(40 nových her, 5 866 zalogovaných rozhodnutí s visit distribucemi a board
snapshoty; nice -19, žádný trénink neběžel, produkční kód nedotčen, váhy jen čteny)

---

## 0. Shrnutí

1. **Rámující hypotéza uživatele („elfí intuici jsme dopustili tím, že jsme
   neměli pro trpaslíky styl hry") se POTVRZUJE — a v ostřejší podobě.**
   Změřeno dvojím způsobem: (a) sám search bez policy hraje trpaslíka téměř
   stejným makro-mixem jako wood-elfa (tabulka §1.2) — v self-play datech
   trpasličí styl skutečně nikdy nebyl; (b) policy síť je na profil schopností
   téměř slepá: když trpasličím stavům kontrafaktově přepíšu capability featury
   (ST/AG/AV/Block/Dodge) na elfí hodnoty, typová distribuce priorů se pohne
   v průměru o **3,3 %** (TV vzdálenost 0,033; §1.3). Síť má JEDEN styl pro
   všechny.
2. **Korekce hypotézy:** hlavní škoda nevzniká tím, že by policy trpaslíkovi
   našeptávala jiné TYPY tahů než search (agregátní odchylky jsou ±3 pp,
   §1.2). Vzniká dvěma jemnějšími mechanismy: **(i) akční bias** — policy
   prakticky nikdy nedá top-1 na END_TURN (0 z 9 jednoznačných případů
   u trpaslíka, 0 z 11 u elfa; §1.4-A) a tlačí „ještě něco udělej", což je pro
   AG4 tým levné a pro AG2,5 tým drahé (každá kostka navíc ~riziko turnoveru);
   **(ii) strukturní slepota featur** — u REPOSITION (40–50 % všech rozhodnutí!)
   jsou priory policy mezi kandidáty **naprosto identické** (spread 0,000 vs
   0,109 u search visitů; §1.4-B), protože makro-featury vůbec nekódují cílové
   pole. Policy tedy neumí říct KAM se postavit — jen „stavěj se". Blend 0,2
   pak na těchto uzlech prokazatelně jen ředí heuristické priory směrem
   k uniformě.
3. **Proč to samé našeptávání elfy zlepšuje a trpaslíky potápí:** stejná rada
   („jednej, seber míč, postupuj") má rasově asymetrickou cenu — pro AG4
   je marže akce kladná, pro AG2,5 záporná. PICKUP kvantifikovaně: proti tomu,
   co chce search, ho policy trpaslíkovi našeptává o +2,7 pp VÍC a elfovi
   o 10,3 pp MÍŇ (§1.4-C) — přesně chování průměru přes rasy.
4. **Plateau top1 ~42 % je featurový strop, ne kapacitní:** na 40–50 %
   rozhodnutí (REPOSITION) policy z principu nemůže rozlišit kandidáty,
   protože dostávají identický vstup. Žádná kapacita to nespraví (páka 3);
   spraví to jen vstupy (páka 2).
5. **Doporučené pořadí pák: 2 → 1 → 4 → 5 → 3** a první krok je **offline
   featurový A/B** na už existujících board snapshotech — bez zásahu do
   produkce, výsledek za 1 den (§3).

---

## 1. Páka 1: root-cause trpasličí regrese intuice

### 1.0 Co přesně regreduje (rekapitulace vstupů, neměřil jsem znovu)

- fairtest 31.07.: dwarf s promotnutou policy 32,7 % = −17 pp (zadání).
- H2H 05.08. (`diag_policy_vs_policy_20260805/results.json`): pokračující
  učení přes rejecty — wood-elf 73,8 % [63,2–82,1] z=4,25, dwarf 37,8 %
  [28,1–48,6] z=−2,21. Učení táhne oběma směry zároveň.
- gate 06.08. (`gate_history.jsonl` poslední záznam): race_guard dwarf
  cand 34,1 % vs frozen 48,8 %, delta −14,7 pp, z=−1,9 → veto. Regrese trvá.

### 1.1 Metodika nového měření

`evidence/diag_learning_mechanism_20260811/diag_collect.py`: 16 her
dwarf–skaven bez policy (run A), 16 se stejnými seedy s promotnutou policy
blend 0,2 na obou stranách (run B), 8 her wood-elf–skaven bez policy (run C);
TV 1200, MCTS 100, vf_blend 0,15 = produkční konfigurace. Z každého
rozhodnutí: state featury, top-20 makro kandidátů s visit frakcemi
a akčními featurami, board snapshot. Offline (`diag_analyze.py`,
`diag_analyze2.py`) jsem na kandidáty každého rozhodnutí pustil promotnutou
policy (weights_best_policy.json, cd72ed6b; softmax při temp=1,0 — přesně to,
co MCTS blenduje, `macro_mcts.cpp:378`) a srovnal s visit distribucí searche.

### 1.2 Nález A: v datech trpasličí styl nikdy nebyl — search hraje všechny rasy stejně

Makro-mix toho, co search (bez policy) skutečně hraje (visit masa, run A/C):

| typ | dwarf | wood-elf | skaven |
|---|---|---|---|
| REPOSITION | 46,4 % | 45,8 % | 49,9 % |
| END_TURN | 10,6 % | 10,5 % | 12,6 % |
| BLITZ | 9,3 % | 8,0 % | 10,8 % |
| BLOCK | 8,7 % | 9,7 % | 6,4 % |
| FOUL | 7,9 % | 7,6 % | 7,1 % |
| CAGE | 7,5 % | 7,4 % | 4,1 % |
| PICKUP | 4,3 % | 4,1 % | 4,3 % |
| ADVANCE | 2,8 % | 2,8 % | 1,8 % |
| SCORE | 0,1 % | 1,0 % | 0,5 % |

Trpaslík a wood-elf se liší prakticky jen v SCORE (10× méně — trpaslík se
tam nedostane). **Self-play korpus, ze kterého se policy učí imitací
(`policy_trainer.py:35-93`, CE na visit frakce), žádný odlišný trpasličí styl
neobsahuje — přesně jak říká hypotéza.** Policy pak logicky zesílila jediný
existující styl.

### 1.3 Nález B: policy je na schopnosti slepá — jeden styl pro všechny

Kontrafakt (`diag_analyze.py` §3): 400 reálných trpasličích stavů, přepis
capability featur na elfí hodnoty (avg ST 4,0→3,0, AG 2,4→4,0, AV 8,8→7,7,
Block 0,45→0,10, Dodge 0→0,50; indexy 19/38/36/48/50
`feature_extractor.cpp:530-584`). Průměrný posun typové distribuce priorů:
**TV = 0,0335**. Policy s elfím týmem by trpasličí pozici hrála z 96,7 %
stejně. Styl podle schopností v síti neexistuje.

Proč nemůže existovat ani po lepším tréninku — **vstupy ho neunesou**
(`macro_actions.cpp:1503-1620`):

- **risk featura [13] je KONSTANTA podle typu makra** (PICKUP=0,33,
  BLITZ=0,25, …; řádky 1558-1601) — nezávisí na AG hráče, počtu tackle zón,
  ničem. Síť nemá jak vidět, že pickup AG2 Blockera je ~2× riskantnější než
  AG4 elfa.
- **Žádné AG ani MA aktéra** v makro-featurách (jen ST, řádek 1555); ve state
  featurách žádné průměrné MA týmu (mobilita — HLAVNÍ osa dwarf↔elf — je
  téměř neviditelná).
- **REPOSITION nekóduje cílové pole**: `Macro.targetPos` se do featur vůbec
  nepromítá; identifikační featury [15-22] existují jen v mikro-extractoru
  (`action_features.cpp:121-161`), makro-extractor je nechává nulové.

### 1.4 Nález C: co konkrétně policy trpaslíkovi našeptává (situace, ne agregáty)

**(A) „Nikdy nekonči tah" — akční bias.** Ve všech 9 trpasličích rozhodnutích,
kde search jednoznačně (≥50 % visitů) volí END_TURN, ho policy NIKDY nemá
jako top-1 (medián: 4. z 5 kandidátů). Konkrétní situace (plné boardy
v `diag_analyze.out`, situace 1–5):

- **Situace 1** (seed 91000009, T8, remíza 0:0, trpasličí Blitzer s míčem na
  (11,5) v rozehrané kleci, skaveni okolo): search END_TURN 67 %; policy
  **ADVANCE 81 %** — vytáhni nosiče AG2,3 z klece dopředu proti čtyřem
  skavenům. Klasická elfí rada; trpaslík na ni nemá nohy ani AG.
- **Situace 2** (seed 91000011, volný míč na (15,8) uprostřed skrumáže
  4 skavenů): search END_TURN 50 %; policy **PICKUP 74 %** — AG2,4 pickup
  v tackle zónách, vysoká šance turnoveru. Přesně proti doktríně odepření
  volného míče (07.08.).
- **Situace 6** (volný míč v rohu (25,6)): search **BLITZ 73 %** (odmarkovat
  a zavázat = správné pořadí fází), policy **PICKUP 57 %** a na blitz dává 4 %.
- **Situace 4 a 5** ukazují, že bias je „vždy jednej", ne „hraj elfa": policy
  tlačí i CAGE 64 % a FOUL 59 % tam, kde search říká „už dost, konči tah".
  Problém není výběr elfích typů, ale neschopnost říct STOP — u rasy, kde
  každá kostka navíc bolí nejvíc.

Dopad na chování (run A→B, trpaslík s policy): END_TURN volen 10,4 %→5,5 %
(polovina!), ADVANCE 4,5 %→6,8 %, rozhodnutí/hru 78,6→85,9, průměrná
riskovost zvoleného makra +4,9 % rel. (skaven kontrola: stejný směr, menší
citlivost). Policy trpaslíka nutí hrát DELŠÍ a riskantnější tahy.

**(B) „Stavěj se, ale je mi jedno kam" — REPOSITION slepota.** V 946
trpasličích rozhodnutích s ≥3 REPOS kandidáty: spread priorů policy mezi
kandidáty **0,000** (search visity 0,109). Na největší třídě rozhodnutí
(40–50 %) policy nenese žádnou informaci a blend 0,2 tam pouze ředí
heuristické priory (floory/screeny z `macro_mcts.cpp:507-534`) k uniformě.
Pro pomalý tým je špatné postavení nevratné — elf uteče, trpaslík ne.

**(C) „Seber míč" bez ohledu na AG.** V loose-ball stavech: policy vs search
prior masa na PICKUP — dwarf **+2,7 pp** (našeptává víc, než chce search),
wood-elf **−10,3 pp** (míň), skaven −3,2 pp. Učebnicový průměr přes rasy:
jedno číslo pro všechny → moc pro AG2,5, málo pro AG4.

### 1.5 Jak z toho vzniká −17 pp (a co jsem nereplikoval)

Mechanismus: stejné jednostylové našeptávání má zápornou marži jen u pomalé
nízko-AG rasy (víc kostek, horší postavení), zatímco elfí stranu zlepšuje —
proto H2H ukazuje obě znaménka najednou. Poctivost velí dodat: v mém malém
A/B (16+16 her dw–sk, obě strany s policy) trpaslík s policy skóroval VÍCE
(6 TD vs 2) — N je hluboko pod šumovým dnem a chybí párování i dw–we
matchupy, takže to −17 pp nevyvrací (race_guard 06.08. na N=600 hrách drží
veto), ale znamená to, že **čistý dopad na dw–sk může být menší než
na dw–we**; rozhodne až měření M1 níže.

**Odpověď na otázku z 10.08.** („našeptává mu policy tahy, které drží pozici
a nikdy nedokončí drive?"): **NE, opak.** Policy tlačí aktivitu a postup
(END_TURN ↓, ADVANCE ↑). Neschopnost dokončit drive je vlastnost searche
a value (SCORE visit masa 0,1 % u trpaslíka vs 1,0 % u elfa — search se
k šanci nedostane), ne policy.

### 1.6 Spoluviník: umlčená attrition (hypotéza s oporou)

Zranění se po každém TD mažou (balík G; rozdíl přeživších 0,2 hráče,
DEAD/hru 0,00 ve 3200 hrách — vstup 10.08.). Value tedy nemá z čeho poznat,
že trpasličí mlácení je dobrý obchod → search nemá důvod hrát grind → v datech
není grind → policy se ho nemá kde naučit. To je datová strana téže mince:
i kdyby featury uměly styl vyjádřit (páka 2), signál „mlátit se vyplácí"
v datech dnes fyzicky není. Označuji jako hypotézu — kauzalitu ověří až
persistence fix (M4).

---

## 2. Seřazený plán pák

**Pořadí: 2 → 1 → 4 → 5 → 3.** (Páky 1 a 2 se z root-cause analýzy z velké
části slily: hlavní oprava trpasličí regrese JE cílené rozšíření vstupů.)

### #1 = Páka 2: cílené přírůstky akčních featur (capability-aware, bez ras)

- **Co:** rozšířit `extractMacroFeatures` o: (i) identitu cíle pro REPOSITION
  (přenést hotové featury [15-22] z `action_features.cpp` — kód existuje);
  (ii) skutečný odhad rizika místo konstanty — p(fail) složená z AG aktéra,
  tackle zón na trase/cíli, počtu kostek (vše už engine umí spočítat);
  (iii) AG a MA aktéra + MA-normalizovaná vzdálenost k cíli. Vše odvozené ze
  schopností a situace — **žádné rasové labely** („generic over skills"),
  klec/grind má vyjít emergentně pro každý pomalý silný tým.
- **Přínos:** vysoký — odstraňuje oba změřené stropy najednou (REPOS slepota
  = ~45 % rozhodnutí bez informace; konstantní risk = nemožnost naučit se
  rasově správnou opatrnost). Jediná páka, která útočí na diagnostikovanou
  příčinu plateau (strop featur, 17.07.).
- **Náklad:** malý až střední. Krok 0 je čistě offline (viz §3); engine změna
  potom ~1 den + retrain policy hlavy (16 epoch imitace). Změna vstupní
  dimenze resetuje akumulovanou policy — o nic nepřicházíme, je plochá.
- **Riziko:** nízké pro šampiona (gate + race_guard chrání); střední pro
  výsledek (featury nemusí stačit bez lepších dat — viz páka 4). POZOR:
  nejde o čtvrtý per-player pokus — žádné per-player vstupy do VALUE, jen
  akční featury policy; 3× NO-GO se netýká.
- **Jak změřit (M2):** offline pre-registrace: top1/CE na held-out
  rozhodnutích, zvlášť na REPOS podmnožině (dnes ~uniform), + kontrafakt
  citlivosti (TV vzdálenost musí vzrůst z 0,033). GO práh: top1 +5 pp offline.
  Pak teprve ostrá iterace s gate.

### #2 = Páka 1: dwarf regrese — mitigace a prubířský kámen

- **Co:** (a) krátkodobě zvážit **no-information-loss fix**: neblendovat
  policy na uzlech, kde jsou její priory ~uniformní mezi kandidáty (dnes
  všechny čistě-REPOS uzly) — vyřazuje prokázané ředění heuristik, nulová
  ztráta informace, žádné rasové labely; (b) hlavní fix = featury z #1;
  (c) END_TURN bias: po (i)+(ii) dostane síť šanci naučit se „STOP", protože
  uvidí, že zbylé akce jsou drahé.
- **Přínos:** odemyká promoce (race_guard veto je dnes hlavní blokátor);
  přímý příspěvek k trvalému cíli dwarf AI.
- **Náklad:** (a) ~2 h implementace za GO; (c) zdarma s #1.
- **Riziko:** (a) může sebrat i užitečné ředění? Ne — priory jsou IDENTICKÉ,
  blend k uniformě nenese žádný signál; přesto A/B změřit.
- **Jak změřit (M1):** dedikovaný per-race test s dostatečným N: dwarf
  v obou produkčních matchupech (dw–sk i dw–we), stejné seedy, policy-on vs
  policy-off pouze na trpasličí straně, ≥300 párů; pre-registrovaná metrika
  decisive WR + TD/hru trpaslíka. Odliší „policy škodí trpaslíkovi" od
  „policy víc pomáhá soupeři".

### #3 = Páka 4: kvalita dat — ale až po opravě persistence zranění (balík G)

- **Co:** pořadí uvnitř páky: (1) **persistence zranění přes drive** (bug,
  fronta G; bugfix priorita dle zavedené praxe) → attrition signál se vůbec
  poprvé objeví v datech; (2) teprve pak posuzovat plánovačové „výrobníky
  stylu". Varování z 10.08. beru vážně: grind/koridor doktrína se zapínala
  (0,75 plánu/hru) a přinesla NULU — „plátek generuje data" ≠ „data nesou
  signál". Proto před jakýmkoli masovým self-play s novým plánovačem chci
  důkaz signálu.
- **Přínos:** potenciálně největší dlouhodobě (nový signál, který imitace
  dosud neviděla), ale podmíněný #1 (featury ho musí umět reprezentovat).
- **Náklad:** persistence fix = střední (zásah do drive-reset logiky enginu,
  mimo můj mandát — jen doporučuji); zbytek navazuje na běžící staged-pickup
  korpus (první iterace už proběhla, REJECTED 51,7 %).
- **Riziko:** persistence změní herní dynamiku i pro soupeře → nutný plný
  fairtest; prázdné doktríny (viz grind) — proto měřit signál, ne adopci.
- **Jak změřit (M4):** po persistence fixu: (a) DEAD/hru > 0, rozdíl
  přeživších na konci > 0,2 hráče; (b) korelace attrition rozdílu s výhrou
  v self-play datech (dnes nutně ~0); (c) až pak A/B tréninkové iterace.

### #4 = Páka 5: rozvrh učení — ostřejší imitační targety

- **Co:** self-play pro TRÉNINK s MCTS 400 místo 100 (probe 21.07.: nižší
  entropie targetu), případně bez Dirichletu v decision-logu (alpha 0,3 dnes
  šumí přímo do imitačních targetů, `run_iteration.py:137`); gate nechat na
  100. Imitation-only rozvrh už de facto máme (policy se učí z visit frakcí).
- **Přínos:** střední — čistší target téhož jednoho stylu; smysl dává hlavně
  PO #1, jinak ostříme špatný signál.
- **Náklad:** 4× dražší self-play část iterace (nebo méně her — kompromis
  600→300 her při 200 iter). Žádná změna kódu kromě konfigurace.
- **Riziko:** méně her/iter = horší pokrytí; hlídat počet rozhodnutí/iter.
- **Jak změřit (M5):** per-epoch top1/CE na stejném held-out setu; A/B jedna
  iterace 100 vs 400 při stejném počtu ROZHODNUTÍ (ne her).

### #5 = Páka 3: kapacita/architektura — odložit

- **Proč poslední:** plateau 42 % drží od lineární verze přes hidden 64 —
  dvě funkční třídy, stejný strop. Nález B/§1.4-B ukazuje, že strop je ve
  VSTUPECH (identické vstupy → identické výstupy, žádná architektura
  nepomůže). Vrátit se k ní jen pokud po #1 top1 poroste a znovu se zasekne.
- **Jak změřit (M3):** offline: hidden 64→128/dva skryté na rozšířených
  featurách; GO jen při dalším zlomu top1 po saturaci #1.

**Neotvírám** (bez nové evidence, dle zadání): mc_td_mix, fáze A retest,
value akumulace přes rejecty, čtvrtý per-player pokus.

---

## 3. Doporučený PRVNÍ krok

**Offline featurový A/B (M2-krok-0), bez jakéhokoli zásahu do produkce,
~1 den:** z board snapshotů v už nasbíraných decision-lozích (5 866 rozhodnutí
v `diag_learning_mechanism_20260811/`, snadno rozšířitelné o další sběr)
dopočítat rozšířené makro-featury (REPOS cílová identita + p(fail) z AG/TZ +
AG/MA aktéra) čistě v Pythonu, natrénovat policy hlavu offline dvakrát —
staré vs nové featury — a pre-registrovaně srovnat: (a) held-out top1 (práh
GO: +5 pp nad 42 %), (b) top1 na REPOS podmnožině, (c) END_TURN ranking,
(d) kontrafaktová citlivost na capability featury (musí vzrůst řádově nad
0,033). Teprve při GO implementovat do enginu a pustit ostrou iteraci
s gate + race_guard + M1.

Proč právě tohle: útočí přímo na změřenou příčinu (vstupy), stojí skoro nic,
nemůže nic rozbít, a jeho výsledek rozhodne o největší investici
(engine featury + případná kapacita) dřív, než se utratí noční okno.

---

## 4. Limity a poctivost

- Run B (policy-on) A/B má N=16 her — použit jen pro směr chování (END_TURN,
  risk-mix), NE pro outcome závěry; outcome evidence regrese stojí na
  race_guard N=600 (06.08.) a H2H N=600 (05.08.).
- Per-race řezy H2H jsou post-hoc (zadání to samo přiznává); wood-elf přežije
  Bonferroniho, dwarf z=−2,2 je na hraně — proto M1 jako prubířský kámen.
- Kontrafakt přepisuje 5 capability featur; korelované featury (guard/MB
  frakce [52-53]) jsem neměnil — skutečná citlivost může být o něco vyšší,
  závěr „řádově slepá" to nemění.
- Attrition spoluvina (§1.6) je hypotéza; kauzálně ji potvrdí až M4.
- Sběr her proběhl na dw–sk a we–sk; dw–we situace v novém vzorku nejsou
  (fronta na M1).
