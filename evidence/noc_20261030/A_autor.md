# Noc 30.10. — úkol A (příprava ranní partie, zrcadlo zed-lide)

Průběžný zápis.

## 1) Preset `zed-lide` — HOTOVO (necommitnuto, čeká na make check)

- `bb_play_session_20261012.py`: nový preset `zed-lide` = soupisky (human, orc), uživatel HOME = lidé (zeď),
  AWAY = orkové s klecí, `"zrcadlo": "zed-orkove"`, `"na_tahu": "AWAY"`, `"kolo": 4`, adresář `play_20261030_zed_lide`.
- Pozice se **neopisuje ručně**: `zrcadlo()` postaví rozestavení vzoru `zed-orkove` a každého hráče páruje podle
  (soupiska, MA/ST/AG/AV, skilly) — ne podle id (id se s pořadím soupisek mění: orčí thrower nosič je ve vzoru id 3,
  v zrcadle id 14). Pole x' = 25 − x, y stejné. Když by pro nějaké pole hráč chyběl, `ValueError` (ne tichá chyba).
- Nový klíč `na_tahu` (výchozí HOME ⇒ ostatní presety beze změny). `_kope()`: kope strana, která není na tahu.
- ⭐ Nález při psaní: `setup_half` dává skill **Kick** nejhlubšímu hráči **kopající** strany
  (`engine/src/game_simulator.cpp:486-503`), a `zaloz_pozici` volala `setup_half` napevno s kopajícími AWAY ⇒
  v zrcadle by Kick dostal orčí thrower (nosič) místo lidského catchera. Teď se `setup_half` volá s kopající
  stranou presetu (`zed-orkove` dál AWAY = beze změny; ověřeno testem).
- Kola: orkové (AWAY) `turn_number` 4, lidé (HOME) 3, `kicking_team` HOME (lidé kopali, orkové přijímali a táhnou
  v kole první) — konzistentní se vzorem (tam HOME 4 / AWAY 3, kope AWAY).
- Úvodní hláška serveru a odmítnutí před založením teď u presetů s pozicí říkají `pozice` místo `los`;
  docstring: seznam partií, příkaz `pozice`, „uživatel je vždy HOME“.
- Test `python/tests/test_play_zed_lide.py` (3 testy, hlídají zadání, ne kód: každé pole vzoru → zrcadlené pole
  s hráčem téže soupisky/statů/skillů; míč u téhož hráče na zrcadleném poli; na tahu AWAY, AWAY kolo 4, HOME 3,
  kope HOME, PLAY, 1. poločas; `zed-orkove` dál HOME 4 / AWAY 3).
  - na starém kódu (HEAD): **2 failed, 1 passed** (zed-lide neexistuje; test `zed-orkove beze změny` prochází — správně).
  - mutace (vždy jedna část opravy vrácena v kopii skriptu): bez zrcadlení x → 2 F · `setup_half` napevno AWAY → 2 F ·
    na tahu napevno HOME → 2 F · kola obráceně → 3 F · párování podle id → 1 F · `_kope` bez `na_tahu` → 2 F.
    **6/6 mutací zabito.**
- ⚠️ Vedlejší nález (neopraveno, mimo zadání): `enum_players()` filtruje `if p.is_on_pitch:` — v Python vazbě je
  `is_on_pitch` **metoda**, takže podmínka je vždy pravdivá a vrací i náhradníky na (−1,−1) (proto je vidíme v panelu
  hráčů). Oprava na `p.is_on_pitch()` by změnila výpis desky i `vykop_po` (hlášku „odchází ze hřiště“ hledá právě
  u hráčů na (−1,−1)) ⇒ ne potichu v noci; návrh pro uživatele.

## 2) Jakým AI hraje engine stranu AWAY — zjištěno

- **Skript AWAY sám nehraje.** `bb_play_session_20261012.py` nemá automatický tah AI: stranu AWAY táhne
  Claude (sezení) příkazy `actions AWAY` + `do <i>`; návrh dává `tahai` (= „engine hraje AWAY“ z 27.10.:
  `tahai macro 1 200`, přeplánovat při každé odchylce kostek).
