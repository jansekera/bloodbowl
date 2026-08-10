# ID-based tie-break audit: může pořadí hráčů (home ID 1-11 first) zvýhodnit home?

**Datum:** 2026-07-15 (Fable 5) · **Nástroj:** čistý code audit (žádný nový běh —
empirická mez převzata z existujících ramen `arm_first_possession_postfix_20260714_*`
a `arm_h1ko_split_h1ko_20260715_*`). Follow-up lead ze sekce 7.5
`fable_h1ko_home_split_20260715.md`.

---

## VERDIKT

**ID-based tie-break bias jako mechanismus home-slot / opening-drive mezery je
VYLOUČEN konstrukcí kódu.** Tie-breaky typu „při shodě vyhrává první nalezený"
jsou v enginu všudypřítomné a shody (ties) jsou reálně ČASTÉ — ale **neexistuje
jediné místo, kde by se v soutěžním rozhodnutí skenovala obě družstva dohromady
v ID pořadí**. Všechny výběrové skeny jsou per-team (`forEachOnPitch(side)`),
a within-team ID pořadí = slot pořadí, které je pro home i away identické nad
zrcadlovými formacemi → tie se u obou stran rozřeší na TÝŽ slot v zrcadlově
ekvivalentní pozici. Statisticky symetrické, žádná výhoda nižších ID.

Nezávisle na tom: **jakýkoli always-on stranový bias (ID i jiný) je už empiricky
shora omezen existujícími daty** — post-TD drivy používají stejný setup i search
kód a konverze je stranově nulová (home 3.0 % vs away 3.1 %, sekce 3b postfix
reportu). Strukturní bias působící na všechny drivy by se tam propsat musel.

Vedlejší nálezy (mimo ID hypotézu, obě třídy geometrie-chirality, nízká priorita):

1. **NOVÉ: pushback chirality** — `getPushbackSquares()` vrací [přímo, CW45,
   CCW45] a block handler bere first-empty/first-max → home v remízových
   případech tlačí soupeře k +y postranní čáře, away k −y. Při x-zrcadlení se
   CW↔CCW prohodí, takže zrcadlové situace se nerozřeší zrcadlově. Velikost
   omezená post-TD null výsledkem (viz výše) → NEvysvětluje mezeru, jen hygiena.
2. Home i away MacroMCTSPolicy dostávají v `bb_module.cpp` STEJNÝ seed —
   korelace obou searchů, ne stranová výhoda; poznámka pro budoucí variance
   analýzy.

**Lead uzavřít. Žádný fix, žádný nový sběr dat není potřeba.**

---

## 1. Ověření ID číslování (úkol 1)

Potvrzeno přímo v kódu, shoduje se se shrnutím z 07-15:

- `game_state.cpp:11-18` (konstruktor `GameState`): `players[0..10].id = 1..11`
  = HOME, `players[11..21].id = 12..22` = AWAY. Komentář v
  `game_state.h:19` totéž.
- `game_simulator.cpp:163` (`buildTeam`): `baseId = (side == HOME) ? 1 : 12`.
- `getPlayer(id)` (`game_state.cpp:21-33`) mapuje ID→index deterministicky,
  home vždy v poli první.

Home tedy skutečně drží nižší ID a nižší array indexy — **předpoklad leadu platí**.
Otázka je, jestli na tom někde záleží.

## 2. Inventura tie-break míst (úkol 2)

### 2a. MCTS vrstva (`macro_mcts.cpp`) — tie-breaky existují, ties jsou časté

- `bestChildPUCT()` :83 — strict `score > bestScore`, při shodě vyhrává dřívější
  dítě (pořadí z `getAvailableMacros`).
- `mostVisitedChild()` :96 — strict `visits > bestVisits`, totéž.
- Descend do nejvyššího prioru při expanzi :183-186 — strict `>`, first-max.

