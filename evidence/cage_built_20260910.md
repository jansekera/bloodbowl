# STAVÍME VŮBEC KLEC? — KONTROLA PŘED MĚŘENÍM POSUNU (10.09.2026)

**Zadání uživatele 10.09.:** *„Hlavní je kontrola, že stavíme správnou a čistou
klec — předtím jsme kontrolovali posun klece a až potom jsme zjistili, že klec
nestavíme; tak nemůžeme měřit její posun."*

⇒ Je to jeho vlastní pravidlo z `celotah_situace.md`: ⛔ **NEMĚŘIT, CO MODEL
NEUMÍ.** Skript `diag_cage_built_20260910.py`, korpus `corpus_m11_20260909_data`
*(1000 her, engine `cf8634e8`, produkční nastavení)*. Definice drží
`cageSnapshot` *(`cage_advance.cpp:113-135`)*, ať jsou čísla srovnatelná s tím,
co engine počítá sám.

## ⚠️⚠️⚠️ OPRAVA TÉHOŽ DNE: VŠECHNA ZDEJŠÍ ČÍSLA JSOU ZE **ZAČÁTKU** KOLA

`captureTurnSnapshot` *(`game_simulator.cpp:748`)* se volá **na hranici kola**
*(`:851`, `:905`)*, tedy **před** první akcí — razítkuje si to samo polem
`eligible_at_start` a odtud jsou i `cage_corners` a **`carrier_tz`**
*(`:797-804`)*. ⇒ **Snímek je stav PO SOUPEŘOVĚ KOLE.**

⭐ **Pro „stavíme klec?" to nevadí** a čísla níž **platí**: klec, která na
začátku našeho kola nestojí, nebyla postavena v tom minulém.
⛔ **Ale pro cokoli, co závisí na KONCI kola, se použít nesmí** — a přesně to
udělal odstavec o nosiči níž. Konec kola se čte z `turn_logs[i+1]`; přeměřeno
v **[[carrier_marked_end_20260910]]**.
⚠️ **Chyba je moje, ne uživatelova** — číslo i rámování vzniklo v mém rozboru.

## ⛔⛔⛔ ODPOVĚĎ: KLEC NESTAVÍME. ČISTÁ KLEC JE V 8,3 % KOL.

**Trpaslík, 7 173 kol se stojícím nosičem** *(histogram, ne průměr — na tom to
stojí)*:

| rohů obsazených naším stojícím hráčem | | |
|---|---|---|
| **0 rohů** | 2 372 | **33,1 %** |
| 1 roh | 1 548 | 21,6 % |
| 2 rohy | 1 314 | 18,3 % |
| 3 rohy | 909 | 12,7 % |
| **4 rohy** | 1 030 | **14,4 %** |
| *(zbytek — musí být 0)* | 0 | |

⇒ **průměr 1,54 ze 4 — a právě ten se dřív čítal.** Histogram ale říká něco
jiného než *„většinou stavíme dva"*: **v každém třetím kole nestojí ani jeden
roh**, a plný prstenec je jen v **14,4 %**.

| ⭐ ČISTÁ KLEC *(4 rohy · 0 označených · nosič mimo TZ)* | **593 = 8,27 %** |
|---|---|

**Pozitivní kontrola:** nejlepší dosažené kolo = **4 rohy, 0 označených, nosič
mimo TZ** ⇒ čítač jedničku najít **umí**, těch 8 % není rozbité měřidlo.

## ⛔⛔ NOSIČ PŘICHÁZÍ DO KOLA OZNAČENÝ — ⚠️ ALE JE TO Z VĚTŠÍ ČÁSTI SOUPEŘOVA PRÁCE

| | trpaslík | ork |
|---|---|---|
| **nosič v soupeřově TZ NA ZAČÁTKU našeho kola** | **53,1 %** | 41,9 % |
| aspoň 1 roh označený *(taky začátek kola)* | 30,7 % | 24,0 % |
| čistá klec *(taky začátek kola)* | 8,27 % | 12,10 % |

