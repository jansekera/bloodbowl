#!/usr/bin/env python3
"""Rozehraná situace — čitelná deska.

Proti verzi z 11.08. přibylo:
  * mřížka kolem polí
  * v každém VOLNÉM poli číslo = kolik soupeřových tackle zón na něj dosáhne
    (0 se kreslí jako tečka).  Pole s 0 je jediné, kam smí roh klece.
  * u obsazeného pole /N = v kolika soupeřových tackle zónách hráč stojí
    (to je přirážka k dodge, když z pole odchází)
  * značky za jménem: * míč · _ leží · x omráčen · - už hrál v tomhle kole
  * panel s hráči (staty, kdo už hrál), dovednosti a značkování ve vlastních
    sekcích pod ním; barvy v terminálu

Použití:
    diag_play_session_20260812.py [reset|newturn] [ID@x,y|ID@stand ...]
"""
import gzip, json, os, sys

# Barvy jen do terminálu — když výstup jde do roury nebo souboru (čte ho
# i jiný skript), escape sekvence by ho rozbily. NO_COLOR=1 je vypne natvrdo.
COLOR = sys.stdout.isatty() and not os.environ.get("NO_COLOR")
NAS, JEJICH, SLABE, KONEC = "\033[94m", "\033[91m", "\033[90m", "\033[0m"


def obarvi(text, kdo):
    """kdo: True naši (světle modrá) · False jejich (světle červená) · None mřížka."""
    if not COLOR:
        return text
    return {True: NAS, False: JEJICH, None: SLABE}[kdo] + text + KONEC

ST = "evidence/play_session_20260812_state.json"
SEED = "evidence/play_session_20260811_state.json"
REPLAY = "diag_replay_mine_20260811b_data/g0000.json.gz"

# Útočíme směrem k rostoucímu x; naše endzone je vlevo.
ENDZONE_X = 25

STANDING, PRONE, STUNNED = 0, 1, 2


def load_init():
    r = json.load(gzip.open(REPLAY, "rt"))
    t = next(x for x in r["turn_logs"]
             if x["half"] == 1 and x["turn"] == 2 and x["active_team"] == "home")
    P = {}
    for side, us in (("home", True), ("away", False)):
        for p in t[f"{side}_players"]:
            if p["state"] == 3:
                continue
            P[str(p["id"])] = {"x": p["x"], "y": p["y"], "st": p["state"], "us": us,
                               "name": p["name"], "ball": p["has_ball"],
                               "ma": p["ma"], "stt": p["st"], "ag": p["ag"], "av": p["av"],
                               "acted": False, "act": ""}
    return {"players": P, "log": []}


# Kód hráče = RASA + POZICE, každé jedním písmenem (uživatel 13.08.), za ním
# index a pak skilly, taky po jednom písmenu:  DB9G = dwarf Blitzer #9 s Guard.
#
# Rasa je nutná právě proto, aby nebyly kolize: `Lineman`, `Blitzer` i
# `Thrower` má víc ras a bez prefixu se náš Blitzer nedal odlišit od jejich.
# Uvnitř jedné rasy je pozice jednoznačná:
#   D  L Longbeard · B Blitzer · T Troll Slayer · R Runner
#   S  L Lineman · G Gutter Runner · B Blitzer · T Thrower
#   W  L Lineman · W Wardancer · C Catcher · T Thrower · E Treeman
#   H  L Lineman · B Blitzer · T Thrower · C Catcher · O Ogre
#   O  L Lineman · B Blitzer · K Black Orc · T Thrower
RACE_MARK = {"dwarf": "D", "skaven": "S", "wood-elf": "W",
             "human": "H", "orc": "O"}
# Charakteristická jména, podle kterých se rasa týmu pozná (replay ji nevozí)
RACE_TELL = (("Longbeard", "dwarf"), ("Troll Slayer", "dwarf"),
             ("Gutter Runner", "skaven"), ("Wardancer", "wood-elf"),
             ("Treeman", "wood-elf"), ("Black Orc", "orc"), ("Ogre", "human"))

