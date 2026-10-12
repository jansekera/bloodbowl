#!/usr/local/bin/python3.8
"""Živá partie 12.10.2026 — uživatel (LIDÉ, HOME) proti Claude (ORKOVÉ, AWAY), TV1500.

Vychází z bb_play_session_20261002.py (server na pozadí, příkazy přes soubory, log se po
restartu přehraje). Navíc P130: rozhodnutí kouče uprostřed akce.

Komunikace přes soubory v evidence/play_20261012/:
  cmd.txt    -- napíše se sem příkaz, server ho přečte a smaže
  out.txt    -- server sem napíše odpověď
Pohodlněji: ./bb_play_cmd.py <příkaz ...>  (pošle příkaz, počká a vypíše odpověď)

Příkazy:
  board                    -- vykreslí desku a panel hráčů (se skilly a rerolly)
  actions <HOME|AWAY>      -- vypíše dostupné akce pro danou stranu, číslovaně
  do <index>               -- provede akci podle čísla z 'actions'
  kouc <HOME|AWAY|ne>      -- P130: za tuto stranu rozhoduje KOUČ; `ne` = engine jako dřív
  los                      -- P195: začátek zápasu — počasí a los (D6: 1-3 HOME, 4-6 AWAY); vítěz volí
  volba <kopat|prijimat>   -- P195: volba kouče po vyhraném losu (za AI volí engine sám)
  odp <odpověď>            -- P130: odpověď na otázku, na které se akce zastavila:
                              přehoz: ne | tym | pro · skill: ano | ne · kostka: číslo kostky
  push <x> <y>             -- příští odtlačení na toto pole (volba útočníka; jinak engine)
  sidestep <x> <y>         -- P130: pole, kam uhne hráč se Side Stepem (volba jeho kouče)
  follow <0|1>             -- příští follow-up ano/ne (volba kouče; jinak engine)
  blitzfrom <x> <y>        -- příští blitz blokuje z tohoto pole (volba kouče; jinak engine)
  kostka <0-4>             -- příští blok: kterou z hozených kostek vzít (pro stranu bez `kouc`)
  beztackle                -- příští blok: útočník nepoužije Tackle
  vykop <HOME|AWAY>        -- po TD: rozestavení + výkop, kdo KOPE
  polocas [HOME|AWAY]      -- 2. poločas: rozestavení + výkop; kope, kdo v 1. poločase přijímal
  tahai [greedy|macro] [semínko] [iterace]
                           -- jak by tah odehrála AI — NANEČISTO (kopie stavu, vlastní kostky)
  kontrola [posledni]      -- kontrola rozehraného / posledního dokončeného tahu
  quit                     -- ukončí server

P130 — jak otázka probíhá: akce se spustí; narazí-li na rozhodnutí kouče (přehoz, kostka
bloku, Dodge / Wrestle / Stand Firm / Side Step jeho hráče), server vrátí stav i kostky
před akci, ukáže, co padlo, a čeká na `odp`. Po odpovědi akci spustí znovu se stejnými
kostkami — výsledek přehozu tak předem nevidí nikdo. Dokud otázka čeká, jde zadat jen
`odp`, `push`, `sidestep`, `follow`, `board`. Skillové rerolly (Dodge, Sure Hands, Sure
Feet, Pass, Catch) a Block při Both Down zůstávají automatické.

Každý příkaz měnící stav se zapisuje do prikazy.log; po restartu serveru se
log přehraje (stejné semínko ⇒ stejné kostky ⇒ stejná partie).
"""
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "engine", "build"))
import bb_engine as bb  # noqa: E402
import bb_kontrola_tahu as kt  # noqa: E402

# BB_PLAY_DIR = jiný adresář partie (zkoušky); stejnou proměnnou čte bb_play_cmd.py
DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   os.environ.get("BB_PLAY_DIR", os.path.join("evidence", "play_20261012")))
os.makedirs(DIR, exist_ok=True)
CMD = os.path.join(DIR, "cmd.txt")
OUT = os.path.join(DIR, "out.txt")

ENDZONE_HOME_X = 0   # lidé (HOME) brání nižší x
ENDZONE_AWAY_X = 25  # orkové (AWAY) brání vyšší x


def new_game(seed=20261012):
    global ROSTERS
    human = bb.get_developed_roster("human", 1500)
    orc = bb.get_developed_roster("orc", 1500)
    ROSTERS = (human, orc)
    state = bb.GameState()
    dice = bb.DiceRoller(int(os.environ.get("BB_SEED", seed)))   # BB_SEED jen pro zkoušky
    return state, dice


POCASI = {"SWELTERING_HEAT": "úmorné vedro", "VERY_SUNNY": "ostré slunce", "NICE": "pěkně",
          "POURING_RAIN": "lije", "BLIZZARD": "vánice"}


