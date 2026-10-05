"""P146 (05.10.2026): „AI plánuje tah“ + „kontrola tahu“ pro živou partii i pro zkoušku na dohrané partii.

Dvě věci:
  plan_tahu(state, ...)        -- jak by AI odehrála celý tah týmu na tahu (nanečisto, vlastní kostky,
                                  vazba bb.ai_plan_turn); vrací kroky a stav po tahu
  kontrola_tahu(start, konec, strana, kroky)
                               -- co po tahu zůstalo nebezpečného nebo nevyužitého; řádky textu

Kontrola hlídá doktríny z partie 02.-03.10. (kniha P131, P137, P138, P142):
  1. kdo ze soupeřů dosáhne v příštím tahu na nosiče (blitz: MA + 2 GFI, blok stojí pole,
     vstávání 3 pole)
  2. kdo stojí na kraji (y=0/14) -- a o pole vedle, když má soupeř hráče s Frenzy
  3. kdo zůstal v kontaktu bez převahy (soupeř by ho blokoval aspoň 1:1)
  4. nevyužitý blitz / faul, který byl k dispozici
  5. ležící hráč, který se nepokusil vstát

⚠️ Zjednodušení (vědomá): cesta k nosiči ignoruje Leap, Shadowing, Diving Tackle, Sprint, Sure
Feet, Jump Up (vstává zdarma) a Rooted (bez GFI); hody na uhýbání jen spočítá; pozice bere jako
pevné (soupeř si cestu může napřed uvolnit blokem). Asistence znají jen Guard; síla bloku nebere
Dauntless, Horns ani Foul Appearance. Kontrola je měřidlo pro nálezy, ne pravidla hry.
Druhá implementace dosahu vedle enginu (pathfinder canReachAdjacentTo): druhé review 05.10.
porovnalo nabídku BLITZ enginu s tímto hledáním na 8868 náhodných pozicích — 0 neshod
(pozitivní kontrola: rozpočet +1 dal 617 neshod).
"""
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "engine", "build"))
import bb_engine as bb  # noqa: E402

SIRKA, VYSKA = 26, 15


def _skill_names():
    # pořadí z engine/include/bb/enums.h (enum class SkillName) — jediný zdroj
    txt = open(os.path.join(os.path.dirname(__file__), "engine", "include", "bb", "enums.h")).read()
    body = txt[txt.index("enum class SkillName"):]
    body = body[body.index("{") + 1: body.index("};")]
    body = re.sub(r"//[^\n]*", "", body)
    return [n.split("=")[0].strip() for n in body.split(",") if n.strip()]


SKILL_NAMES = [n for n in _skill_names() if n != "SKILL_COUNT"]   # SKILL_COUNT je jen hranice výčtu
SKILL = {n: i for i, n in enumerate(SKILL_NAMES)}


def ma_skill(p, jmeno):
    return p.has_skill_index(SKILL[jmeno])


def hraci(state):
    out = []
    for pid in range(0, 40):
        try:
            p = state.get_player(pid)
        except IndexError:
            continue
        if p.is_on_pitch():
            out.append(p)
    return out


def _xy(p):
    return (p.position.x, p.position.y)


def _vedle(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1])) == 1


def _stoji(p):
    return p.state == bb.PlayerState.STANDING


def jmeno(p):
    return f"{'H' if p.team_side == bb.TeamSide.HOME else 'A'}{p.id}@{_xy(p)}"


def plan_tahu(state, ai="macro_mcts", seed=1, weights_path="weights_best.json", mcts_iterations=50):
    """Tah AI nanečisto. Vrací (kroky, stav_po_tahu, dokončeno); originál, kostky partie ani
    nachystané volby kouče se nemění."""
    if weights_path and not os.path.isabs(weights_path):
        weights_path = os.path.join(os.path.dirname(__file__), weights_path)
    return bb.ai_plan_turn(state, ai=ai, seed=seed, weights_path=weights_path,
                           mcts_iterations=mcts_iterations)


