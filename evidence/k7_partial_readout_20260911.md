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
