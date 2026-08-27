# LEAP: kudy má vstoupit do plánování — návrh (Fable, 25.08.2026)

Zadání: `evidence/fable_brief_leap_20260825_DRAFT.md`. Kód čten 25.08. dopoledne,
nic nespouštěno, nic neměněno. Pravidla citována z `rules_bb2016.txt` s čísly řádků.

## 0. Pravidlový základ (ověřeno v souboru, ne z hlavy)

`rules_bb2016.txt` ř. 8270-8283, po řádcích:

- ř. 8271-8273: *„allowed to jump to any empty square within 2 squares even if
  it requires jumping over a player from either team. Making a leap costs the
  player two squares of movement."*
- ř. 8275-8277: cíl je *„empty square 1 to 2 squares from their current
  square"*, pak *„make an Agility roll for the player. No modifiers apply to
  this D6 roll unless he has Very Long Legs."*
- ř. 8277-8278: *„The player does not have to dodge to leave the square he
  starts in."* — TOHLE je hlavní hodnota skoku.
- ř. 8280-8282: při nezdaru *„he is Knocked Down in the square that he was
  leaping to, and the opposing coach makes an Armour roll"*.
- ř. 8283: *„A player may only use the Leap skill once per turn."*
- ř. 1701 (GFI): *„Roll a D6 for the player after they have moved each extra
  square."* — skok o 2 MA na deficitu = až DVA hody GFI (N7, opraveno 24.08.).
- ř. 9104: *„0-2 Wardancers 120'000 8 3 4 7 Block, Dodge, Leap"* — v TV1200
  korpusu je Leap jen tady (`roster.cpp:583,585`); trpaslík žádný.

⭐ **Klíčové odvození z ř. 8273-8275: skok NIKDY neprodlužuje dosah.** Stojí
2 pole pohybu za cíl ve vzdálenosti ≤ 2 — poměr MA:vzdálenost je nejvýš 1:1,
stejný jako chůze. Skok mění PRŮCHODNOST (přes těla) a CENU (bez dodge při
odchodu z TZ), ne maximální vzdálenost. Z toho plyne půlka celého návrhu:
aritmetické podmínky nabídky maker (SCORE: `dist <= movementRemaining + 2`,
`macro_actions.cpp:432-438`) zůstávají platnou HORNÍ MEZÍ a nemusí se jich
dotýkat.

## 1. Mapa: v enginu jsou DVĚ oddělené pohybové rodiny

Anti-P35 úvaha musí začít tím, že „plánování pohybu" nejsou jedny osudy, ale
dvě uzavřené rodiny, každá se svou vlastní vnitřní konzistencí:

**Rodina M (makrová chůze) — ADVANCE / SCORE / REPOSITION a spol.:**

```
nabídka akcí        rules_engine.cpp getAvailableActions (LEAP už nabízí, :49-55)
   ↓
výběr kroku         findMoveToward          macro_actions.cpp:90-120  ← filtruje jen MOVE
   ↓ (cena)         scoreMoveAction         macro_actions.cpp:35-88
   ↓
provedení           executeAndRecord → executeAction → dispatch (action_resolver.cpp:190)
smyčka              movePlayerToward        macro_actions.cpp:1197-1228
```

⭐ Vlastnost rodiny M: **plán JE provedení.** `findMoveToward` vybírá přímo
`Action` z nabídky, `scoreMoveAction` oceňuje TENTÝŽ objekt a
`executeAndRecord` ho beze změny provede — po každém kroku se nabídka čte
znovu. Není tu žádný oddělený odhad, který by se mohl rozjet s exekucí.
„Dosah" rodina M nepočítá dopředu vůbec: chůze ho OBJEVUJE krok po kroku,
a nabídkové brány maker jsou čistá aritmetika vzdálenosti, kterou skok
nepřekročí (viz §0).

**Rodina B (blitz) — trojice svázaná od item7/14 a P35:**