def _dosah_na_nosice(utocnik, nosic, nasi_stojici, obsazeno):
    """Dosáhne útočník blitzem na nosiče? None, nebo slovník: „nejkratsi“ = (kroky, gfi, uhýbání)
    nejkratší cesty; „bez_uhybani“ = táž trojice pro cestu s nejméně uhýbáními, když je jiná
    (delší); „hod_na_vstani“ = ležící s MA < 3.

    Pravidla: blitz = pohyb do MA a jeden blok, který „stojí“ jedno pole (ř. 347-350); navíc 2× GFI
    (ř. 1694-1700). Vstávání stojí 3 pole; pod 3 MA vstává na 4+ a dál jen přes GFI (ř. 690-695).
    Uhýbání = odchod z pole v tackle zóně stojících hráčů strany nosiče (ř. 480-486; ležící
    zóny nemají, ř. 478-479). O dosahu rozhoduje počet kroků; každá vypsaná trojice popisuje
    jednu skutečnou cestu (review 05.10. S1: dřív se slepovaly kroky jedné a uhýbání jiné).
    """
    ma = utocnik.stats.movement
    lezi = utocnik.state == bb.PlayerState.PRONE
    po_vstani = (ma - 3 if ma >= 3 else 0) if lezi else ma     # normální pohyb po vstání
    hod_na_vstani = lezi and ma < 3
    start = _xy(utocnik)
    cil = _xy(nosic)
    if _vedle(start, cil):
        gfi = max(0, 1 - po_vstani)       # i blok vedle stojícího nosiče stojí pole (ř. 347-350)
        return {"nejkratsi": (0, gfi, 0), "bez_uhybani": None, "hod_na_vstani": hod_na_vstani}
    rozpocet = po_vstani + 2 - 1          # + 2× GFI, − pole za blok (rozpočet je vždy ≥ 1)
    v_zone = lambda sq: any(_vedle(sq, q) for q in nasi_stojici)   # noqa: E731
    # vrstvy podle počtu kroků; ve vrstvě nejméně uhýbání na každém poli
    vrstva = {start: 0}
    na_cili = {}                          # kroky -> nejméně uhýbání při právě tolika krocích
    for kroky in range(1, rozpocet + 1):
        dalsi = {}
        for sq, uh in vrstva.items():
            uh2 = uh + (1 if v_zone(sq) else 0)
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    if dx == dy == 0:
                        continue
                    n = (sq[0] + dx, sq[1] + dy)
                    if not (0 <= n[0] < SIRKA and 0 <= n[1] < VYSKA) or n in obsazeno:
                        continue
                    if n not in dalsi or uh2 < dalsi[n]:
                        dalsi[n] = uh2
        vrstva = dalsi
        for sq, uh in vrstva.items():
            if _vedle(sq, cil) and (kroky not in na_cili or uh < na_cili[kroky]):
                na_cili[kroky] = uh
    if not na_cili:
        return None

    def cesta(kroky):
        return (kroky, max(0, kroky + 1 - po_vstani), na_cili[kroky])   # +1 = pole za blok
    k_min = min(na_cili)
    k_bez = min(na_cili, key=lambda k: (na_cili[k], k))   # nejméně uhýbání, z nich nejkratší
    bez = cesta(k_bez) if na_cili[k_bez] < na_cili[k_min] else None
    return {"nejkratsi": cesta(k_min), "bez_uhybani": bez, "hod_na_vstani": hod_na_vstani}


def _sila_bloku(utocnik, obrance, utocnici_tym, obranci_tym):
    """(síla útočníka, síla obránce) včetně asistencí (ř. 1660-1668: vedle protivníka, mimo zónu
    jiného hráče soupeře, stojí); Guard tu druhou podmínku ruší (ř. 8158-8160)."""
    def asistence(pomocnici, cil, nepratele, vynechat):
        n = 0
        for q in pomocnici:
            if q.id == vynechat.id or not _stoji(q) or not _vedle(_xy(q), _xy(cil)):
                continue
            jine_zony = [r for r in nepratele if r.id != cil.id and _stoji(r) and _vedle(_xy(r), _xy(q))]
            if not jine_zony or ma_skill(q, "Guard"):
                n += 1
        return n
    a = utocnik.stats.strength + asistence(utocnici_tym, obrance, obranci_tym, utocnik)
    d = obrance.stats.strength + asistence(obranci_tym, utocnik, utocnici_tym, obrance)
    return a, d


def _kostky(a, d):
    if a > 2 * d:
        return "3 kostky pro soupeře"
    if a > d:
        return "2 kostky pro soupeře"
    return "1 kostka (síly rovné)"