- `tahai [greedy|macro] [semínko] [iterace]` → `bb_kontrola_tahu.plan_tahu()` → C++ `bb.ai_plan_turn`
  (`engine/python/bb_module.cpp:667`, `TurnPlanner` ř. 582–620):
  - AI `macro_mcts` (výchozí; `greedy` na přání), semínko výchozí 1, iterace výchozí **50** (27.10. se hrálo s 200);
  - váhy `weights_best.json` (načte se value i policy síť), ale `vf_blend = 0`, `policy_blend = 0`,
    `exploration_c = 1.0`, `dirichlet = 0`, `time_budget = 0`, `risk_deferral = true`, nejvýš 200 akcí;
  - hody nanečisto z vlastního semínka (`seed ^ 0x5bd1e995`) — partie ani kostky partie se nemění;
  - plánuje za stranu, která je **na tahu** (`state.active_team`) ⇒ v partii 1 za orky (AWAY), v partii 2 na
    tahu uživatele za orky (HOME) = rada.
- **Masky P228–P233: zapnuté, stejně jako v main enginu.** Skript ani `bb_kontrola_tahu.py` nevolají
  `set_cage_features_off` / `set_extra_features_off`; výchozí hodnoty `g_cageFeaturesOff = 0`,
  `g_extraFeaturesOff = 0` (`engine/src/macro_actions.cpp:63,66`; bit = úprava VYPNUTA) ⇒ vše zapnuto:
  P228 špinavý roh 1024 · P229 prolomení zdi 2048 · P230 posun po řadách 4096, záloha 8192, hranice do strany 16384 ·
  P232 vpřed s rizikem 32768, hrozba i po tahu 65536 · P233 čištění zdi od kraje 131072
  (`engine/include/bb/macro_actions.h:113-120`). Řetěz jen od klece (43e08d71, `hitKeepsCage`,
  `engine/src/one_cage.cpp:1728`) **masku nemá — platí vždy**. `mcts_cli` masky taky nenastavuje (grep setterů
  v `engine/cli`, `engine/src` = jen definice) ⇒ stejné chování jako v main.
- Větev `noc-partie-zrcadlo` = main 51430a39 (obsahuje 9afd73a7 sloučení P228–P233 i 43e08d71).
  ⚠️ Python modul se staví zvlášť: v worktree jsem `engine/build` nově nakonfiguroval (CMake jako v main)
  a postavil `bb_engine_py` z tohoto kódu. Kdyby se ráno hrálo z jiného checkoutu, napřed `make check-cpp` tam.
- **Pozitivní kontrola** (`/tmp/noc_A/kontrola_masek.py`, mimo git): pozice `zed-lide`, `macro_mcts` 200 iterací,
  semínka 1–3, vše zapnuto × bity P228–P233 vypnuté (`set_extra_features_off(260096)`): plán se liší ve 3/3
  semínkách — zapnuto: 2 přesuny + **BLITZ black orka 18 do středu zdi z pole před nosičem** (P229) + přesuny;
  vypnuto: jen 11 přesunů (REPOSITION) bez rány. Masky tedy v `tahai` opravdu působí. Nic jsem nezapínal ani neměnil.

## 3) Kouřová zkouška — PROŠLA

Servery `BB_PARTIE=zed-lide` a `BB_PARTIE=zed-orkove`, `BB_PLAY_DIR=/tmp/noc_A/hra_*` (mimo git), `setsid`,
ukončeny `kill -- -PGID`.
- Obě strany: `kouc HOME`, `pozice`, `board`. Úvodní hláška zed-lide: „napřed `kouc HOME`, pak `pozice`“.
- Strojové porovnání panelů hráčů z obou výpisů: **22/22 hráčů na hřišti** — na poli (25 − x, y) stojí hráč téže
  strany, role, statů a skillů; míč (12,7) → (13,7), nosič orčí thrower (id 3 → id 14); `aktivni tym AWAY`,
  `kolo H:3 A:4` (vzor HOME, `H:4 A:3`). Kick má v zrcadle lidský catcher na (8,3) jako ve vzoru na (17,3).
