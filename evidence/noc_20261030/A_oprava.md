# Oprava nálezů recenze úkolu A (zed-lide) — 30.10.2026

Autor, opravný průchod. Nálezy z `A_recenze.md`.

## 1. MEDIUM — test přes server (`pozice`) chybí → **opraveno**

- Nový test `test_zed_lide_pres_server_prikaz_pozice` (`python/tests/test_play_zed_lide.py`): spustí server
  s `BB_PARTIE=zed-lide`, přes `bb_play_cmd.py` pošle `kouc HOME`, `pozice`, `board` a čeká
  „aktivni tym: TeamSide.AWAY“ + „kolo H:3 A:4“ a žádné „zápas ještě nezačal“. Infrastruktura `_cmd` jako v `test_play_zpet.py`.
- **Spadne bez opravy:** mutace z recenze `MA_POZICI = "pozice" in P` ⇒ `1 failed, 3 passed` (padá právě nový test;
  staré tři projdou — přesně díra, kterou recenzent popsal). Mutace vrácena `git checkout HEAD --`.
- S kódem HEAD: `4 passed`.

## 2. LOW — `echo $!` nedá PGID v interaktivním terminálu → **opraveno** (ranní postup v `A_autor.md`)

- Start serveru teď: `setsid bash -c 'echo $$ > /tmp/partieN.pgid; exec venv/bin/python bb_play_session_20261012.py' > … &`.
  `$$` je PID vedoucího nové relace (= PGID = SID), `exec` ho zachová pro Python.
- **Ověřeno pod pty s řízením úloh** (`script -qc "bash -i -c …"`): `$!` = 1788928 (rodič, hned skončí), v souboru
  1788930 = PID = PGID = SID procesu; `kill -- -PGID` ⇒ proces pryč. Totéž se skutečným serverem `zed-lide`
  (PID 1789352 = PGID, po `kill -- -PGID` ZABITO). Do zápisu přidána poznámka, co bylo špatně.
- Varianta recenzenta s `pgrep -n -f` nepoužita: záleží na tom, který proces je „nejnovější“ (dvě partie souběžně).

## 3. LOW — `_hraci()` je kopie smyčky z `enum_players()` → **opraveno** (DRY)

- `enum_players` je teď `[p for p in _hraci(state) if p.is_on_pitch]` — jedna smyčka přes id. Chování beze změny
  (filtr zůstal doslova, vč. dnešního vždy pravdivého `p.is_on_pitch` bez závorek — jeho oprava je samostatný
  návrh autora 1 a mění chování, tady se nedělá). Docstring `_hraci` říká, proč `zaloz_pozici`/`zrcadlo`
  nesmějí přejít na `enum_players`.
- Testy všech souborů, které skript importují (`test_coach_choice`, `test_play_zpet`, `test_play_zed_lide`,
  `test_vypis_udalosti`): **26 passed**.

## Kontrola

- `make check` (HEAD a947b390 + tyto změny): **„== check: vše zelené“**, EXIT=0 (log mimo git `/tmp/noc_A_oprava/make_check.log`).
- Žádný nález neodmítnut, žádná nová logika AI.

HOTOVO
