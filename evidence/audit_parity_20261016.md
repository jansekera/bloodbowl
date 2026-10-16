# AUDIT PARITY PHP × C++ — 16.10.2026 (pondělní kontrola, spuštěná v pátek na pokyn uživatele)

Uživatel 16.10.: „můžeš pak spustit pondělní kontroly ještě dnes potom?“
Rozsah: pravidlové commity v obou enginech od auditu 08.10. (`audit_parity_20261008.md`, kniha P182) + stav otevřených nálezů + převody P210.
Pravidla = `rules_bb2016.txt`. Audit čtením; kde je oprava, je u ní test (napřed spadl).

## Bezpečnost závislostí

- `composer audit`: **16 upozornění na twig/twig 3.23.0** (1 kritické, 4 vysoká, 5 středních, 4 nízká, 2 bez hodnocení; sandbox, `template_from_string` …). Opraveno aktualizací v rámci `^3.23` na **3.30.0** (+ symfony/deprecation-contracts 3.7.1, polyfill-ctype 1.37.0, polyfill-mbstring 1.43.0). Po aktualizaci `No security vulnerability advisories found`.
- `npm audit --omit=dev`: **0 zranitelností**.
- gitleaks + Semgrep: nejsou nainstalované (kniha P108 neprovedena) — beze změny.

## C++ → PHP: pravidlové změny v C++ od 08.10.

| commit | co | PHP |
|---|---|---|
| `0169f3eb` nález 3b — u lajny bez volného pole dav, ne řetěz | C++ vada | PHP má správně od 08.10. (3a) |
| `cc398a8a` M3 — řetěz zaseknutý o Stand Firm nikým nepohne | | PHP `d3f2c3ad` ✅ |
| `b334f2ca` nález 17 — Side Step neplatí ležícímu v řetězu | | PHP správně (`canAct()`) |
| `f64cb396` nález 19 — **hráč bez tackle zón nechytá** (ř. 7983-7986, 8401-8405, 8185-8188) | | ⛔ **PHP chytání to nehlídá** — `hasLostTacklezones` je jen u zachycení (`PassResolver.php:496`), ne v `BallResolver` při chytání ⇒ **do P210** |
| `1a39b96e` T5.26 — míč nosiče opouštějícího hřiště z jeho pole | | PHP správně |
| `5fca70fb` T5.24 — vhazování 2D6 − 1 | | P197 ✅ |
| `2336f560` P194 (6) — follow-up před dalšími hody | | P198 (1) ✅; (2) otevřené (jen pořadí uvnitř odtlačení, výsledek stejný) |
| `3b036e3e` P192 (1) — hráč pryč, pak odraz | | P199 ✅ |
| P130, P201, P200 — volby kouče v živé partii (C++ server partie) | | PHP má vlastní dialogy; výkop v PHP dál neportovaný (známé) |
| `5ece146c`, `53f11f4e` P209 — blitz: jeden blok, pohyb po bloku, Foul Appearance | | **P210** |
| `18a9ebdc` P156 — Wild Animal ohlásí blitz bez cíle | | **P210** |
| 16.10. P199 zbytek — **Stab bez účinku míč nevyrazí** | | PHP správně (míč jen když obránce nestojí, `BlockHandler.php:743`) |
| 16.10. P212 — opravy z review (AI, Python vazby, maska otázky KOR) | | netýká se PHP |

## PHP → C++: změny v PHP od 08.10.

Všechny jsou opravy vad PHP podle auditu 08.10. a review P186 (nálezy 1–13, M3, H1, L1, L2, L7, P197, P198, P199, P206 a); C++ je v těch bodech správně (ověřeno 08.10.). **Nic k převodu do C++.**

## Otevřené z auditu 08.10. (stav dnes, čteno)

- **15** intercepce s Catch bez přehozu (PHP) · **16** vstávání pod 3 MA bez týmového přehozu (`MoveHandler.php:628-631`) · **22** strop odrazů 5 v PHP (`BallResolver.php:19`) · **14** druhá půlka Diving Catch (oba) · **18, 20, 21** latentní (skilly nejsou v soupiskách) — beze změny, otevřené.
- **Dřívější:** P84, P86, P87, P90, P91, P92, P94, P96, výkop v PHP — beze změny. P88 lékárník v PHP: P206 (a) ✅, (b) volba kouče otevřená.
- **K rozhodnutí uživatelem z 08.10.** (a) Wrestle obránce s Blockem na nosiče bez Blocku, (b) Dauntless v Multiple Block, (c) follow-up na pole s odskočeným míčem — beze změny.

## Převody P210 (dnes)

Viz kniha P210 — výsledek doplněn tam.