```
dosah    canReachAdjacentTo         pathfinder.cpp:20   (BFS, těla = zdi)
chůze    pickApproachStep           helpers.cpp:33      (sdílen executor + odhad)
cena     estimateApproachFailChance macro_actions.cpp:327 (jde PO TRASE pickApproachStep)
```

Rodina B je dnes **jednotně leap-slepá**, a to je KONZISTENTNÍ stav: nabídka
neslíbí nic, co chůze neujde, odhad oceňuje přesně tu trasu, která se půjde.
Cena té slepoty je jen PODCENĚNÝ dosah (žádný falešný slib) — tedy hygiena,
ne vada rodiny B.

## 2. Návrh: Leap vstupuje do rodiny M, rodina B se NEDOTÝKÁ

### 2a. Tři místa, která se mění SPOLU (jeden commit, jedno rameno)

**(1) CHŮZE — `findMoveToward`, `macro_actions.cpp:90-120`.** Filtr
`a.type != ActionType::MOVE` se za zapnutým ramenem rozšíří o
`ActionType::LEAP` — kandidáty už od 24.08. dodává `getAvailableActions`
(`rules_engine.cpp:49-55`: jen hráč s Leapem, ne `leapUsedThisTurn`, ne
`rooted`, budget MA/GFI hlídá nabídka). Kontrola `avoid` (volný míč) platí pro
cíl skoku stejně jako pro krok; PŘESKOK pole s míčem je legální a je to bonus
(skok se pole nedotkne, ř. 8271-8272 — „jump to any empty square", vstupuje se
jen do cíle).

**Admisní predikát (ochrana proti tiché inflaci skoků):** LEAP kandidát se
připustí, jen když (a) hráč stojí v ≥ 1 nepřátelské TZ — skok tam nahrazuje
dodge (ř. 8277-8278), NEBO (b) žádný MOVE kandidát nesnižuje vzdálenost
k cíli — hráč je obezděný a skok je jediná cesta vpřed. Bez tohoto predikátu
by greedy krokové skóre srovnávalo akci o 2 polích s akcí o 1 poli a na
volném hřišti by „výhodně" skákalo tam, kam se dá dojít zadarmo dvěma kroky
(viz §3, past ceny). Na volné ploše skok nemá co nabídnout — jeho hodnota
je průchodnost a úspora dodge, obojí predikát vystihuje.

**(2) CENA — `scoreMoveAction`, `macro_actions.cpp:35-88`.** Nová větev pro
`ActionType::LEAP` (detaily §3). ⭐ Cíl hodu se NESMÍ opsat: z
`move_handler.cpp:276-278` se extrahuje sdílený helper
`calculateLeapTarget(player)` vedle `calculateDodgeTarget` v `helpers.cpp`,
a resolver i ocenění volají TENTÝŽ výpočet — stejná unifikace, jakou po item7
dostal `pickApproachStep`, a přesně to, co drží cenu u pravdy, až se jednou
změní VLL nebo clamp.

**(3) SMYČKA — `movePlayerToward`, `macro_actions.cpp:1197-1228`.** Musí
dostat guard „úspěch bez pohybu": `resolveLeap` při chycení Tentacles vrací
`ok()` a hráč se NEPOHNE (`move_handler.cpp:262-270`; pravidlo ř. 8586-8587
jmenuje leap výslovně). Blitzová smyčka tenhle guard má
(`action_resolver.cpp:137-146`, dopsán právě kvůli Tentacles); makrová chůze
ho nemá, protože do ní dosud žádná „ok-ale-stojím" akce nevedla. Připuštěním
LEAPu do chůze se ta cesta OTEVŘE — bez guardu smyčka točí další iterace
naprázdno až do `maxSteps` (leapUsedThisTurn už brání druhému skoku, ale
mrtvé iterace a nedeterministické chování zůstávají). To je přesně třída
„latentní = odložené" z 24.08., tentokrát chycená předem.

### 2b. Proč se ta tři místa nedají měnit odděleně

- (1) bez (2): plánovač skáče a myslí si, že to bylo zadarmo — skok vstoupil
  do chůze, ne do ceny. Druhé P35 v čisté formě.
