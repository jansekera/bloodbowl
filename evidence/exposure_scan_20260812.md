# Exposure scan — co deska na konci našeho kola dává soupeři (12.08.)

Skript: `diag_exposure_scan_20260812.py` · korpus `diag_replay_mine_20260811b_data`
(120 her, dwarf vs human/orc/skaven/wood-elf po 30). **1603 vzorků** = naše kola,
po kterých následuje celé soupeřovo kolo na stejné desce; vyloučeno 320
(46 naše kolo s TD, 55 soupeřovo kolo s TD, 219 hranice poločasu/konec hry).

Deska na konci našeho kola i = `turn_logs[i+1]` (pozice v logu = začátek tahu,
ověřeno na pohybech). Výsledek soupeřova kola = srovnání stavů `[i+1] → [i+2]`
po id hráče — mezi tím jedná jen soupeř, takže každý náš stojící, který je pak
na zemi, padl v jejich kole.

## 1) Kandidátní veličiny

| | definice |
|---|---|
| **FB** | bezplatné bloky: kolik JEJICH stojících sousedí s naším stojícím (Block bez blitzu) |
| **FB2** | z toho s ≥2 kostkami na nejlepší sousední cíl (ST + asistence, Guard obou stran; Guard = `+Guard` ve jménu) |
| MARKED | kolik NAŠICH stojí vedle jejich stojícího (duál FB) |
| SURF | naši stojící na y≤1 / y≥13 se soupeřem vedle |
| ESC | čistá úniková pole nosiče (volné sousední pole s 0 jejich TZ) |
| REACH | kolik jejich stojících dosáhne k nosiči (BFS 8směr přes volná pole, MA+2) |
| **REACH0** | z toho **bez jediného dodge** (cesta neopouští pole v naší TZ) |
| **BLZ** | nejlepší kostky jejich blitzu na nosiče (asistence = jejich hráči už u nosiče; obranné asistence u neznámé pozice útočníka ~0 → jejich kostky mírně nadsazené) |
| CCBAD | zlé rohy klece: diagonály nosiče, roh dobrý jen s naším stojícím mimo TZ |

Výsledky soupeřova kola: `down` = naši sražení (0,571/kolo), `lost` = naši pryč
ze hřiště (0,066/kolo), `ball_lost` = drželi jsme a už nedržíme (14,1 %, n=951),
`appr` = posun nejbližšího soupeře k nosiči.

## 2) Hodnoty na korpusu (průměr / kolo, podle rasy soupeře)

| | vše | human | orc | skaven | wood-elf |
|---|---|---|---|---|---|
| FB | 1,89 | 2,04 | 1,61 | 1,99 | 1,91 |
| FB2 | 1,03 | 1,18 | 1,18 | 0,81 | 0,91 |
| REACH0 | 1,89 | 1,96 | 1,69 | 1,94 | 1,93 |
| BLZ (prům.) | 1,40 | 1,56 | 1,85 | 0,87 | 1,50 |
| down | 0,571 | 0,633 | 0,655 | 0,441 | 0,547 |
| ball_lost | 14,1 % | 13,7 % | 16,9 % | 9,5 % | 16,7 % |

## 3) ⭐ Co předpovídá (Pearson r; * = významné při 2/√n)

| | down | lost | ball_lost | appr |
|---|---|---|---|---|
| FB | **0,395*** | 0,097* | 0,124* | −0,083* |
| FB2 | **0,399*** | 0,096* | 0,098* | −0,070* |
| MARKED | 0,250* | 0,055* | 0,099* | −0,039 |
| SURF | −0,032 | 0,010 | −0,007 | 0,008 |
| ESC | −0,070* | −0,026 | −0,145* | 0,039 |
| REACH | 0,219* | 0,122* | 0,178* | −0,116* |
| REACH0 | 0,077* | 0,097* | **0,336*** | −0,039 |
| BLZ | 0,164* | 0,080* | **0,205*** | **−0,241*** |
| CCBAD | 0,005 | 0,010 | 0,165* | 0,068 |

Uvnitř každé rasy zvlášť drží totéž (není to artefakt míchání ras):
FB×down 0,36–0,46 u všech čtyř; REACH0×ball_lost 0,31–0,35 u všech čtyř.

**Gradienty (to hlavní):**

