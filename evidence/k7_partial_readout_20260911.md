# K7 — KORPUS NEDOBĚHL (171/600), ALE ODPOVĚĎ UŽ DAL (11.09.2026)

**Stav sběru:** `corpus_k7_20260910_data`, engine `496f5a03`, produkční
nastavení, žádné rameno. Plán byl 600 her, **sebráno 171** — proces
`pid 1716280` padl spolu se session 10.09. ve 20:22 (došly tokeny).
Nic neběží, `chain.log` končí startem.

## ⭐⭐⭐ ZÁVĚR: DOBÍHAT SE NEMUSÍ

171 her dalo **1 227 trpasličích kol** se stojícím nosičem a **1 076
spárovaných kol** — dost na obě čísla, na která se korpus sbíral. Noc navíc
by je jen zúžila, ne změnila směr. [[feedback_never_trade_power_for_walltime]]
platí obráceně taky: **noc, která nic nerozhodne, se neplatí.**

⭐ **Kontrola složení PŘED čtením** (jinak by to byl rozdíl v matchupech, ne
v enginu): useknutí sběru proporce nerozbilo —
dw-dw **19,9 %** proti bázovým **20,0 %**, zbylých osm matchupů 9,9-10,5 %
proti 10,0 %. ⇒ populace je srovnatelná.

## SROVNÁNÍ S BÁZÍ

Báze = `corpus_m11_20260909_data`, engine **`cf8634e8`**, 1 000 her
([[cage_built_20260910]], [[carrier_marked_end_20260910]]). Trpaslík.

| | K7 `496f5a03` | báze `cf8634e8` | delta | σ |
|---|---|---|---|---|
| **0 rohů** *(ani jeden)* | 37,41 % | 33,07 % | **+4,34 pp** | **+2,99** |
| **4 rohy** *(plný prstenec)* | 11,74 % | 14,36 % | −2,62 pp | **−2,42** |
| čistá klec | 7,09 % | 8,27 % | −1,18 pp | −1,38 |
| nosič ZDARMA *(konec kola)* | 25,00 % | 24,60 % | +0,40 pp | +0,28 |
| nosič ZA BLITZ *(konec kola)* | 16,26 % | 19,20 % | −2,94 pp | −2,26 |
| EXPOZICE celkem | 41,26 % | 43,80 % | −2,54 pp | −1,55 |

## ⛔⛔ ČTE SE TO TAKTO — A DÁL NE

1. **Obě čísla, kvůli kterým se korpus sbíral, se reprodukovala.** „Klec
   nestavíme" *(čistá klec ~7-8 %)* i „nosič končí kolo označený ~25 %" platí
   dál. **Pořadí prací, které z nich vzešlo, se tím nemění.**
2. ⚠️ **Tvar klece se posunul K HORŠÍMU, ~3σ.** „Ani jeden roh" přibylo
   o 4,3 pp, plný prstenec ubyl o 2,6 pp. To není šum.
3. ⛔⛔⛔ **PŘIČÍST TO ČEMUKOLI KONKRÉTNÍMU NELZE.** Mezi `cf8634e8` a
   `496f5a03` se změnilo **všechno z 10.09. naráz**: celá série KLEC
   K1-K5+K4c, blitzové změny (`a1d9b77d`, `91ebc96d`) **a pravidlová oprava
   dodge při VÝSTUPU** (`496f5a03`). Je to **nepárové srovnání napříč érami**
   — přesně to, co zakazuje [[feedback_moving_baseline_only_paired_ab]], a
   přesně ta past, do které spadla noc Q3 *(dvě změny naráz)*.
   ⇒ **Tohle je SIGNÁL, ŽE SE MÁ MĚŘIT, ne naměřený výsledek.**
4. ⭐ **Nejpravděpodobnější kandidát je pravidlová oprava, ne klec.** Před
   `496f5a03` vracela `pathFailProb` pro ústup z kontaktu **0,0000** —
   odchod od nosiče byl zadarmo. Po opravě stojí. Že se tělo přestalo
   vyskytovat u nosiče, by tedy mohl být **důsledek správného ocenění**, ne
   regrese volby pole. ⚠️ **Tohle je hypotéza, moje, nezměřená.**
   Souhlasí s ní i druhá polovina tabulky: ubylo LEŽÍCÍCH sousedů
   *(ZA BLITZ −2,94 pp)*, tedy soupeřů, ne jen našich těl.

## ⇒ CO BY ROZHODLO

Párové A/B na `cageCornerArm`-like vypínači to **není** — ta změna vypínač
nemá, je to pravidlová oprava. Rozhodlo by **přeměření báze na `496f5a03`
s vypnutou jedinou věcí**, nebo — levněji — **korpus na commitu těsně PŘED
`496f5a03`** (tj. `41a2fb44`) proti tomuhle. Jeden korpus, ~171 her stačí
podle stejné úvahy jako výš.