def pocasi_str(state):
    jm = str(state.weather).split(".")[-1]
    return POCASI.get(jm, jm)


def strana_str(side):
    return "LIDÉ (HOME)" if side == bb.TeamSide.HOME else "ORKOVÉ (AWAY)"


def druha(side):
    return bb.TeamSide.AWAY if side == bb.TeamSide.HOME else bb.TeamSide.HOME


def zahaj(state, dice, kope):
    """Rozestavení a první výkop; `kope` = strana, která kope."""
    bb.setup_half(state, ROSTERS[0], ROSTERS[1], kope)
    bb.simple_kickoff(state, dice)


def enum_players(state):
    out = []
    for pid in range(1, 33):
        try:
            p = state.get_player(pid)
        except Exception:
            continue
        if p.is_on_pitch:
            out.append(p)
    return out


def skilly(p):
    """Jména skillů hráče (pořadí podle enum SkillName)."""
    out = []
    for i, n in enumerate(SKILL_NAMES):
        if n == "SKILL_COUNT":
            continue
        try:
            if p.has_skill_index(i):
                out.append(n)
        except Exception:
            pass
    return out


def role(p):
    """Krátký kód role v TV1500 podle statů a skillů (lidé HOME, orkové AWAY)."""
    st = (p.stats.movement, p.stats.strength, p.stats.agility, p.stats.armour)
    sk = set(skilly(p))
    if p.team_side == bb.TeamSide.HOME:      # lidé
        if st == (8, 2, 3, 7):
            return "C"                       # Catcher +Block+Side Step (Dodge, Catch)
        if st == (5, 5, 2, 9):
            return "OG"                      # Ogre +Block (Loner)
        if st == (7, 3, 3, 8):               # Blitzer
            if "Dauntless" in sk:
                return "BD"                  # +Guard+Stand Firm+Dauntless
            if "Guard" in sk:
                return "BG"                  # +Guard+Stand Firm+Mighty Blow
            if "StripBall" in sk:
                return "BH"                  # lovec míče +Wrestle+Tackle
            return "BT"                      # +Mighty Blow+Tackle
        if "Pass" in sk:
            return "T"                       # Thrower +Block+Pro
        if "Wrestle" in sk:
            return "LW"                      # Lineman +Wrestle
        return "L"                           # Lineman +Block
    if st == (4, 4, 2, 9):
        return "KM" if "MightyBlow" in sk else "K"   # Black Orc (+Mighty Blow)
    if st == (6, 3, 3, 9):                   # Blitzer
        if "Pro" in sk:
            return "BP"                      # +Mighty Blow+Pro
        if "StripBall" in sk:
            return "BH"                      # lovec míče +Tackle
        return "BG"                          # +Guard
    if st == (5, 3, 3, 8):
        return "T"                           # Thrower +Block
    if "Wrestle" in sk:
        return "LW"                          # Lineman +Wrestle
    return "L"


def zon(sq, kdo):
    """Kolik hracu z mnoziny `kdo` (souradnice) ma tacklezonu na `sq`."""
    return sum(1 for q in kdo if max(abs(q[0] - sq[0]), abs(q[1] - sq[1])) == 1)