def kontrola_tahu(start, konec, strana, kroky=None):
    """Řádky nálezů po tahu strany `strana` (bb.TeamSide). `start` = stav na začátku tahu."""
    out = []
    vsichni = hraci(konec)
    nasi = [p for p in vsichni if p.team_side == strana]
    jejich = [p for p in vsichni if p.team_side != strana]
    nasi_stojici = [_xy(p) for p in nasi if _stoji(p)]
    obsazeno = {_xy(p) for p in vsichni if p.state in (bb.PlayerState.STANDING, bb.PlayerState.PRONE,
                                                      bb.PlayerState.STUNNED)}

    # 1. nosič
    if konec.ball.is_held:
        nosic = konec.get_player(konec.ball.carrier_id)
        if nosic.team_side == strana:
            dosah = []
            for o in jejich:
                if o.state not in (bb.PlayerState.STANDING, bb.PlayerState.PRONE):
                    continue
                r = _dosah_na_nosice(o, nosic, nasi_stojici, obsazeno)
                if r is not None:
                    dosah.append((r, o))
            # nejnebezpečnější napřed: nejméně uhýbání, pak GFI, pak kroky (nejlepší z obou cest)
            def klic(t):
                r = t[0]
                c = r["bez_uhybani"] or r["nejkratsi"]
                return (c[2], c[1], c[0])
            dosah.sort(key=klic)
            if dosah:
                out.append(f"⚠️ NOSIČ {jmeno(nosic)}: v příštím tahu na něj dosáhne {len(dosah)} soupeřů "
                           f"(blitz má soupeř jen jeden):")
                for r, o in dosah:
                    lezi = (", vstává na 4+" if r["hod_na_vstani"] else ", vstává") \
                        if o.state == bb.PlayerState.PRONE else ""
                    kr, gfi, uh = r["nejkratsi"]
                    radek = f"     {jmeno(o)}: {kr} kroků, {gfi}× GFI, {uh}× uhýbání{lezi}"
                    if r["bez_uhybani"]:
                        kr2, gfi2, uh2 = r["bez_uhybani"]
                        radek += f"; nebo {kr2} kroků, {gfi2}× GFI, {uh2}× uhýbání"
                    out.append(radek)
            else:
                out.append(f"✅ NOSIČ {jmeno(nosic)}: žádný soupeř na něj v příštím tahu nedosáhne")

    # 2. kraj
    # Frenzy: druhý blok po Pushed (ř. 8134-8145) ⇒ i pole od kraje je kraj; jen ti, kdo příště hrají
    frenzy = [o for o in jejich if ma_skill(o, "Frenzy")
              and o.state in (bb.PlayerState.STANDING, bb.PlayerState.PRONE)]
    for p in nasi:
        if not _stoji(p):
            continue
        y = p.position.y
        if y in (0, VYSKA - 1):
            out.append(f"⚠️ KRAJ {jmeno(p)} stojí na kraji hřiště (Pushed = dav)")
        elif y in (1, VYSKA - 2) and frenzy:
            out.append(f"⚠️ KRAJ {jmeno(p)} o pole od kraje, soupeř má Frenzy "
                       f"({', '.join(jmeno(o) for o in frenzy)})")

    # 3. kontakt bez převahy
    for p in nasi:
        if not _stoji(p):
            continue
        for o in jejich:
            if not _stoji(o) or not _vedle(_xy(p), _xy(o)):
                continue
            a, d = _sila_bloku(o, p, jejich, nasi)
            if a >= d:
                out.append(f"⚠️ KONTAKT {jmeno(p)} vedle {jmeno(o)}: soupeř {a} proti {d} ⇒ {_kostky(a, d)}")

    # 4. + 5. potřebují kroky tahu
    if kroky is not None:
        hrali = {k["action"].player_id for k in kroky}
        # příslušnost ke straně ze STARTU: blitzer/faulující mohl skončit mimo hřiště (vyloučení, KO)
        nasi_start = {p.id for p in hraci(start) if p.team_side == strana}
        typy = {k["action"].type for k in kroky if k["action"].player_id in nasi_start}
        nabidka = {a.type for a in bb.get_available_actions(start)}
        if bb.ActionType.BLITZ in nabidka and bb.ActionType.BLITZ not in typy:
            out.append("ℹ️ BLITZ nevyužit, i když byl k dispozici")
        if bb.ActionType.FOUL in nabidka and bb.ActionType.FOUL not in typy:
            out.append("ℹ️ FAUL nevyužit, i když byl k dispozici")
        for p0 in hraci(start):
            if p0.team_side == strana and p0.state == bb.PlayerState.PRONE and p0.id not in hrali:
                p1 = konec.get_player(p0.id)
                if p1.state == bb.PlayerState.PRONE:
                    out.append(f"⚠️ LEŽÍ {jmeno(p1)}: ani se nepokusil vstát")

    if not out:
        out.append("✅ kontrola tahu: nic k hlášení")
    return out


def popis_kroku(krok):
    a = krok["action"]
    cil = f" cíl hráč {a.target_id}" if a.target_id and a.target_id > 0 else ""
    return (f"{a.type.name} hráč {a.player_id} → ({a.target.x},{a.target.y}){cil}"
            + ("  ⛔ TURNOVER" if krok["turnover"] else ""))
