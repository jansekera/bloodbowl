# P233 na konkrétních situacích (29.10.2026, uživatel: „nachystat konkrétní situace, kde předpokládáš odlišné chování
# od předchozího commitu, a porovnat s předchozím commitem“). Jedna situace × jedna knihovna × jedno semínko na běh:
#   python3.8 situace_p233.py KOREN SITUACE SEMINKO VYSTUP.json
# KOREN = zmrazená knihovna (P233 = ./zmrazeno_p233, commit 8fa3c1db; předchozí = ../p230_20261027/zmrazeno_p232,
# commit 407527c6). Jen náš tah + odpověď soupeře (nic dál).
# Naši: základní nemrtví = zombíci MA 4, ST 3, bez Block (pomalá klec). Soupeř: základní chaos, beastmeni MA 6, ST 3,
# bez Block (rychlejší zeď). Geometrie jako testy OneCageWallClear: klec kolem (12,7), zeď na x = 14.
import sys, os, json
KOREN, SIT, SEED, VYST = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
D = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, KOREN)
sys.path.insert(0, os.path.join(D, "..", "p232_20261028"))
import bb_kontrola_tahu as kt
import mereni as m232
bb = kt.bb
H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
assert os.path.realpath(bb.__file__).startswith(os.path.realpath(KOREN)), bb.__file__

POMOCNICI = {6: (10, 3), 7: (10, 11), 8: (9, 7)}
SITUACE = {
    # příklad uživatele: zeď pěti, tři volní pomocníci
    "S1_zed5_pomocnici3": dict(zed=range(5, 10), pomocnici=POMOCNICI),
    # zeď čtyř (nesouměrná): krajní hráči (14,5) a (14,8)
    "S2_zed4_pomocnici3": dict(zed=range(5, 9), pomocnici=POMOCNICI),
    # jen jeden pomocník: na 2 kostky to nevyjde ⇒ čekám STEJNÉ chování (kontrola)
    "S3_zed5_pomocnik1": dict(zed=range(5, 10), pomocnici={8: (9, 7)}),
    # řetězové odtlačení (review F6): za zdí leží soupeři na x = 15, řádky 4–10 ⇒ všechna pole odtlačení plná
    #   ⇒ P233 nebije ⇒ čekám STEJNÉ chování jako předtím
    "S4_retez_vsude": dict(zed=range(5, 10), pomocnici=POMOCNICI, lezi=[(15, y) for y in range(4, 11)]),
    # příklad z review F6: jediný ležící soupeř na (15,6) — blitz z (14,4) na (14,5) by neměl kam odtlačit
    #   (13,6) náš roh, (14,6) zeď, (15,6) ležící ⇒ tahle rána se nehraje, jiná ano
    "S5_retez_f6": dict(zed=range(5, 10), pomocnici=POMOCNICI, lezi=[(15, 6)]),
}


def pozice(sit):
    s = bb.GameState()
    bb.setup_half(s, bb.get_roster("undead"), bb.get_roster("chaos"), A)
    for pid in range(0, 40):
        try: p = s.get_player(pid)
        except IndexError: continue
        p.position = bb.Position(-1, -1); p.state = bb.PlayerState.OFF_PITCH

    def dej(side, pid, x, y, stav=bb.PlayerState.STANDING):
        p = next(q for q in (s.get_player(i) for i in range(0, 40) if _je(s, i)) if q.team_side == side and q.id == pid)
        p.position = bb.Position(x, y); p.state = stav
        p.movement_remaining = p.stats.movement; p.has_moved = False; p.has_acted = False
        return p
    for pid, (x, y) in {1: (12, 7), 2: (11, 6), 3: (11, 8), 4: (13, 6), 5: (13, 8)}.items(): dej(H, pid, x, y)
    for pid, (x, y) in sit["pomocnici"].items(): dej(H, pid, x, y)
    sou = [q.id for q in kt_vsichni(s) if q.team_side == A]
    sou.sort(key=lambda i: (-s.get_player(i).stats.movement, i))      # beastmeni (MA 6) do zdi
    for pid, y in zip(sou, sit["zed"]): dej(A, pid, 14, y)
    zbytek = sou[len(sit["zed"]):]
    for pid, (x, y) in zip(zbytek, sit.get("lezi", [])): dej(A, pid, x, y, bb.PlayerState.PRONE)
    zbytek = zbytek[len(sit.get("lezi", [])):]
    for pid, (x, y) in zip(zbytek, [(20, 3), (20, 7), (20, 11), (22, 5), (22, 9), (23, 7)]): dej(A, pid, x, y)
    nosic = s.get_player(1) if s.get_player(1).team_side == H else None
    s.ball.position = bb.Position(12, 7); s.ball.is_held = True; s.ball.carrier_id = _id_at(s, 12, 7)
    s.kicking_team = A; s.active_team = H; s.phase = bb.GamePhase.PLAY; s.half = 1
    s.home_team.turn_number = 3; s.away_team.turn_number = 2
    return s


