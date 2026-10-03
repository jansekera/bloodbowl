# Sjednocení výkopu v C++ enginu — 02.10.2026

Uživatel 02.10.: **sjednotit dvě výkopové cesty a zapnout plný výkop** (F13, Ž1–Ž7, P90, P91, P122, P123, P125).

Do dneška v `engine/`:

| | kde | kdo ji volal |
|---|---|---|
| `simpleKickoff` | `src/game_simulator.cpp` | `simulateGame`/`simulateGameLogged` (výchozí `useFullKickoff=false`), Python `simple_kickoff` ⇒ **všechny hry AI, korpusy, `mcts_cli`, živá partie** |
| `resolveKickoff` | `src/kickoff_handler.cpp` | jen testy (`useFullKickoff=true`) ⇒ **mrtvá cesta** |

Po sjednocení zbývá **jediná** funkce `resolveKickoff`; `simpleKickoff` a přepínač `useFullKickoff` jsou pryč
(bez fallbacku), Python `simple_kickoff` volá `resolveKickoff` (jméno a signatura zůstaly kvůli skriptům).

⛔ **Nová báze měření.** Výkop teď hází jiné kostky v jiném pořadí (tabulka 2D6, odrazy, KOR, Blitz!) ⇒
**staré korpusy a CRN páry nejsou srovnatelné** s novými. Párové A/B jen v rámci jedné báze.

## Pořadí výkopu (BB2016 ř. 1242–1248)

> Place the ball · Scatter ball to determine where the ball is about to land · Roll on the Kick-Off table ·
> Resolve the Kick-Off table result · Bounce/ catch/or touchback the ball

Kick-Off Return (ř. 8251–8253) se vkládá *„after the ball has been scattered but before rolling on the Kick-Off table“*.

## Rozdíly obou cest proti pravidlům a co se s nimi stalo

| # | věc | pravidla | `simpleKickoff` (živá) | `resolveKickoff` (mrtvá) | po sjednocení |
|---|---|---|---|---|---|
| 1 | výkop **mimo hřiště** (P123) | ř. 280–283: *„scatters or bounces off the pitch … touchback“* | touchback ✅ (F10, 24.08.) | **ořízne na okraj** (`clamp`) ⇒ touchback z autu nikdy | touchback |
| 2 | výkop do **kopající poloviny** | ř. 274, 280–282 | touchback ✅ | touchback ✅ | touchback |
| 3 | **komu** touchback | ř. 281–283: *„give the ball to any player in his team“* (volba trenéra) | nejhlubší stojící bez No Hands, Sure Hands přednost | nejbližší k místu dopadu, **i s No Hands** | převzato ze `simpleKickoff` (`awardTouchback`) |
| 4 | míč na **prázdné pole** | ř. 277–278: *„bounce one more square“* | odraz ✅ | **bez odrazu** | odraz |
| 5 | odraz **ven / do kopající poloviny** | ř. 280–282 (táž věta: *„scatters or bounces“*) | touchback, ale až **po vhazování z davu** (aproximace) | nic (vhazování) | touchback **přímo při odrazu** — vlastní smyčka dopadu, žádné vhazování |
| 6 | míč dopadne na hráče **kopajícího týmu** nebo na **ležícího** | ř. 278–279 *„the player must try to catch“*; ř. 857–858 ležící chytat nesmí | míč zůstal **ležet pod hráčem** | totéž | stojící chytá (kteréhokoli týmu), ležící/omráčený ⇒ odraz |
| 7 | **kde je míč** během tabulky | ř. 1244–1248: dopadá až po tabulce | — (tabulka není) | položen na zem **před** tabulkou | ve vzduchu (`offPitch`) až do dopadu; místo dopadu v `GameState::kickoffLanding` |
| 8 | **výkopová tabulka** | ř. 1265–1356 | **chybí** | je, s vadami Ž1–Ž7, P90, P91 | je, opravená (níž) |
| 9 | **Kick-Off Return** | ř. 8249–8256 | **chybí** | je (P122, a0bb6b09) | je; test na úrovni `simulateGame` |
| 10 | **Kick** půlí rozptyl dolů | ř. 8211–8213 | floor ✅ | floor ✅ | sdílené |
| 11 | události chycení/odrazu | — | `nullptr` (bez stopy) | do `events` | do `events` |
| 12 | počasí | P66, ř. 2551, 2571–2573 | nehází ✅ | jen Changing Weather ✅ | jen Changing Weather |
| 13 | výkop, když přijímající **už nemá kolo** (TD v jeho 8. kole) | ř. 1033–1035: *„Play stops when both coaches have had eight turns each“* | kop proběhne, pak konec poločasu | totéž | **kop se nekoná** (jinak by Blitz!/Throw a Rock/Pitch Invasion běžely po konci poločasu) |
| 14 | hráč **omráčený při výkopu** (Pitch Invasion, Throw a Rock, Blitz!) | ř. 703–708: lícem nahoru na konci **příštího** kola svého týmu | — | `stunnedThisTurn=true` ⇒ přijímající ležel o kolo déle než kopající | příznak se po výkopu u přijímajících čistí (reset kola až po dopadu) |

