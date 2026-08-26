# Zadání pro Fable, 26.08.2026 — ČÍM SE LIŠÍ DRIVY, KTERÉ TD DALY

## Otázka

**Máme 241 přijímacích drivů, které skončily touchdownem, a 1 443, kterým došla
kola. Startují prakticky ze stejné vzdálenosti — 20,03 proti 21,08 pole.
Čím se ty dvě skupiny liší, a je ten rozdíl NAŠE VOLBA, nebo soupeřova?**

## Proč to má cenu (a proč zrovna teď)

Trpaslík nedá touchdown v **83,6 % zápasů** (korpus 26.08., 3 000 her). Ranní
čtení noci zúžilo problém na tvar, který dosud nikdo nerozebral:

| | skórovali (241) | nestihli čas (1 443) |
|---|---|---|
| vzdálenost na startu | **20,03** | **21,08** |
| **1. držení míče** | **kolo 1,20** | **kolo 3,10** |
| tempo | 2,86 pole/kolo | 2,02 |
| odpor | 3,50 soupeřů | 5,49 |

⇒ **Neplatí, že úspěšné drivy mají lehčí start.** Startují stejně a dopadnou
jinak. Rozdíl dvou kol v prvním držení × 2,61 pole ≈ 5 polí je přesně to, co
chybí. A **není to fumble** (0,10 pickupů/drive) **ani zpožděný start drivu**
(0,24 kola). Příčina není známá.

⭐ Druhý tvar, který stojí za pozornost: tempo skórujících drivů je **2,86 dnes
i 19.08.**, zatímco ve všech ostatních kategoriích spadlo. Vypadá to na PRAHOVÉ
chování — buď se koridor otevře a jede se 2,86, nebo se plazíme kolem 2,0, a nic
mezi tím. Jestli je to pravda, není úkolem „zrychlit průměr", ale **častěji
překlopit do rychlého režimu.**

## Data — máš je, simulovat nemusíš

- `crosses_20260821_data/` — **18 000 her**, 15 matchupů *(⚠️ je to SYMLINK do
  `/home/jan/claude/bb-data/`, čte se normálně)*. Starší engine, před pravidlovým
  kolem.
- `blitzlanding_replic_20260825_corpus_data/` — **3 000 her**, čerstvé z noci
  25.→26.08., dnešní engine. Trpaslík proti všem pěti rasám.

**Co snímek kola nese:** pozice/stav/staty všech 22 hráčů, jména rolí
(`Runner`, `Longbeard`, `Wardancer +Side Step`…), `has_ball`, `ball_x/y`,
`ball_carrier_id`, `turnover`, `touchdown`, `corridor_resistance`,
`corridor_strength`, `required_pace`, `achievable_pace`, `dist_to_endzone_board`,
a blok `plan{}`.
**Události:** `MOVE` (s `from`/`to` u každého kroku!), `BLOCK`, `PUSH`, `DODGE`,
`GFI`, `PICKUP`, `KNOCKED_DOWN`, `ARMOR_BREAK`, `INJURY`, `TURNOVER`,
`STAND_UP`, `FOUL`, `TOUCHDOWN`.
⇒ **Trajektorii každého drivu jde rekonstruovat pole po poli**, včetně toho, kdo
koho kdy srazil.

**Hotový nástroj:** `diag_drive_failure_20260811.py` dělá kategorizaci
A/B/C/D1/D2, ze které čísla výše pocházejí. Čti ho, než budeš psát vlastní.

## ⛔ CO Z DAT NEJDE — neobcházej to, označ to

**Nabídky maker v korpusu NEJSOU.** Jsou to vnitřnosti hledání, ne zaznamenané
události. ⇒ Na otázku *„co jsme mohli udělat místo toho"* data neodpovídají.
Co jde: kdo inicioval kontakt, kdo koho srazil, kudy se šlo, jak se hýbal odpor.
Co nejde: co bylo v nabídce a nevzalo se.

⭐ **Když narazíš na hranici dat, napiš to jako nález** *(„tohle by potřebovalo
přehrání přes engine")*, ne jako odhad. Máme na to zavedený tvar: měření, které
nespadne a přitom lže, je nejdražší chyba, jakou v projektu děláme.

## Co čekám jako výstup

1. **Rozdíl mezi skupinami rozložený na složky** — a u každé, jestli ji drží
   naše rozhodnutí, nebo soupeřův zásah.
2. **Ověření nebo vyvrácení prahové hypotézy** (2,86 vs 2,02, nic mezi tím).
   Rozdělení tempa, ne průměr.
3. **Proč je první držení v kole 3,10** — když to není fumble ani start drivu.
   Tohle je hlavní neznámá.
4. Kde data nestačí, seznam toho, co by se muselo doměřit.

⛔ **Nepiš doporučení, co změnit v kódu.** Pořadí okruhů je závazné
(PRAVIDLA ✅ → POHYB → KLEC → BLITZ → PŘIHRÁVKA → CELOTAH) a tenhle rozbor je
vstup, ne oprava. Nálezy zapiš tak, aby šly zařadit.

## Kontext, který nemusíš dohledávat

- **70–87 % ztrát míče je sražený nosič blitzem/blokem** (1 405 z 1 622 C-drivů).
- **Klec nestojí ve 28–30 % kol s míčem**, a je to z 97,7 % naše volba.
- **Brána klece se nikdy nekonzultuje** — `NOT_CONSULTED` 100 % na 44 585 kolech.
- **Metr D1 („pozdní start") je na hraně definice:** 8 kol × 2,61 = 20,9 pole
  proti 21,08 pole startovní vzdálenosti ⇒ plný drive nemá žádnou rezervu.
  ⚠️ To NEZNAMENÁ, že je nesplnitelný — 241 drivů ho splnilo.
