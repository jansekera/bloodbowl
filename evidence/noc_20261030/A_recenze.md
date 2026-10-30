# Recenze úkolu A (partie zed-lide) — 30.10.2026

Recenzent, oddělený průchod. Diff `main...noc-partie-zrcadlo` (commit 5c4b90bc).

## Nálezy

## Ověřeno (bez nálezu)

- **Testy padají bez opravy** — sám ověřeno: `git checkout main -- bb_play_session_20261012.py` → `2 failed, 1 passed`
  (zed-lide: `KeyError`, `zed-orkove beze změny` projde — správně); strom vrácen `git checkout HEAD --`, `git status` čistý.
- **Mutace** (vlastní, vždy jedna řádka opravy vrácena, pak `git checkout HEAD --`):
  `setup_half(..., AWAY)` napevno → 2 F (Kick padne na orka, `zrcadlo` hodí `ValueError`) ·
  `na_tahu = HOME` napevno → 1 F · `kicking_team = AWAY` napevno → 1 F · nosič = id vzoru (3) místo dohledání → 1 F.
  4/4 zabito. Strom po mutacích shodný s HEAD.
- **Pravidla — pořadí tahů a kola:** ř. 304–307 (kdo staví první = kopající, druhý = přijímající), ř. 396–398
  (A. tah přijímajících, B. tah kopajících), ř. 405–409 (značka kola se posune na začátku každého vlastního tahu).
  Orkové na tahu ⇒ přijímali ⇒ kopají lidé (HOME); orkové 4. kolo, lidé mají za sebou 3 ⇒ `H:3 A:4`, `kicking_team=HOME`. Konzistentní.
- **Směr útoku:** HOME skóruje na x=25, AWAY na x=0 (`engine/include/bb/position.h:20-23`, `engine/src/turn_handler.cpp:71-76`,
  i `ENDZONE_*` ve skriptu ř. 256–257) ⇒ orkové jako AWAY útočí doleva, zrcadlo x' = 25 − x je správné.
- **Symetrie AI na zrcadle (C++ engine, `bb.ai_plan_turn`)** — `/tmp/noc_A_recenze/sym.py` (mimo git): plán tahu orků
  v `zed-orkove` (HOME) a v `zed-lide` (AWAY), greedy i `macro_mcts` 200 iterací, semínka 1–3: **77/77 řádků plánu
  shodných po zrcadlení** (typ akce, pole x→25−x, id přes párování 3→14, 7→18, 21→10 …; i turnovery na stejném místě).
  ⇒ zrcadlení je úplné (i Kick, skilly) a engine nemá v makroakcích P228–P233 stranovou asymetrii, aspoň pro tuhle pozici.
  Potvrzuje tvrzení autora „AI hraje stranu AWAY stejně jako v main“.
- **Artefakty v gitu:** diff obsahuje jen `bb_play_session_20261012.py`, `python/tests/test_play_zed_lide.py`,
  `evidence/noc_20261030/A_autor.md`. Žádné logy/cmd.txt/out.txt, symlinky `vendor/node_modules/venv` nejsou v gitu.

## Nálezy

1. **MEDIUM — test neprochází vstupem, kterým partie opravdu začíná (příkaz `pozice` přes server).**
   `bb_play_session_20261012.py:172` (`MA_POZICI`), užito na ř. 904 a 908. Testy volají `zaloz_pozici()` přímo.
   Ověřeno mutací: `MA_POZICI = "pozice" in P` (bez `or "zrcadlo" in P`) ⇒ **3 passed**, přitom server pak na
   `./bb_play_cmd.py pozice` u `zed-lide` odpoví „zápas ještě nezačal … pak `los`“ (větev ř. 908) a ranní
   partie 1 se nedá založit. Podle CLAUDE.md (test v místě, kudy prochází všechno) by patřil jeden test přes
   server: `BB_PARTIE=zed-lide`, `kouc HOME`, `pozice`, `board` → „aktivni tym: TeamSide.AWAY … kolo H:3 A:4“;
   infrastruktura už je v `python/tests/test_play_zpet.py:22-24` (`_cmd` přes `bb_play_cmd.py`). Kouř autora
   (bod 3) tuhle cestu dnes ručně ověřil, takže **funkčně je to v pořádku** — chybí jen hlídač na příště.

