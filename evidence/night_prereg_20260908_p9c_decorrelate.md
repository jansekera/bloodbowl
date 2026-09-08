# PŘEDREGISTRACE — P9c DEKORELACE AGILITY CÍLE vs TACKLE ODSOUVAJÍCÍHO
**Zapsáno PŘED spuštěním. Prahy platí tak, jak stojí tady.**

---

## Proč tenhle běh existuje

`pushGeometryArm`/P9c (výběr cílového pole při odsunu — pryč od rohu naší
klece, pryč od nosiče) vyšel 18.-19.08. na `dw-we` (6800 párů) jako
**"efekt se nekonal"** (+0,0017 ± 0,0060 SE). 08.09. přepočet stejného
korpusu podle strany ukázal **skrytou asymetrii, 5,28σ jistou**: efekt na
trpaslíka (drží-li rameno on) +0,0187, na elfa (drží-li on) −0,0171 —
každý zvlášť šum, ale rozdíl mezi nimi jistý.

Dvě soupeřící vysvětlení, nerozlišitelná v dw-we, protože tam cestují
spolu s rasou:
* **(A) agilita cíle** — mrštný odsouvaný (Dodge, MA6+) si špatné pole
  kompenzuje pohybem.
* **(B) Tackle odsouvající strany** — Tackle přímo ruší Dodge při
  výsledku bloku DEFENDER STUMBLES (`block_handler.cpp:898`), takže
  odsouvaný s Dodge proti Tackle spadne stejně, jako by Dodge neměl.
  Trpaslík má Tackle na 9/13, elf skoro nikde.

Nový matchup **`orc-mb` vs `wood-elf-agile`** (index 8, `roster.cpp`)
tohle rozděluje: ork má Tackle jen na 1/19 (skoro žádný), elfí fill
Linemani dostali navíc Dodge (agilní cíl BEZ Tackle na útočící straně).
Ork navíc dostal Mighty Blow na fill Linemany — nesouvisí s Tackle/Dodge,
jen vyrovnává sílu týmů, ať se neměří rozdíl v TV místo rozdílu v
mechanice.

**Tohle je SONDA, ne rozhodná noc** — uživatel 08.09.: *"to se snad
změří rychle"* / *"ale pak jen kratší test"*. Cílem je směr, ne přesné
číslo. Pokud se ukáže něco silného, doměří se to větší nocí zvlášť.

## Co se čte jako rozhodné

**Ne** vestavěná "PAIRED delta chess" sama o sobě — ta sčítá obě
orientace dohromady a přesně tenhle součet už jednou schoval asymetrii
(to je to, co se stalo 18.08.). Rozhodné je **ruční rozdělení podle
`cand_home` ze syrových řádků `diag_pushgeom_rows.jsonl`**, stejná
metoda jako 08.09.:

1. `cand_home=true` (ork drží rameno, odsouvá agilního elfa bez Tackle
   na své straně) — **klíčové čtení**. Kladný efekt → podporuje (A)
   agilitu. Nulový/záporný → podporuje (B) Tackle.
2. `cand_home=false` (elf drží rameno, odsouvá orka bez Dodge/agility) —
   kontrolní čtení, obě hypotézy tu čekají null.
3. K oběma je potřeba **baseline `p_hat`** z `mode 2` (SEED-CHECK,
   stejná konfigurace na obou stranách) na TÉMŽ matchupu (index 8) —
   dosavadní `control_mode2` (n=50) byl pro `dw-we`, pro tenhle nový pár
   ras neplatí a musí se přeběhnout znovu, i když jen v malém.

## Konfigurace

* **Mode 5** (`pushGeometryArm`), **matchup index 8** (`orc-mb` /
  `wood-elf-agile`).
* **PAIRS = 80** na mode 5 (sonda, ne rozhodná síla — zmenšeno z
  původních 200 po zjištění tempa, viz níž; uživatel 08.09.: "ale pak
  jen kratší test").
* **Mode 2 kontrola: 16 párů (32 her)** na témže indexu — taky zmenšeno
  ze 50 z téhož důvodu.
* ✅ **TEMPO ZMĚŘENO 08.09.:** 4 páry mode 2 na matchupu 8, jedno vlákno,
  8m35s ⇒ **~129 s/pár**. Při WORKERS=4 (standardní laptop-noc default,
  `run_laptop_night.sh`): 80 párů ≈ 43 min, 16 párů kontroly ≈ 9 min.
  Běží se sekvenčně (kontrola, pak sonda), ne najednou — celkem ~52 min,
  ne den stroje.
* Cage gate zůstává vypnutá v obou ramenech (stejně jako 18.08.).

## Nulová kontrola

Stejný problém jako 18.08.: matchup s nulovou expozicí neexistuje,
odsouvá se v každé hře. Skutečná kontrola je `MOVED WITHOUT THE ARM
ACTING` == 0, ne separátní nulový matchup.

## Předregistrované předpovědi *(strojově kontrolované, `.preds`)*

Standardní pole (`leak`, `arm_acted`, `n_nonzero`, `delta`, `delta_1s`)
se čtou ze sloučené (obě orientace dohromady) delty jako vždy — ale
**tahle sloučená delta NENÍ to rozhodné číslo**, viz sekce výš. Slouží
jen jako kontrola, že běh proběhl čistě, stejně jako u každé jiné noci.

| | čekám | proč |
|---|---|---|
| leak | 0 | jinak se nic nečte |
| arm_acted | ≥ 0,90 | odsun nastává skoro v každé hře |
| n_nonzero | 0,40–0,75 | geometrie odsunu se přesměrovává podobně často jako u dw-we (57,5 %), rasa by tenhle podíl neměla moc měnit |
| delta (sloučená) | −0,10 až +0,10 | široké, protože se PŘEDPOVÍDÁ asymetrie mezi orientacemi, ne jejich součet — součet může vyjít cokoliv |
| delta_1s | polovina delty | totéž jednostranně |

## Předpovědi na SPLIT (ruční, ne strojově kontrolované, ale zapsané předem)

* `cand_push_dodge` podíl u `cand_home=true` (ork odsouvá elfa): **75–95 %**
  — fill Linemani mají teď Dodge navíc, Wardancer/Catcher ho měli už dřív,
  jen Thrower/Treeman/2×Lineman+Wrestle ho nemají.
* `cand_push_fastma` podíl u `cand_home=true`: **~100 %** — MA se
  u `wood-elf-agile` nezměnilo, pořád skoro celá soupiska MA6+.
* `cand_push_dodge`/`cand_push_fastma` u `cand_home=false` (elf odsouvá
  orka): **blízko 0** — `orc-mb` dostal jen Mighty Blow, ne agilitu.
* **Rozhodné, ale bez číselné předpovědi (opravdu nevíme):** znaménko
  efektu u `cand_home=true` po odečtení `p_hat` z mode-2 kontroly.
  Kladné → (A) agilita. Nulové/záporné → (B) Tackle. Predikce se
  záměrně nezavazuje k číslu, protože jde o SONDU na neznámý matchup,
  ne o doměření síly.

## Pořadí čtení výsledku

1. `MOVED WITHOUT THE ARM ACTING` = 0? Ne ⇒ konec.
2. `arm acted in N/M`.
3. `n_nonzero`.
4. Sloučená delta (kontrola čistoty běhu, ne odpověď na otázku).
5. **Teprve pak** ruční split podle `cand_home` s mode-2 `p_hat` —
   tohle je odpověď na otázku, proč tenhle běh vůbec je.