- (2) bez (1): mrtvý kód, nic se neměří, čítač věčná nula.
- (1)+(2) bez (3): první wardancerův skok ze sevření chapadlového soupeře
  (slann/upír mimo korpus, ale nabídka je rasově slepá) roztočí smyčku.
- A záměrně NIC v rodině B: naučit `canReachAdjacentTo` skok bez
  `pickApproachStep` = nabídnout blitz, který executor neprovede (fail na
  `action_resolver.cpp:120`); naučit `pickApproachStep` bez
  `estimateApproachFailChance` = executor skáče, odhad to neocení. Trojice
  rodiny B se mění JEN celá najednou, v samostatném pozdějším rameni —
  do fronty, ne do tohoto zásahu.

## 3. Jak se skok ocení (větev LEAP v `scoreMoveAction`)

Stávající měna funkce (`macro_actions.cpp:51-87`): vzdálenost k cíli ×10 je
primární; TZ na mezicíli +20 (vstup z bezpečí) / +12 (už dodgeuje); GFI +8;
sideline +6; cílové pole chůze je od TZ/sideline penalt osvobozeno (riziko
vlastní makro). Dodge se dnes neoceňuje pravděpodobností, ale právě těmi TZ
body — LEAP větev musí mluvit toutéž měnou, jinak se porovnává hruška
s jablkem uvnitř jednoho výběru.

**Návrh ceny LEAP kandidáta:**

```
score  = dist(cíl_skoku, cíl_chůze) * 10          // primární, stejné jako MOVE
score += 6 * (leapTarget - 1)                     // riziko hodu; leapTarget ze
                                                  // sdíleného calculateLeapTarget
score += 8 * gfiRolls                             // gfiRolls = max(0, 2 - max(0, movementRemaining)) ∈ {0,1,2}
score += destTZ penalty 20/12                     // jen na mezicíli, stejná výjimka
                                                  // pro finální pole jako u MOVE
score += 6 při y<=1 nebo y>=13                    // sideline, beze změny
```

- `6 * (leapTarget - 1)` je `36 * p_fail`: AG4 wardancer (cíl 3+, p_fail 2/6)
  = +12 — stejná váha jako jedna TZ, když už hráč dodgeuje; AG3 (cíl 4+) = +18.
  Kalibrace drží vnitřní logiku stupnice: skok AG4 je zhruba tak drahý jako
  jeden hlídaný krok, skok AG3 znatelně dražší. Není to odhad dopadu, je to
  směnný kurz uvnitř heuristiky — dopad změří A/B.
- **ŽÁDNÝ dodge za opuštění výchozího pole** (ř. 8277-8278) — v této funkci se
  odchozí dodge stejně neoceňuje (platí ho resolver), takže úspora se projeví
  správně sama: MOVE kandidát z TZ ponese riziko v exekuci, LEAP ne, a TZ
  penalty na cílech obou zůstávají srovnatelné.