- `tahai macro 1 200` ukázal celý tah orků NANEČISTO (partie beze změny) + kontrolu tahu (nosiče dosáhne 7 lidí,
  2 kontakty 1 kostka). Pak **provedeno po krocích** `actions AWAY` + `do i`: 8 přesunů (19 a 17 na kraje zdi),
  přeplánování dalo tentýž zbytek, `do` blitz 18 → 10: kostky partie POW / Both Down, **server se zeptal
  uživatele (HOME) na Stand Firm** hráče 10 → `odp ano` → stojí, ale sražen (POW). Nové `tahai` po odchylce kostek
  přidalo bloky LW 12 a 13 na zeď. Dál jsem netáhl (kouř).
- Log příkazů kouře: `kouc HOME;pozice;actions AWAY;…;do 94;odp ano`.

### Výpis `board` — zed-orkove (vzor; uživatel orkové HOME)
```
počasí: pěkně
aktivni tym: TeamSide.HOME  fáze: GamePhase.PLAY  kolo H:4 A:3
mic: (12,7)  drzeny=True  nosic=3
vyrez x=5..22 y=1..13 (cele hriste je x=0..25 y=0..14)

x=   5          6          7          8          9          10         11         12         13         14         15         16         17         18         19         20         21         22         
y=1 |          |          |          |          |          |          |          |          |          |          |          |          |          |          |          |          |          |          |
y=2 |          |          |          |          |OBH8/0    |          |          |          |          |          |          |  1       |  1       |  1       |          |          |          |          |
y=3 |          |          |          |          |          |          |          |          |          |          |          |  1       |HC16      |  1       |          |          |          |          |
y=4 |          |OBP9/0    |          |          |          |          |          |          |  1       |  1       |  1       |  1       |  1       |  2       |  1       |  1       |          |          |
y=5 |          |          |          |          |          |          |          |          |  2       |HLW13     |  2       |          |          |  1       |HBT20     |  1       |          |          |
y=6 |          |          |          |          |          |          |OKG4/0    |          |OLW1/3    |HLW14     |  3       |          |          |  1       |  2       |  2       |  2       |  1       |
y=7 |OBG11/0   |          |          |OKG7/0    |          |          |          |OT3o/0    |  3       |HBG21     |  3       |          |          |          |  1       |HT18      |  2       |HOG15     |
y=8 |          |          |          |          |          |          |OKG5/0    |          |OLW2/3    |HL12      |  3       |          |          |  1       |  2       |  2       |  2       |  1       |
y=9 |          |          |          |          |          |          |          |          |  2       |HBG22     |  2       |          |          |  1       |HBH19     |  1       |          |          |
y=10|          |OBG10/0   |          |          |          |          |          |          |  1       |  1       |  1       |  1       |  1       |  2       |  1       |  1       |          |          |
y=11|          |          |          |          |          |          |          |          |          |          |          |  1       |HC17      |  1       |          |          |          |          |
y=12|          |          |          |          |OKX6/0    |          |          |          |          |          |          |  1       |  1       |  1       |          |          |          |          |
y=13|          |          |          |          |          |          |          |          |          |          |          |          |          |          |          |          |          |          |
x=   5          6          7          8          9          10         11         12         13         14         15         16         17         18         19         20         21         22         

O ork (náš, HOME) · H člověk (jejich, AWAY) · +role +ID
role orkové: L · LW lineman Wrestle · T thrower · BG blitzer s GUARD · BP blitzer MB+Pro · BH lovec míče · KG black orc s GUARD · KX black orc s GUARD a GRAB
role lidé: L lineman · LW lineman Wrestle · T thrower (Pro) · C catcher · BG blitzer s GUARD · BT blitzer MB+Tackle · BH lovec míče · OG ogre
G v kódu role = Guard (asistuje i v soupeřově zóně)
```

