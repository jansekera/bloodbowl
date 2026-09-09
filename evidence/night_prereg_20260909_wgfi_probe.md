# PŘEDREGISTRACE — W-GFI (`repositionGfiArm`), mode 18, dw-dw, SONDA
**Zapsáno PŘED spuštěním. Pokračování sekce POHYB po nasazení M14b.**

---

## Proč tenhle běh existuje

`expandReposition` (volný pohyb — screen, safety, značkování) dnes zakazuje
GFI paušálně ("Deliberately NO +2 GFI headroom" -- `macro_actions.cpp:3083`).
Uživatel 02.09.: *„na pickup musíme hodit — ať s trpaslíky dojdeme, musíme
hodit blitz a GFI třeba."* Měření 02.09. (mode 14, dw-dw): **70 % všech
vzdání chůze je "došel mi pohyb"** (181 152 z 259 562) — a právě tomu je
GFI dnes zakázané, i když by o 1-2 pole stačilo dojít.

W-GFI rameno (implementováno 04.09., `setRepositionGfiArm`, default OFF)
nahrazuje paušální zákaz počítanou cenou — stejný tvar jako Q3-N: GFI se
povolí jen když `P_fail(gap, reroll, počasí) * zbývající_aktivace < 1`
(riziko vážené počtem spoluhráčů, kteří ještě mohou jednat, ne paušál).
**Nikdy nezměřeno** — kód čeká na noc od 04.09.

## MECHANISMUS: co arm dělá, na místě kde se rozhoduje

Arm mění, jestli volně se přesouvající hráč (screen/safety/značkovač, NE
nosič — ten má vlastní `cage_advance.cpp` cestu) smí hodit GFI, aby došel
o 1-2 pole dál, když je vypočtená cena (pravděpodobnost pádu × počet
spoluhráčů, které by to připravilo o tah) nízká. **Přímý mechanismus =
z případů, kdy arm GFI POVOLÍ, kolik hráčů skutečně dojde na cílové pole
(screen/safety se postaví) a kolik skončí turnoverem** (GFI je vždy
turnover při pádu, ne jen "tělo na zemi" — `move_handler.cpp:216`).
Měří se přímo přes nové čítače `g_repositionGfiReached`/`g_repositionGfiTurnover`
(`engine/src/macro_actions.cpp`, commit viz níž) — **ne** souhrnná win-rate,
ta je až druhotná kontrola.

## SANITY-TEST

**Kolo 1** (2 páry, stará hladová chůze) — příležitost 18 265, povoleno
10 757 (58,9 %, pozitivní kontrola ✅), z povolených dosel na cíl 2 365
(22,0 %), turnover 1 567 (14,6 %), **ZBYTEK 6 825 (63,4 %)** — ani jedno.
⛔⛔⛔ **SONDA POZASTAVENA** — zbytek dohledán na `movePlayerToward`/
`findMoveToward` (stejná hladová chůze, kterou M14b opravil pro blitz, ale
tahle cesta tu opravu neměla), viz task_queue.md.

**Kolo 2** (09.09., po opravě `movePlayerToward` na BFS, commit `75f9df43`,
2 páry) — dosel na cíl 24,0 %, turnover 24,5 %, **ZBYTEK 51,5 %** (klesl
z 63,4 %). Granted GFI teď mnohem častěji skutečně dojde ke kostce.
Zbytek 51,5 % je nově přisouzen tomu, že `gap` počítá PŘÍMOU vzdálenost,
ne skutečnou délku cesty kolem překážek — otevřená kalibrační otázka,
zapsaná zvlášť, netýká se téhle sondy. **SONDA POKRAČUJE** na opravené
chůzi.

## Co se měří

Mode 18, `repositionGfiArm` zapnuté pro kandidátní stranu. Matchup **dw-dw**
(index 2) — trpaslík má nejvíc "limit" vzdání ze všech ras (2 729 volných
tahů proti 194 u skavena, `M11` nález 26.08.), takže je to matchup s
NEJVĚTŠÍ expozicí příležitosti, ne nejmenší.

## Dimenzování — SONDA, ne rozhodná noc

Stejný vzor jako P9c/M12(B)/M14b sondy: **PAIRS = 80, WORKERS = 4,
CHUNKS = 8**. Tempo se měří čerstvě (mode 18 nikdy neběžel) —
viz SANITY-TEST výš.

## Nulová kontrola

`MOVED WITHOUT THE ARM ACTING` == 0, standardní.
Pozitivní kontrola v samotném výstupu: `povoleno + zamítnuto jako drahé`
MUSÍ souhlasit s `příležitost (gap>0)` (`W-GFI/RAMENO` řádek,
`diag_f1_cage_advance_harness.cpp:972`).

## Předregistrované předpovědi (`.preds`, strojově kontrolované)

| | čekám | proč |
|---|---|---|
| leak | 0 | jinak se nic nečte |
| arm_acted | ≥ 0,85 | příležitost (limit vzdání) nastává ve většině her u dw-dw |
| n_nonzero | 0,10–0,60 | ne každý granted GFI změní výsledek hry, ale dost často na signál |
| delta | −0,05 až +0,10 | čekám neutrálně-pozitivní (doktrína i měření 02.09. podporují GFI tam, kde jinak vzdá), ale nejde vyloučit, že cena (1/6 pád = turnover) převáží u sondy tohoto rozsahu |
| delta_1s | −0,03 až +0,05 | totéž jednostranně |

## Pořadí čtení výsledku

1. `MOVED WITHOUT THE ARM ACTING` = 0?
2. `W-GFI/RAMENO`: `povoleno + zamítnuto == příležitost`?
3. **`W-GFI/VYSLEDEK`: z povolených, kolik dojde na cíl vs turnover** —
   tohle je odpověď na otázku, co arm dělá.
4. Sloučená delta + `night_summarize.py` — až teď, jako druhotná kontrola.

## Co dál podle výsledku

- **Dosahuje cíle ve většině povolených případů, málo turnoverů:** kandidát
  na nasazení stejným způsobem jako M14b/P9c (mechanismový doklad, ne
  nutně rozhodná noc).
- **Vysoký podíl turnoverů z povolených:** cena (`P_fail * zbývající < 1`)
  je špatně kalibrovaná — revize prahu, ne zamítnutí konceptu.