- **Cíl hodu bez modifikátorů za TZ cíle** (ř. 8276-8277) — proto se
  `destTZ` penalta v LEAP větvi NEsmí přičítat k hodu, jen k poziční ceně
  mezicíle (tam vyjadřuje „budu stát vedle nepřátel", ne obtížnost hodu).
- **Nezdar = ležící tělo v CÍLOVÉM poli + hod na brnění + turnover**
  (ř. 8280-8282; `move_handler.cpp:286-297`). To je TÁŽ třída následku jako
  padlý dodge — a právě padlý dodge je referenční bod, vůči němuž je celá
  20/12/8 stupnice postavená. Cena hodu ×36 je proto konzistentní: neplatí
  se zvlášť „že se padá", platí se pravděpodobnost pádu v měně, která pád
  už předpokládá. Wardancer AV7 dělá nezdar dražším než u AV9 těla — to
  stupnice dnes nerozlišuje ani u dodge; nezavádět to jednostranně jen pro
  LEAP (byla by to skrytá anti-leap přirážka), případně obojí najednou
  v jiném rameni.
- **1× za kolo** (ř. 8283): plánovač s druhým skokem nepočítá automaticky —
  chůze čte nabídku po každém kroku a `rules_engine.cpp:49` po prvním skoku
  LEAP prostě nenabídne. Konzistence konstrukcí, ne kontrolou.
- **Dopředný odhad NEEXISTUJE a nezavádí se.** Rodina M žádnou
  `estimate*`-cestu pro chůzi nemá; jediný odhad v okolí
  (`estimateApproachFailChance`) patří rodině B a zůstává leap-slepý spolu
  s celou rodinou. Kdo by chtěl skok „ocenit dopředu" v nabídce maker, musel
  by nejdřív postavit odhad chůze rodiny M — a tím teprve vytvořit prostor
  pro rozjezd plánu a exekuce, který dnes neexistuje.

**REPOSITION zvláštnost:** je to jediné makro deklarované jako bez kostek
(`macro_actions.cpp:1767-1772`; GFI si smí říct jen `macro.gfiAllowance`).
Skok kostku hází VŽDY. Návrh: v REPOSITION chůzi se LEAP kandidát s
`gfiRolls > gfiAllowance` nepřipustí (týmž mechanismem jako GFI), samotný
skok v rámci MA připuštěný je — admisní predikát (§2a) stejně zaručí, že
volný hráč na volné ploše nikdy neskočí. Jestli má REPOSITION skákat vůbec
(doktrína „free player nehazarduje" vs. hodnota přeskočit zeď při stavbě
screenu), je volba, kterou měření nerozhodne → řádek do FRONTY ROZHOVORU.

## 4. Rameno

Vzor `setBlitzLandingArm` (`macro_actions.h:102`, impl `macro_actions.cpp:209-221`):

- **`setLeapWalkArm(TeamSide side, bool on)` / `leapWalkArm(side)`** —
  thread_local per side, default OFF. Gate se vyhodnocuje podle
  `state.getPlayer(playerId).teamSide` (strana MOVERA, ne `activeTeam`) přímo
  v admisi ve `findMoveToward` — jediné místo, kde se OFF/ON liší; cena i
  guard smyčky jsou mrtvé, dokud admise nepustí kandidáta, takže OFF rameno
  je bajtově identické s dneškem = čistý nulový test.
- **`takeLeapWalkPicksInSearch()`** — tiká, když `findMoveToward` VRÁTÍ LEAP
  akci jako vítěze výběru. Jelikož OFF rameno LEAP vrátit nemůže, každý tik
  je z definice změněná volba (přesnější než počítat pouhé připuštění do
  kandidátů — to by tikalo i tam, kde stejně vyhrál MOVE). Platí standardní
  varování z `macro_actions.h:70-80`: je to počet VYHODNOCENÍ V SEARCHI, ne
  skoků odehraných na hřišti; nula napříč matchupem = pravé nulové rameno.
- **Odehrané skoky pro A/B report:** ⚠️ dnes NEJDOU spolehlivě spočítat
  z logu — `resolveLeap` emituje `GameEvent::Type::DODGE`
  (`move_handler.cpp:283-284`) a skok na vzdálenost 1 je v záznamu k
  nerozeznání od dodge. Součástí commitu ramene (instrumentace, ne chování)
  musí být rozlišitelný zápis — nejmenší zásah: emitovat před DODGE eventem
  `SKILL_USED` s `SkillName::Leap`, jako to dělá Dauntless. Bez toho nejde
  po nočním A/B napsat „hráno X skoků/hru".
- **Předregistrace znaménka:** Leap mají jen soupeřovi wardanceři
  (ř. 9104; `roster.cpp:583,585`) ⇒ očekává se, že NAŠE čísla proti
  wood-elfovi PŮJDOU DOLŮ, a to je správně — implementujeme pravidlo, ne
  výsledek (třída F6/F7/F8). Zapnutí per-side navíc dovoluje čistý pár
  „wood-elf s rukama vs. bez rukou" na témž enginu.

## 5. ⭐ Zadání měření stropu — OFFLINE, před řádkou kódu (vzor M9/P31)

⛔ Nespouštět před 16:00 (noční A/B P35 na stroji); skript jen NAPSAT dle
tohoto zadání.

**Soubor:** `diag_leap_ceiling_20260825.py`, vzor
`diag_standup_ceiling_20260824.py` (týž formát dat, multiprocessing Pool).

**Data:** `crosses_20260821_data/*wood-elf*` — všechny adresáře matchupů, kde
hraje wood-elf (`dwarf-wood-elf`, …; glob na oba slovosledy). Záznam:
`turn_logs[]` se snímky `home_players`/`away_players`
(`id,x,y,state,name,ma,st,ag,av`) a `events`. Wardancer se pozná podle
`name == 'Wardancer'` (skilly ve snímku nejsou; ř. 9104 a `roster.cpp:583,585`
zaručují, že jméno stačí). ⚠️ Snímek je začátek kola a `movementRemaining`
v něm není — počítá se s plným MA 8; omezení poznamenat do výstupu.

**Za každé kolo wood-elfa, za každého STOJÍCÍHO wardancera (state ==
STANDING, dle číselníku v `diag_utils.py`):**

Postavit mřížku obsazenosti ze snímků obou týmů (jen hráči na hřišti).

- **(A) SKOK OTEVÍRÁ POLE:** existuje prázdné pole `s`,
  `cheb(wardancer, s) <= 2` (ř. 8271-8272), které BFS přes prázdná pole
  s rozpočtem `MA + 2` (GFI) z wardancerovy pozice NEDOSÁHNE (těla = zdi,
  přesná replika logiky `canReachAdjacentTo`, `pathfinder.cpp:20`)?
  Počítat: kol s aspoň jedním takovým polem / wardancer-kol / průměr na hru.
  - **(A-fwd)** podmnožina, kde aspoň jedno otevřené pole je BLÍŽ soupeřově
    endzoně než KTERÉKOLI pole dosažitelné chůzí — jen tehdy skok mění
    plánovací obzor, ne jen topologii.
  - **(A-carrier)** podmnožina, kde wardancer nese míč (`ball_carrier_id`) —
    přímá relevance pro SCORE/ADVANCE.
- **(B) SKOK ŠETŘÍ KOSTKU:** wardancer stojí v ≥ 1 nepřátelské TZ (sousední
  stojící soupeř; `lostTacklezones` ve snímku není — přiznaná aproximace).
  Pro každé prázdné `s` do 2 polí srovnat:
  `p_fail(leap) = 2/6` (AG4, cíl 3+, bez rerollu — Dodge skill na skok
  NEPLATÍ, reroll dává jen na dodge, ř. 8086-8092) proti nejlepší chůzi do
  `s`: součin dodge selhání po krocích (cíl `6 - AG + destTZ`, clamp,
  `helpers.cpp:52-75`), Dodge reroll ⚠️ jen na JEDEN neúspěšný dodge za kolo
  (ř. 8090-8091: „may only re-roll one failed Dodge roll per turn") —
  v řetězu se `p²` přizná jedinému, nejrizikovějšímu kroku. Počítat kola,
  kde leap je OSTŘE bezpečnější než nejlepší chůze do téhož pole.
  ⚠️ Poctivá nulová hypotéza: wardancer je AG4 + Dodge — jeden dodge na
  čisté pole má s rerollem p_fail 1/36 proti skokovým 1/3. Čekám, že (B)
  vyjde MALÉ a nález ponese (A); jestli vyjdou malé obě, je celé téma
  hygiena, ne vada — přesně to má měření rozhodnout.

**Výstup:** tabulka kol: `wardancer-kol celkem / A / A-fwd / A-carrier / B`,
absolutně, na hru a na wardancer-kolo; k tomu 3 vypsané konkrétní situace
(gid, half, turn, pozice) pro ruční kontrolu — pravidlo z 20.08.: když bude
N/A koš větší než n, nález je v koši.

**Rozhodovací kritérium (předregistrované):** srovnat A-fwd na hru s M9
metrem (4,09 blitzů/hru u P31 = „velká vada"); pod ~0,5/hru je to hygiena
a zásah může počkat ve frontě za okruhem POHYB.

## 6. Rizika a co se probudí

1. **Tentacles „ok-bez-pohybu" v makrové smyčce** — NOVĚ dosažitelné, guard
   je součást zásahu (§2a bod 3). Bez něj mrtvé iterace do `maxSteps`.
2. **`leapUsedThisTurn` se propálí i při fail() validaci** —
   `action_resolver.cpp:193-194` nastaví flag PŘED `resolveLeap`, který ještě
   umí vrátit fail (vzdálenost/obsazenost, `move_handler.cpp:230-236`).
   V navrhované cestě se akce bere z čerstvé nabídky a hned provádí, takže
   se to nespustí — ale je to nastražené pro každého budoucího volajícího.
   Hygienický řádek do fronty (přesun flagu až za validaci).
3. **Past ceny „skok zadarmo po volné ploše"** — krokové greedy skóre srovnává
   akce s různou spotřebou MA; bez admisního predikátu (§2a) LEAP na volném
   hřišti „ušetří" 10 bodů vzdálenosti za +12 rizika a začne se skákat tam,
   kde dva kroky dojdou zadarmo. Predikát (v TZ ∨ obezděn) tu past zavírá.
4. **REPOSITION přestává být bez kostek** — řešeno přes `gfiAllowance` (§3);
   otázka „má screen-hráč vůbec skákat" → FRONTA ROZHOVORU.
5. **Event log neodliší skok od dodge** — instrumentace v commitu ramene
   (§4), jinak A/B report neumí „hráno na hřišti".
6. **Rodina B zůstává leap-slepá ZÁMĚRNĚ** — wardancer nedoskáče do klece
   blitzem ani po tomto zásahu; to je vědomě odložený druhý krok (trojice
   `canReachAdjacentTo` + `pickApproachStep` + `estimateApproachFailChance`
   jen celá najednou). Řádek do fronty s výslovným spouštěčem: „až projde
   leap-walk A/B".
7. **Shadowing po skoku** (ř. 8456-8458, N6) — přitáhne soupeře k cíli skoku;
   chůze čte nabídku po každém kroku, takže další krok se ocení už s novou
   TZ. Konzistence konstrukcí, jen to zmínit v testech.
8. **Team reroll v searchi** — `resolveLeap` jde přes `attemptRoll` se
   zapnutým team rerollem (`move_handler.cpp:280-281`), stejně jako dodge;
   žádná změna, jen vědět, že skoky budou rerolly i pálit.
9. **Mimo-korpusové rostery** (Leap+VLL, `roster.cpp:391-394`) — rameno je
   rasově slepé a `calculateLeapTarget` VLL zná ze sdíleného helperu; nic
   zvláštního, jen důvod, proč se cíl hodu NEHARDCODUJE na 3+.
10. **Feature vektor beze změny** — žádný nový MacroType (rozhodnuto),
    `extractMacroFeatures` se nedotýká, šířka vektoru drží.

## 7. Testy hranice PŘED opravou kódu (metodika 24.08.)

- nulový test: OFF rameno ⇒ expanze ADVANCE/SCORE/REPOSITION bajtově shodná
  s dneškem (čítač 0 přes celý matchup);
- vlastnost plán==exekuce: ON rameno, wardancer obezděný tělem — expanze
  zaznamená LEAP akci a stav po `greedyExpandMacro` odpovídá přehrání TÝCHŽ
  akcí přes `executeAction` (soused testu `test_macro_actions.cpp:848`);
- Tentacles-hold: skok chycen chapadly ⇒ smyčka končí, ne točí;
- N7 hrana: skok na deficitu 2 v ceně i exekuci nese DVA GFI (ř. 1701);
- 1×/kolo: po skoku v téže expanzi už `findMoveToward` LEAP nevidí.

*(Konec návrhu. Nic z tohoto nebylo implementováno ani spuštěno.)*