## Výkopová tabulka (ř. 1265–1356) — FAME = 0, roztleskávačky = 0, asistenti = 0 (engine je nemodeluje)

| 2D6 | výsledek | pravidla | dřív (`resolveKickoff`) | po sjednocení |
|---|---|---|---|---|
| 2 | Get the Ref | ř. 1270–1282: +1 úplatek každému týmu | nic | **nic — úplatky engine nemá** (`game_simulator.cpp`: *„Bribes are not implemented yet“*). Zbývá, dokud nejsou úplatky |
| 3 | Riot | ř. 1284–1296 | **Ž3**: hýbe jen přijímajícím, bez D6, podmínka podle „≤1“ | obě značky; 7. kolo přijímajícího ⇒ zpět; dosud bez kola ⇒ vpřed; jinak D6 1–3 vpřed, 4–6 zpět |
| 4 | Perfect Defence | ř. 1297–1301: kopající **smí** přestavit | nic | nic — **volba „may“**, AI ji nevyužívá (legální). Zbývá jako AI rozhodnutí |
| 5 | High Kick | ř. 1302–1308 | **Ž5**: nejbližší stojící, i v soupeřově TZ | jen hráč **mimo soupeřovu TZ**, bez No Hands, pole volné a v přijímající polovině; volba: nejlepší chytač (Catch, AG), pak nejbližší |
| 6 | Cheering Fans | ř. 1309–1314 | **Ž4/P90**: D6, remíza = nikdo | **D3**, remíza = **oba** |
| 7 | Changing Weather | ř. 1316–1320 | **P91**: chybí poryv při „Nice“ | nový hod počasí; „Nice“ ⇒ míč se před dopadem rozptýlí o **1 pole** navíc (D8) |
| 8 | Brilliant Coaching | ř. 1321–1326 | **Ž4/P90**: D6, remíza = nikdo | **D3**, remíza = **oba** |
| 9 | Quick Snap! | ř. 1327–1333 | **Ž6**: posun k LoS, jen do vlastní poloviny (cíl na LoS) | **volný krok o 1 pole** na libovolné sousední volné pole, ignoruje TZ, smí do soupeřovy poloviny; volba AI: k místu dopadu míče, nejbližší napřed |
| 10 | Blitz! | ř. 1334–1341 | **Ž1**: kopající jen šoupnuti o 1 pole k LoS | **bonusové kolo kopajícího týmu** hrané politikou (MCTS/greedy); hráči v soupeřově TZ na začátku nesmí jednat; týmové přehozy smí; turnover kolo končí; míč je ve vzduchu (nikdo ho nezvedne), dopadne po konci kola |
| 11 | Throw a Rock | ř. 1342–1350 | **Ž2**: náhodný stojící z **obou** týmů, jen stun, výběr `D6 % n` | D6 proti D6 (+FAME 0): vyšší hází na **soupeře**, remíza ⇒ na oba; náhodný hráč **na hřišti** (rovnoměrně); **hod na zranění** bez brnění (Thick Skull, Stunty… přes `resolveInjuryRoll`) |
| 12 | Pitch Invasion | ř. 1351–1356 | **Ž7**: jen stojící; Ball & Chain jako ostatní | **každý hráč na hřišti** (i ležící); 6 ⇒ Stunned; Ball & Chain ⇒ KO; 1 nikdy |

## Co zůstává jinak, než by mohlo (rozhodnutí AI, ne vada pravidel)

- Kick-Off Return: AI ho použije vždy, hráčem nejblíž míči (P122 (c)).
- Kick: rozptyl půlí vždy (pravidlo: „may choose“).
- Perfect Defence: AI nepřestavuje.
- Quick Snap: heuristika „krok k místu dopadu“ — volba, ne pravidlo.
- High Kick: AI ho využije vždy, když je kdo způsobilý.
