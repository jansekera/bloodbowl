#!/usr/local/bin/python3.8
"""Živá partie 04.09.2026 — uživatel (trpaslíci, HOME) proti Claude (wood-elf, AWAY).

Drží stav hry mezi jednotlivými tahy jako dlouho běžící proces (bb_engine
GameState se nedá picklovat, proto server na pozadí místo souboru se stavem).
Komunikace přes soubory v evidence/play_20260904/:
  cmd.txt    -- napíše se sem příkaz, server ho přečte a smaže
  out.txt    -- server sem napíše odpověď

Příkazy:
  board                    -- vykreslí desku a panel hráčů
  actions <HOME|AWAY>      -- vypíše dostupné akce pro danou stranu, číslovaně
  do <HOME|AWAY> <index>   -- provede akci podle čísla z 'actions'
  quit                     -- ukončí server
"""
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "engine", "build"))
import bb_engine as bb  # noqa: E402

DIR = os.path.join(os.path.dirname(__file__), "evidence", "play_20260904")
os.makedirs(DIR, exist_ok=True)
CMD = os.path.join(DIR, "cmd.txt")
OUT = os.path.join(DIR, "out.txt")

ENDZONE_HOME_X = 0   # trpaslici (HOME) brani nizsi x
ENDZONE_AWAY_X = 25  # wood-elf (AWAY) brani vyssi x


def new_game(seed=20260904):
    dwarf = bb.get_developed_roster("dwarf", 1200)
    elf = bb.get_developed_roster("wood-elf", 1200)
    state = bb.GameState()
    bb.setup_half(state, dwarf, elf, bb.TeamSide.AWAY)  # AWAY kope, HOME prijima
    dice = bb.DiceRoller(seed)
    bb.simple_kickoff(state, dice)
    return state, dice


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