def _je(s, i):
    try: s.get_player(i); return True
    except IndexError: return False


def kt_vsichni(s):
    return [s.get_player(i) for i in range(0, 40) if _je(s, i)]


def _id_at(s, x, y):
    return next(q.id for q in kt_vsichni(s) if q.position.x == x and q.position.y == y)


def deska(s, xlo=7, xhi=21):
    """Deska podle zvyklosti: UZ = náš zombík (/n = v kolika zónách soupeře stojí), CB = beastman soupeře,
    o = míč, _ = leží; prázdné pole = počet zón soupeře na něm."""
    tela = {kt._xy(q): q for q in kt.hraci(s) if q.is_on_pitch()}
    zony = [kt._xy(q) for q in kt.hraci(s) if q.team_side == A and kt._ma_zony(q)]
    nz = lambda xy: sum(1 for z in zony if kt._vedle(xy, z))
    rad = ["     " + "".join(f"{x:^7}" for x in range(xlo, xhi + 1))]
    for y in range(1, 14):
        r = f"{y:>3}  "
        for x in range(xlo, xhi + 1):
            q = tela.get((x, y))
            if q is None: c = str(nz((x, y))) if nz((x, y)) else "."
            elif q.team_side == H:
                c = "UZ" + str(q.id) + ("o" if s.ball.is_held and s.ball.carrier_id == q.id else "") + \
                    ("_" if q.state != bb.PlayerState.STANDING else "") + "/" + str(nz((x, y)))
            else: c = "CB" + str(q.id) + ("_" if q.state != bb.PlayerState.STANDING else "")
            r += f"{c:^7}"
        rad.append(r)
    return "\n".join(rad)


s = pozice(SITUACE[SIT])
kroky, po, _ = kt.plan_tahu(s, ai="macro_mcts", seed=SEED)
k2 = po2 = None
if po.phase == bb.GamePhase.PLAY and po.active_team == A:
    k2, po2, _ = kt.plan_tahu(po, ai="macro_mcts", seed=SEED + 1)
x = m232.rozbor(kt, bb, s, kroky, po, k2, po2, H, A)
zed_po = sum(1 for q in kt.hraci(po) if q.team_side == A and kt._ma_zony(q) and kt._xy(q)[0] == 14)
rany = []
for k in kroky:
    a = k["action"]
    if a.type.name in ("BLOCK", "BLITZ"):
        t = s.get_player(a.target_id)
        rany.append(f"{a.type.name} {a.player_id}→{a.target_id}@({t.position.x},{t.position.y})"
                    + (" TURNOVER" if k["turnover"] else ""))
c1 = m232.nosic_nas(po, H)
json.dump(dict(sit=SIT, koren=os.path.basename(KOREN), seed=SEED, rany=rany, zed_po=zed_po,
               nosic_po=list(kt._xy(c1)) if c1 else None, kroky=[kt.popis_kroku(k) for k in kroky],
               deska_pred=deska(s), deska_po=deska(po), **x), open(VYST, "w"), ensure_ascii=False, indent=1)