### Výpis `board` — zed-lide (zrcadlo; uživatel lidé HOME, na tahu orkové AWAY)
```
počasí: pěkně
aktivni tym: TeamSide.AWAY  fáze: GamePhase.PLAY  kolo H:3 A:4
mic: (13,7)  drzeny=True  nosic=14
vyrez x=3..20 y=1..13 (cele hriste je x=0..25 y=0..14)

x=   3          4          5          6          7          8          9          10         11         12         13         14         15         16         17         18         19         20         
y=1 |          |          |          |          |          |          |          |          |          |          |          |          |  1       |  1       |  1       |          |          |          |
y=2 |          |          |          |          |          |          |          |          |          |          |          |          |  1       |OBH19     |  1       |          |          |          |
y=3 |          |          |          |          |          |HC5/0     |          |          |          |          |          |          |  1       |  1       |  1       |  1       |  1       |  1       |
y=4 |          |          |          |          |          |          |          |          |          |          |          |          |          |          |          |  1       |OBP20     |  1       |
y=5 |          |          |          |HBT9/0    |          |          |          |          |HLW2/1    |  1       |  2       |  1       |  1       |          |          |  1       |  1       |  1       |
y=6 |          |          |          |          |          |          |          |          |HLW3/1    |OLW12     |  3       |OKG15     |  1       |  1       |  1       |  1       |  1       |  1       |
y=7 |HOG4/0    |          |HT7/0     |          |          |          |          |          |HBG10/2   |  3       |OT14o     |  3       |  2       |  1       |OKG18     |  1       |  1       |OBG22     |
y=8 |          |          |          |          |          |          |          |          |HL1/1     |OLW13     |  3       |OKG16     |  1       |  1       |  1       |  1       |  1       |  1       |
y=9 |          |          |          |HBH8/0    |          |          |          |          |HBG11/1   |  1       |  2       |  1       |  1       |          |          |  1       |  1       |  1       |
y=10|          |          |          |          |          |          |          |          |          |          |          |          |          |          |          |  1       |OBG21     |  1       |
y=11|          |          |          |          |          |HC6/0     |          |          |          |          |          |          |  1       |  1       |  1       |  1       |  1       |  1       |
y=12|          |          |          |          |          |          |          |          |          |          |          |          |  1       |OKX17     |  1       |          |          |          |
y=13|          |          |          |          |          |          |          |          |          |          |          |          |  1       |  1       |  1       |          |          |          |
x=   3          4          5          6          7          8          9          10         11         12         13         14         15         16         17         18         19         20         

H člověk (náš, HOME) · O ork (jejich, AWAY) · +role +ID
role lidé: L lineman · LW lineman Wrestle · T thrower (Pro) · C catcher · BG blitzer s GUARD · BT blitzer MB+Tackle · BH lovec míče · OG ogre
role orkové: L · LW lineman Wrestle · T thrower · BG blitzer s GUARD · BP blitzer MB+Pro · BH lovec míče · KG black orc s GUARD · KX black orc s GUARD a GRAB
G v kódu role = Guard (asistuje i v soupeřově zóně)
```

## 4) Ranní postup

Hraje se z worktree větve (do main není sloučeno). Proměnnou `BB_PLAY_DIR` čte server i `bb_play_cmd.py`
— **musí být exportovaná**, jinak `bb_play_cmd.py` píše do `evidence/play_20261012` (výchozí jiné partie)
a čeká marně. ⚠️ Pro partii 2 **nový adresář**: v main checkoutu leží v `evidence/play_20261027_zed/`
`prikazy.log` z 27.10. a server by ho po startu přehrál (= pokračování staré partie).

Oprava 30.10. (recenze, nález 2): dřív `echo $! > …pgid` — v interaktivním terminálu je `$!` PID
rodiče, který `setsid` hned opustí, takže záložní `kill -- -PGID` mířil vedle.