# ⭐⭐⭐ RASA+ROLE, ODVOZENO ZE ZDROJE (engine/src/roster.cpp,
#   getDwarfRoster1200/getWoodElfRoster1200, TV1200 -- uzivatel 04.09.:
#   "prepni na roster 1200"), NE dotazem na dovednosti -- Python vazba
#   `has_skill` vyzaduje SkillName, a ten v Pythonu vubec neni
#   zpristupneny (overeno 04.09., bez rebuildu nejde volat).
# ⛔⛔ ZNAMY LIMIT, ROZHODNUTO SE S NIM SMIRIT (uzivatel: "nebudeme
#   vymyslet vic"): v TV1200 sdileji STEJNE staty tri ruzne role --
#   dwarf (4,3,2,9) = Longbeard / Longbeard+Guard / Longbeard+Wrestle;
#   wood-elf (7,3,4,7) = Lineman / Thrower+Block / Lineman+Wrestle.
#   Kod ukazuje jen ZAKLADNI jmeno ("L"), Guard/Wrestle na desce NENI
#   VIDET -- kdo presne Guard ma, se z tohoto nastroje nezjisti.
# ⭐ "G" pripona jen tam, kde v TV1200 NEEXISTUJE zadna jina varianta se
#   stejnymi staty -- tam je Guard JISTY, ne odhad (Blitzer/Troll Slayer/
#   Treeman maji v tomto rosteru jen tu jednu "+Guard(+Tackle)" varintu).
#   Longbeard a Lineman zustavaji BEZ "G" -- tam Guard-nesouci jedinec
#   neni od ostatnich rozlisitelny (viz vyse).
ROLE_BY_STATS = {
    (True, 4, 3, 2, 9): "L",     # dwarf Longbeard (+Guard/+Wrestle nerozliseno)
    (True, 6, 3, 3, 8): "R",     # dwarf Runner +Block
    (True, 5, 3, 3, 9): "BG",    # dwarf Blitzer +Guard+Tackle (jen tato varianta existuje)
    (True, 5, 3, 2, 8): "TG",    # dwarf Troll Slayer +Guard+Tackle (jen tato varianta existuje)
    (False, 7, 3, 4, 7): "L",    # wood-elf Lineman (+Wrestle/Thrower nerozliseno)
    (False, 8, 2, 4, 7): "C",    # wood-elf Catcher +Block
    (False, 8, 3, 4, 7): "W",    # wood-elf Wardancer (obe varianty)
    (False, 2, 6, 1, 10): "TRG", # wood-elf Treeman +Guard (jen tato varianta existuje)
}


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
        role = ROLE_BY_STATS.get(
            (p.team_side == bb.TeamSide.HOME, p.stats.movement, p.stats.strength,
             p.stats.agility, p.stats.armour), "?")
        side = "D" if p.team_side == bb.TeamSide.HOME else "W"
        # ⭐ ID jako pripona: bez ni by 4 stejni Longbeardove vypadali
        #   identicky a nesli by odlisit pro hlaseni tahu (na rozdil od
        #   diag_board_render.py, ktery jen CTE pozici, tady se podle
        #   kodu VOLA konkretni akce).
        code = f"{side}{role}{p.id}"
        if p.state == bb.PlayerState.STUNNED:
            code = code.lower()
        if state.ball.is_held and state.ball.carrier_id == p.id:
            code += "o"
        elif p.state == bb.PlayerState.PRONE:
            code += "_"
        elif p.has_acted:
            code += "-"
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
    # `/n` jen u NASICH stojicich (trpaslici = HOME), VZDY i s /0.
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
    xlo, xhi = max(0, min(occx) - 2), min(25, max(occx) + 2)
    ylo, yhi = max(0, min(occy) - 1), min(14, max(occy) + 1)
    venku = [(sq, v) for sq, v in cell.items() if not (xlo <= sq[0] <= xhi)]

    lines = []
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
    #   sedet se sirkou bunky (7 = 6 obsah + 1 oddelovac).
    lines.append("     " + "".join(f"{x:<6} " for x in range(xlo, xhi + 1)))
    hline = "    +" + "------+" * (xhi - xlo + 1)
    for y in range(ylo, yhi + 1):
        lines.append(hline)
        row = f"y={y:<2}|"
        for x in range(xlo, xhi + 1):
            v = cell.get((x, y))
            if v:
                row += f"{v:<6}|"
            else:
                z = zon((x, y), st_away)
                row += (f"  {z}   |" if z else "      |")
        lines.append(row)
    lines.append(hline)
    if venku:
        lines.append(f"⚠️ MIMO VYREZ stoji: " + ", ".join(f"{v}@{sq}" for sq, v in sorted(venku)))
    lines.append("")
    lines.append("D trpaslik (nas, HOME) · W wood-elf (jejich, AWAY) · +role (L/R/B/T/DR/C/W/TR) +ID")
    lines.append("cislo v prazdnem poli = kolik JEJICH tacklezon na nej dosahuje")
    lines.append("/n u D.. = VZDY (i /0) -- v kolika JEJICH zonach stoji")
    lines.append("u W.. cislo NENI (matoucí) · '_' lezi · 'o' drzi mic · '-' uz hral · malymi = stunned")
    lines.append("")
    lines.append("--- hráči (staty) ---")
    for p in sorted(players, key=lambda q: (q.team_side != bb.TeamSide.HOME, q.id)):
        strana = "TRP" if p.team_side == bb.TeamSide.HOME else "ELF"
        acted = "hral" if p.has_acted else "ceka"
        lines.append(
            f"  {p.id:2d} {strana} ({p.position.x:2d},{p.position.y:2d}) "
            f"MA{p.stats.movement} ST{p.stats.strength} AG{p.stats.agility} AV{p.stats.armour} "
            f"{p.state} {acted}"
        )
    return "\n".join(lines)


def action_str(a):
    return f"{a.type} hrac={a.player_id} cil_hrac={a.target_id} cil_pole=({a.target.x},{a.target.y})"


def main():
    state, dice = new_game()
    last_actions = []
    with open(OUT, "w") as f:
        f.write("=== NOVA HRA ZALOZENA (trpaslici HOME vs wood-elf AWAY) ===\n")
        f.write(board(state) + "\n")

    while True:
        if not os.path.exists(CMD):
            time.sleep(0.3)
            continue
        with open(CMD) as f:
            line = f.read().strip()
        os.remove(CMD)
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
            elif cmd == "board":
                result_lines.append(board(state))
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
            elif cmd == "do" and len(parts) == 2:
                idx = int(parts[1])
                a = last_actions[idx]
                r = bb.execute_action(state, a, dice)
                result_lines.append(f"provedeno: {action_str(a)}")
                result_lines.append(f"vysledek: turnover={r.turnover}")
                result_lines.append(board(state))
            else:
                result_lines.append(f"neznamy prikaz: {line}")
        except Exception as e:
            result_lines.append(f"CHYBA: {e}")
        with open(OUT, "w") as f:
            f.write("\n".join(result_lines) + "\n")


if __name__ == "__main__":
    main()