def board(state):
    # ⭐ Format podle feedback_board_render_format.md (diag_board_render.py),
    #   ZJEDNODUSENO na jednom miste: kod je RASA+ID (D9, E14), ne RASA+ROLE
    #   (DLG, Wwd), protoze Python vazba enginu nevraci jmeno pozice/role --
    #   jen staty a skilly jednotlive dotazem. Vse ostatni (mrizka, tacklezony,
    #   `/n` VZDY u nasich, bez cisla u jejich, stav '_'/'o'/'-') je STEJNE.
    players = enum_players(state)
    cell = {}
    for p in players:
        if p.state not in (bb.PlayerState.STANDING, bb.PlayerState.PRONE,
                            bb.PlayerState.STUNNED):
            continue
        side = "H" if p.team_side == bb.TeamSide.HOME else "O"
        rl = role(p)
        # ⭐ ID jako pripona: bez ni by 4 stejni Longbeardove vypadali
        #   identicky a nesli by odlisit pro hlaseni tahu (na rozdil od
        #   diag_board_render.py, ktery jen CTE pozici, tady se podle
        #   kodu VOLA konkretni akce).
        code = f"{side}{rl}{p.id}"
        if p.state == bb.PlayerState.STUNNED:
            code = code.lower()
        if state.ball.is_held and state.ball.carrier_id == p.id:
            code += "o"   # a pokračuje se: nosič, který už táhl, má mít i '-'/'B'
        if p.state == bb.PlayerState.PRONE:
            code += "_"
        elif p.team_side != state.active_team:
            pass   # '-'/'B' jen u týmu na tahu (soupeřovy značky z minulého tahu matou)
        elif p.used_blitz:
            code += "B"   # uživatel 02.10.: kdo v tomto tahu blitzoval, má B místo -
        elif p.has_acted or p.has_moved:
            code += "-"
        # ⭐ 02.10.2026 (uživatel: „DR5 a všichni pohnutí mají mít za sebou -"):
        #   '-' dostane i ten, kdo se už pohnul a aktivaci má ještě otevřenou.
        # ⭐ 04.09.2026: NENI TO VADA -- overeno primo v kodu, oprava puvodni
        #   spatne domenky. `p.has_acted` se NASTAVUJE AZ pri "activation
        #   close-out at the actor-switch boundary" (action_resolver.cpp
        #   ~r.484-496): kdyz zacne akce JINEHO hrace, teprve TEHDY se
        #   predchozimu hraci (pokud se hnul) nastavi `hasActed = true`.
        #   Zamerne, s vlastni regresni poznamkou proti bugu, kdy hrac po
        #   pohybu dostaval druhou akci navic (evidence/
        #   fable_hasacted_bug_20260715.md). Takze DR5 po 6 krocich MOVE
        #   spravne ukazovalo "/0" (aktivace jeste neni uzavrena) a "-" se
        #   objevilo, az zacala akce jineho hrace (DL2 blok) -- overeno.
        cell[(p.position.x, p.position.y)] = code

    st_home = {(p.position.x, p.position.y) for p in players
               if p.team_side == bb.TeamSide.HOME and p.state == bb.PlayerState.STANDING}
    st_away = {(p.position.x, p.position.y) for p in players
               if p.team_side == bb.TeamSide.AWAY and p.state == bb.PlayerState.STANDING}
    # `/n` jen u NASICH stojicich (lidé = HOME), VZDY i s /0.
    for (x, y) in list(st_home):
        if (x, y) in cell:
            cell[(x, y)] = f"{cell[(x, y)]}/{zon((x, y), st_away)}"

    # ⭐ VOLNY MIC NA PRAZDNEM POLI nemel zadnou znacku (jen textovy radek
    #   "mic: ..." nad mrizkou) -- uzivatel si vsiml, ze v samotne desce
    #   neni videt vubec. Znacka se pise, jen kdyz pole neobsazuje hrace
    #   (drzeny mic uz ma 'o' na hraci, viz vyse).
    ball_xy = (state.ball.position.x, state.ball.position.y)
    if not state.ball.is_held and ball_xy not in cell:
        cell[ball_xy] = "*MIC*"

    # ⭐ OREZ SE ODVOZUJE Z OBSAZENYCH POLI (feedback_board_render_format.md),
    #   NE z cele desky -- 26 sloupcu na plnou sirku se v terminalu zalomi
    #   do necitelneho zmatku. Vcetne mice, kdyby lezel mimo hrace.
    occx = [x for (x, y) in cell] + [state.ball.position.x]
    occy = [y for (x, y) in cell] + [state.ball.position.y]
    xlo, xhi = max(0, min(occx)), min(25, max(occx))   # uživatel 02.10.: bez okraje navíc, hřiště je široké
    ylo, yhi = max(0, min(occy) - 1), min(14, max(occy) + 1)
    venku = [(sq, v) for sq, v in cell.items() if not (xlo <= sq[0] <= xhi)]

    lines = []
    lines.append(f"počasí: {pocasi_str(state)}")
    lines.append(f"aktivni tym: {state.active_team}  fáze: {state.phase}  "
                 f"kolo H:{state.home_team.turn_number} A:{state.away_team.turn_number}")
    lines.append(f"mic: ({state.ball.position.x},{state.ball.position.y})  "
                 f"drzeny={state.ball.is_held}  nosic={state.ball.carrier_id}")
    lines.append(f"vyrez x={xlo}..{xhi} y={ylo}..{yhi} (cele hriste je x=0..25 y=0..14)")
    lines.append("")
    # ⛔ 04.09.: hlavicka mela 6 znaku na sloupec (" {x:<5}"), zatimco bunky
    #   maji 7 (6 obsah + '|') -- sloupce se s kazdym dalsim polem rozjizdely
    #   o 1 znak, presne vada popsana v feedback_board_render_format.md
    #   ("hlavicka zarovnana na zacatek bunky, ne centrovana"). Sirka MUSI
    #   sedet se sirkou bunky. 02.10.2026: 9 = 8 obsah + 1 oddelovac -- kod
    #   s pomlckou ("DL10-/0", "DTG6-/0") ma 7 znaku a sest uz nestacilo.
    # uživatel 02.10.: „nevidím nahoře čísla x" ⇒ řádek začíná „x=" a opakuje se i pod mřížkou
    xhdr = "x=   " + "".join(f"{x:<10} " for x in range(xlo, xhi + 1))
    lines.append(xhdr)
    hline = "    +" + "----------+" * (xhi - xlo + 1)
    for y in range(ylo, yhi + 1):
        lines.append(hline)
        row = f"y={y:<2}|"
        for x in range(xlo, xhi + 1):
            v = cell.get((x, y))
            if v:
                row += f"{v:<10}|"
            else:
                z = zon((x, y), st_away)
                row += (f"  {z}       |" if z else "          |")
        lines.append(row)
    lines.append(hline)
    lines.append(xhdr)
    if venku:
        lines.append(f"⚠️ MIMO VYREZ stoji: " + ", ".join(f"{v}@{sq}" for sq, v in sorted(venku)))
    lines.append("")
    lines.append("H člověk (náš, HOME) · O ork (jejich, AWAY) · +role +ID")
    lines.append("role lidé: L lineman · LW lineman Wrestle · T thrower (Pro) · C catcher · BG/BD blitzer Guard+Stand Firm"
                 " (MB / Dauntless) · BT blitzer MB+Tackle · BH lovec míče · OG ogre")
    lines.append("role orkové: L · LW lineman Wrestle · T thrower · BG blitzer Guard · BP blitzer MB+Pro · BH lovec míče"
                 " · K black orc · KM black orc +MB")
    lines.append("cislo v prazdnem poli = kolik JEJICH tacklezon na nej dosahuje")
    lines.append("/n u H.. = VZDY (i /0) -- v kolika JEJICH zonach stoji")
    lines.append("u O.. cislo NENI (matoucí) · '_' lezi · 'o' drzi mic · 'B' blitzoval · '-' uz hral · malymi = stunned")
    lines.append("")
    for jm, t in (("LIDÉ (HOME)", state.home_team), ("ORKOVÉ (AWAY)", state.away_team)):
        lines.append(f"{jm}: skóre {t.score} · týmové rerolly {t.rerolls}"
                     + (" (v tomto tahu už použit)" if t.reroll_used_this_turn else ""))
    lines.append("")
    lines.append("--- hráči (staty, skilly) ---")
    for p in sorted(players, key=lambda q: (q.team_side != bb.TeamSide.HOME, q.id)):
        strana = "LID" if p.team_side == bb.TeamSide.HOME else "ORK"
        acted = "hral" if (p.has_acted or p.has_moved) else "ceka"
        lines.append(
            f"  {p.id:2d} {strana} {role(p):<2} ({p.position.x:2d},{p.position.y:2d}) "
            f"MA{p.stats.movement} ST{p.stats.strength} AG{p.stats.agility} AV{p.stats.armour} "
            f"{p.state} {acted} · {', '.join(skilly(p)) or '-'}"
        )
    return "\n".join(lines)


