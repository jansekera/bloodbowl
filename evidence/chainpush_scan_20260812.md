# Chain-push scan — dostavitelné vzory v korpusu (12.08.2026)

Otázka: kolik dvojic „dostav vzor bez hodu → chain push" leží v trpasličím
korpusu nevyužitých a co by přinesly. Měřeno skriptem
`diag_chainpush_scan_20260812.py` nad
`diag_replay_mine_20260811{,b,c}_data` (280 her, 4488 našich kol; trpaslík
vs human/orc/skaven/wood-elf, obě strany hřiště). Snapshot = začátek našeho
kola, všichni naši „volní". Push logika zrcadlí `block_handler.cpp` po
fea042c (pushCandidates → prázdné pole má přednost → řetěz jen při plném
obsazení; směry z `getPushbackSquares`), asistence dle `countAssists`.

Definice vzoru: náš B vedle stojícího E; prázdná odsunová pole E (≤2) dojdou
dostavět volní hráči cestou BEZ DODGE (opouštěná pole mimo TZ, vstup do TZ
jen posledním krokem) a BEZ GFI; pak blok a řetěz posune naše tělo zdarma.
Soupeř s Stand Firm / Side Step se přeskakuje (konzervativně, jako E i jako
článek řetězu).

## Čísla

### Nevyužité dvojice (feasibilní vzor, ≤2 dostavovaná těla)
- s kladným tempem: **2172 = 0,48 dvojice/kolo** (z toho hotových bez
  dostavby jen 204 — uživatelův předpoklad „hotových skoro nula" sedí,
  93 % vzorů je potřeba dostavět 1–2 těly)
- s únikem kohokoli z TZ: 957 = 0,21/kolo
- s únikem nosiče: 148 = 0,033/kolo

### TEMPO (best-per-turn, jedna využitá příležitost za kolo)
- kol s ≥1 příležitostí: 1435 (32,0 %)
- **RAW 0,345 pole vpřed/kolo · EV (× P_push) 0,294 pole/kolo**
- NET vč. chůze fillerů +1,24/kolo (fillery jdou převážně VPŘED, stroj
  netahá těla dozadu)
- řetěz: 1 naše tělo v 90 % (2 těla 9 %, 3+ pod 1 %); dostavba: 2 těla
  65 %, 1 tělo 27 %, 0 těl 8 %
- kdo získává: jiný hráč 79 %, ležící 10 %, roh klece 6 %, eskorta 3 %,
  **nosič jen 23 kol z 1435 (1,6 %)**
- směr prvního odsunu E: vpřed 69 %, laterálně 29 %, vzad 2 %
- **beneficientem je sám filler v 78 %** — mechanika je „pěšák dojde MA
  polí do odsunové řady a blok ho posune o pole navíc (MA+1 zdarma)"
- rozbití klece to chce jen ve 167 z 1435 (12 %) — vzor s R1 většinou
  NEkoliduje, fillery chodí ze zadního pole
- bloker s Frenzy 272 z 1435 (19 %) — druhý povinný blok nemodelován

Podle soupeře (EV tempo/kolo): skaven 0,379 · human 0,317 · orc 0,274 ·
wood-elf 0,206.

### ÚNIK (odsun není dodge)
- nosič stojí v ≥1 TZ v 784 kolech (17,5 %)
- vzor pro únik nosiče jde dostavět ve **131 kolech = 0,29/10 kol**;
  vážené pravděpodobností úspěchu **0,18/10 kol**
- 124 ze 131 je „1. článek" — nosič je hned za E, po odsunu zůstává E
  vedle něj; plný únik chce E na zem (POW / DS s Tackle, p ≈ 0,5–0,56)
- únik kohokoli: 1,16/10 kol (vážené 0,73) — skoro vždy obyčejné tělo,
  ne nosič

## Verdikt proti předregistrovaným branám

- **TEMPO: NA HRANĚ, poctivé čtení = zapsat, neimplementovat.**
  Vážené EV 0,294 < 0,3 (pásmo 0,1–0,3); RAW 0,345 těsně nad. EV je horní
  odhad — nezapočítává opportunity cost 1–2 fillerů (65 % vzorů chce 2 celé
  Move akce) ani riziko Frenzy druhého bloku. Proti skavenům (0,379) a
  lidem (0,317) brána prochází i váženě, proti wood-elfům (0,206) zřetelně
  ne. Kdyby se linka někdy otevírala, tak jako matchupová (skaven/human),
  ne plošná.
- **ÚNIK: LINKA SE ZAVÍRÁ.** 0,29/10 kol geometricky (vážené 0,18) proti
  bráně ≥1/10 kol — chybí řádově. Důvod je strukturální: nosič je podle
  doktríny schovaný, v TZ stojí jen v 17,5 % kol, a i tehdy je skoro vždy
  „1. článek", takže plný únik ještě chce E na zem.

## Tvar na desce (kdyby se matchupová linka otevřela)

Převažující vzor (90 % případů): **B a E v kontaktu na čele; dvě volná těla
dojdou zezadu na dvě ze tří odsunových polí E (cesty mimo TZ, poslední krok
do kontaktu); blok pošle E do třetího — obsazeného — pole a tamní tělo
(v 78 % právě jeden z došedších fillerů) dostane pole zadarmo, v 69 %
směrem vpřed.** Prakticky: +1 pole nad MA pro jednoho pěšáka za cenu dvou
Move akcí a bloku, plus vedlejší efekty bloku (E odsunut/na zemi). Není to
posun klece ani nosiče — ti z toho těží výjimečně (roh 6 %, nosič 1,6 %).

## Co jsem NEZMĚŘIL a proč

1. **Opportunity cost fillerů** — jejich Move mohl mít lepší užití; tempo
   je proto horní odhad. Vyžadovalo by celotahový plánovač, mimo rozsah.
2. **Frenzy druhý blok** (19 % nejlepších případů) — povinný druhý blok
   může řetěz prodloužit i rozbít; počítán jen první push.
3. **Dauntless při počtu kostek** (ignorován) — mírně podceňuje kostky
   Slayerů proti ST4+ (Black Orc, Ogre, Treeman).
4. **Interakce cest dvou fillerů** — BFS každého fillera jede nad původní
   deskou (nevidí pole uvolněné/obsazené tím druhým); malá aproximace
   oběma směry.
5. **Víc příležitostí za kolo** — počítám jen nejlepší; 0,48 dvojic/kolo
   existuje, ale sdílejí fillery a bloky, sčítat je nelze bez plánovače.
6. **Skull riziko bloku** (1/36 při 2d s Block) do EV nezapočteno.
7. Snapshot je začátek kola — skutečné pořadí akcí AI může vzor zničit
   dřív, než na něj dojde.

## Vedlejší nález (rules parity, nemeřeno)

`pushOne` po fea042c v řetězu správně přenáší Side Step, ale
**nekontroluje Stand Firm u sekundárně odsunovaných** — podle CRP smí hráč
se Stand Firm odmítnout i sekundární odsun. Týká se jen řetězů přes
Treemana (wood-elf); scan přes něj konzervativně neřetězil.
