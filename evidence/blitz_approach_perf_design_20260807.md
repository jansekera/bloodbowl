# Návrh: jak ocenit celou trasu blitzu, aniž se zpomalí trénink

Sepsáno 07.08. na žádost uživatele („zkus navrhnout tu cache"). Navazuje na
`blitz_approach_detour_plan_20260807.md` (fáze 0 změřena: dnešní greedy má
riziko cesty Ø 10,2 %, optimum 8,2 %, nejhorší jednotlivé případy 83 pp).

## Kde přesně to bolí (ověřeno v kódu 07.08.)
Estimátor `estimateBlitzFailChance` se volá **při EXPANZI makra**, ne při
generaci: `expandBlitz` (macro_actions.cpp:1122) a `expandBlitzAndScore`
(:1159) projdou všechny BLITZ akce na daný cíl a pro každého kandidáta
spočítají `estimateApproachFailChance`, který dnes prochází trasu krok po
kroku přes `pickApproachStep`. Expanze běží v každé MCTS iteraci i v
rolloutech ⇒ řádově stovky volání na jedno rozhodnutí, každé přes několik
kandidátních blitzerů.

Dnešní cena jednoho odhadu: ~8 kontrol na krok × délka trasy ≈ stovky
operací. Plný Dijkstra přes (pole × počet kroků) je ~4 700 stavů /
~37 000 relaxací — **dva řády navíc**. Bez opatření by to trénink
znatelně zpomalilo.

---

## Vrstva 1 (nejdůležitější): NEPOČÍTAT, KDYŽ NENÍ CO ZÍSKAT
Maximální možný zisk je **přesně rovný riziku dnešní trasy**: když greedy
trasa nemá žádný hod, optimum ji nemůže porazit (0 % je dno).

```
Route g = greedy(...);            // dnešní výpočet, beze změny
if (g.fail <= EPS) return g;      // hotovo, nic dražšího se nespouští
```
**✅ ZMĚŘENO 07.08. (dw-we, N=78): 87 % přiblížení má riziko přesně 0 %**
(rozložení: 0 % → 68 případů; 5-20 % → 2; >20 % → 8; nic mezi 0 a 5 %).
Drahá větev by tedy běžela jen na **13 %** volání — a jsou to přesně ta,
kde je co získat. Tohle je hlavní výkonová páka a je potvrzená daty.
Bonus: rozložení je bimodální (buď cesta bez hodu, nebo rovnou drahá),
takže práh nemusí být jemně laděný — stačí „greedy má nenulové riziko".
Rozšíření: práh ne 0, ale `g.fail < MIN_GAIN` (např. 2 %) — pod ním se
nevyplatí ani počítat, protože zisk je shora omezený tímto číslem.

## ❌ Vrstva 2 VYVRÁCENA MĚŘENÍM (07.08., dw-we N=78): lookahead 2 nezachytí NIC
Změřeno jako varianta C: **střední riziko 7,1 % = přesně jako dnešní
greedy, 0 zlepšených případů, 0 zhoršených → zachytí 0 % zisku orákula.**
Důvod (po rozboru): dnešní pravidlo `dist*100 + 20/12 * TZ_cíle` už
**dělá dobrý bezprostřední tiebreak** — méně TZ na poli, kam vstupuji, je
silně korelované s levnějším dodgem hned poté. Rozdíl proti optimu tedy
NEvzniká na jednom kroku, ale **volbou KORIDORU o několik kroků dřív**:
greedy se brzy upne na jednu stranu překážky a o 3-4 kroky později se
ocitne v pasti, ze které se musí dodgovat na 6+. Depth-2 tak daleko
nedohlédne. Optimální trasa má přitom stejnou délku i stejné finální pole
— jde skutečně o jinou cestu, ne o detour.

⇒ **Levná varianta neexistuje; zisk vyžaduje trasový výpočet.** Text níže
zůstává jako záznam vyvrácené hypotézy.

## Vrstva 2 (PŮVODNÍ NÁVRH — VYVRÁCEN, viz výše)
Naměřené katastrofy jsou **jednokroková krátkozrakost**: greedy vejde do
tackle zóny (lokálně nejkratší) a příští krok z ní musí dodgovat na 6+.
To zachytí **lookahead o hloubce 2** — vzdálenost zůstává striktně
primární (žádné detoury, žádné riziko nedoražení, stejná délka trasy),
jen se tiebreak dělá podle skutečné ceny „tenhle krok + nejlevnější
vynucené pokračování". Cena ≈ 8× dnešek (jednotky mikrosekund), tedy
plně v rozpočtu i bez cache.

Ve v2 běhu se měří jako varianta C proti orákulu B1 — číslo „zachytí X %
zisku B1" rozhodne: **když C zachytí většinu, drahá varianta se nemusí
stavět vůbec** a celý problém s výkonem mizí.

## Vrstva 3: OŘEZANÝ A* (když bude potřeba přesnost)
Když se ukáže, že C nestačí:
* **Dolní mez z greedy**: inicializovat nejlepší známé přežití hodnotou
  greedy trasy a zahazovat každý uzel, který ji už nemůže překonat.
  Prořeže drtivou většinu prostoru.
* **Přípustný ořez dosahu**: uzel zahodit, pokud
  `kroky + vzdálenost(pole, cíl) − 1 > maxKroků` (nemůže stihnout).
  Prostor se scvrkne na „elipsu" mezi startem a cílem — typicky desítky
  polí, ne 390.
* **Bez alokací**: scratch pole `thread_local` (390 × (maxKroků+1) double
  ≈ 37 kB), přepisované mezi voláními; žádný `new` v horké cestě.
* **Předčasný konec**: první vytažený cílový uzel je optimum (max-heap
  Dijkstra) — už implementováno v měřicím nástroji.

## ⭐ Vrstva 3b: JEDEN VÝPOČET PRO VŠECHNY BLITZERY (obrácený směr + branch & bound)
Otázka uživatele (07.08.): „jde cache nachystat levněji, aby se trasy
nepočítaly pro každý rizikový pohyb zvlášť?" — jde, a nejde přitom o cache
v obvyklém smyslu (ukládání výsledků), ale o **jiný tvar výpočtu**.

**(1) Obrátit směr hledání.** `expandBlitz` má cíl PEVNÝ a prochází
kandidátní blitzery. Dijkstra ale umí jedním během spočítat cestu ze
*všech* polí — stačí ho pustit **od cíle po obrácených hranách**
(single-destination místo single-source). Výsledkem je mapa
„nejlepší přežití trasy z pole X do některého pole u cíle"; odpověď pro
každého kandidáta je pak **jen čtení z mapy na jeho startovním poli**.
⇒ z „jeden výpočet na (blitzer, cíl)" se stane **jeden výpočet na cíl**.
* Hrana není symetrická (dodge se hází při OPUŠTĚNÍ pole a obtížnost dává
  pole cílové) — proto se obrácená sousednost musí ohodnotit cenou
  PŮVODNÍ hrany. Standardní postup, jen na to nezapomenout.
* Obsazená pole: blokují průchod pro všechny; **startovní pole kandidáta
  musí být přesto ohodnoceno** (do obsazeného pole se smí „vstoupit" jako
  do zdroje, ale nepokračuje se skrz něj). Jeden řádek v relaxaci.
* Předčasný konec: skončit, jakmile jsou vyřešena startovní pole všech
  kandidátů — ti stojí typicky blízko cíle, takže se prohledá malý ostrov.

**(2) Seskupit kandidáty podle profilu.** Cena hrany závisí na hráči jen
přes `7 − AG` a skilly (Dodge, Break Tackle, Stunty). V rosteru jsou
takové profily 2-3, ne 11 ⇒ **jedna mapa na (cíl, profil)**, ne na hráče.
Rozpočet pohybu (GFI) profil nemění: mapa se drží po délkách trasy
(pole × délka), takže každý kandidát si z ní vybere délku podle SVÉHO
rozpočtu. Tabulka je tatáž, jen se čte jinde.

**(3) Branch & bound nad kandidáty — nepočítat přesně ty, co nemůžou
vyhrát.** `expandBlitz` hledá jen ARGMIN, ne přesné hodnoty:
* horní mez kandidáta = kombinace(kostky bloku, greedy trasa) — levné,
  počítá se dnes;
* dolní mez = kombinace(kostky bloku, **nulové** riziko cesty);
* kdo má dolní mez horší než nejlepší známou horní mez, **nemůže vyhrát
  ani při dokonalé cestě** → přesný výpočet se pro něj nespouští.
Protože člen kostek bloku bývá dominantní, tohle odřízne většinu pole
kandidátů dřív, než se sáhne na trasy.

**Výsledná posloupnost v jednom volání `expandBlitz`:**
```
pro každého kandidáta: kostky bloku + greedy trasa   (dnešní cena)
if žádný kandidát nemá rizikovou trasu -> hotovo     (84 % volání)
ořež kandidáty branch & bound                        (zbytek zlevní)
pro každý zbylý profil: 1x obrácený Dijkstra od cíle (ne na hráče!)
odpovědi = čtení z mapy
```
⇒ místo „N rizikových kandidátů × plný výpočet" typicky **1-2 výpočty na
volání**, a jen v tom šestnáctiprocentním zbytku.

### ⚠️ PLATNOST MAPY (upozornění uživatele 07.08. — KORektnostní podmínka, ne optimalizace)
„Máš spočítané cesty k jednomu elfovi platné toto kolo — a navíc, když
někoho pošleš směrem k němu blíž (třeba ani ne až do jeho TZ), tak si
následné cesty změníš."

**Mapa platí pro JEDNU konfiguraci hřiště, ne pro tah.** Každá aktivace ji
zneplatňuje, i když se hráč nikam „nedostal": tělo, které se posunulo,
(a) uvolnilo pole, kterým teď cesta vede levněji, a (b) obsadilo jiné
pole, kterým cesta vést přestala. Nejde jen o blitzera, který dojde k cíli
— stačí libovolný spoluhráč, který se pohnul kamkoli.
**A může ji i ZABLOKOVAT ÚPLNĚ** (doplnění uživatele): vlastní tělo
postavené do úzkého hrdla může být jediná cesta, kterou blitzer měl —
pak nejde o dražší trasu, ale o **žádnou trasu**. Zastaralá mapa by
v takové situaci tvrdila „jde to za X %", zatímco skutečnost je „nejde to
vůbec" (exekutor by blitz odmítl / došel by nakrátko a spálil aktivaci).
Proto se mapa nesmí přenášet přes aktivace ani „konzervativně" — mýlí se
oběma směry, ne jen v ceně.

Důsledky, závazné pro implementaci:
* Mapa se smí sdílet **jen v rámci jednoho volání `expandBlitz`** (tam je
  hřiště pevné a kandidáti se jen porovnávají) — to je i celý zdroj úspory.
  Přes aktivace se NIKDY nepřenáší.
* Kdyby se někdy sahalo na memo (vrstva 4), klíč musí obsahovat **plnou
  obsazenost hřiště**, ne jen číslo tahu / id cíle. Jinak vrací tiše
  špatné trasy. Tohle je teď důvod korektnosti, ne výkonu.
* Táž výhrada platí pro **kostky bloku** v branch & bound: asistence se
  mění s tím, jak se spoluhráči přesouvají, takže i ta čísla jsou snímek
  okamžiku.
* Známá (dnes už existující) modelová hrubost: trasa počítá spoluhráče
  jako **neprůchodné**, i když se za chvíli pohnou. Proto může být odhad
  pesimistický u cest vedoucích „skrz vlastní chumel". Souvisí s pořadím
  aktivací a vacate-first z klecové doktríny; NEŘEŠIT spolu s touhle
  změnou (jedna věc najednou), jen si toho být vědom.

## Vrstva 4: MEMOIZACE (až kdyby vrstvy 1-3b nestačily)
MCTS **opakovaně přehrává tytéž cesty maker** (`replayToNode` jede
open-loop s čerstvými kostkami) ⇒ tytéž stavy se v rámci jednoho
rozhodnutí vracejí. Nabízí se memo:
* klíč = 64bit otisk stavu (pozice + stavy hráčů, míč, číslo tahu)
  + `blitzerId` + `targetId`;
* hodnota = odhad selhání cesty;
* `thread_local` mapa, **mazaná na začátku každého `search()`** (žádná
  invalidace, žádná staleness přes tahy);
* ⚠️ **jen pro ODHAD, nikdy pro exekutor.** Kolize otisku by u odhadu
  znamenala mírně chybné skóre (heuristika to unese), u exekutoru
  špatně odehraný tah. Exekutor počítá vždy načisto — je volaný jednou
  za skutečný blitz, výkon tam nehraje roli.

## Konzistence odhadu a exekuce (nepřekročitelná podmínka)
Commit 23d652a sjednotil odhad a exekuci do jedné funkce, aby se nemohly
rozejít. **Ať zvítězí kterákoli varianta, musí zůstat jediným zdrojem
pravdy pro obojí.** U trasového výpočtu to platí i po krocích: optimální
trasa má optimální podstrukturu, takže exekutor může počítat znovu
z aktuální pozice a dostane totéž pokračování (ostatní hráči se mezi
kroky nehýbou).

## Plán ověření výkonu (před nasazením)
1. Referenční čas: `time` na pevném seedu, N rozhodnutí, dnešní kód.
2. Totéž s variantou C, pak s A* + vrstvou 1.
3. Přijatelné zpomalení: **< 5 %** celkového času self-play hry
   (trénink je hlavní odběratel). Nad to → zůstat u C.
4. Až po výkonu párové A/B na kvalitu (mění i výběr blitzera).

## Doporučené pořadí (AKTUALIZOVÁNO po měření — původní návrh viz historie)
Varianta C odpadá (vyvrácena). Zbývá:
**vrstva 1 (přeskoč, když greedy nemá riziko — 84 % volání)
+ vrstva 3b (branch & bound nad kandidáty, pak 1 obrácený Dijkstra od cíle
na profil) + ořezy z vrstvy 3 → změřit výkon → memo (vrstva 4) jen kdyby
to nestačilo.**
Poznámka k pořadí implementace: vrstva 1 a branch & bound jsou levné
a dávají smysl i samostatně; obrácený směr je větší zásah, ale je to
právě on, kdo mění složitost z „na kandidáta" na „na cíl".

---

# FINÁLNÍ ČÍSLA (celý běh 12 her, N=226, 07.08.)
Log: `evidence/blitz_approach_measure_v2_20260807.log`.

| | dw-we (78) | dw-sk (79) | orc-sk (69) | CELKEM (226) |
|---|---|---|---|---|
| riziko A = riziko C | 7,1 % | 11,8 % | 12,0 % | **10,2 %** |
| zisk C proti A | 0,00 pp | **−0,14 pp** | 0,00 pp | **−0,05 pp** |
| zachycený zisk orákula | 0 % | −15 % | 0 % | **−6 %** |
| přiblížení s rizikem 0 % | 87 % | 84 % | 81 % | **84 %** |

**Lookahead 2 je VYVRÁCEN definitivně:** nulový přínos ve dvou matchupech
a v jednom dokonce mírné zhoršení (jeden případ), celkově −6 % zisku
orákula. Hypotéza „katastrofy jsou jednokroková krátkozrakost" NEPLATÍ —
jde o volbu koridoru několik kroků předem.

**Vrstva 1 potvrzena na plném vzorku: 84 % přiblížení má riziko 0 %**
(a rozložení je ostře bimodální: 0 % → 190, mezi 0-5 % → 0, 5-20 % → 6,
nad 20 % → 30). Drahý trasový výpočet by tedy běžel na **16 %** volání,
práh netřeba ladit.

---

# MĚŘENÍ „VLASTNÍ TĚLO V HRDLE" (07.08., podnět uživatele) — JEV JE VZÁCNÝ
Sken jednou za týmový tah přes 12 her: existuje soupeř, na kterého se
**nikdo nedostane teď, ale dostal by se, kdyby spoluhráči nepřekáželi**?

| | |
|---|---|
| týmových tahů skenováno | 385 |
| tahů s cílem blitzovatelným JEN po uvolnění spoluhráče | **3 (0,8 %)** |
| zablokovaných dvojic (blitzer, cíl) | 404 |

Těch 404 dvojic vypadá hrozivě, ale je to jiný jev: „tenhle konkrétní
hráč neprojde, jiný ano" — blitz se prostě provede někým jiným, nikdo
nepřichází o nic. Rozhodující je první řádek: **reálná ztráta nastává
v 0,8 % tahů.**

⇒ **Vacate-first jen kvůli blitzu se nevyplatí stavět.** Položka se
uzavírá; zůstává jako člen obecného mechanismu (rozpočet polí), pokud se
ten bude stavět kvůli jiným fázím tahu. Data: `evidence/blitz_selfblock_measure_20260807.log`.
