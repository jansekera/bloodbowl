# CLAUDE.md — Blood Bowl (dva enginy, AI, webová hra)

## Než řekneš „hotovo“

```bash
make check          # formát PHP + ESLint · PHPStan + PHPUnit · tsc + vitest · C++ bb_tests (+ přestaví mcts_cli a bb_engine_py) · 6 her greedy
```
Musí skončit **„== check: vše zelené“** (~1,5 min). Delší `make check-all` (+ Python testy a e2e v prohlížeči, ~4 min) před dávkou commitů a jednou denně. Jednotlivě `make check-lint`, `check-php`, `check-front`, `check-cpp`, `check-smoke`.
Po zelené: **commit + push bez ptaní**.

## Pravidelně — spouští se RUČNĚ (uživatel 01.10.: „naplánování neřeš — mělo by se spustit např. každé pondělí, pak spustíme ručně“)

| kdy | co |
|---|---|
| před každým commitem | `make check` |
| po dávce oprav (~5–10 commitů) | `/code-review` + `/simplify`; refaktor ⇒ stejné zápasy se stejným semínkem před/po; pravidlová oprava ⇒ řádek „převést do druhého enginu“ |
| **každé pondělí** | **audit parity PHP × C++** (`evidence/fable_brief_rules_parity_20260821.md`) · `php composer.phar audit` + `npm audit --omit=dev` · (po P108) gitleaks + Semgrep |
| v noci, volná kapacita | mutační testy (po P104) |
| měsíčně / větší změna webu | security review (jako P98), aktualizace závislostí |
| ⛔ ne pravidelně | měření síly AI jako „regresní test“ — jen párově na konkrétní otázku |

## Dva enginy — ⛔ most mezi nimi není

| | kde | k čemu |
|---|---|---|
| **C++** | `engine/` | AI: MCTS, makroakce, měření, trénink. `engine/build/mcts_cli`, Python modul `bb_engine_py` |
| **PHP** | `src/`, `tests/` | webová hra pro člověka (`public/`, `frontend/`), vlastní kouči |

- U nálezu se vždy ptej **„ve kterém enginu?“** — oprava v jednom **neopravuje** druhý.
- Každá pravidlová oprava ⇒ v knize úkolů řádek **„převést do druhého enginu“** (audit parity jednou týdně).
- ⛔ `make bb_engine` **nepřestaví** Python modul — cíl je `bb_engine_py`, jinak měření běží na starém kódu
  a výsledek vypadá jako „žádná změna“ (`make check-cpp` ho staví).
- ⛔ PHP: `seed` **neřídí kostky** (`random_int()` ignoruje `mt_srand`) ⇒ A/B napříč běhy PHP není párové.

## Python prostředí

- **`venv` na Pythonu 3.8** (engine se sestavuje jako `bb_engine.cpython-38`): `/usr/local/bin/python3.8 -m venv venv && venv/bin/python -m pip install -r requirements-py38.txt`.
- `venv` **není v gitu** (P118 — dřív byl, z jiného stroje a na Pythonu 3.12, tady se nenačítal). Stará záloha: `venv.py312-jiny-stroj-20261001/`.

## Pravidla hry

- Zdroj pravdy je **`rules_bb2016.txt`** — test pravidla cituje **řádek pravidel**, ne implementaci.
  Testy psané podle kódu v minulosti **kódovaly vadu**.
- **Test napřed, a musí spadnout.** Kostky volit tak, aby **odlišily starou a novou mechaniku** —
  napoprvé prošlo 5 z 8 nových testů i bez opravy (kostky, na kterých se obě mechaniky shodnou).
- ⛔ **„Smí jen X“ není „musí X“.** Omezení v pravidlech nečíst jako povinnost.
- ⛔ **Pravidlová oprava se neměří zlepšením.** Otázka zní *„nerozbilo se to?“*, ne *„hraje AI líp?“*.
  Kouř: `engine/build/mcts_cli --home=greedy --away=greedy --games=6 --home-roster=wood-elf --away-roster=orc`.
- ⭐ **Novou věc v enginu (nový mechanismus, nová volba AI) napřed představit uživateli.** Opravu jasné vady ne.

## Měření

- ⛔⛔ **Nula se nečte bez pozitivní kontroly** — ověř, že měřidlo umí najít jedničku.
- Pohyblivá báze ⇒ jen **párové A/B** (stejné semínko). `mcts_cli --seed=N`: hra *g* má semínko *N+g*.
- Refaktor bez změny chování ⇒ **stejné zápasy se stejným semínkem** před a po.
- Kapacita: laptop 8 jader, noc = den stroje. Noční běh třídit předem, dimenzovat s rezervou.
- Do gitu nepatří artefakty běhů (logy, korpusy, váhy) — jen kód, testy, `evidence/*.md`.

## Kniha úkolů — `evidence/task_queue.md`

- **Jediný platný seznam.** ID (`P98`…) se **nikdy nepřečíslovávají**.
- Mění se jen stav ve sloupci; **přepisuje se jedině oddíl „CO JE TEĎ PRVNÍ“**.
- Co se rozhodlo **nedělat**, se zapisuje taky (s důvodem) — jinak se po roce neodliší „nevyplatilo se“ od „nikoho to nenapadlo“.

## Kód

- Komentář u opravy pravidla: **co bylo špatně předtím + řádek pravidel** (`OPRAVENO 16. 9. — tady bylo +1. Pravidla ř. 651–654: …`).
- Vadu opravit, ne schovat (žádné `try { } catch {}` kolem příznaku).
- Ke každé nesrovnalosti hledej **týž princip jinde** — jedna oprava je jeden vzorek.

## Známé mezery (01.10.2026)

- PHPUnit hlásí 3 *PHPUnit Deprecations*.
- Testy architektury, mutační testy, bezpečnostní brány a CI zatím nejsou — kniha **P98–P116**.
- Lint: `vendor/bin/php-cs-fixer fix` opraví formát PHP; commity jen s formátem patří do `.git-blame-ignore-revs`.
