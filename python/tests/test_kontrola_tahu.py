"""P146: nástroj „kontrola tahu“ a vazba ai_plan_turn.

Každý nález má test na pozici, kde být MUSÍ, a na pozici, kde být NESMÍ (pozitivní kontrola):
měřidlo, které hlásí vždy, nebo nikdy, by jinak prošlo. Testy citují řádky rules_bb2016.txt.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
import bb_kontrola_tahu as kt  # noqa: E402

bb = kt.bb
H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY


def prazdne_hriste(na_tahu=H):
    """Trpaslíci (HOME) × wood-elf (AWAY), všichni hráči mimo hřiště; míč volný mimo dění."""
    s = bb.GameState()
    bb.setup_half(s, bb.get_developed_roster("dwarf", 1200), bb.get_developed_roster("wood-elf", 1200), A)
    for p in kt.hraci(s):
        p.state = bb.PlayerState.OFF_PITCH
        p.position = bb.Position(-1, -1)
    s.ball.is_held = False
    s.ball.position = bb.Position(13, 7)
    s.active_team = na_tahu
    s.phase = bb.GamePhase.PLAY
    return s


def vsichni(s):
    for pid in range(0, 40):
        try:
            yield s.get_player(pid)
        except IndexError:
            continue


def postav(s, strana, x, y, stav=None, podminka=lambda p: True):
    """Vezme dalšího hráče strany z lavičky (splňujícího podmínku) a postaví ho na (x, y)."""
    for p in vsichni(s):
        if p.team_side == strana and p.state == bb.PlayerState.OFF_PITCH and podminka(p):
            p.state = stav or bb.PlayerState.STANDING
            p.position = bb.Position(x, y)
            return p
    raise AssertionError("došli hráči")


def dej_mic(s, p):
    s.ball.is_held = True
    s.ball.carrier_id = p.id
    s.ball.position = p.position


def nalezy(s, druh, strana=H, kroky=None):
    """Řádky nálezu daného druhu i s odsazenými podřádky (u nosiče seznam soupeřů)."""
    out, uvnitr = [], False
    for r in kt.kontrola_tahu(s, s, strana, kroky):
        if r.startswith("     "):
            if uvnitr:
                out.append(r)
            continue
        uvnitr = druh in r
        if uvnitr:
            out.append(r)
    return out


def radek_hrace(radky, p):
    return [r for r in radky if f"A{p.id}@" in r or f"H{p.id}@" in r]


# --- 1. nosič: blitz = MA + 2 GFI, blok stojí pole (ř. 347-350, 1694-1700) ---------------------

def test_nosic_hranice_dosahu_blitzu():
    # elf MA7: rozpočet 7 + 2 − 1 = 8 kroků na pole vedle nosiče
    s = prazdne_hriste()
    dej_mic(s, postav(s, H, 5, 7))
    elf = postav(s, A, 14, 7)                   # na (6,7) je to 8 kroků ⇒ dosáhne
    r = radek_hrace(nalezy(s, "NOSIČ"), elf)
    assert r and "8 kroků, 2× GFI" in r[0]      # 8 kroků + pole za blok = 9 = MA + 2
    s2 = prazdne_hriste()
    dej_mic(s2, postav(s2, H, 5, 7))
    postav(s2, A, 15, 7)                        # 9 kroků + blok = 10 > 9 ⇒ nedosáhne
    assert "žádný soupeř" in nalezy(s2, "NOSIČ")[0]


def test_nosic_vstavani_stoji_tri_pole():
    # ležící elf MA7: 7 − 3 (vstávání, ř. 690-692) + 2 GFI − 1 blok = 5 kroků
    s = prazdne_hriste()
    dej_mic(s, postav(s, H, 3, 7))
    elf = postav(s, A, 9, 7, bb.PlayerState.PRONE)    # na (4,7) 5 kroků ⇒ dosáhne
    r = radek_hrace(nalezy(s, "NOSIČ"), elf)
    assert r and "5 kroků" in r[0] and "vstává" in r[0]
    s2 = prazdne_hriste()
    dej_mic(s2, postav(s2, H, 3, 7))
    postav(s2, A, 10, 7, bb.PlayerState.PRONE)        # 6 kroků ⇒ nedosáhne
    assert "žádný soupeř" in nalezy(s2, "NOSIČ")[0]


def test_nosic_kratsi_cesta_s_uhybanim_neni_zahozena():
    # pozice ze sondy review (V3): jediná cesta v rozpočtu (8 kroků) vede přes 3 uhýbání; algoritmus,
    # který si na poli drží jen cestu s nejméně uhýbáními, ji zahodil a hlásil „nikdo nedosáhne“
    s = prazdne_hriste()
    dej_mic(s, postav(s, H, 9, 9))
    for xy in [(7, 0), (8, 11), (10, 3), (10, 6), (11, 6), (12, 4), (19, 4), (20, 12), (21, 10), (21, 12)]:
        postav(s, H, *xy)
    elf = postav(s, A, 18, 0)                   # MA7 ⇒ rozpočet 8
    r = radek_hrace(nalezy(s, "NOSIČ"), elf)
    assert r and "8 kroků" in r[0], nalezy(s, "NOSIČ")


def test_nosic_uhybani_nejmene_mozne():
    # elf v zóně tří trpaslíků: odchod = 1 uhýbání; objížďka bez dalšího existuje (ř. 480-486)
    s = prazdne_hriste()
    dej_mic(s, postav(s, H, 5, 7))
    elf = postav(s, A, 8, 7)
    for y in (6, 7, 8):
        postav(s, H, 7, y)
    r = radek_hrace(nalezy(s, "NOSIČ"), elf)
    assert r and "1× uhýbání" in r[0], r
    # bez trpaslíků kolem: 0 uhýbání
    s2 = prazdne_hriste()
    dej_mic(s2, postav(s2, H, 5, 7))
    elf2 = postav(s2, A, 8, 7)
    r2 = radek_hrace(nalezy(s2, "NOSIČ"), elf2)
    assert r2 and "0× uhýbání" in r2[0], r2


def test_nosic_zed_nejde_projit_a_lezici_zonu_nema():
    # obsazená pole (stojící i ležící) se neprocházejí; ležící hráč tackle zónu nemá (ř. 478-479)
    s = prazdne_hriste()
    dej_mic(s, postav(s, H, 1, 7))
    postav(s, A, 6, 7)                          # volné pole vedle nosiče by měl na 4 kroky
    for i, xy in enumerate([(0, 6), (0, 8), (1, 6), (1, 8), (2, 6), (2, 7), (2, 8), (0, 7)]):
        postav(s, H, *xy, bb.PlayerState.PRONE if i % 2 else None)   # všechna pole vedle obsazená
    assert "žádný soupeř" in nalezy(s, "NOSIČ")[0], nalezy(s, "NOSIČ")
    s2 = prazdne_hriste()
    dej_mic(s2, postav(s2, H, 5, 7))
    elf2 = postav(s2, A, 8, 7)
    for y in (6, 7, 8):
        postav(s2, H, 7, y, bb.PlayerState.PRONE)   # ležící vedle cesty ⇒ žádné uhýbání
    r = radek_hrace(nalezy(s2, "NOSIČ"), elf2)
    assert r and "0× uhýbání" in r[0] and ";" not in r[0], r


def test_nosic_radek_popisuje_skutecne_cesty():
    # pozice ze druhého review (S1): nejkratší cesta má 3 uhýbání, objížďka bez uhýbání je delší
    s = prazdne_hriste()
    dej_mic(s, postav(s, H, 8, 7))
    for xy in [(5, 4), (8, 4), (10, 4), (11, 4), (13, 11)]:
        postav(s, H, *xy)
    elf = postav(s, A, 11, 2)
    r = radek_hrace(nalezy(s, "NOSIČ"), elf)
    assert r and "4 kroků, 0× GFI, 3× uhýbání" in r[0].split(";")[0], r   # 4 kroky + blok ≤ MA7
    assert "; nebo " in r[0] and "0× uhýbání" in r[0].split(";")[1], r


def test_nosic_lezici_vedle_nosice_potrebuje_gfi_na_blok():
    # ležící MA≤3 vedle nosiče: po vstání 0 pohybu, blok stojí pole ⇒ 1× GFI (ř. 347-350, 690-695)
    s = prazdne_hriste()
    dej_mic(s, postav(s, H, 5, 7))
    strom = postav(s, A, 6, 7, bb.PlayerState.PRONE, podminka=lambda p: p.stats.movement <= 3)
    r = radek_hrace(nalezy(s, "NOSIČ"), strom)
    assert r and "0 kroků, 1× GFI" in r[0], r


# --- 2. kraj ------------------------------------------------------------------------------------

def test_kraj_obe_krajni_rady_hlaseny_stred_ne():
    s = prazdne_hriste()
    p0 = postav(s, H, 10, 0)
    p14 = postav(s, H, 12, 14)
    q = postav(s, H, 10, 7)
    radky = nalezy(s, "KRAJ")
    assert radek_hrace(radky, p0) and radek_hrace(radky, p14)
    assert not radek_hrace(radky, q)


def test_pole_od_kraje_jen_proti_frenzy():
    s = prazdne_hriste()
    postav(s, H, 10, 1)
    postav(s, A, 20, 7)                         # elfové Frenzy nemají
    assert nalezy(s, "KRAJ") == []
    s2 = prazdne_hriste(na_tahu=A)
    elf = postav(s2, A, 10, 13)
    postav(s2, H, 20, 7, podminka=lambda p: kt.ma_skill(p, "Frenzy"))   # Troll Slayer
    radky = nalezy(s2, "KRAJ", strana=A)
    assert radek_hrace(radky, elf) and "Frenzy" in radek_hrace(radky, elf)[0]
    # omráčený Troll Slayer v příštím tahu nehraje ⇒ nehlásit (review N6)
    s3 = prazdne_hriste(na_tahu=A)
    postav(s3, A, 10, 13)
    postav(s3, H, 20, 7, bb.PlayerState.STUNNED, podminka=lambda p: kt.ma_skill(p, "Frenzy"))
    assert nalezy(s3, "KRAJ", strana=A) == []


# --- 3. kontakt: síla + asistence (ř. 1660-1668), Guard (ř. 8158-8160) --------------------------

def test_kontakt_rovna_sila_hlasen_prevaha_ne():
    s = prazdne_hriste()
    postav(s, H, 10, 7)                         # ST3
    postav(s, A, 11, 7)                         # elf ST3 ⇒ 3 proti 3 = hlásit
    assert len(nalezy(s, "KONTAKT")) == 1
    postav(s, H, 12, 6)                         # dvě asistence trpaslíkovi u elfa ⇒ 3 proti 5
    postav(s, H, 12, 8)
    assert nalezy(s, "KONTAKT") == []


def test_kontakt_dve_kostky_pro_soupere():
    s = prazdne_hriste()
    postav(s, H, 10, 7)
    postav(s, A, 11, 7)
    postav(s, A, 9, 6)                          # asistence elfovi, sám mimo jinou trpasličí zónu
    radek = nalezy(s, "KONTAKT")[0]
    assert "4 proti 3" in radek and "2 kostky" in radek


def test_asistence_v_cizi_zone_neplati_s_guard_ano():
    # strana = elfové, útočí trpaslík; pomocník trpaslíků stojí v zóně druhého elfa
    def pozice(guard):
        s = prazdne_hriste(na_tahu=A)
        postav(s, A, 10, 7)                     # obránce (elf ST3)
        postav(s, H, 11, 7, podminka=lambda p: p.stats.strength == 3 and not kt.ma_skill(p, "Guard"))
        postav(s, H, 9, 6, podminka=lambda p: p.stats.strength == 3 and kt.ma_skill(p, "Guard") == guard)
        postav(s, A, 8, 5)                      # druhý elf: pomocník je v jeho zóně
        return s
    bez = nalezy(pozice(guard=False), "KONTAKT", strana=A)
    assert any("3 proti 3" in r for r in bez), bez       # asistence se nepočítá (ř. 1666-1667)
    s_guard = nalezy(pozice(guard=True), "KONTAKT", strana=A)
    assert any("4 proti 3" in r for r in s_guard), s_guard   # Guard asistuje i v zóně


# --- 4. nevyužitý blitz / faul ------------------------------------------------------------------

def pozice_s_faulem():
    s = prazdne_hriste()
    trp = postav(s, H, 10, 7)
    postav(s, A, 11, 7, bb.PlayerState.PRONE)   # ležící elf vedle ⇒ faul je v nabídce
    return s, trp


def test_faul_nabidnuty_a_nevyuzity_hlasen():
    s, _ = pozice_s_faulem()
    assert bb.ActionType.FOUL in {a.type for a in bb.get_available_actions(s)}
    assert nalezy(s, "FAUL", kroky=[])


def test_faul_zahrany_nehlasen_i_kdyz_faulujici_skoncil_mimo_hriste():
    s, trp = pozice_s_faulem()
    faul = next(a for a in bb.get_available_actions(s) if a.type == bb.ActionType.FOUL)
    konec = s.clone()
    konec.get_player(trp.id).state = bb.PlayerState.EJECTED      # vyloučen za dublet (review S1)
    konec.get_player(trp.id).position = bb.Position(-1, -1)
    radky = [r for r in kt.kontrola_tahu(s, konec, H, [{"action": faul, "turnover": True}]) if "FAUL" in r]
    assert radky == []


def test_blitz_nabidnuty_a_nevyuzity_hlasen_zahrany_ne():
    s = prazdne_hriste()
    postav(s, H, 10, 7)
    postav(s, A, 13, 7)
    blitz = [a for a in bb.get_available_actions(s) if a.type == bb.ActionType.BLITZ]
    assert blitz, "pozice musí blitz nabízet"
    assert nalezy(s, "BLITZ", kroky=[])
    assert nalezy(s, "BLITZ", kroky=[{"action": blitz[0], "turnover": False}]) == []


# --- 5. ležící ----------------------------------------------------------------------------------

def test_lezici_bez_pokusu_vstat_hlasen_kdo_hral_ne():
    s = prazdne_hriste()
    p = postav(s, H, 10, 7, bb.PlayerState.PRONE)
    assert radek_hrace(nalezy(s, "LEŽÍ", kroky=[]), p)
    tah = bb.Action()
    tah.type = bb.ActionType.MOVE
    tah.player_id = p.id
    assert nalezy(s, "LEŽÍ", kroky=[{"action": tah, "turnover": True}]) == []   # pokusil se vstát


# --- vazba ai_plan_turn -------------------------------------------------------------------------

def nova_partie(semínko=7):
    s = bb.GameState()
    bb.setup_half(s, bb.get_developed_roster("dwarf", 1200), bb.get_developed_roster("wood-elf", 1200), A)
    kostky = bb.DiceRoller(semínko)
    bb.simple_kickoff(s, kostky)
    return s, kostky


def otisk(s):
    hr = [(p.id, kt._xy(p), p.state, p.has_acted, p.has_moved, p.used_blitz) for p in vsichni(s)]
    tymy = [(t.score, t.rerolls, t.turn_number, t.blitz_used_this_turn, t.foul_used_this_turn)
            for t in (s.home_team, s.away_team)]
    return (hr, tymy, s.active_team, s.phase, s.ball.is_held, s.ball.carrier_id,
            (s.ball.position.x, s.ball.position.y))


def test_plan_nemeni_stav_partie():
    s, _ = nova_partie()
    pred = otisk(s)
    kroky, konec, hotovo = kt.plan_tahu(s, ai="macro_mcts", seed=5)
    assert kroky and hotovo
    assert otisk(s) == pred
    assert otisk(konec) != pred, "plán musí něco změnit na KOPII"


def test_plan_nespotrebuje_volbu_kouce():
    # kouč si nachystá kostku Pushed pro svůj příští blok; plán AI ji nesmí smazat ani spotřebovat
    # (review V1, druhé review N3: plán AI musí sám blokovat, jinak test hlídá jen smazání).
    # Blok na 2 kostky (asistence), semínko 1 hodí [Pushed, POW]; engine sám bere POW.
    def pozice():
        s = prazdne_hriste()
        trp = postav(s, H, 10, 7)
        elf = postav(s, A, 11, 7)
        postav(s, H, 12, 6)
        s.ball.position = bb.Position(0, 0)     # míč z dosahu ⇒ AI nemá co sbírat a blokuje
        return s, trp, elf

    def blok(volba, plan_mezi):
        s, trp, elf = pozice()
        if volba is not None:
            bb.set_manual_block_face(volba)
        if plan_mezi:
            kroky = kt.plan_tahu(s, ai="macro_mcts", seed=1)[0]
            assert any(k["action"].type in (bb.ActionType.BLOCK, bb.ActionType.BLITZ) for k in kroky), \
                "plán AI musí obsahovat blok, jinak test nic nedokazuje"
        a = next(a for a in bb.get_available_actions(s)
                 if a.type == bb.ActionType.BLOCK and a.player_id == trp.id and a.target_id == elf.id)
        _, udalosti = bb.execute_action_logged(s, a, bb.DiceRoller(1))
        bb.clear_manual_block_choices()
        return [e["roll"] for e in udalosti if e["type"] == "BLOCK"]
    assert blok(None, False) == [4], "engine sám vybere POW"
    assert blok(2, False) == [2], "pozitivní kontrola: ruční volba Pushed se uplatní"
    assert blok(2, True) == [2], "plán AI mezi tím volbu kouče nesmí spotřebovat"


def test_plan_mimo_hru_je_chyba():
    s, _ = nova_partie()
    s.phase = bb.GamePhase.TOUCHDOWN
    try:
        kt.plan_tahu(s, ai="greedy", seed=1)
    except ValueError:
        return
    raise AssertionError("plán mimo fázi PLAY má vyhodit chybu, ne vrátit prázdný „dokončený“ tah")


def test_plan_konci_po_tahu_jedne_strany():
    s, _ = nova_partie()
    kroky, konec, hotovo = kt.plan_tahu(s, ai="macro_mcts", seed=5)
    strana = s.active_team
    assert hotovo
    assert all(k["action"].type == bb.ActionType.END_TURN
               or s.get_player(k["action"].player_id).team_side == strana for k in kroky)
    posledni = kroky[-1]
    assert posledni["turnover"] or posledni["action"].type == bb.ActionType.END_TURN


def test_plan_je_deterministicky_a_zavisi_na_seminku():
    s, _ = nova_partie()

    def podpis(sd):
        return [(k["action"].type, k["action"].player_id, k["action"].target.x, k["action"].target.y,
                 tuple((e["type"], e["roll"], e["success"]) for e in k["events"]))
                for k in kt.plan_tahu(s, ai="macro_mcts", seed=sd)[0]]
    assert podpis(5) == podpis(5)
    assert any(podpis(5) != podpis(sd) for sd in (3, 4, 6, 8))


# --- P149: ke kroku plánu je přiloženo, PROČ se hraje (makro, kdo rozhodl, jak ho hledání ocenilo) ---

MAKRA = {"SCORE", "ADVANCE", "CAGE", "BLITZ", "BLOCK", "PICKUP", "PASS_ACTION", "FOUL", "REPOSITION",
         "END_TURN", "BLITZ_AND_SCORE", "HAND_OFF_SCORE", "PASS_SCORE", "CHAIN_SCORE"}


def test_plan_rika_proc_macro_mcts_ano_greedy_ne():
    s, _ = nova_partie()
    kroky = kt.plan_tahu(s, ai="macro_mcts", seed=5)[0]
    assert "macro" in kroky[0], "první krok tahu vždy začíná novým rozhodnutím"
    rozhodnuti = [k["macro"] for k in kroky if "macro" in k]
    assert all(m["type"] in MAKRA and m["source"] in ("cage", "search", "rescue", "greedy_fallback")
               for m in rozhodnuti)
    # krok, kterým AI tah sama končí, nese rozhodnutí END_TURN (hráče makra nehlídáme: BLITZ a BLOCK
    # ho v makru nemají, útočníka vybírá až rozbalení)
    if kroky[-1]["action"].type == bb.ActionType.END_TURN and "macro" in kroky[-1]:
        assert kroky[-1]["macro"]["type"] == "END_TURN"
    assert all("macro" not in k for k in kt.plan_tahu(s, ai="greedy", seed=5)[0])


def test_plan_rozhodnuti_hledani_ma_deti_s_navstevami_klec_ne():
    s, _ = nova_partie()
    videno, s_detmi = set(), 0
    for sd in range(1, 6):
        for k in kt.plan_tahu(s, ai="macro_mcts", seed=sd)[0]:
            m = k.get("macro")
            if m is None:
                continue
            videno.add(m["source"])
            if m["source"] == "cage":
                assert m["children"] == []
            if m["source"] == "search" and m["children"]:   # jediné makro v nabídce ⇒ hledání neběží, dětí není
                s_detmi += 1
                assert len(m["children"]) >= 2
                assert sum(c["visits"] for c in m["children"]) <= 50   # výchozích 50 iterací
                assert all(c["visits"] > 0 and -1.5 <= c["q"] <= 1.5 for c in m["children"])
    # pozitivní kontrola: test viděl oba zdroje i hledání s dětmi, jinak by větve výš nic nehlídaly
    assert {"cage", "search"} <= videno
    assert s_detmi > 0