2. **LOW — ranní postup: `echo $! > /tmp/partieN.pgid` v interaktivním terminálu NEdá PGID serveru.**
   `evidence/noc_20261030/A_autor.md:147-148, 161, 168-169, 176` (komentář „setsid ⇒ PID = PGID“).
   S řízením úloh (běžný terminál) je úloha na pozadí sama vedoucí skupiny ⇒ `setsid` se forkne a `$!` je PID
   rodiče, který hned skončí. Ověřeno pod pty (`script -qc "bash -i -c 'setsid sleep 32 & echo $!; ps …'"`):
   `$!` = 1773401, skutečný proces PID = PGID = 1773403 ⇒ `kill -- -$(cat /tmp/partie1.pgid)` skončí
   „No such process“ a server běží dál. V neinteraktivním shellu (nástroj Bash v sezení) `$!` sedí — proto to autor
   v kouři neviděl. Hlavní cesta `./bb_play_cmd.py quit` funguje; záložní cesta ne. Návrh: PGID číst až po startu
   (`ps -o pgid= -p $(pgrep -n -f "bb_play_session_20261012.py")`), nebo `setsid -f` + totéž.

3. **LOW — `_hraci()` je kopie smyčky z `enum_players()`** (`bb_play_session_20261012.py:181-189` × `:341-350`).
   Dnes se chovají stejně jen proto, že filtr `if p.is_on_pitch:` v `enum_players` je vždy pravdivý (metoda bez
   závorek — vedlejší nález autora, ověřen: `engine/python/bb_module.cpp:146` `.def("is_on_pitch", …)`).
   `_hraci` záměrně potřebuje i náhradníky, takže oddělení je obhajitelné; jen pozor, až se opraví `enum_players`
   (návrh autora 1): `zaloz_pozici` a `zrcadlo` na něj nesmí přejít. Ne vada, poznámka k DRY.

## Tvrzení v zápisu autora — kontrola

- Pravdivá (ověřeno): 2 F / 1 P na starém kódu; Kick podle kopající strany (`engine/src/game_simulator.cpp:486-503`);
  masky `g_cageFeaturesOff`/`g_extraFeaturesOff` = 0 (`engine/src/macro_actions.cpp:63,66`), bity P228–P233
  (`engine/include/bb/macro_actions.h:113-120`), settery se volají jen v C++ testech; `ai_plan_turn`
  (`engine/python/bb_module.cpp:667`), `TurnPlanner` ř. 582, `explorationC = 1.0` ř. 603, `vf_blend/policy_blend = 0`,
  `risk_deferral = true`, `max_actions = 200`, hody `seed ^ 0x5bd1e995` (ř. 672), plánuje za `activeTeam` (ř. 674);
  `tahai` výchozí 50 iterací (`bb_play_session_20261012.py:1005`); `hitKeepsCage` bez masky (`engine/src/one_cage.cpp:1728`);
  Python testy skriptu partie **49 passed** (sám spuštěno).
- Nepravdivé: jen komentář „setsid ⇒ PID = PGID“ (nález 2) — platí jen bez řízení úloh.
- Body zadání 1–5 splněny. Bod 2 (masky) navíc potvrzen mou zkouškou symetrie výš.

## Kontrola

- `make check` (recenzent, HEAD 5c4b90bc + tento soubor): C++ **1289 tests PASSED**, kouř 6 her bez pádu,
  **„== check: vše zelené“** (log mimo git v `/tmp/noc_A_recenze/make_check.log`).
- Strom po všech mutacích shodný s HEAD (`git status`: jen tento soubor + nesledované symlinky).

## Verdikt

Bez HIGH nálezů. Preset `zed-lide` je správné zrcadlo (ověřeno i shodou plánů AI 77/77), kola a kopající
strana odpovídají pravidlům ř. 304–307, 396–398, 405–409. Doporučení: doplnit test přes server (nález 1) a opravit
záložní ukončení serveru v ranním postupu (nález 2) — obojí na ráno, partii to neblokuje (`quit` funguje).

HOTOVO