def _skill_names():
    # pořadí z engine/include/bb/enums.h (enum class SkillName) — jediný zdroj
    import re
    txt = open(os.path.join(os.path.dirname(__file__), "engine", "include", "bb", "enums.h")).read()
    body = txt[txt.index("enum class SkillName"):]
    body = body[body.index("{") + 1: body.index("};")]
    body = re.sub(r"//[^\n]*", "", body)
    return [n.split("=")[0].strip() for n in body.split(",") if n.strip()]


SKILL_NAMES = _skill_names()

BLOCK_FACES = ["Attacker Down (lebka)", "Both Down", "Pushed", "Defender Stumbles", "Defender Down (POW)"]


def kostky_str(packed):
    """Kostky bloku zakódované po 3 bitech jako (face+1) -> jména."""
    out = []
    while packed:
        out.append(BLOCK_FACES[(packed & 7) - 1])
        packed >>= 3
    return ", ".join(out)


PREHOZ = {"ne": 0, "tym": 1, "tým": 1, "pro": 2}
ANO_NE = {"ne": 0, "ano": 1}
SKILL_OTAZKY = {"USE_DODGE": "Dodge", "USE_WRESTLE": "Wrestle", "USE_STAND_FIRM": "Stand Firm",
                "USE_SIDE_STEP": "Side Step"}


def moznosti_prehozu(q):
    return " / ".join(n for n, k in (("ne", 0), ("tym", 1), ("pro", 2)) if q["options"] & (1 << k))