⛔⛔⛔ **PŮVODNÍ NADPIS ZNĚL „DOMINANTNÍ DÍRA JE NOSIČ SÁM" A BYL ŠPATNĚ.**
Tohle číslo je ze **začátku** kola, tedy **po soupeřově kole** ⇒ měří
z převážné části **JEHO** označkování, ne naši volbu cílového pole. Použít ho
jako motivaci `P42` *(„nosič nekončí v kontaktu")* znamená **číst jeden konec
kola jako druhý.**

⭐ **Předěláno na správném snímku** *(`turn_logs[i+1]`, týž korpus)* —
plné čtení **[[carrier_marked_end_20260910]]**:
**na začátku 53,0 %** *(zdejší číslo reprodukováno)* → **na KONCI našeho kola
24,6 %** stojící soused *(+ 19,2 % jen ležící, který stojí soupeře blitz)*.
A **z těch porušení jen 0,5 % vzniklo tím, že chůze nosiče skončila
v kontaktu** — dimenze „volba pole" je v `expandAdvance` **hotová od 07.08.**

⭐ **Doktrína z kódu** *(`cage_advance.cpp:35-38`, 11.08.: „nosič končí označený
ve 40 % advance kol proti 11 % u rohů")* **tím vyvrácená není** — je to jiná
populace *(advance kola plánovače)* a **nesrovnává se napříč érami**
[[feedback_moving_baseline_only_paired_ab]].

## ⭐ ALE TVAR SE NEZTRATIL — DIAGONÁLY SE PREFERUJÍ 2,3×

Aby se to nečetlo jako „těla se poflakují kdekoliv": obsazenost **na pole** je
**38,5 % u diagonál** *(1,54 ze 4)* proti **17,0 % u ortogonál** *(0,68 ze 4)*.
⇒ **Nějaký klecový záměr v enginu existuje** — chybí ho **dokončit**, ne
nasměrovat. A těla u nosiče vůbec jsou: aspoň jedno sousedí v **79,2 %** kol.

## ⇒ CO Z TOHO PLYNE PRO POŘADÍ PRACÍ *(moje odvození, ne uživatelova věta)*

1. ⛔ **Měřit posun klece je pořád předčasné** — přesně jak uživatel řekl. Klec
   je celá jen v 8,3 % kol; posun něčeho, co ve 92 % neexistuje, není měřitelný.
2. ⭐⭐ **První položkou okruhu se stává `P42`**, ne `W-CIL`.
   ⚠️ **ZDŮVODNĚNÍ OPRAVENO 10.09.:** původně tu stálo *„nosič v TZ v 53 % kol"*
   — to je začátek kola, viz výš. **Platný důvod je z konce kola**
   *([[carrier_marked_end_20260910]])*: **26,2 % našich kol končí tak, že
   soupeř má na míč blok ZDARMA** *(13,4 % dokonce dva a víc)*, je to
   **jedno tělo bez koordinace**, a **strop opravitelnosti je 97,6 %**
   *(tolik porušení mělo kam uhnout)*. ⛔ A **řešení není to, které má `P42`
   zapsané**: „volba pole" pokrývá 0,5 %, zbytek jsou dvě jiná ramena.
3. ⏰ **`W-CIL` zůstává druhý** — a jeho zadání se tím zpřesnilo: nejde
   o *„špatný směr"*, ale o **doplnění chybějících rohů** *(0 rohů ve 33 % kol
   při 79 % kol s aspoň jedním tělem u nosiče ⇒ těla tam jsou, jen ne na
   diagonálách)*.
4. ⚠️ **A pozor na past, do které jsme málem šlápli znovu:** kdyby se teď
   opravovalo „následování těl do boku" *(P32/P46/W-CIL)*, měřilo by se to na
   populaci, kde klec v 92 % kol není — tedy **stejná chyba jako s posunem**.
