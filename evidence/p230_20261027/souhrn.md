# P230 — měření 27. 10. 2026 (worktree `p228-spinavy-roh`, stav 7745277e)

48 poločasů útoku (orc, human, dwarf proti human, wood-elf, skaven; 8 dvojic × 6), soupeř kope. Každý náš tah
s míčem dvakrát ze stejného stavu a semínka: **vše** (P228 + P229 + P230) a **bez P230** (maska 4096). Po
rameni jen odpověď soupeře. 361 tahů (296 s míčem), 0 chyb. Běh 19:06–21:12. Tabulka `souhrn_tabulka.txt`.

## Výsledek: P230 v náhodné hře nenastal

- Ramena se neliší **v žádném z 296 tahů s míčem** — přestavba po řadách se nespustila ani jednou.
- 5 tahů „klec o řadu bokem“ (`radu`) proběhlo v obou ramenech stejně ⇒ udělalo je hledání, ne P230.
- Souhlasí s měřením P229 (`evidence/p229_20261027/souhrn.md`): zeď před pomalejší klecí bez povolené rány
  nastala ve 2 tazích z 297. P230 se spouští jen uvnitř té situace.
- ⛔ Nula tu **neznamená „nic nedělá“** — situace v těchto hrách skoro není. Pozitivní kontrola v enginu:
  testy `OneCageWallShift.*` (řadič přestavbu naplánuje). Co P230 dělá ve hře, ukáže jen měření
  **na cílených pozicích** (zeď před pomalejší klecí, např. pozice partie `zed-orkove`), ne náhodné hry.

## Kalibrace hrozby (první odhad, obě ramena = stejné stavy dvakrát)

| předpověď hrozby po našem tahu | tahů | nosič sražen | míč ztracen |
|---|---:|---:|---:|
| 0,00–0,05 | 262 | 0 % | 0 % |
| 0,05–0,10 | 210 | 1 % | 1 % |
| 0,10–0,15 | 66 | 6 % | 6 % |
| 0,15–0,20 | 8 | 0 % | 0 % |
| 0,20–0,25 | 8 | 0 % | 0 % |
| 0,25–0,29 | 10 | 20 % | 20 % |
| 0,29–0,35 | 30 | 13 % | 20 % |
| 0,35+ | 10 | 60 % | 80 % |

- Pod 0,15 je skutečnost **pod** předpovědí (předpověď počítá nejhorší odpověď, soupeř ji ne vždy najde).
- V pásmu 0,15–0,29 je jen 26 tahů (13 různých stavů) — na hranici 0,20 to nestačí. Proto delší
  kalibrace `kalibrace.py`: 80 her (10 dvojic × 8), každý stav po našem tahu **8× s jinými kostkami**
  soupeře (uživatel: „hodnoty, se kterými to porovnáváme, jsou dány hody kostek“). Výsledek
  `kalibrace_tabulka.txt`.

## Kalibrace hrozby — dlouhá (28.10., `kalibrace_tabulka.txt`)

485 stavů po našem tahu, každý 8× s jinými kostkami soupeře (3 880 odpovědí):

| předpověď hrozby | odpovědí | míč ztracen (95% interval) |
|---|---:|---:|
| 0,00–0,15 | 3 464 | 0,4 % (0–1 %) |
| 0,15–0,20 | 120 | 4 % (2–9 %) |
| 0,20–0,29 | 120 | 14 % (9–22 %) |
| 0,29 a víc | 176 | 30 % (24–37 %) |

⇒ uživatel 28.10. „hranice podle času“: s časovou rezervou 0,15, bez ní 0,30 (`riskEdge`, P232). Pod ~0,25 je
skutečná ztráta pod předpovědí (soupeř nejhorší odpověď vždy nenajde), nad 0,29 se blíží.

## Cílené pozice se zdí (28.10., `cilene_tabulka.txt`)

90 pozic (orkové / trpaslíci × lidé / skaveni / lesní elfové, 15 od každé dvojice), zeď 5 nejrychlejších soupeřů
dva sloupce před klecí. Knihovna `zmrazeno_p232` (commit 407527c6). 1. běh 27.10. v noci spadl — ve zmrazené kopii
chyběl soubor s vahami (kouř běžel odjinud); záloha `zaloha_selhany_beh_20261028/`.

| na 100 pozic | vše | bez zálohy vzadu | rozdíl ± SE |
|---|---:|---:|---:|
| hráčů zálohy (průměr) | 2,99 | 2,60 | +0,39 ± 0,06 |
| nekrytých řad 1–13 (průměr) | 1,08 | 2,41 | **−1,33 ± 0,42** |
| posun míče vpřed (polí na 100) | 34 | 38 | −3,3 ± 1,9 |
| míč ztracen v odpovědi | 6,7 | 7,8 | −1,1 ± 1,1 |

- **Záloha vzadu (P230, 8192) dělá, co má:** nekrytých řad o 1,3 na pozici méně (trpaslíci 0,33 × 1,96, orkové
  1,82 × 2,87). Cena: postup klece o ~3 pole na 100 pozic menší (na hranici šumu).
- **Posun po řadách (4096), hranice u zdi (16384) a P232 (32768): 0 rozdílů z 90.** V těchto pozicích zeď
  P229 prolamuje (157 ran na 100 pozic) — orkové i trpaslíci mají hráče s Block, rána na 1 kostku je povolená.
  Situace „nikdo nesmí bít“, pro kterou P230 a P233 jsou, tu nenastala. Pozitivní kontrola mechaniky je v testech
  (`OneCageWallShift.*`, `OneCageWallClear.*`); měřit je chce pozice, kde útočníci Block nemají.
- P232 u zdi nic nemění (správně — vpřed tam nejde), stojí ~1 s na pozici.