ball_lost podle REACH0: **0 → 1,8 %** · 1 → 8,3 % · 2–3 → 21,9 % · 4+ → 32,9 %.
Koleno je ostře na nule — už jediný hráč, který dosáhne bez dodge, ztrátu ztrojnásobí.

ball_lost podle BLZ: nedosáhne → 0 % · 1 kostka → 9,1 % · 2 kostky → 23,3 % · 3 → 69 %.

Křížem (nezávislý přínos obou):

| ball_lost | BLZ ≤1d/— | BLZ 2d | BLZ 3d |
|---|---|---|---|
| REACH0 = 0 | 0,01 (299) | 0,04 (96) | — |
| REACH0 1–2 | 0,12 (152) | 0,14 (95) | 0,50 (2) |
| REACH0 3+ | 0,15 (127) | **0,40 (169)** | 0,73 (11) |

down podle FB2: 0 → 0,32 · 1 → 0,70 · 2 → 0,82 · 3 → 1,00 · 4+ → 1,19 —
každý bezplatný ≥2kostkový blok ≈ **+0,2–0,35 sraženého našeho za kolo**;
FB2 ≤ 1 → 0,41 vs FB2 ≥ 2 → 1,00. Cross-tab MARKED×FB2 ukazuje, že gradient
nese FB2, ne samotný kontakt (MARKED je slabší v každém sloupci).

**Zahazuje se (předregistrace):** SURF (r≈0 všude; výskyt jen 2,6 % kol — moc
vzácné, nic o něm nejde říct) · MARKED (podmnožina signálu FB2) · ESC a CCBAD
(slabé duplikáty signálu REACH0 — roh klece škodí jen tím, že otevírá cestu) ·
REACH (dominován REACH0). `lost` (odchody ze hřiště) předpovídá všechno jen slabě
— base rate 0,066/kolo je pod rozlišením korpusu.

## 4) Návrh povinnosti do procedury (dvě čísla, ne dashboard)

**E1 (míč): Na konci našeho kola nesmí na nosiče bez dodge dosáhnout ŽÁDNÝ
jejich stojící hráč (REACH0 = 0).**
X = 0, protože koleno je na nule: 0 → 1,8 % ztráty míče, 1 → 8,3 %, 2–3 → 22 %.
Dnes to plníme ve 42 % kol s míčem (395/951), takže je to dosažitelné, ne utopie.
Když to nejde, záložní mez: žádný ≥2kostkový blitz na nosiče (BLZ ≤ 1) — 2kostkový
blitz znamená 23 % (a při REACH0 ≥ 3 dokonce 40 %) ztrátu.

**E2 (mlácení): Na konci našeho kola smí mít soupeř nejvýš 1 bezplatný blok
se ≥2 kostkami (FB2 ≤ 1); cíl 0.**
X = 1: pod ním 0,41 sraženého/kolo, nad ním 1,00. Každý povolený 2kostkový blok
zdarma ≈ +0,2–0,35 sraženého. Záměrně NE mez na FB (prostý kontakt) — markování
je naše doktrína (boxing-in) a MARKED×FB2 tabulka ukazuje, že škodí kostky, ne
kontakt: markovat smíme, ale tak, aby na nás neměli 2 kostky (Guard k tomu máme
na 6 z 11).

## 5) Co jsem NEZMĚŘIL a proč

- **Turnover v našem NÁSLEDUJÍCÍM kole** — měřil jsem ztrátu míče během jejich
  kola (čistší kauzálně); náš vlastní turnover o kolo později je zprostředkovaný
  a chtěl by vlastní řetěz.
- **Kauzalitu** — korelace na neintervenčních datech; FB2↔down může částečně být
  „kontakt plodí kontakt". Předregistrovaný test byl ale predikční a ten prošel.
- **Surf** — výskyt 2,6 % kol nedává sílu; nezamítám surf-doktrínu, jen ji tenhle
  korpus neumí změřit.
- **Ostatní korpusy** (20260811, 20260811c) — zadání: jeden korpus; čísla nejsou
  replikovaná.
- **BLZ přesně** — obranné asistence u neznámé pozice útočníka ignoruji a soupeř
  by si mohl asistence teprve přivést; BLZ je dolní odhad práce, kterou soupeř
  musí udělat.
- Kolo s TD na obou stranách a hranice poločasů (320 kol) — deska se přestavuje,
  vyloučeno dle zadání.