# Strana -> písmeno rasy; ve zrcadlovém zápase se před něj přidá D/H
# (domácí/hosté), takže DDB = domácí dwarf Blitzer, HDB = hostující.
SIDE_RACE = {True: "D", False: "S"}
MIRROR = False


def detect_races(P):
    """Zjistí rasu obou stran a jestli je to zrcadlový zápas."""
    global SIDE_RACE, MIRROR
    out = {}
    for us in (True, False):
        names = {p["name"] for p in P.values() if p["us"] is us}
        race = next((r for tell, r in RACE_TELL
                     if any(n.startswith(tell) for n in names)), None)
        out[us] = RACE_MARK.get(race, "?")
    SIDE_RACE = out
    MIRROR = out[True] == out[False]


def code(p):
    pref = ("D" if p["us"] else "H") if MIRROR else ""
    return pref + SIDE_RACE[p["us"]] + _pos_code(p["name"])


def _pos_code(n):
    return ("R" if n.startswith("Runner") else
            "T" if n.startswith("Troll") or n.startswith("Thrower") else
            "B" if n.startswith("Blitzer") else
            "G" if n.startswith("Gutter") else
            "W" if n.startswith("Wardancer") else
            "C" if n.startswith("Catcher") else
            "E" if n.startswith("Treeman") else
            "O" if n.startswith("Ogre") else
            "K" if n.startswith("Black") else "L")


# Dovednosti se v replayi nevozí — jsou dané pozicí v rosteru (roster.cpp,
# getDwarfRoster1200 / getSkavenRoster1200).  Jméno nese jen ten "+" přídavek,
# takže Block / Tackle / Sure Hands / Frenzy by jinak zmizely, a to jsou přesně
# ty, na kterých tah stojí.  Názvy zůstávají anglicky.
SKILLS = {
    "Longbeard":                 "Block, Tackle, Thick Skull",
    "Longbeard +Guard":          "Block, Tackle, Thick Skull, Guard",
    "Blitzer +Guard+Tackle":     "Block, Tackle, Thick Skull, Guard",
    "Troll Slayer +Guard+Tackle": "Block, Tackle, Thick Skull, Guard, Frenzy!, Dauntless",
    "Runner +Block":             "Block, Sure Hands, Thick Skull",
    "Lineman":                   "—",
    "Lineman +Wrestle":          "Wrestle",
    "Gutter Runner +Sure Feet":  "Dodge, Sure Feet",
    "Blitzer +Guard":            "Block, Guard",
    "Blitzer ball-hunter":       "Block, Strip Ball!, Tackle",
    "Blitzer +Mighty Blow":      "Block, Mighty Blow",
    "Thrower +Block":            "Block, Sure Hands, Pass",
    # wood-elf: Stand Firm a Side Step jsou jediné dvě mimo trpaslíky, které
    # mění, co se stane při odsunu — proto se kreslí na desku
    "Wardancer +Side Step":      "Block, Dodge, Leap, Side Step",
    "Wardancer ball-hunter":     "Block, Dodge, Leap, Strip Ball!",
    "Catcher +Block":            "Catch, Dodge, Block",
    "Treeman +Guard":            "Loner, Take Root, Stand Firm, Mighty Blow, "
                                 "Thick Skull, Guard",
    # orc / human
    "Black Orc +Guard+Block":    "Guard, Block",
    "Ogre +Block":               "Loner, Bone-head, Mighty Blow, Thick Skull, "
                                 "Throw Team-Mate, Block",
}


def skills(p):
    return SKILLS.get(p["name"], p["name"])