### Partie 1 — AI (AWAY) hraje orčí klec, uživatel (HOME) lidskou zeď
```bash
cd /home/jenda/claude/blood-bowl/.claude/worktrees/noc-partie-zrcadlo
make check-cpp                                   # jistota: bb_engine_py postavený z tohoto kódu
export BB_PLAY_DIR=evidence/play_20261030_zed_lide
BB_PARTIE=zed-lide setsid bash -c 'echo $$ > /tmp/partie1.pgid; exec venv/bin/python bb_play_session_20261012.py' \
    > /tmp/partie1_server.log 2>&1 &              # $$ = PID vedoucího nové relace = PGID; exec ho zachová
./bb_play_cmd.py kouc HOME                        # uživatel rozhoduje za lidi (přehozy, Stand Firm, Side Step…)
./bb_play_cmd.py pozice                           # rozestaví zrcadlo; na tahu ORKOVÉ (AWAY), kolo H:3 A:4
./bb_play_cmd.py tahai macro 1 200                # tah orků NANEČISTO — ukázat uživateli PŘED provedením
./bb_play_cmd.py actions AWAY                     # najít index akce = první krok plánu
./bb_play_cmd.py do <i>                           # provést krok; opakovat actions AWAY + do <i> po krocích
#   čeká-li akce na uživatele: ./bb_play_cmd.py odp <ano|ne|tym|pro|číslo>
#   po každé odchylce kostek od plánu (jiná kostka bloku, GFI…) znovu: ./bb_play_cmd.py tahai macro 1 200
#   blitz z pole jako v plánu: ./bb_play_cmd.py blitzfrom <x> <y>   (před `do`)
./bb_play_cmd.py do 0                              # END_TURN (v kouři [0] — ověř ve výpisu actions), až plán skončí
#   tah uživatele: ./bb_play_cmd.py actions HOME → ./bb_play_cmd.py do <i> (push / follow / kostka podle potřeby)
./bb_play_cmd.py kontrola posledni                # kontrola posledního tahu (kdykoli)
./bb_play_cmd.py zpet                             # vrátí poslední akci, kdyby šlo o chybné zadání
./bb_play_cmd.py quit                             # konec; nebo kill -- -$(cat /tmp/partie1.pgid)
```

### Partie 2 — uživatel (HOME) orčí klec, engine (AWAY) lidská zeď, `tahai` jako rada
```bash
cd /home/jenda/claude/blood-bowl/.claude/worktrees/noc-partie-zrcadlo
export BB_PLAY_DIR=evidence/play_20261030_zed_orkove      # NE play_20261027_zed (starý log by se přehrál)
BB_PARTIE=zed-orkove setsid bash -c 'echo $$ > /tmp/partie2.pgid; exec venv/bin/python bb_play_session_20261012.py' \
    > /tmp/partie2_server.log 2>&1 &              # $$ = PID vedoucího nové relace = PGID; exec ho zachová
./bb_play_cmd.py kouc HOME
./bb_play_cmd.py pozice                           # na tahu ORKOVÉ (HOME) = uživatel, kolo H:4 A:3
./bb_play_cmd.py tahai macro 1 200                # RADA: jak by tah orků zahrála AI (nanečisto)
./bb_play_cmd.py actions HOME                     # uživatel volí sám
./bb_play_cmd.py do <i>                           # … až END_TURN
#   tah lidí (AWAY): ./bb_play_cmd.py tahai macro 1 200 → actions AWAY → do <i> po krocích, přeplánovat po odchylce
./bb_play_cmd.py quit                             # nebo kill -- -$(cat /tmp/partie2.pgid)
```
Po partiích: do gitu jen případný `zaver.md`, ne `prikazy.log` / `cmd.txt` / `out.txt`.

## 5) Test a kontrola

- `python/tests/test_play_zed_lide.py` — viz bod 1 (padá na starém kódu, 6/6 mutací zabito).
- `make check` → **„== check: vše zelené“** (C++ 1289/1289, PHPUnit, vitest, kouř 6 her).
- `check-py` není v `make check` ⇒ zvlášť testy skriptu partie: `test_play_zed_lide`, `test_play_zpet`,
  `test_coach_choice`, `test_vypis_udalosti`, `test_kontrola_tahu` — **49 passed**.
- Review testu v odděleném průchodu (podle CLAUDE.md) zatím neproběhlo — čeká na reviewera.

## Návrhy pro uživatele (neprogramováno)

1. `enum_players()` (`p.is_on_pitch` bez závorek) — viz bod 1; oprava mění výpis desky a `vykop_po`.
2. `bb_play_cmd.py` má výchozí adresář napevno `play_20261012`; mohl by brát adresář z `BB_PARTIE`
   (stejně jako server), aby se ráno nedalo zapomenout na `export BB_PLAY_DIR`.
3. `tahai` má výchozí 50 iterací, partie 27.10. se hrála s 200 — případně změnit výchozí na 200.

HOTOVO
