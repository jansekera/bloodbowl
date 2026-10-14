#!/usr/local/bin/python3.8
"""Živá partie — uživatel (HOME, kouč) proti Claude (AWAY), TV1500. Partii vybírá BB_PARTIE
(slovník PARTIE níž): `lide-orkove` (výchozí, 12.–13.10.2026) · `skaveni-trpaslici` (14.10.2026).

Vychází z bb_play_session_20261002.py (server na pozadí, příkazy přes soubory, log se po
restartu přehraje). Navíc P130: rozhodnutí kouče uprostřed akce.

Komunikace přes soubory v evidence/<adresář partie>/ (play_20261012, play_20261014):
  cmd.txt    -- napíše se sem příkaz, server ho přečte a smaže
  out.txt    -- server sem napíše odpověď
Pohodlněji: ./bb_play_cmd.py <příkaz ...>  (pošle příkaz, počká a vypíše odpověď)

Příkazy:
  board                    -- vykreslí desku a panel hráčů (se skilly a rerolly)
  actions <HOME|AWAY>      -- vypíše dostupné akce pro danou stranu, číslovaně
  do <index>               -- provede akci podle čísla z 'actions'
  kouc <HOME|AWAY|OBE|ne>  -- P130: za tuto stranu rozhoduje KOUČ; OBE = za obě (P201 (6)); `ne` = engine
  los                      -- P195: začátek zápasu — počasí a los (D6: 1-3 HOME, 4-6 AWAY); vítěz volí
  volba <kopat|prijimat>   -- P195: volba kouče po vyhraném losu (za AI volí engine sám)
  odp <odpověď>            -- P130: odpověď na otázku, na které se akce zastavila:
                              přehoz: ne | tym | pro · skill: ano | ne · kostka: číslo kostky
  push <x> <y>             -- příští odtlačení na toto pole (volba útočníka; jinak engine)
  sidestep <x> <y>         -- P130: pole, kam uhne hráč se Side Stepem (volba jeho kouče)
  follow <0|1>             -- příští follow-up ano/ne pro stranu BEZ kouče (za kouče se server ptá po odtlačení, P201)
  blitzfrom <x> <y>        -- příští blitz blokuje z tohoto pole (volba kouče; jinak engine)
  kostka <0-4>             -- příští blok: kterou z hozených kostek vzít (pro stranu bez `kouc`)
  beztackle                -- příští blok: útočník nepoužije Tackle
  vykop <HOME|AWAY>        -- po TD: rozestavení + výkop, kdo KOPE
  polocas [HOME|AWAY]      -- 2. poločas: rozestavení + výkop; kope, kdo v 1. poločase přijímal
  tahai [greedy|macro] [semínko] [iterace]
                           -- jak by tah odehrála AI — NANEČISTO (kopie stavu, vlastní kostky)
  kontrola [posledni]      -- kontrola rozehraného / posledního dokončeného tahu
  do! <i>                  -- P201 (7): akce i přes varování, že nosič s rozehranou akcí ztratí přihrávku
  zpet                     -- P201 (14): vrátí poslední akci (do + odp + volby), záloha logu vedle
  quit                     -- ukončí server

P130 — jak otázka probíhá: akce se spustí; narazí-li na rozhodnutí kouče (přehoz, kostka
bloku, Dodge / Wrestle / Stand Firm / Side Step jeho hráče, follow-up a lékárník -- P201),
server vrátí stav i kostky před akci, ukáže, co padlo, a čeká na `odp`. Po odpovědi akci spustí znovu se stejnými
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



def _role_lide(p, st, sk):
    if st == (8, 2, 3, 7):
        return "C"                       # Catcher +Block+Side Step (Dodge, Catch)
    if st == (5, 5, 2, 9):
        return "OG"                      # Ogre +Block (Loner)
    if st == (7, 3, 3, 8):               # Blitzer
        # uživatel 13.10.: v kódu role ukazovat G = Guard (mění asistence), Mighty Blow ne
        if "Guard" in sk:
            return "BG"                  # +Guard+Stand Firm (+Dauntless / +Mighty Blow)
        if "StripBall" in sk:
            return "BH"                  # lovec míče +Wrestle+Tackle
        return "BT"                      # +Mighty Blow+Tackle
    if "Pass" in sk:
        return "T"                       # Thrower +Block+Pro
    if "Wrestle" in sk:
        return "LW"                      # Lineman +Wrestle
    return "L"                           # Lineman +Block


def _role_orkove(p, st, sk):
    if st == (4, 4, 2, 9):
        return "KG" if "Guard" in sk else "K"        # Black Orc (+Guard); Mighty Blow se neznačí
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


def _role_skaveni(p, st, sk):
    if st == (6, 5, 2, 8):
        return "OG"                          # Rat Ogre +Mighty Blow (Frenzy, Wild Animal, Loner)
    if st[0] >= 9:                           # Gutter Runner
        if "Sprint" in sk:
            return "RS"                      # MA10 +Block+Sure Feet+Sprint
        if "Wrestle" in sk:
            return "RW"                      # +Horns+Wrestle
        return "R"                           # +Block
    if st == (7, 3, 3, 8):
        return "BG"                          # Storm Vermin +Block+Guard (+Mighty Blow)
    if "Pass" in sk:
        return "T"                           # Thrower +Block+Pro (Sure Hands, Kick-Off Return)
    return "LW"                              # Lineman +Wrestle


def _role_trpaslici(p, st, sk):
    if st == (6, 3, 3, 8):
        return "R"                           # Runner (Sure Hands; jeden +Kick+Kick-Off Return)
    if st == (5, 3, 2, 8):
        return "SG"                          # Troll Slayer +Guard+Stand Firm+Mighty Blow (Frenzy, Dauntless)
    if st == (5, 3, 3, 9):
        return "BP" if "Pro" in sk else "B"  # Blitzer +Mighty Blow+Stand Firm+Tackle (+Pro)
    if "Guard" in sk:
        return "LG"                          # Longbeard +Guard+Stand Firm+Tackle
    if "Wrestle" in sk:
        return "LW"                          # Longbeard +Tackle+Wrestle
    return "L"                               # Longbeard (+Tackle / +Mighty Blow)


# Partie = soupisky, jména a kódy rolí; BB_PARTIE vybírá. Uživatel je vždy HOME (kouč).
PARTIE = {
    "lide-orkove": {   # 12.–13.10.2026
        "soupisky": ("human", "orc"), "tv": 1500, "seminko": 20261012, "adresar": "play_20261012",
        "jmena": ("LIDÉ", "ORKOVÉ"), "kratce": ("LID", "ORK"), "pismena": ("H", "O"),
        "kdo": ("člověk", "ork"), "role": (_role_lide, _role_orkove),
        "legenda": ("role lidé: L lineman · LW lineman Wrestle · T thrower (Pro) · C catcher · BG blitzer s GUARD"
                    " · BT blitzer MB+Tackle · BH lovec míče · OG ogre",
                    "role orkové: L · LW lineman Wrestle · T thrower · BG blitzer s GUARD · BP blitzer MB+Pro"
                    " · BH lovec míče · KG black orc s GUARD")},
    "skaveni-trpaslici": {   # 14.10.2026 (P203: Claude hraje trpaslíky s návrhem AI po ruce)
        "soupisky": ("skaven", "dwarf"), "tv": 1500, "seminko": 20261014, "adresar": "play_20261014",
        "jmena": ("SKAVENI", "TRPASLÍCI"), "kratce": ("SKA", "TRP"), "pismena": ("S", "D"),
        "kdo": ("skaven", "trpaslík"), "role": (_role_skaveni, _role_trpaslici),
        "legenda": ("role skaveni: LW lineman Wrestle · T thrower (Pro) · BG storm vermin s GUARD · R gutter runner"
                    " · RW runner Horns+Wrestle · RS runner Sprint · OG rat ogre",
                    "role trpaslíci: L longbeard · LW longbeard Wrestle · LG longbeard s GUARD · R runner"
                    " · B blitzer · BP blitzer s Pro · SG troll slayer s GUARD")},
}
P = PARTIE[os.environ.get("BB_PARTIE", "lide-orkove")]

# BB_PLAY_DIR = jiný adresář partie (zkoušky); stejnou proměnnou čte bb_play_cmd.py
DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   os.environ.get("BB_PLAY_DIR", os.path.join("evidence", P["adresar"])))
os.makedirs(DIR, exist_ok=True)
CMD = os.path.join(DIR, "cmd.txt")
OUT = os.path.join(DIR, "out.txt")

ENDZONE_HOME_X = 0   # HOME brání nižší x
ENDZONE_AWAY_X = 25  # AWAY brání vyšší x


def new_game(seed=None):
    global ROSTERS
    ROSTERS = tuple(bb.get_developed_roster(r, P["tv"]) for r in P["soupisky"])
    seed = P["seminko"] if seed is None else seed
    state = bb.GameState()
    dice = bb.DiceRoller(int(os.environ.get("BB_SEED", seed)))   # BB_SEED jen pro zkoušky
    return state, dice


POCASI = {"SWELTERING_HEAT": "úmorné vedro", "VERY_SUNNY": "ostré slunce", "NICE": "pěkně",
          "POURING_RAIN": "lije", "BLIZZARD": "vánice"}


def pocasi_str(state):
    jm = str(state.weather).split(".")[-1]
    return POCASI.get(jm, jm)


def strana_str(side):
    return f"{P['jmena'][0]} (HOME)" if side == bb.TeamSide.HOME else f"{P['jmena'][1]} (AWAY)"


def druha(side):
    return bb.TeamSide.AWAY if side == bb.TeamSide.HOME else bb.TeamSide.HOME


VYKOPOVA_TABULKA = {2: "Get the Ref (úplatky engine nemá — bez účinku)", 3: "Riot (posun značek kol)",
                    4: "Perfect Defence (kopající smí přestavět — engine nepřestavuje)",
                    5: "High Kick (hráč přijímajících pod míč)", 6: "Cheering Fans (D3: vyšší dostane reroll)",
                    7: "Changing Weather", 8: "Brilliant Coaching (D3: vyšší dostane reroll)",
                    9: "Quick Snap (přijímající se posunou o pole)", 10: "Blitz! (kopající mají volný tah)",
                    11: "Throw a Rock", 12: "Pitch Invasion"}


def vykop_pred(state, dice, kope, rozestaveni):
    """P200 (14.10.2026): rozestavení a snímek stavu před výkopem. `rozestaveni` = funkce
    (state, kope, dice) -> události rozestavení (návrat z KO). Vrací (řádky, snímek)."""
    out = [f"rozestavení + výkop, kope {strana_str(kope)}"]
    out += ["   " + event_str(e) for e in rozestaveni(state, kope, dice)]
    pred = {"rr": (state.home_team.rerolls, state.away_team.rerolls),
            "kolo": (state.home_team.turn_number, state.away_team.turn_number),
            "pole": {p.id: (p.position.x, p.position.y) for p in enum_players(state)},
            "pocasi": pocasi_str(state)}
    return out, pred


def vykop_po(state, kope, pred, ev):
    """Řádky výpisu výkopu: události a co výkopová tabulka změnila (rozdíl proti snímku)."""
    pred_rr, pred_kolo, pred_pole, pred_pocasi = pred["rr"], pred["kolo"], pred["pole"], pred["pocasi"]
    out = ["   " + event_str(e) for e in ev]
    kor = {e["player"] for e in ev if e["type"] in ("SKILL", "SKILL_USED")}
    for p in enum_players(state):
        a, b = pred_pole.get(p.id), (p.position.x, p.position.y)
        if a and a != b and p.id not in kor:
            out.append(f"   hráč {p.id} při výkopu odchází ze hřiště: {str(p.state).split('.')[-1]}" if b == (-1, -1)
                       else f"   přesun při výkopu: hráč {p.id} {a} -> {b}")
    if pocasi_str(state) != pred_pocasi:
        out.append(f"   počasí teď: {pocasi_str(state)} (bylo {pred_pocasi})")
    for jm, a, b in (("HOME", pred_rr[0], state.home_team.rerolls), ("AWAY", pred_rr[1], state.away_team.rerolls)):
        if a != b:
            out.append(f"   týmové rerolly {jm}: {a} -> {b}")
    prijima = druha(kope)   # výkop přijímajícím vždy začne další kolo; jiný posun = Riot
    if pred_kolo != (state.home_team.turn_number - (prijima == bb.TeamSide.HOME),
                     state.away_team.turn_number - (prijima == bb.TeamSide.AWAY)):
        out.append(f"   značky kol po výkopu: H:{state.home_team.turn_number} A:{state.away_team.turn_number}")
    if state.active_team == kope:
        out.append("   ⚠️ na tahu jsou KOPAJÍCÍ (Blitz!) — míč je ve vzduchu")
    return out


def vykop(state, dice, kope, rozestaveni):
    """Rozestavení + výkop bez otázek kouče (zkoušky, přehrání)."""
    out, pred = vykop_pred(state, dice, kope, rozestaveni)
    return out + vykop_po(state, kope, pred, bb.simple_kickoff(state, dice))


def zahaj(state, dice, kope):
    """Rozestavení a první výkop; `kope` = strana, která kope. Vrací řádky výpisu."""
    return vykop(state, dice, kope, lambda s, k, d: bb.setup_half(s, ROSTERS[0], ROSTERS[1], k) or [])


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
    """Krátký kód role podle statů a skillů (podle partie, HOME / AWAY)."""
    st = (p.stats.movement, p.stats.strength, p.stats.agility, p.stats.armour)
    return P["role"][0 if p.team_side == bb.TeamSide.HOME else 1](p, st, set(skilly(p)))


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
        side = P["pismena"][0 if p.team_side == bb.TeamSide.HOME else 1]
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
    # `/n` jen u NASICH stojicich (HOME), VZDY i s /0.
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
    lines.append(f"{P['pismena'][0]} {P['kdo'][0]} (náš, HOME) · {P['pismena'][1]} {P['kdo'][1]} (jejich, AWAY)"
                 " · +role +ID")
    lines += list(P["legenda"])
    lines.append("G v kódu role = Guard (asistuje i v soupeřově zóně)")
    lines.append("cislo v prazdnem poli = kolik JEJICH tacklezon na nej dosahuje")
    lines.append(f"/n u {P['pismena'][0]}.. = VZDY (i /0) -- v kolika JEJICH zonach stoji")
    lines.append(f"u {P['pismena'][1]}.. cislo NENI (matoucí) · '_' lezi · 'o' drzi mic · 'B' blitzoval · '-' uz hral · malymi = stunned")
    lines.append("")
    for jm, t in ((strana_str(bb.TeamSide.HOME), state.home_team), (strana_str(bb.TeamSide.AWAY), state.away_team)):
        lines.append(f"{jm}: skóre {t.score} · týmové rerolly {t.rerolls}"
                     + (" (v tomto tahu už použit)" if t.reroll_used_this_turn else ""))
    lines.append("")
    lines.append("--- hráči (staty, skilly) ---")
    for p in sorted(players, key=lambda q: (q.team_side != bb.TeamSide.HOME, q.id)):
        strana = P["kratce"][0 if p.team_side == bb.TeamSide.HOME else 1]
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
                "   (před odpovědí lze zadat push x y)")
    if t == "KICKOFF_RETURN":   # P200 (1)
        pole = " · ".join(f"{i} = {tuple(xy)}" for i, xy in enumerate(q.get("squares", []), 1))
        return (f"❓ KICK-OFF RETURN hráč {p}: míč dopadne na ({q['target'] // 100},{q['target'] % 100}) — kam jít (až 3 pole)?"
                f"  ->  odp 0 = nepoužít · {pole}")
    if t == "USE_FOLLOW_UP":
        return f"❓ hráč {p} odtlačil hráče {o}: následovat (follow-up)?  ->  odp ano / ne"
    if t == "USE_APOTHECARY":
        co = "KO" if q["roll"] < 0 else f"zranění {zraneni_str(q['roll'])}"
        return f"❓ LÉKÁRNÍK? hráč {p}: {co} — použít lékárníka (jednou za zápas)?  ->  odp ano / ne"
    if t == "APOTHECARY_PICK":
        return (f"❓ LÉKÁRNÍK hráč {p}: první hod {zraneni_str(q['roll'])}, druhý {zraneni_str(q['target'])}"
                " — který platí?  ->  odp 1 / 2")
    if t in SKILL_OTAZKY:
        kdo = f" (v bloku s hráčem {o})" if o > 0 else ""   # P201 (5): ptá se i útočník
        navic = "   (před `odp ano` lze zadat sidestep x y)" if t == "USE_SIDE_STEP" else ""
        return f"❓ hráč {p}{kdo}: použít {SKILL_OTAZKY[t]}?  ->  odp ano / ne{navic}"
    return f"❓ {q}"


def preloz_odpoved(q, text):
    """Odpověď kouče -> číslo pro engine; None = nerozumím / není mezi možnostmi."""
    t = q["question"]
    if t in ("BLOCK_FACE", "KICKOFF_RETURN"):
        a = int(text) if text.isdigit() else None
    elif t == "APOTHECARY_PICK":
        a = {"1": 0, "2": 1}.get(text)
    elif t in SKILL_OTAZKY or t in ("USE_FOLLOW_UP", "USE_APOTHECARY"):
        a = ANO_NE.get(text.lower())
    else:
        a = PREHOZ.get(text.lower())
    return a if a is not None and q["options"] & (1 << a) else None


ZRANENI = ["Badly Hurt (do konce zápasu mimo)", "Miss Next Game", "Niggling Injury", "-1 MA", "-1 AV",
           "-1 AG", "-1 ST", "MRTEV"]   # enums.h CasualtyResult


def zraneni_str(i):
    return ZRANENI[i] if 0 <= i < len(ZRANENI) else f"? ({i})"


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
        # `roll` = CÍLOVÉ číslo hodu, `die1` = kostka, která platí (P145; 0 = engine ji neposlal,
        # např. chytání bez hodu nebo PASS)
        if not e["roll"]:   # vstávání s MA 3+, No Hands, ležící „chytač“ -- nehází se
            return f"{t} {who}: bez hodu -> {'OK' if e['success'] else 'NEPROŠEL'}"
        padlo = f", padlo {e['die1']}" if e["die1"] else ""
        return f"{t} {who}: potřeba {e['roll']}+{padlo} -> {'OK' if e['success'] else 'NEPROŠEL'}"
    if t in ("ARMOR_BREAK", "INJURY"):
        # P201 (8): modifikátor (Mighty Blow, Stunty, asistence u faulu) zvlášť: „9 (4+4 +1)“
        mod = e["roll"] - e["die1"] - e["die2"]
        dice = (f" ({e['die1']}+{e['die2']}" + (f" {'+' if mod > 0 else '-'}{abs(mod)}" if mod else "") + ")"
                if e["die1"] else "")
        co = ""
        if t == "INJURY":   # injury.cpp: do 7 omráčen, 8-9 KO; 10+ jen při lékárníkovi / Regeneraci (P201 (2))
            co = " -> omráčen" if e["roll"] <= 7 else " -> KO" if e["roll"] <= 9 else " -> ZRANĚNÍ (hod na tabulku zranění)"
        return f"{t} {who}: {e['roll']}{dice}{co}"
    if t == "CASUALTY":
        # P145 (c): `target` je VÝSLEDEK tabulky zranění, ne hráč (tisklo se „CASUALTY hráč 1“)
        dice = f" ({e['die1']}+{e['die2']})" if e["die1"] else ""
        return f"CASUALTY {who}: {e['roll']}{dice} -> {zraneni_str(e['target'])}"
    if t == "APOTHECARY":
        if e["roll"] < 0:   # P201 (1): lékárník na KO -- hráč zůstává na hřišti omráčený
            return f"LÉKÁRNÍK {who}: místo KO zůstává na hřišti omráčený"
        rezervy = " -> lékárník ho poslal do REZERV, v zápase pokračuje" if e["roll"] == 0 else ""   # 0 = BADLY_HURT
        return (f"LÉKÁRNÍK {who}: {zraneni_str(e['die1'] - 1)} / {zraneni_str(e['die2'] - 1)}"
                f" -> platí {zraneni_str(e['roll'])}{rezervy}")
    if t == "FOUL":
        # uživatel 13.10.: u faulu ukázat kostky kvůli kontrole vyloučení (dvojice na brnění NEBO
        # na zranění = vyloučen, ř. 1876-1880). roll = brnění včetně asistencí, die1/die2 = kostky.
        d1, d2 = e["die1"], e["die2"]
        navic = e["roll"] - d1 - d2
        plus = f" {'+' if navic >= 0 else '-'}{abs(navic)} asistence" if navic else ""
        dvojice = " · DVOJICE -> vyloučení" if d1 == d2 else " · dvojice nepadla"
        return (f"FAUL {who} na hráče {e['target']}: brnění {d1}+{d2}{plus} = {e['roll']} -> "
                f"{'PROLOMENO' if e['success'] else 'vydrželo'}{dvojice}")
    if t == "EJECTED":
        return f"VYLOUČEN {who} (dvojice při faulu)"
    if t == "CROWD_SURF":
        return f"DO DAVU {who} z pole {e['from']}"
    if t == "BALL_BOUNCE":
        # die1 = kostka šablony ⇒ vhazování (roll = 2D6, první pole je výchozí, FAQ ř. 9307-9314);
        # jinak odraz (roll = D8 směr). `to` mimo hřiště = míč vyletěl do autu.
        fx, fy = e["from"]
        tx, ty = e["to"]
        aut = "" if 0 <= tx < 26 and 0 <= ty < 15 else " — DO AUTU"
        if e["die1"]:
            return f"VHAZOVÁNÍ z ({fx},{fy}): šablona {e['die1']}, 2D6 = {e['roll']} -> ({tx},{ty}){aut}"
        return f"ODRAZ míče ({fx},{fy}) -> ({tx},{ty}), D8 = {e['roll']}{aut}"
    if t == "REROLL":
        # P130: roll = druh (0 přehoz hodu, 1 hod na Pro, 2 přehoz hodu na Pro, 3 Loner neprošel)
        if e["roll"] == 1:
            return f"PRO {who}: hod {e['die1']} (potřeba 4+) -> {'smí přehodit' if e['success'] else 'NEPROŠEL, původní hod platí'}"
        if e["roll"] == 2:
            return f"TÝMOVÝ REROLL na hod Pro {who}: {e['die1']} -> {e['die2']} ({'smí přehodit' if e['success'] else 'NEPROŠEL'})"
        if e["roll"] == 4:   # P201 (11): týmový reroll vzal engine (strana bez kouče)
            return f"TÝMOVÝ REROLL {who}: použil ho engine (strana bez kouče)"
        if e["roll"] == 3:
            return f"TÝMOVÝ REROLL {who}: Loner hodil {e['die1']} (potřeba 4+) -> reroll PROPADL, původní hod platí"
        if e["target"] > 0:   # blok: die1/die2 = kostky před a po, po 3 bitech (face+1)
            return f"PŘEHOZ BLOKU {who} na {e['target']}: {kostky_str(e['die1'])} -> {kostky_str(e['die2'])}"
        return f"TÝMOVÝ REROLL {who}: původní hod {e['die1']} -> nový hod {e['die2']} ({'OK' if e['success'] else 'NEPROŠEL'})"
    if t == "FOLLOW_UP":
        return f"follow-up {who}: {'ano' if e['success'] else 'ne'}"
    if t == "KICKOFF":
        # P200: from = kam se mířilo, roll = rozptyl po Kicku, die1 = D6, die2 = D8 (1 = sever, po směru hodin)
        kick = f" (Kick půlí {e['die1']} -> {e['roll']})" if e["roll"] != e["die1"] else ""
        return (f"VÝKOP: míří na {e['from']}, rozptyl D6 = {e['die1']}{kick}, směr D8 = {e['die2']}"
                f" -> dopad {e['to']}")
    if t == "KICKOFF_TABLE":
        return f"VÝKOPOVÁ TABULKA: 2D6 = {e['roll']} -> {VYKOPOVA_TABULKA.get(e['target'], e['target'])}"
    if t == "WEATHER":
        return f"ZMĚNA POČASÍ: 2D6 = {e['roll']}"
    if t == "KO_RECOVERY":
        return f"NÁVRAT Z KO {who}: D6 = {e['roll']} -> {'vrací se' if e['success'] else 'zůstává v KO'} (4+)"
    if t == "MOVE":
        return f"krok {who} na {e['to']}"
    if t in ("SKILL", "SKILL_USED"):
        # roll = číslo skillu (enum SkillName), die1 = hozená kostka (big_guy_handler, 02.10.)
        jm = SKILL_NAMES[e["roll"]] if 0 <= e["roll"] < len(SKILL_NAMES) else e["roll"]
        if e["die2"]:   # přehoz skillem (helpers.cpp attemptRoll): die2 = původní hod, die1 = nový
            return f"SKILL {jm} {who}: přehoz {e['die2']} -> {e['die1']} ({'OK' if e['success'] else 'NEPROŠEL'})"
        if jm == "MightyBlow":   # P201 (8): die1 = 1 brnění, 2 zranění
            return f"MIGHTY BLOW: +1 na {'brnění' if e['die1'] == 1 else 'zranění'} {who}"
        if jm == "StandFirm":   # P201 (4)
            return f"STAND FIRM {who}: zůstává stát, odtlačen není"
        if jm == "KickOffReturn":
            return f"SKILL Kick-Off Return {who}: {e['from']} -> {e['to']}"
        hod = f" hod {e['die1']}" if e["die1"] else ""
        return f"SKILL {jm} {who}:{hod} -> {'OK' if e['success'] else 'NEPROŠEL'}"
    return f"{t} {who} cíl {e['target']} hod {e['roll']}"


def action_str(a):
    return f"{a.type} hrac={a.player_id} cil_hrac={a.target_id} cil_pole=({a.target.x},{a.target.y})"


def varovani_nosic(state, akce, a):
    """P201 (7) a (15), 14.10.2026: nosič s rozehranou akcí, který ještě smí přihrát nebo předat,
    a akce JINÉHO hráče ⇒ nosičova akce skončí a přihrávka propadne (v partii 13.10. dvakrát).
    Vrací text varování, nebo None."""
    if not state.ball.is_held or a.player_id is None or a.player_id < 1:
        return None
    nosic = state.get_player(state.ball.carrier_id)
    if nosic.team_side != state.active_team or not nosic.has_moved or nosic.has_acted or a.player_id == nosic.id:
        return None
    muze = sorted({x.type.name for x in akce if x.player_id == nosic.id and x.type.name in ("PASS", "HAND_OFF")})
    if not muze:
        return None
    return (f"⚠️ nosič {nosic.id} má rozehranou akci a ještě může {' / '.join(muze)} — akcí hráče {a.player_id} "
            f"jeho akce skončí a tahle možnost propadne. Opravdu? `do! <číslo>`")


VOLBY_AKCE = ("push", "follow", "blitzfrom", "sidestep", "kostka", "beztackle", "actions")


def odstrihni_posledni_akci(radky):
    """P201 (14), 14.10.2026: řádky logu bez poslední akce (`do` i s odpověďmi `odp` a volbami
    zadanými před ní a po ní). None = není co vrátit (poslední změnou stavu je los / výkop).
    Dřív se vracelo ručně ustřižením prikazy.log (13.10. dvakrát)."""
    i = len(radky)
    while i > 0 and radky[i - 1].split()[0] in VOLBY_AKCE + ("odp",):
        i -= 1
    if i == 0 or radky[i - 1].split()[0] not in ("do", "do!"):
        return None
    i -= 1
    # `actions` před akcí zůstává: přehrání pak skončí se stejnou nabídkou, jakou má
    # uživatel na obrazovce (review 14.10.: jinak by další `do N` sáhl do zastaralé nabídky)
    while i > 0 and radky[i - 1].split()[0] in VOLBY_AKCE and radky[i - 1].split()[0] != "actions":
        i -= 1
    return radky[:i], radky[i:]


def main():
    state, dice = new_game()
    last_actions = []
    # P195: zápas začíná počasím a losem (ř. 2551, 304-307). Vítěz losu volí, zda kope, nebo
    # přijímá; za AI volí defaultTossElection (rychlé soupisky přijímají, ostatní kopou).
    # `kouc` říká, která strana je člověk -- los vyhraný druhou stranou rozhodne AI hned.
    zalozeno = False
    los_vitez = None      # kdo vyhrál los a ještě nezvolil
    prvni_kope = None     # kdo kopal první výkop (2. poločas kope druhá strana, ř. 1016-1017)
    # review 14.10.: po `zpet` (restart + přehrání) by úvodní hláška přepsala výstup dřív, než
    # přehrání doběhne, a klient by ji vypsal místo „vráceno“
    if "BB_ZPET" not in os.environ:
        with open(OUT, "w") as f:
            f.write(f"=== NOVÁ HRA: {P['jmena'][0]} HOME × {P['jmena'][1]} AWAY, TV{P['tv']} — napřed `kouc HOME`,"
                    " pak `los` ===\n")

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
    oba = False           # P201 (6): `kouc OBE` = kouč rozhoduje za obě strany (druhou hraje Claude ručně)
    cekajici = None       # {"action": akce, "answers": [dosavadní odpovědi], "q": otázka}

    def proved(a, answers):
        """Provede akci; narazí-li na otázku kouče, nic se nezmění a otázka zůstane čekat."""
        nonlocal cekajici
        if coach is None:
            r, events = bb.execute_action_logged(state, a, dice)
            q = None
        else:
            r, events, q = bb.execute_action_coach(state, a, dice, coach, answers, oba)
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

    def vykop_kouc(kope, pred, answers):
        """P208 (9): výkop s otázkami kouče (lékárník u zranění z výkopové tabulky)."""
        nonlocal cekajici
        _, ev, q = bb.kickoff_coach(state, dice, coach, answers, oba)
        if q is not None:
            cekajici = {"vykop": (kope, pred), "answers": list(answers), "q": q}
            return (["výkop ČEKÁ na rozhodnutí kouče"] + ["   (zatím) " + event_str(e) for e in ev]
                    + [otazka_str(q)])
        cekajici = None
        return vykop_po(state, kope, pred, ev)

    def kopni(kope, rozestaveni):
        out, pred = vykop_pred(state, dice, kope, rozestaveni)
        if coach is None:
            return out + vykop_po(state, kope, pred, bb.simple_kickoff(state, dice))
        return out + vykop_kouc(kope, pred, [])

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
            ticho = bool(replay)
            zalogovano = False   # během přehrávání se odpověď zapíše jen po posledním řádku
        elif not os.path.exists(CMD):
            time.sleep(0.3)
            continue
        else:
            with open(CMD) as f:
                line = f.read().strip()
            os.remove(CMD)
            ticho = False
            zalogovano = False
            if line.split() and line.split()[0] in ("do", "do!", "push", "follow", "blitzfrom", "actions", "vykop", "polocas", "kostka", "beztackle",
                                                      "kouc", "odp", "sidestep", "los", "volba"):
                with open(LOG, "a") as f:
                    f.write(line + "\n")
                zalogovano = True
        parts = line.split()
        if not parts:
            continue
        cmd = parts[0]
        result_lines = []
        odmitnuto = False   # review 14.10.: odmítnutý příkaz z logu ven, jinak ho `zpet` vezme za akci
        try:
            if cmd == "quit":
                with open(OUT, "w") as f:
                    f.write("server ukoncen\n")
                break
            elif cmd == "zpet":
                # P201 (14): log bez poslední akce, záloha vedle, server se spustí znovu a přehraje ho
                with open(LOG) as f:
                    radky = [l.strip() for l in f if l.strip()]
                rez = odstrihni_posledni_akci(radky)
                if rez is None:
                    result_lines.append("⛔ není co vrátit (poslední změnou je los / výkop / poločas)")
                else:
                    nove, pryc = rez
                    zaloha = os.path.join(DIR, time.strftime("prikazy_pred_vracenim_%Y%m%d_%H%M%S.log"))
                    with open(zaloha, "w") as f:
                        f.write("\n".join(radky) + "\n")
                    with open(LOG, "w") as f:
                        f.write("".join(l + "\n" for l in nove))
                    os.environ["BB_ZPET"] = f"↩ vráceno ({len(pryc)} řádků logu: {' · '.join(pryc)}); záloha {os.path.basename(zaloha)}"
                    os.execv(sys.executable, [sys.executable] + sys.argv)
            elif cmd == "los" and not zalozeno and los_vitez is None:
                bb.roll_match_weather(state, dice)
                result_lines.append(f"počasí: {pocasi_str(state)}")
                hod = dice.roll_d6()
                vitez = bb.TeamSide.HOME if hod <= 3 else bb.TeamSide.AWAY
                result_lines.append(f"los: hod {hod} (1-3 HOME, 4-6 AWAY) -> vyhrávají {strana_str(vitez)}")
                if coach is not None and (vitez == coach or oba):
                    los_vitez = vitez
                    result_lines.append("❓ vyhrál jsi los: `volba kopat`, nebo `volba prijimat`?")
                else:
                    kopou = bb.ai_toss_elects_kick(ROSTERS[0] if vitez == bb.TeamSide.HOME else ROSTERS[1])
                    prvni_kope = vitez if kopou else druha(vitez)
                    result_lines.append(f"AI volí: {'KOPAT' if kopou else 'PŘIJÍMAT'} -> první výkop kopou {strana_str(prvni_kope)}")
                    result_lines += kopni(prvni_kope, lambda s, k, d: bb.setup_half(s, ROSTERS[0], ROSTERS[1], k) or [])
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
                    result_lines += kopni(prvni_kope, lambda s, k, d: bb.setup_half(s, ROSTERS[0], ROSTERS[1], k) or [])
                    zalozeno = True
                    result_lines.append(board(state))
            elif not zalozeno and cmd != "kouc":
                result_lines.append("zápas ještě nezačal: napřed `kouc HOME` (kdo je člověk), pak `los`"
                                    + ("; teď čeká `volba kopat|prijimat`" if los_vitez is not None else ""))
            elif cmd == "board":
                result_lines.append(board(state))
                if cekajici:
                    result_lines.append(otazka_str(cekajici["q"]))
            elif cekajici and cmd not in ("odp", "push", "sidestep", "follow", "zpet"):
                # hod už byl vidět -- z akce nejde vycouvat ani ji měnit
                result_lines.append("⛔ akce čeká na rozhodnutí kouče; teď jde jen odp / push / sidestep / follow / board")
                odmitnuto = True
                result_lines.append(otazka_str(cekajici["q"]))
            elif cmd == "odp" and len(parts) == 2:
                if not cekajici:
                    result_lines.append("žádná otázka nečeká")
                    odmitnuto = True
                else:
                    a = preloz_odpoved(cekajici["q"], parts[1])
                    if a is None:
                        result_lines.append(f"⛔ odpověď `{parts[1]}` není mezi možnostmi")
                        odmitnuto = True
                        result_lines.append(otazka_str(cekajici["q"]))
                    elif "vykop" in cekajici:
                        kope_v, pred_v = cekajici["vykop"]
                        result_lines += vykop_kouc(kope_v, pred_v, cekajici["answers"] + [a])
                        result_lines.append(board(state))
                    else:
                        result_lines += proved(cekajici["action"], cekajici["answers"] + [a])
            elif cmd == "kouc" and len(parts) == 2:
                v = parts[1].upper()
                oba = v == "OBE"
                coach = bb.TeamSide.HOME if v in ("HOME", "OBE") else bb.TeamSide.AWAY if v == "AWAY" else None
                result_lines.append("rozhodnutí kouče (P130): " + ("OBĚ STRANY" if oba else
                                    str(coach) if coach is not None else "vypnuto, rozhoduje engine"))
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
                result_lines += kopni(kick, lambda s, k, d: bb.setup_drive(s, ROSTERS[0], ROSTERS[1], k, d))
                result_lines.append(board(state))
            elif cmd == "polocas" and len(parts) in (1, 2):
                # začátek 2. poločasu: kope ten, kdo v 1. poločase přijímal (ř. 1016-1017);
                # parts[1] (HOME|AWAY) to smí přepsat
                if len(parts) == 2:
                    kick = bb.TeamSide.HOME if parts[1].upper() == "HOME" else bb.TeamSide.AWAY
                else:
                    kick = druha(prvni_kope)
                result_lines.append("2. poločas")
                result_lines += kopni(kick, lambda s, k, d: bb.setup_second_half(s, ROSTERS[0], ROSTERS[1], k, d))
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
            elif cmd in ("do", "do!") and len(parts) in (2, 3):
                a = last_actions[int(parts[-1])]   # `do 5` i starší tvar `do HOME 5`
                varovani = varovani_nosic(state, last_actions, a) if cmd == "do" else None
                if varovani:
                    result_lines.append(varovani)
                    odmitnuto = True
                else:
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
            odmitnuto = True
        if odmitnuto and zalogovano:
            with open(LOG) as f:
                radky = f.readlines()
            with open(LOG, "w") as f:
                f.writelines(radky[:-1])
        if ticho:
            continue
        zpet = os.environ.pop("BB_ZPET", None)
        if zpet:   # první odpověď po přehrání zkráceného logu
            result_lines = [zpet] + result_lines + [board(state)]
            if cekajici:
                result_lines.append(otazka_str(cekajici["q"]))
        with open(OUT, "w") as f:
            f.write("\n".join(result_lines) + "\n")


if __name__ == "__main__":
    main()