# Na desce se ukazují jen dovednosti, které mění ROZHODNUTÍ o pohybu a odsunu
# (uživatel 13.08.). Ostatní (Block, Tackle, Thick Skull…) má skoro každý náš
# hráč a v buňce by jen zabíraly místo — zůstávají v panelu pod deskou.
BOARD_SKILLS = (("Guard", "G"), ("Stand Firm", "F"), ("Side Step", "S"))


def board_skills(p):
    s = skills(p)
    return "".join(mark for name, mark in BOARD_SKILLS if name in s)


def threatens_R1(p):
    """Ohrožuje tenhle soupeř roh klece, aniž by to soupeře stálo blitz?

    Pravidlo uživatele (12.08.): do R1 se počítá
      * STOJÍCÍ — může na roh hodit Block, pohyb ho nestojí nic;
      * LEŽÍCÍ s Jump Up — ten smí Block Action vyhlásit vleže (AG hod +2),
        postaví se zadarmo, a tím soupeři blitz ZŮSTANE.
    Nepočítá se nikdo další — klasický ležící ani omráčený se k nám dostanou
    jedině za cenu blitzu, a ten pak nezbude na nosiče míče.  Právě tahle
    úvaha je ta osa: R1 hlídá údery, které soupeře nestojí blitz.
    """
    return p["st"] == STANDING or (p["st"] == PRONE and "Jump Up" in skills(p))


def tz_map(S):
    """Kolik soupeřových tackle zón dosáhne na které pole — podle R1."""
    tz = {}
    for p in S["players"].values():
        if p["us"] or not threatens_R1(p):
            continue
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if dx or dy:
                    tz[(p["x"] + dx, p["y"] + dy)] = tz.get((p["x"] + dx, p["y"] + dy), 0) + 1
    return tz


