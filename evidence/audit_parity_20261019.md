# AUDIT PARITY PHP × C++ — 19.10.2026 (pondělní kontrola)

Rozsah: commity v `src/`, `engine/src/`, `engine/include/` od auditu 16.10. 13:19 (`bac9763b`). Audit čtením.

## Bezpečnost závislostí

- `composer audit`: bez nálezů.
- `npm audit`: 9 nálezů (vite, vitest, rollup, postcss, nanoid, picomatch, source-map-js, tinypool, @vitest/mocker — vše dev nástroje). `npm audit fix` (bez `--force`) opravil 6; **zbývají 3** (vitest / @vitest/mocker / tinypool: 1 střední, 2 kritické) — oprava jen `--force` = nová hlavní verze vitest ⇒ rozhodne uživatel. Zranitelnosti se týkají dev serveru a UI vitestu, ne běhu hry.
- `make check-all`: vše zelené.

## PHP → C++

| commit | co | C++ |
|---|---|---|
| `d8d1f0d2` follow-up chytá míč odražený na uvolněné pole | oprava PHP | C++ správně (audit 08.10. (c)) |
| `4dee9ac3`, `c537a045`, `ce735aa3` P206 (b) lékárník — volba kouče-člověka, KO hráč drží pole | web PHP | C++ má vlastní volbu v serveru partie (P130/P201); AI netýká |
| `ce735aa3` nález 19 i pro vhazování (hráč bez zón nechytá) | oprava PHP | C++ `ball_handler.cpp:42` — jedno místo pro všechna chytání ✅ |
| `ce735aa3` Stakes předán do odložené volby lékárníka | oprava PHP | C++ `injury.cpp:157,190` Stakes ruší Regeneration ✅ |
| `fb0b79c9` P210 (1) blitz bez cíle; Wrestle obránce | PHP + oba | P156 v C++ ✅; Wrestle v obou týmž commitem ✅ |
| `8aef701f` Foul Appearance v blitzu | oprava PHP | C++ P209 ✅ |

## C++ → PHP

- `roster.cpp` (wood-elf, skaven TV1500): PHP rozvinuté soupisky nemá ⇒ netýká se.
- P159, P213, P216, P180, P212 (4): AI řadič klece / makra — jen C++.

## Výsledek

**Žádná nová mezera.** Otevřené z 08.10./16.10. beze změny (15, 16, 22, 14, 18, 20, 21; P84, P86, P87, P90–P96; výkop v PHP).