def otazka_str(q):
    """P130: otázka na kouče — co padlo a co lze odpovědět."""
    t, p, o = q["question"], q["player"], q["other"]
    if t == "REROLL":
        return f"❓ PŘEHOZ? hráč {p}: padlo {q['roll']}, potřeba {q['target']}+  ->  odp {moznosti_prehozu(q)}"
    if t == "PASS_REROLL":
        return f"❓ PŘEHOZ PŘIHRÁVKY? hráč {p}: padlo {q['roll']}, potřeba {q['target']}+  ->  odp {moznosti_prehozu(q)}"
    if t == "PRO_GATE_REROLL":
        return (f"❓ hod na Pro hráče {p} NEPROŠEL: padlo {q['roll']}, potřeba 4+ — přehodit HOD NA PRO týmovým "
                f"rerollem?  ->  odp {moznosti_prehozu(q)}")
    if t == "BLOCK_REROLL":
        return (f"❓ BLOK hráč {p} na {o}: hozeno {q['target']}: {kostky_str(q['roll'])} — přehodit kostky?  ->  "
                f"odp {moznosti_prehozu(q)}")
    if t == "BLOCK_FACE":
        volby = " · ".join(f"{i} = {BLOCK_FACES[i]}" for i in range(5) if q["options"] & (1 << i))
        return (f"❓ BLOK hráč {p} na {o}: hozeno {kostky_str(q['roll'])} — kterou kostku?  ->  odp <číslo>: {volby}"
                "   (před odpovědí lze zadat push x y / follow 0|1)")
    if t in SKILL_OTAZKY:
        kdo = f" (blokuje hráč {o})" if o > 0 else ""
        navic = "   (před `odp ano` lze zadat sidestep x y)" if t == "USE_SIDE_STEP" else ""
        return f"❓ hráč {p}{kdo}: použít {SKILL_OTAZKY[t]}?  ->  odp ano / ne{navic}"
    return f"❓ {q}"


def preloz_odpoved(q, text):
    """Odpověď kouče -> číslo pro engine; None = nerozumím / není mezi možnostmi."""
    t = q["question"]
    if t == "BLOCK_FACE":
        a = int(text) if text.isdigit() else None
    elif t in SKILL_OTAZKY:
        a = ANO_NE.get(text.lower())
    else:
        a = PREHOZ.get(text.lower())
    return a if a is not None and q["options"] & (1 << a) else None


def event_str(e):
    """Jeden řádek: co se stalo a co padlo na kostkách."""
    t = e["type"]
    who = f"hráč {e['player']}"
    if t == "BLOCK":
        face = BLOCK_FACES[e["roll"]] if 0 <= e["roll"] < len(BLOCK_FACES) else e["roll"]
        # die1 = všechny hozené kostky po 3 bitech (face+1), die2 = počet (block_handler.cpp, 02.10.)
        vse = [BLOCK_FACES[((e["die1"] >> (3 * i)) & 7) - 1] for i in range(e["die2"])] if e["die2"] else []
        hozeno = f" · hozeno {e['die2']}: " + ", ".join(vse) if vse else ""
        return f"BLOK {who} na {e['target']}: zvolená kostka = {face}{hozeno}"
    if t in ("DODGE", "GFI", "PICKUP", "CATCH", "PASS", "STAND_UP", "LEAP"):
        # engine v `roll` posílá CÍLOVÉ číslo hodu (move_handler.cpp: `target`), ne padlou kostku
        return f"{t} {who}: potřeba {e['roll']}+ -> {'OK' if e['success'] else 'NEPROŠEL'}"
    if t in ("ARMOR_BREAK", "INJURY", "CASUALTY"):
        dice = f" ({e['die1']}+{e['die2']})" if e["die1"] else ""
        return f"{t} hráč {e['target'] if e['target'] > 0 else e['player']}: {e['roll']}{dice}"
    if t == "REROLL":
        # P130: roll = druh (0 přehoz hodu, 1 hod na Pro, 2 přehoz hodu na Pro, 3 Loner neprošel)
        if e["roll"] == 1:
            return f"PRO {who}: hod {e['die1']} (potřeba 4+) -> {'smí přehodit' if e['success'] else 'NEPROŠEL, původní hod platí'}"
        if e["roll"] == 2:
            return f"TÝMOVÝ REROLL na hod Pro {who}: {e['die1']} -> {e['die2']} ({'smí přehodit' if e['success'] else 'NEPROŠEL'})"
        if e["roll"] == 3:
            return f"TÝMOVÝ REROLL {who}: Loner hodil {e['die1']} (potřeba 4+) -> reroll PROPADL, původní hod platí"
        if e["target"] > 0:   # blok: die1/die2 = kostky před a po, po 3 bitech (face+1)
            return f"PŘEHOZ BLOKU {who} na {e['target']}: {kostky_str(e['die1'])} -> {kostky_str(e['die2'])}"
        return f"TÝMOVÝ REROLL {who}: původní hod {e['die1']} -> nový hod {e['die2']} ({'OK' if e['success'] else 'NEPROŠEL'})"
    if t == "FOLLOW_UP":
        return f"follow-up {who}: {'ano' if e['success'] else 'ne'}"
    if t == "MOVE":
        return f"krok {who} na {e['to']}"
    if t in ("SKILL", "SKILL_USED"):
        # roll = číslo skillu (enum SkillName), die1 = hozená kostka (big_guy_handler, 02.10.)
        jm = SKILL_NAMES[e["roll"]] if 0 <= e["roll"] < len(SKILL_NAMES) else e["roll"]
        hod = f" hod {e['die1']}" if e["die1"] else ""
        return f"SKILL {jm} {who}:{hod} -> {'OK' if e['success'] else 'NEPROŠEL'}"
    return f"{t} {who} cíl {e['target']} hod {e['roll']}"