def render(S, pad=2, w=None):
    """Výřez se počítá jen z hlavního chumlu.

    Hlídač zaparkovaný ve vlastní endzone jinak roztáhne desku přes celé
    hřiště a hrací prostor se scvrkne na nečitelný proužek — vypíše se pod
    deskou místo toho.
    """
    P = S["players"]
    tz = tz_map(S)
    med = sorted(p["x"] for p in P.values())[len(P) // 2]
    core = [p for p in P.values() if abs(p["x"] - med) <= 9]
    X0, X1 = max(0, min(p["x"] for p in core) - pad), min(25, max(p["x"] for p in core) + pad)
    Y0, Y1 = max(0, min(p["y"] for p in core) - pad), min(14, max(p["y"] for p in core) + pad)
    away = [f"{code(p)}{i} ({p['x']},{p['y']})" for i, p in P.items()
            if not (X0 <= p["x"] <= X1 and Y0 <= p["y"] <= Y1)]

    cells, owner = {}, {}
    for i, p in P.items():
        owner[(p["x"], p["y"])] = p["us"]
        # * míč · _ leží · - už se aktivoval · -B blitz · -P přihrávka
        # OMRÁČENÝ se píše MALÝMI písmeny (uživatel 13.08.) -- na desce se
        # to pozná koutkem oka, kdežto značka `x` splývala s `_` i `-`.
        c = code(p).lower() if p["st"] == STUNNED else code(p)
        mark = "*" if p["ball"] else ("_" if p["st"] == PRONE else "")
        mark += board_skills(p)
        if p["acted"]:
            # Blitz a přihrávka jsou jednou za kolo na CELÝ tým, takže se na
            # desce musí poznat, že jsou spotřebované -- ne jen že se hráč hnul.
            mark += "-" + p.get("act", "")
        n = tz.get((p["x"], p["y"]), 0)
        cells[(p["x"], p["y"])] = f"{c}{i}{mark}" + (f"/{n}" if n and p["us"] else "")

    # šířka buňky se dopočítá z nejdelšího obsahu, ať se nic neořízne
    if w is None:
        w = max([len(v) for v in cells.values()] + [3]) + 2

    def bar(l, m, r):
        return "      " + obarvi(l + m.join("─" * w for _ in range(X1 - X0 + 1)) + r, None)

    out = ["       " + "".join(f"{x:^{w}} " for x in range(X0, X1 + 1)) + "  x",
           bar("┌", "┬", "┐")]
    for y in range(Y0, Y1 + 1):
        row = []
        for x in range(X0, X1 + 1):
            if (x, y) in cells:
                row.append(obarvi(f"{cells[(x, y)]:^{w}}", owner[(x, y)]))
            else:
                n = tz.get((x, y), 0)
                row.append(obarvi(f"{(str(n) if n else '·'):^{w}}", None))
        out.append(f" y={y:>2} " + obarvi("│", None)
                   + obarvi("│", None).join(row) + obarvi("│", None))
        out.append(bar("├", "┼", "┤") if y < Y1 else bar("└", "┴", "┘"))
    if away:
        out.append("  mimo výřez: " + " · ".join(away))
    return "\n".join(out)


def panel(S):
    """Tabulka drží jen to, co se mění tahem.

    Dovednosti jdou dolů do vlastní sekce po TYPECH hráče — jeden dlouhý
    seznam (Nurgle Beast) by jinak roztáhl každý řádek tabulky.
    """
    tz = tz_map(S)
    rows = []
    for us, title in ((True, "MY (trpaslíci) — útočíme k x=25"), (False, "ONI (skaveni)")):
        rows.append("")
        rows.append(title)
        rows.append(f"  {'kdo':<8}{'pole':>8}  {'MA ST AG AV':<13}{'TZ':>3}  stav")
        grp = [(i, p) for i, p in S["players"].items() if p["us"] == us]
        for i, p in sorted(grp, key=lambda kv: (kv[1]["x"], kv[1]["y"])):
            stav = ("LEŽÍ" if p["st"] == PRONE else
                    "OMRÁČEN" if p["st"] == STUNNED else
                    "hrál" if p["acted"] else "")
            ball = "  ⬤ MÍČ" if p["ball"] else ""
            rows.append(f"  {code(p) + i:<8}{f'({p['x']},{p['y']})':>8}  "
                        f"{p['ma']:<4}{p['stt']:<4}{p['ag']:<4}{p['av']:<4} "
                        f"{tz.get((p['x'], p['y']), 0):>2}  {stav:<8}{ball}")
    return "\n".join(rows)


def skill_block(S):
    """Dovednosti podle typu hráče; ! = nutí ruku (povinné / krade míč)."""
    byname = {}
    for i, p in sorted(S["players"].items(), key=lambda kv: int(kv[0])):
        byname.setdefault((p["us"], p["name"]), []).append(code(p) + i)
    out = ["", "DOVEDNOSTI (podle typu — čísla hráčů sedí na desku)"]
    for us in (True, False):
        for (u, name), ids in byname.items():
            if u != us:
                continue
            out.append(f"  {', '.join(ids):<26} {name:<26} {SKILLS.get(name, '?')}")
    return "\n".join(out)


def marking(S):
    """Kdo koho drží — a hlavně: drží se ti, u kterých to k něčemu je.

    Gutter Runner má AG4 + Dodge: z jedné naší zóny do čistého pole odejde
    na 2+ s rerollem, značka na něm skoro nic nestojí.  AG3 bez Dodge odchází
    na 4+ bez rerollu — tam značka drží.  Proto se oddělují.
    """
    P = S["players"]
    ours = [(i, p) for i, p in P.items() if p["us"] and p["st"] == STANDING]
    out = ["", "ZNAČKOVÁNÍ (kdo z našich STOJÍ vedle koho)"]
    for grp, title in ((3, "AG3 — odchod 3+ (s naší zónou na cílovém poli 4+)"),
                       (4, "AG4 — odchod 2+; náš Tackle ruší jejich Dodge včetně rerollu")):
        out.append(f"  {title}")
        for i, e in sorted(P.items(), key=lambda kv: kv[1]["x"]):
            if e["us"] or e["ag"] != grp:
                continue
            held = [code(p) + j for j, p in ours
                    if max(abs(p["x"] - e["x"]), abs(p["y"] - e["y"])) == 1]
            stav = (" LEŽÍ" if e["st"] == PRONE else
                    " OMRÁČEN" if e["st"] == STUNNED else "")
            out.append(f"    {code(e) + i:<7}{f'({e['x']},{e['y']})':<8}{stav:<9}"
                       + (f"drží: {', '.join(held)}" if held else "— VOLNÝ"))
    return "\n".join(out)


def legend():
    return ("\nČTENÍ DESKY\n"
            "  číslo ve volném poli = kolik soupeřových tackle zón na něj dosáhne.\n"
            "  · = nula ⇒ jediná pole, kam smí ROH KLECE.\n"
            "  /N u našeho hráče = v kolika TZ stojí (přirážka k dodge při odchodu).\n"
            "  * drží míč · _ leží · MALÝMI PÍSMENY = omráčen\n"
            "  - už se v tomhle kole aktivoval · -B provedl blitz · -P přihrával\n"
            "  G Guard · F Stand Firm · S Side Step  (ostatní skilly v panelu níž)\n"
            "  kód = RASA + POZICE, každé jedním písmenem, pak index a skilly:\n"
            "    rasa   D dwarf · S skaven · W wood-elf · H human · O orc\n"
            "    pozice L Lineman/Longbeard · B Blitzer · T Troll Slayer/Thrower ·\n"
            "           R Runner · G Gutter Runner · W Wardancer · C Catcher ·\n"
            "           E Treeman · O Ogre · K Black Orc\n"
            "    ⇒ DB9G = dwarf Blitzer #9 s Guard\n"
            "  ve zrcadlovém zápase přibude D/H (domácí/hosté): DDB vs HDB\n"
            "  (ležící ani omráčený nemá tackle zónu)")


def main():
    S = json.load(open(ST)) if os.path.exists(ST) else (
        json.load(open(SEED)) if os.path.exists(SEED) else load_init())
    detect_races(S["players"])
    for arg in sys.argv[1:]:
        if arg == "reset":
            S = load_init()
            continue
        if arg == "newturn":          # nové kolo — všem spadnou pomlčky
            for q in S["players"].values():
                q["acted"] = False
                q["act"] = ""
            S["log"].append("--- nové kolo ---")
            continue
        pid, dest = arg.split("@")
        # Druh akce za dvojtečkou: `3@12,5:B` = přesun a blitz, `3@:P` = jen
        # přihrávka bez pohybu. Blitz i pass jsou jednou za kolo na celý tým,
        # takže se musí poznat, že jsou spotřebované.
        kind = ""
        if ":" in dest:
            dest, kind = dest.split(":", 1)
            kind = kind.upper()
        if dest == "stand":          # postavit se stojí 3 pole, ale NENÍ to hod
            S["players"][pid]["st"] = STANDING
            S["players"][pid]["acted"] = True
            S["players"][pid]["act"] = kind
            S["log"].append(f"{code(S['players'][pid])}{pid} vstal")
            continue
        if dest:
            x, y = map(int, dest.split(","))
            S["players"][pid]["x"], S["players"][pid]["y"] = x, y
        S["players"][pid]["acted"] = True
        S["players"][pid]["act"] = kind
        akce = {"B": " BLITZ", "P": " PŘIHRÁVKA"}.get(kind, "")
        S["log"].append(f"{code(S['players'][pid])}{pid}"
                        + (f" -> ({dest})" if dest else "") + akce)
    json.dump(S, open(ST, "w"))
    print(render(S))
    print(panel(S))
    print(skill_block(S))
    print(marking(S))
    print(legend())
    if S["log"]:
        print("\nprovedeno: " + " · ".join(S["log"]))


if __name__ == "__main__":
    main()