**Frekvence ties (úkol 3): vysoká.** V diag/gate konfiguraci je
`weights_best.json` S policy sekcí (`policy_W1` ověřeno), takže `cfg.policy`
je nastavené a heuristické prior floory (blok B, :292-398) běží; policy priory
(blok A) se při `policyBlend=0` přeskočí. Priory tedy vznikají z uniformu
1/n + diskrétních floorů {0.02, 0.05, 0.08, 0.12, 0.20, ...} + renormalizace —
všechny macros stejného typu se stejným floorem mají **exaktně stejný float
prior** (např. každý BLOCK kandidát 0.12). U neexpandovaných dětí je
q = FPU pro všechny a u ∝ prior → shoda → rozhoduje pořadí v seznamu.
Tie-break pořadí tedy REÁLNĚ řídí, která větev dostane první simulace.

### 2b. Proč to přesto není stranový bias: slot-mirror argument

Pořadí dětí = pořadí z `getAvailableMacros()` (`macro_actions.cpp:136-715`),
které je určeno (i) pevným pořadím bloků podle typu macra (END_TURN, SCORE,
HAND_OFF_SCORE, ..., REPOSITION — stejné pro obě strany) a (ii) uvnitř bloku
iterací `forEachOnPitch(mySide)` resp. `forEachOnPitch(opponent(mySide))` —
tj. sloty 0-10 JEDNÉ strany (`game_state.h:40-53`). Nikdy ne 22 hráčů dohromady.

Klíčové: `buildTeam()` staví obě strany IDENTICKOU logikou (specialisti od
slotu 10 dolů, lineman zbytek) nad zrcadlovými formacemi (dx negované, y stejné,
`game_simulator.cpp:20-109`). Slot k home a slot k away je tentýž positional
na zrcadlové pozici. Když tedy first-found/first-max tie-break vybere pro home
„slot 3", vybere v zrcadlové situaci pro away také „slot 3" — zrcadlově
ekvivalentní volba. **Within-team ID pořadí je stranově symetrické konstrukcí.**

Totéž platí pro všechny ostatní within-team first-max/first-found skeny
(úkol 2, čtvrtá odrážka — všechny prověřeny):

| místo | tie-break | verdikt |
|---|---|---|
| `macro_actions.cpp:363` BLITZ kandidáti | `std::sort` (nestabilní) podle int skóre; ties dle vstupního pořadí = opponent slot order | symetrické |
| `macro_actions.cpp:194-217` PASS_SCORE, :229-262 CHAIN_SCORE, :436-456 PICKUP best-picker | strict `>` first-max přes slot order | symetrické |
| `macro_actions.cpp:117-132` `findNearestFreePlayer` | strict `<` first-min | symetrické |
| `kickoff_handler.cpp:12-24` `findClosestPlayer` (touchback, HIGH_KICK) | strict `<` first-min přes slot order | symetrické |
| `macro_mcts.cpp:446-453` greedyLookahead rank | first-max přes pořadí macros | symetrické |
| `rules_engine.cpp` `getAvailableActions` | slot order + `getAdjacent()` | symetrické až na známou −x geometrii (viz 07-15 sekce 5) |

### 2c. Cross-team skeny: žádný soutěžní neexistuje

Jediná místa iterující všech 22 hráčů (kandidáti na skutečný ID bias):

- `GameState` konstruktor a `setupHalfOrDrive` reset :258 — inicializace, bez volby.
- `PITCH_INVASION` (`kickoff_handler.cpp:171`) — každý hráč hází NEZÁVISLE D6,
  pořadí mění jen spotřebu RNG streamu, ne pravděpodobnosti.
- `getPlayerAtPosition` (`game_state.cpp:35-47`) — first-match, ale na jednom
  čtverci stojí max. 1 hráč → shoda nemožná.

Simultánní/soutěžní eventy: CHEERING/BRILLIANT_COACHING házejí home a away
oddělené kostky, remíza = no-op (symetrické). Turn order je strict alternace
(`turn_handler.cpp:18`), žádná ID priorita. Bloky, pickupy, interception
(pass path se prochází od házejícího k cíli — relativní směr, symetrické),
bounce (D8 scatter) — nic z toho nesrovnává hráče obou týmů v ID pořadí.

**Závěr úkolů 2+3: hypotetický mechanismus „nižší ID = první na tahu / priorita"
nemá v kódu jediné místo, kde by mohl působit.** Ties jsou časté, ale všechny
tie-breaky jsou per-team a slot-symetrické.