def action_str(a):
    return f"{a.type} hrac={a.player_id} cil_hrac={a.target_id} cil_pole=({a.target.x},{a.target.y})"


def main():
    state, dice = new_game()
    last_actions = []
    # P195: zápas začíná počasím a losem (ř. 2551, 304-307). Vítěz losu volí, zda kope, nebo
    # přijímá; za AI volí defaultTossElection (rychlé soupisky přijímají, ostatní kopou).
    # `kouc` říká, která strana je člověk -- los vyhraný druhou stranou rozhodne AI hned.
    zalozeno = False
    los_vitez = None      # kdo vyhrál los a ještě nezvolil
    prvni_kope = None     # kdo kopal první výkop (2. poločas kope druhá strana, ř. 1016-1017)
    with open(OUT, "w") as f:
        f.write("=== NOVÁ HRA: lidé HOME × orkové AWAY, TV1500 — napřed `kouc HOME`, pak `los` ===\n")

    LOG = os.path.join(DIR, "prikazy.log")
    replay = []
    if os.path.exists(LOG):
        with open(LOG) as f:
            replay = [l.strip() for l in f if l.strip()]

    # P146: začátek rozehraného tahu a jeho akce (pro příkaz `kontrola`)
    tah_klic, tah_start, tah_kroky = None, None, []
    posledni_tah = None   # (start, konec, strana, kroky) posledního dokončeného tahu
    # P130: strana, za kterou rozhoduje kouč (None = engine jako dřív), a akce čekající na odpověď
    coach = None
    cekajici = None       # {"action": akce, "answers": [dosavadní odpovědi], "q": otázka}

    def proved(a, answers):
        """Provede akci; narazí-li na otázku kouče, nic se nezmění a otázka zůstane čekat."""
        nonlocal cekajici
        if coach is None:
            r, events = bb.execute_action_logged(state, a, dice)
            q = None
        else:
            r, events, q = bb.execute_action_coach(state, a, dice, coach, answers)
        out = []
        if q is not None:
            cekajici = {"action": a, "answers": list(answers), "q": q}
            out.append(f"akce ČEKÁ na rozhodnutí kouče: {action_str(a)}")
            for e in events:
                out.append("   (zatím) " + event_str(e))
            out.append(otazka_str(q))
            return out
        cekajici = None
        bb.clear_manual_block_choices()   # volba platí jen pro tuto akci
        tah_kroky.append({"action": a, "turnover": r.turnover})
        out.append(f"provedeno: {action_str(a)}")
        for e in events:
            out.append("   " + event_str(e))
        out.append(f"vysledek: turnover={r.turnover}")
        out.append(board(state))
        return out

    while True:
        # skóre v klíči: po TD a výkopu začíná nový drive, i když se číslo kola nezmění
        klic = (state.active_team, state.home_team.turn_number, state.away_team.turn_number, state.half,
                state.home_team.score, state.away_team.score)
        # tahem se rozumí jen fáze PLAY: rozestavení a výkop po TD / o poločase (`vykop`, `polocas`)
        # nesmí přepsat tah, ve kterém padl TD (druhé review 05.10., S2)
        if klic != tah_klic:
            if tah_start is not None and tah_start.phase == bb.GamePhase.PLAY:
                # konec tahu = stav hned po akci, která ho ukončila (i TD — deska před rozestavením)
                posledni_tah = (tah_start, state.clone(), tah_start.active_team, tah_kroky)
            tah_klic, tah_kroky = klic, []
            tah_start = state.clone() if state.phase == bb.GamePhase.PLAY else None
        if replay:
            line = replay.pop(0)
        elif not os.path.exists(CMD):
            time.sleep(0.3)
            continue
        else:
            with open(CMD) as f:
                line = f.read().strip()
            os.remove(CMD)
            if line.split() and line.split()[0] in ("do", "push", "follow", "blitzfrom", "actions", "vykop", "polocas", "kostka", "beztackle",
                                                      "kouc", "odp", "sidestep", "los", "volba"):
                with open(LOG, "a") as f:
                    f.write(line + "\n")
        parts = line.split()
        if not parts:
            continue
        cmd = parts[0]
        result_lines = []
        try:
            if cmd == "quit":
                with open(OUT, "w") as f:
                    f.write("server ukoncen\n")
                break
            elif cmd == "los" and not zalozeno and los_vitez is None:
                bb.roll_match_weather(state, dice)
                result_lines.append(f"počasí: {pocasi_str(state)}")
                hod = dice.roll_d6()
                vitez = bb.TeamSide.HOME if hod <= 3 else bb.TeamSide.AWAY
                result_lines.append(f"los: hod {hod} (1-3 HOME, 4-6 AWAY) -> vyhrávají {strana_str(vitez)}")
                if coach is not None and vitez == coach:
                    los_vitez = vitez
                    result_lines.append("❓ vyhrál jsi los: `volba kopat`, nebo `volba prijimat`?")
                else:
                    kopou = bb.ai_toss_elects_kick(ROSTERS[0] if vitez == bb.TeamSide.HOME else ROSTERS[1])
                    prvni_kope = vitez if kopou else druha(vitez)
                    result_lines.append(f"AI volí: {'KOPAT' if kopou else 'PŘIJÍMAT'} -> první výkop kopou {strana_str(prvni_kope)}")
                    zahaj(state, dice, prvni_kope)
                    zalozeno = True
                    result_lines.append(board(state))
            elif cmd == "volba" and len(parts) == 2 and los_vitez is not None:
                v = parts[1].lower()
                if v not in ("kopat", "prijimat", "přijímat"):
                    result_lines.append("❓ `volba kopat`, nebo `volba prijimat`")
                else:
                    prvni_kope = los_vitez if v == "kopat" else druha(los_vitez)
                    los_vitez = None
                    result_lines.append(f"první výkop kopou {strana_str(prvni_kope)}")
                    zahaj(state, dice, prvni_kope)
                    zalozeno = True
                    result_lines.append(board(state))
            elif not zalozeno and cmd != "kouc":
                result_lines.append("zápas ještě nezačal: napřed `kouc HOME` (kdo je člověk), pak `los`"
                                    + ("; teď čeká `volba kopat|prijimat`" if los_vitez is not None else ""))
            elif cmd == "board":
                result_lines.append(board(state))
                if cekajici:
                    result_lines.append(otazka_str(cekajici["q"]))
            elif cekajici and cmd not in ("odp", "push", "sidestep", "follow"):
                # hod už byl vidět -- z akce nejde vycouvat ani ji měnit
                result_lines.append("⛔ akce čeká na rozhodnutí kouče; teď jde jen odp / push / sidestep / follow / board")
                result_lines.append(otazka_str(cekajici["q"]))
            elif cmd == "odp" and len(parts) == 2:
                if not cekajici:
                    result_lines.append("žádná otázka nečeká")
                else:
                    a = preloz_odpoved(cekajici["q"], parts[1])
                    if a is None:
                        result_lines.append(f"⛔ odpověď `{parts[1]}` není mezi možnostmi")
                        result_lines.append(otazka_str(cekajici["q"]))
                    else:
                        result_lines += proved(cekajici["action"], cekajici["answers"] + [a])
            elif cmd == "kouc" and len(parts) == 2:
                v = parts[1].upper()
                coach = bb.TeamSide.HOME if v == "HOME" else bb.TeamSide.AWAY if v == "AWAY" else None
                result_lines.append(f"rozhodnutí kouče (P130): {coach if coach is not None else 'vypnuto, rozhoduje engine'}")
            elif cmd == "sidestep" and len(parts) == 3:
                bb.set_manual_side_step(int(parts[1]), int(parts[2]))
                result_lines.append(f"Side Step uhne na: ({parts[1]},{parts[2]})")
            elif cmd == "actions" and len(parts) == 2:
                side = bb.TeamSide.HOME if parts[1].upper() == "HOME" else bb.TeamSide.AWAY
                last_actions = [a for a in bb.get_available_actions(state)]
                result_lines.append(f"dostupnych akci celkem: {len(last_actions)}")
                for i, a in enumerate(last_actions):
                    if a.player_id is None or a.player_id < 1:
                        if side == bb.TeamSide.HOME and state.active_team == bb.TeamSide.HOME:
                            result_lines.append(f"  [{i}] {action_str(a)}  (bez konkretniho hrace)")
                        elif side == bb.TeamSide.AWAY and state.active_team == bb.TeamSide.AWAY:
                            result_lines.append(f"  [{i}] {action_str(a)}  (bez konkretniho hrace)")
                        continue
                    p = state.get_player(a.player_id)
                    if p.team_side == side:
                        result_lines.append(f"  [{i}] {action_str(a)}")
            elif cmd == "vykop" and len(parts) == 2:
                # po TD: rozestavení (KO hází na návrat) + výkop; parts[1] = kdo KOPE (HOME|AWAY)
                kick = bb.TeamSide.HOME if parts[1].upper() == "HOME" else bb.TeamSide.AWAY
                bb.setup_drive(state, ROSTERS[0], ROSTERS[1], kick, dice)
                bb.simple_kickoff(state, dice)
                result_lines.append(f"rozestavení + výkop, kope {kick}")
                result_lines.append(board(state))
            elif cmd == "polocas" and len(parts) in (1, 2):
                # začátek 2. poločasu: kope ten, kdo v 1. poločase přijímal (ř. 1016-1017);
                # parts[1] (HOME|AWAY) to smí přepsat
                if len(parts) == 2:
                    kick = bb.TeamSide.HOME if parts[1].upper() == "HOME" else bb.TeamSide.AWAY
                else:
                    kick = druha(prvni_kope)
                bb.setup_second_half(state, ROSTERS[0], ROSTERS[1], kick, dice)
                bb.simple_kickoff(state, dice)
                result_lines.append(f"2. poločas: rozestavení + výkop, kope {kick}")
                result_lines.append(board(state))
            elif cmd == "kostka" and len(parts) == 2:
                # volba kouče: kterou z hozených kostek vzít (0 lebka,1 Both Down,2 Pushed,3 Stumbles,4 POW)
                bb.set_manual_block_face(int(parts[1]))
                result_lines.append(f"příští blok: kostka {BLOCK_FACES[int(parts[1])]}")
            elif cmd == "beztackle":
                bb.set_manual_no_tackle(True)
                result_lines.append("příští blok: útočník NEPOUŽIJE Tackle")
            elif cmd == "push" and len(parts) == 3:
                bb.set_manual_push(int(parts[1]), int(parts[2]))
                result_lines.append(f"příští odtlačení: ({parts[1]},{parts[2]})")
            elif cmd == "blitzfrom" and len(parts) == 3:
                bb.set_manual_blitz_square(int(parts[1]), int(parts[2]))
                result_lines.append(f"příští blitz blokuje z: ({parts[1]},{parts[2]})")
            elif cmd == "follow" and len(parts) == 2:
                bb.set_manual_follow_up(parts[1] == "1")
                result_lines.append(f"příští follow-up: {'ano' if parts[1] == '1' else 'ne'}")
            elif cmd == "do" and len(parts) in (2, 3):
                a = last_actions[int(parts[-1])]   # `do 5` i starší tvar `do HOME 5`
                result_lines += proved(a, [])
            elif cmd == "tahai":
                # tahai [greedy|macro] [semínko] [iterace] — první číslo bez jména AI = semínko
                args = parts[1:]
                ai = "macro_mcts"
                if args and args[0] in ("greedy", "macro"):
                    ai = "greedy" if args.pop(0) == "greedy" else "macro_mcts"
                sd = int(args[0]) if len(args) > 0 else 1
                it = int(args[1]) if len(args) > 1 else 50
                kroky, po, hotovo = kt.plan_tahu(state, ai=ai, seed=sd, mcts_iterations=it)
                result_lines.append(f"=== TAH AI NANEČISTO ({ai}, semínko {sd}, {it} iterací) — partie se nemění ===")
                if not hotovo:
                    result_lines.append("⚠️ plán narazil na limit akcí, tah NENÍ dokončený")
                for k in kroky:
                    result_lines.append("  " + kt.popis_kroku(k))
                    for e in k["events"]:
                        result_lines.append("     " + event_str(e))
                result_lines.append(board(po))
                result_lines.append("--- kontrola tahu ---")
                result_lines += kt.kontrola_tahu(state, po, state.active_team, kroky)
            elif cmd == "kontrola" and len(parts) > 1 and parts[1] == "posledni":
                if posledni_tah is None:
                    result_lines.append("zatím žádný dokončený tah")
                else:
                    st0, st1, strana, kr = posledni_tah
                    result_lines.append(f"--- kontrola posledního dokončeného tahu ({strana}) ---")
                    result_lines += kt.kontrola_tahu(st0, st1, strana, kr)
            elif cmd == "kontrola":
                result_lines.append(f"--- kontrola rozehraného tahu ({state.active_team}) ---")
                if tah_start is None:
                    result_lines.append("teď neběží žádný tah (TD / rozestavení) — zkus `kontrola posledni`")
                else:
                    result_lines += kt.kontrola_tahu(tah_start, state, state.active_team, tah_kroky)
            else:
                result_lines.append(f"neznamy prikaz: {line}")
        except Exception as e:
            result_lines.append(f"CHYBA: {e}")
        with open(OUT, "w") as f:
            f.write("\n".join(result_lines) + "\n")


if __name__ == "__main__":
    main()