Souvisí: [[cage_built_20260910]] · [[carrier_marked_end_20260910]] ·
[[feedback_moving_baseline_only_paired_ab]] ·
[[feedback_measure_what_the_change_does]]

Plné výstupy: `evidence/k7_cage_built_20260911.txt`,
`evidence/k7_carrier_marked_end_20260911.txt`

---

# ⭐⭐⭐ DODATEK 11.09. — KOREKCE RÁMOVÁNÍ OD UŽIVATELE

**Uživatel 11.09.:** *„pravidlová oprava se nedá měřit zlepšením — to je
oprava, ne pokus o vylepšení"* a *„přeměřit asi sedí — ale jen jestli se to
celé nerozbilo."*

⇒ **Tabulka výš se tím NERUŠÍ, ale mění se otázka, kterou zodpovídá.**
Ptát se, jestli `496f5a03` klec *zlepšila*, je špatně položené: oprava
sesazuje engine na text pravidel, ne na metriku.

## ⛔⛔ A DŮSLEDEK, KTERÝ JDE DÁL: BÁZE SAMA JE Z VADNÉHO MĚŘIDLA

`cf8634e8` je engine, kde `pathFailProb` vracela pro ústup z kontaktu
**0,0000** — odchod od nosiče byl zadarmo. Čísla **33,07 % / 8,27 %** tedy
nejsou cíl, ke kterému se vracet; jsou to čísla naměřená enginem, který
cenu pohybu počítal špatně.
⇒ **K7 tu bázi nereprodukuje — K7 ji NAHRAZUJE.** Platná deskriptivní čísla
pro klec jsou od teď ta z `496f5a03`:
**0 rohů 37,41 % · 4 rohy 11,74 % · čistá klec 7,09 % · nosič ZDARMA 25,00 %.**
⚠️ Závěr *„klec nestavíme"* tím nepadá — 7,09 % je pořád ~7 %.

## ✅ „NEROZBILO SE TO?" — ODPOVĚĎ: NE. A NOVÝ KORPUS NA TO NEBYL POTŘEBA

`diag_did_it_break_20260911.py`, oba korpusy už na disku, žádný sběr.
Měří se, jestli engine **hraje hru**, ne jestli je klec hezčí.

| | PO `496f5a03` *(171 her)* | PŘED `cf8634e8` *(1 000 her)* | σ |
|---|---|---|---|
| TD na zápas *(obě strany)* | 0,702 | 0,639 | +1,59 |
| turnoverů na kolo | 0,460 | 0,459 | **+0,12** |
| zápasů kratších než 32 kol | 0,000 | 0,000 | — |
| zápasů 0:0 | 43,3 % | 49,2 % | −1,43 |

⭐ **Turnovery jsou prakticky totožné** *(0,12 σ)* — to je ten ukazatel, na
kterém by se rozbitá cena pohybu projevila nejdřív *(dražší dodge ⇒ víc
pádů ⇒ víc turnoverů)*. Není tam. Žádný zápas se nezasekl. Skóruje se spíš
**víc**, ne míň.
⇒ **Oprava nic nerozbila.** Posun tvaru klece je engine, který nově platí
to, co pravidla účtují — ne porucha.

## ⛔ CO ZŮSTÁVÁ OTEVŘENÉ (a proč se to NEMĚŘÍ řetězem korpusů)

Otázka *„nese ten posun pravidlová oprava, nebo naše práce K1-K5?"* je
**pořád nezodpovězená** — ale rozpad na 8 změn po 171 hrách ji nezodpoví.
Silový propočet (`p=0,33`, 7,18 trpasličích kol na hru):

| her na korpus | kol | SE rozdílu | rozhodne až efekt |
|---|---|---|---|
| 171 | 1 227 | 1,90 pp | **3,80 pp** |
| 524 | 3 762 | 1,08 pp | 2,17 pp |
| 2 468 | 17 720 | 0,50 pp | 1,00 pp |

Celkový posun **4,34 pp na 8 skutečných změn** = rovnoměrně **0,54 pp** na
změnu ⇒ na rozlišení jedné změny by bylo třeba **~8 500 her na korpus,
41 h sběru na JEDEN z devíti**. [[feedback_size_nights_with_margin_not_just_enough]]
⇒ **Řetěz korpusů je neuplatitelný.** Jediný dělicí řez, který by se zaplatil,
je jeden korpus na `41a2fb44` *(commit těsně před pravidlovou opravou,
tytéž seedy `SEED_BASE=20261000`, ~50 min)* — oddělí pravidlovou opravu od
zbylých sedmi změn. **Worktree `bb-pre-dodge-41a2fb44` je postavený a
připravený**, sběr NEspuštěn.
