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