## 3. Empirická horní mez zdarma (bez nového běhu)

I kdyby audit něco přehlédl: každý always-on stranový bias (ID, chirality,
cokoliv ve sdíleném search/setup kódu) působí na VŠECHNY drivy stejně. Post-TD
drivy ale konverzní stranovou asymetrii nemají — 07-14 data: home 6/199 = 3.0 %
vs away 8/256 = 3.1 % (postfix report sekce 3b; per-poločas rozpady bez vzoru).
Return audit 07-15 navíc potvrzuje identické 11v11 podmínky. Mezera je specifická
pro OPENING drivy (H1 vs H2), což s časově invariantním ID biasem nejde dohromady.
Nový sběr dat by tuhle mez nezlepšil (floor efekt ~3 % omezuje citlivost, ale
směr nálezu — nula asymetrie — je konzistentní přes oba batche).

## 4. Vedlejší nález: pushback chirality (NOVÝ, mimo ID třídu)

`getPushbackSquares()` (`helpers.cpp:160-195`) vrací tři push čtverce v pořadí
[přímo, +45° CW, +45° CCW] (:182). `block_handler.cpp` pak:

- default push :111-118 — **first-empty v tomto pořadí**;
- SideStep :88-94 — first-max vzdálenosti (diagonály jsou od útočníka často
  ekvidistantní → rozhoduje pořadí);
- chain push :139-146 — first-empty.

Pod x-zrcadlením (home↔away) se CW a CCW prohodí: home blokující na východ
preferuje při shodě SE (+y), away blokující na západ NW (−y). Zrcadlové herní
situace se tedy NErozřeší zrcadlově — je to druhá instance třídy
„absolutní geometrická chiralita" vedle známého `getAdjacent()` −x-first
(`position.cpp:5-16`, řádek y−1 první, −x před +x). Formace navíc nejsou
y-symetrické (LOS řada y={5,6,7,8}, centroid 6.5), takže +y/−y drift není
ani interně vyrušený.

Očekávaná velikost: malá (uplatní se jen když je přímý čtverec obsazený nebo
při SideStep remíze) a shora omezená post-TD null výsledkem ze sekce 3.
Směr (komu pomáhá) nelze z kódu určit. **Neopravovat teď** — kandidát na
nízkoprioritní hygienickou položku master listu spolu s getAdjacent −x-first
(oba řeší totéž: absolutní enumerační pořadí místo perspective-relativního).
Levná falzifikace, kdyby někdy bylo třeba: čítač do block_handleru (kolikrát
rozhodlo pořadí, tj. přímý čtverec obsazený / SideStep tie) + porovnat
y-drift pushů home vs away — jde přidat do libovolného budoucího diag běhu,
samostatný běh to nezaslouží.

Druhá poznámka: `bb_module.cpp:365+388-389` — home i away MacroMCTSPolicy
dostávají tentýž `seed` (a `expansionDice_ = seed+12345` u obou). Není to
stranová výhoda (streamy divergují se stavem), ale vytváří to cross-side
korelaci search kostek v rámci hry — relevantní nejvýš pro variance úvahy,
ne pro bias.

## 5. Doporučené kroky

1. **Uzavřít lead 7.5 z `fable_h1ko_home_split_20260715.md` jako vyvrácený**
   (čistý negativní výsledek: mechanismus nemá kde působit + empirická mez).
2. Na master list NIC urgentního. Volitelně jedna nízkoprioritní hygienická
   položka: „sjednotit absolutní enumerační chirality (getAdjacent −x-first +
   getPushbackSquares CW-first) na perspective-relativní pořadí" — engine-fix
   třída, resetuje baseline, řadit za funkční fixy.
3. Opening-drive mezera (H1 24.1 % vs H2 19.8 %) zůstává bez engine-bug
   vysvětlení; po (b) KO-return a nyní (c) ID tie-break je nejsilnější zbývající
   hypotéza behaviorální/kontextová (H2 stav skóre/stall dynamika — částečně
   zamítnuto v 4.3, nebo reálný rozdíl kickoff kontextu), ne strukturní bug.
   Před dalším kolem doporučuji žádné nové sběry — signál je ~4pp a klesá.
