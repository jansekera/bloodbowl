# Q3 — ZŮSTAT vs UTÉCT: podmínka pro zeď, ocenění útěku, rozdělení ramene (03.09.2026, Fable)

Vstup: `evidence/night_result_20260903_q3.txt` (−0,0933 ± 0,0086, ŠKODÍ; dodatek
„PŘEHODNOCENO TÝŽ DEN" — vady 1-3) · `engine/src/macro_actions.cpp:819-965`
(rameno) · `evidence/dwarf_turn_procedure_spec_20260811.md` (doktrína).
Analytický dokument — žádný soubor v engine/ se needitoval.

---

## (A) PODMÍNKA PRO ZEĎ — kdy tělo drží pozici, která za blok zdarma STOJÍ

### A.1 Co přesně dnes chybí

`stayEarnsItsKeep` (macro_actions.cpp:941-952) zná dva případy, oba vázané na
míč: značím soupeřova nosiče (d ≤ 1) nebo držím roh vlastní klece (d ≤ 2).
Tělo v obranné zdi s míčem daleko dostane `false` a rameno mu smí zrušit
nabídku „vstát a zůstat" — rozpouští zeď (night_result ř. 68-78).

### A.2 Čím se zeď v doktríně MĚŘÍ — už je to definované, jen pro druhou stranu

Doktrína díru neměří v tělech, ale v **ceně průchodu v dodgích**:

- spec ř. 690-693: *„díra(T) = po odstranění T nejlevnější cesta pro JEDNOHO
  hráče, oceněná počtem dodgů (výstupů ze soupeřovy TZ)"* — a pro trpaslíka je
  tolerovaná cena **0 dodgů**, tvrdý filtr (ř. 696-698).
- spec ř. 559-571 (R5b): totéž pro výběr cíle blitzu — stav bez T, pathfinder
  bez dodge, počet průchozích polí.
- spec ř. 2549-2553 (17.4b, geometrie z webu): screen = 1 pole od soupeře,
  2 pole mezi sebou, **2 do hloubky** — *„i když jednoho z předních blitzneš
  a srazíš, stejně neprojdeš — kvůli obránci za ním."*
- spec ř. 247 (S7.2): počet únikových polí nosiče musí každé kolo klesat;
  ř. 2481: *„L není rozestavení ani screen. Je to ODEBÍRÁNÍ ÚNIKOVÝCH POLÍ."*

Hodnota mého těla na poli q je tedy **zrcadlo díry**: o kolik zdraží soupeřův
nejlevnější průchod. To je spočitatelné z `GameState` bez hádání.

### A.3 Definice (rozhodnutelná, bez ladicích konstant)

**Hrozba a cíl průchodu se berou z větve rozvrhu** (spec ř. 95-107, jeden
řádek na větev, žádný výběr případů):

| stav míče (rozvrh) | hrozba (threat) | cíl průchodu (objective) |
|---|---|---|
| drží SOUPEŘ (S7/S8) | jejich nosič | naše endzóna (jeho směr postupu) |
| držíme MY (S2/S9/S10) | každý soupeř s dosahem na našeho nosiče (dosah = MA+GFI, týž výpočet jako macro_actions.cpp:2061-2082) | pole našeho nosiče |
| míč VOLNÝ (S5/S6) | soupeř s dosahem na míč | pole míče |

**Metrika průchodu** `C(threat → objective | konfigurace)`:
nejlevnější cesta hrozby k cíli, kde krok, který OPOUŠTÍ pole s naší
tacklezónou, stojí jeden vynucený dodge — přesně sémantika
`move_handler.cpp:104` (`needsDodge = countTacklezones(from) > 0`).
Cena kroku = P(selhání) toho dodge z `calculateDodgeTarget`
(helpers.cpp:71-…) **pro hrozbu** — tj. (target−1)/6, týž odhad jako
macro_actions.cpp:715. Cesta se hledá Dijkstrou; infrastruktura existuje
(pathfinder.cpp:76-124 — stačí varianta, která místo kTzCost=2 sčítá
pravděpodobnosti a běží za soupeřova hráče).

Proč pravděpodobnost, a ne počet dodgů: spec ř. 891-908 (9.3) — *„značkuj tam,
kde dodge NĚCO STOJÍ"*; osamocená značka na AG4 nedrží skoro nic (2+, 16,7 %),
táž značka na AG2 drží půlku (4+, 50 %). Počet hodů by obě ocenil stejně;
pravděpodobnost je rozliší a je odvozená z tabulky hodu, ne zvolená.
(Celočíselný počet dodgů je konzervativní záloha, kdyby sčítání
pravděpodobností po cestě bylo v praxi nestabilní.)

**Zátěžový test se soupeřovým blitzem** — bez něj podmínka selže na sloupcích:

⛔ Naivní „C s tělem > C bez těla" je špatně: dvouhluboký sloupec je postavený
PRÁVĚ TAK, aby odstranění jednoho těla nic neotevřelo (spec ř. 697-699).
Marginální příspěvek každého jednotlivého těla intaktní zdi je NULA a rameno
by zeď rozpustilo tělo po těle — každý odchod by „nic nezměnil".

Oprava plyne z doktríny nejhorší odpovědi (paměť worst_case_valuation: osa je
vzácný zdroj — **blitz 1×** za kolo): zeď se hodnotí PO soupeřově nejlepším
jednom odstranění.

```
W(konfigurace) = min přes soupeřův jeden blitz b na kterékoli naše stojící
                 tělo v koridoru hrozby:
                     C(threat → objective | konfigurace bez TZ těla b)
                 (sražení blitzem se předpokládá — konzervativní worst case)

stayEarnsItsKeep_WALL(p) :=  W(p stojí na p.position)  >  W(p stojí na best)
                             kde best = cílové pole útěku vybrané ramenem
```

Ostrá nerovnost dvou spočtených čísel — žádný práh. Porovnává se přesně
alternativa, kterou rameno nabízí (tělo na q vs totéž tělo na únikovém poli),
ne „tělo vs vzduch".

**Co podmínka dává zadarmo:**
- *„blitznou prvního, ale přes druhého se nedostanou"* (spec ř. 599, 2551-2553):
  tělo v druhé řadě má W(zůstat) > 0 i po blitzu na první řadu; po jeho útěku
  W spadne — podmínka je `true`, nabídka zůstat se NESMÍ rušit.
- lajna jako zadní stěna zdarma (S7.3, 17.2-17.3): v ceně automaticky přes
  `Position::isOnPitch()` — hraniční pole v grafu nejsou.
- skutečně nadbytečné tělo (třetí v hloubce, W se útěkem nezmění): podmínka
  `false` — smí odejít. To je správně: doktrína chce 2 do hloubky, ne 3.
- rasová neutralita (paměť cage_is_universal): nikde nefiguruje rasa ani
  dovednost — jen AG hrozby přes `calculateDodgeTarget` a geometrie.

**Vztah ke stávajícím dvěma případům:** oba jsou speciální instance (hrozba =
nosič vedle mě; hrozba = soupeř s dosahem na mého nosiče). Nechat je jako
rychlé zkratky a NEMĚNIT ve stejném rameni — jedna změna najednou.

**Cena výpočtu (odhad, ne měření):** W = |kandidátní blitz-cíle| × Dijkstra na
26×15. Kandidáty omezit na naše těla v koridoru hrozby (box jako
`corridorResistance`, cage_advance.cpp:60-74, DEPTH=4/HALF_WIDTH=2) a počítat
jen pro ležící vedle soupeře (populace ramene). Přesto to běží uvnitř
generování makro-nabídky v MCTS ⇒ **před nocí změřit propustnost uzlů**
(laptop je úzké hrdlo, paměť 29.08.).

---

## (B) OCENĚNÍ ÚTĚKU — dodge a GFI do ceny, turnover v jiné jednotce

### B.1 Fakta z kódu a pravidel

- Odchod z kontaktu VŽDY hází dodge: move_handler.cpp:104.
- Cíl hodu: `calculateDodgeTarget` (helpers.cpp:71-…): **6 − AG + počet
  soupeřových TZ na CÍLOVÉM poli**; Break Tackle nahradí AG silou, Stunty ruší
  penalizaci cíle, Titchy/Two Heads −1. Trpaslík z korpusové sestavy nemá NIC
  z toho a nemá Dodge (roster.cpp:564-586; spec ř. 697-698 „Dodge skill nemá
  nikdo").
- GFI: 2+ (selhání 1/6), v blizzardu 3+ (2/6) — move_handler.cpp:175; max 2
  pole (Sprint 3, zakořeněný 0) — helpers.cpp:59-62.
- Vstání stojí 3 MA: helpers.cpp:53-57 (`movementAfterStandUp = MA−3`).
- Neúspěšný dodge i GFI = pád + hod na zbroj + **TURNOVER**
  (move_handler.cpp:159-169 a GFI větev za ní).

### B.2 Reálné pravděpodobnosti pro trpaslíka (spočteno z pravidel výše)

Rozpočet po vstání (roster.cpp:568-577):

| hráč | MA/AG | polí po vstání | dodge do čistého pole | +1 GFI | +2 GFI |
|---|---|---|---|---|---|
| Longbeard | 4 / 2 | **1** | 50,0 % | 58,3 % | **65,3 %** |
| Troll Slayer | 5 / 2 | 2 | 50,0 % | 58,3 % | 65,3 % |
| Blitzer | 5 / 3 | 2 | 33,3 % | 44,4 % | 53,7 % |
| Runner | 6 / 3 | 3 | 33,3 % | 44,4 % | 53,7 % |

(sloupce = P(turnoveru) celé cesty; každý další dodge po cestě přes cizí TZ
násobí dál; TZ na cílovém poli tu je 0, protože rameno cíle s TZ zahazuje —
macro_actions.cpp:896.)

⭐ Longbeard — páteř zdi — má po vstání JEDNO pole. Skoro každý jeho útěk do
čistého pole jde přes GFI, tj. 58-65 % turnover. Proti tomu „vstát a zůstat":
sražen v 50,1 % — a to jen KDYŽ se soupeř rozhodne bít (Q3/ODPOVED,
night_result ř. 23) — a bez turnoveru. Rameno vybíralo mezi 0 % (cesta se
neoceňovala vůbec) a 50 % místo mezi ~60 % a 50 %.

### B.3 Čím se `worstReplyCost` doplní — dvě složky, společná jednotka

⛔ Zákaz splývání jednotek: sražení stojí JEDNOHO hráče (a jen část jeho
budoucnosti); turnover stojí AKTIVACE VŠECH, kdo ještě nehráli. Sčítat je lze
až po převodu na společnou, z pravidel odvozenou jednotku: **očekávané ztracené
vlastní aktivace**.

```
T(state)     = počet vlastních hráčů na hřišti s !hasActed (bez utíkajícího)
               -- MĚŘENÁ veličina stavu, ne konstanta: přesně tolik aktivací
               turnover sebere (turnover ukončuje kolo celému týmu)

A_down(pl)   = očekávané aktivace, které ztratí SRAŽENÝ hráč pl -- z tabulek:
               zbroj AV9 padne 6/36 = 16,7 % (2D6 ≥ 10);
               nepadne (83,3 %): vstávání 3 MA z příští aktivace = 3/MA
                 (Longbeard 3/4 aktivace; pravidlo M13, helpers.cpp:53-57);
               padne: s Thick Skull stunned 2-8 (26/36), KO 9 (4/36),
                 CAS 10+ (6/36) -- stunned ≈ 1 celá aktivace + vstávání;
                 KO/CAS = aktivace do konce drivu/zápasu (⚠️ délka drivu je
                 očekávaná hodnota Z KORPUSU, viz „co nevím", ne konstanta)

P_path       = P(selhání cesty útěku): TÝŽ mechanismus jako
               estimateApproachFailChance (macro_actions.cpp:686-727):
               po krocích, (dodgeTarget−1)/6 za každý krok opouštějící TZ,
               1/6 (blizzard 2/6) za každý krok nad movementAfterStandUp

escapeCost(p, cesta, cíl) =
      P_path × ( A_down(p) + T(state) )                -- pád na cestě
    + (1 − P_path) × worstReplyCost(cíl) × A_down(p)   -- došel, stojí tam

stayCost(p) = worstReplyCost(p.position) × A_down(p)   -- táž jednotka
```

`worstReplyCost` sám se nemění — pravděpodobnost už v sobě má
(`knockdownChanceFromDice`, macro_actions.cpp:625-635) a vzácnost zdrojů taky
(kBlock/kBlitz/kFoul, ř. 646-651). Chyba noci nebyla v něm, ale v tom, že se
CESTA ocenila nulou. Násobení `A_down` je jen převod obou stran do téže
jednotky; vzácnostní váhy zůstávají symetricky na obou stranách.

**Důsledky, které vypadnou zadarmo (žádný nový přepínač):**
- Útěk brzy v kole (T velké) je drahý, útěk poslední akcí (T→0) levný —
  doktrína S2.14 „bezpečné první, rizikové poslední" (spec ř. 173-174) vyjde
  z výpočtu, nezadává se.
- GFI se nemusí zakazovat: každé pole nad rozpočet si přinese 1/6 do P_path
  a Longbeardův útěk přes 2 GFI se prodraží sám. Rozpočet nabídky
  (macro_actions.cpp:880) může GFI dál obsahovat.
- Trpaslík vedle Mighty Blow orka (uživatelův příklad z 20.08.,
  macro_actions.cpp:830-833): hurtAmplifier zvedne stayCost, a když už
  odehráli ostatní, útěk vyhraje — přesně zamýšlená výjimka.

⚠️ **DRIFT ODHADU OD EXEKUCE:** útěkový REPOSITION vykonává `movePlayerToward`
(macro_actions.cpp:1919) přes `findMoveToward`/`scoreMoveAction`
(macro_actions.cpp:117; skóre dist×100 + TZ×12/20), kdežto
`estimateApproachFailChance` chodí přes `pickApproachStep`. To jsou DVĚ RŮZNÉ
trasy — P_path se musí počítat po trase, kterou útěk skutečně půjde, jinak se
opakuje vada, kterou item7/P35 už jednou platily (macro_actions.cpp:592-600:
„unified ... so the estimate can't drift from execution again").

---

## (C) ROZDĚLENÍ RAMENE NA DVĚ — měřit po jedné změně

Rameno dnes dělá dvě věci pod jedním vypínačem (macro_actions.cpp:876-878 to
přiznává). Rozdělení podle toho, CO se mění v nabídce:

**Rameno Q3-N (NABÍDKA):** jen PŘIDÁ „vstát a odejít" (blok
macro_actions.cpp:879-923); cílové pole vybírá už novou cenou escapeCost
(bez ní by výběr pole opakoval vadu 1). Nabídka „zůstat" se NIKDY neruší —
ř. 953-957 pod tímto ramenem neexistují. Rozhoduje plánovač/MCTS.
Delta měří čistou hodnotu MOŽNOSTI NAVÍC.

**Rameno Q3-O (ODEBRÁNÍ):** předpokládá nasazené Q3-N. Přidá JEN bránu
odebrání: zrušit „zůstat", když `escapeCost < stayCost` (ostrá nerovnost,
společná jednotka z B) A ZÁROVEŇ `!stayEarnsItsKeep` rozšířené o zeď (A).
Delta měří čistě cenu vynucení.

Pořadí drží dohodu z 03.09. (night_result ř. 85-89):
(a) čítače → (b=Q3-N s cenou) noc → při nasazení (c=Q3-O se zdí) noc.
Každá noc = jedna změna na jedné binárce, párové A/B (paměť
moving_baseline_only_paired_ab).

**Čítače PŘED první nocí** (podmínka (a), a poučení z night_result ř. 80-83
+ paměť arm_signal_incomplete):
- útěk nabídnut / vzat / dodge hozen / GFI hozen / **turnover na cestě**
  (dodge a GFI zvlášť) — per-rameno, ne sloučeně (omezení měřidla
  night_result ř. 43-46);
- u Q3-O navíc: „zůstat" odebráno / z toho stayEarnsItsKeep=zeď zachránilo;
- u každého čítače ověřit celý čtyřdílný řetěz čítač→sběr→printf→výstup
  (paměť registered_reading_needs_a_print_line).

Seznam všeho, co které rameno mění (pro leak test): Q3-N mění jen množinu
nabídek (+1 makro), signál `armChangedOffer` tiká při každém přidání
(vzor macro_actions.cpp:913-922 zůstává). Q3-O mění jen odebrání (−1 makro),
tiká při každém odebrání. Nic jiného se hýbat nesmí.

---

## ⚠️ CO NEVÍM A ČÍM BY SE TO ZMĚŘILO

(populace vyhlášeny předem; žádné „zajímavé případy")

1. **P(sražení|rána) po vstání podle kostek.** 50,1 % (night ř. 23) je průměr
   přes obě ramena a všechny útočníky. Populace: všechny bloky/blitzy na
   hráče, který v tomto kole vstal vedle soupeře; osa: počet kostek a Block/
   Wrestle obránce. Ověří `knockdownChanceFromDice` proti realitě.
2. **Chvost A_down (KO/CAS → aktivace).** Populace: všechna sražení v korpusu;
   měřit ušlé aktivace sraženého do konce drivu. Dnes odhad, korpus to dá.
3. **Velikost T v okamžicích útěku.** Populace: všechna kola s nabídnutým
   útěkem; rozdělení počtu neodehraných spoluhráčů. Řekne, jak velká je
   turnoverová složka doopravdy a jestli pořadí akcí v kole umí T stlačit.
4. **Kolik ležících vedle soupeře je „zeď".** Populace: čitatel
   `g_standOfferedNextToEnemy` (macro_actions.cpp:852); u každého spočíst
   stayEarnsItsKeep_WALL a rozpad podle větve rozvrhu (S2/S7/…). Když vyjde
   ~0 %, podmínka je mrtvá a vada byla jinde; když ~100 %, Q3-O nebude mít co
   měřit.
5. **Drift trasy odhad vs exekuce.** Populace: všechny VZATÉ útěky; podíl, kde
   trasa `movePlayerToward` ≠ trasa použitá pro P_path (a rozdíl v počtu
   dodge/GFI hodů). Nad ~0 se stepper musí sjednotit před nocí.
6. **Cena W ve výkonu.** Uzly/s se zapnutým výpočtem W vs bez něj, tentýž
   stroj (laptop = úzké hrdlo, paměť 29.08.). Kdyby propustnost spadla, noc by
   se platila silou — to je zakázáno (never_trade_power_for_walltime).
7. **hurtAmplifier (0,35/0,35/0,25, macro_actions.cpp:637-643) je poslední
   vymyšlená konstanta v tomhle ocenění.** A_down z tabulek zbroje/zranění
   (MB/Claw mění právě ty hody) by ho uměl NAHRADIT výpočtem. Neměnit teď —
   jiná změna, jiné rameno; jen zapsáno.
