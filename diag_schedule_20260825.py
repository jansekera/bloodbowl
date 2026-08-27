#!/usr/bin/env python3
"""ROZVRH DRIVU — plníme plán „TD v kole 8", a co se stane, když ho přestaneme stíhat?

Zadání z rozhovoru 25.08. Uživatel: *„máš za úkol jít a dát TD v kole 8 — to je
plán, ne v kole 6."* ⇒ výběh z klece NENÍ pravidlo s konstantou; je to reakce na
SKLUZ proti rozvrhu, a rozvrh se počítá z desky.

Počítá se čtvero, všechno nad týmž průchodem:
  (1) V KTERÉM KOLE dáváme TD (a soupeř) -- plní se plán „skóruj na konci půle"?
  (2) DOKONČITELNOST: podíl kol, kde `achievable_pace >= required_pace`,
      po kolech -- kdy rozvrh přestává platit.
  (3) ROZPAD PODLE MA NOSIČE -- uzávěrka není číslo kola, je funkcí toho, kdo nese.
  (4) CENA ZTRÁTY: když míč ztratíme, jak daleko je od NAŠÍ endzóny a stihne
      soupeř skórovat? Hypotéza uživatele: ztratit ho daleko je LEVNĚJŠÍ,
      takže výběh nemusí být tak drahý, jak se zdá.

⛔⛔ PAST, KTERÁ TENHLE ROZBOR UŽ JEDNOU ROZBILA (25.08.):
    `t['active_team']` je ŘETĚZEC 'home'/'away', NE číslo. Porovnání `== 0` je
    vždy nepravdivé ⇒ u trpaslíka-doma se přeskočí VŠECHNA kola a u trpaslíka-
    venku se počítají i SOUPEŘOVA. Vzorek pak vypadá normálně a je vedle.
    ⇒ Typ pole se OVĚŘUJE, nepředpokládá.
"""
import gzip, json, glob, os, sys, collections, statistics
from multiprocessing import Pool

STANDING = 0


def one(path):
    try:
        d = json.load(gzip.open(path))
    except Exception:
        return None
    hr, ar = d['home_race'], d['away_race']
    out = collections.Counter()
    out['games'] = 1
    for me_home in (True, False):
        race = hr if me_home else ar
        side = 'home' if me_home else 'away'
        our_ez = 0 if me_home else 25          # endzóna, kterou BRÁNÍME
        # ⛔ JMENOVATEL: rasa nehraje ve VŠECH hrách korpusu. 15 dvojic z 5 ras
        # ⇒ každá rasa nastoupí v 5 dvojicích, a ve své zrcadlové na OBOU
        # stranách: 4×1200 + 2×1200 = 7 200 nástupů, ne 18 000. Dělit počtem
        # her dá číslo 2,5× menší a vypadá to úplně věrohodně (25.08.).
        out[f'sides|{race}'] += 1
        logs = d['turn_logs']
        for i, t in enumerate(logs):
            # ⛔ TD se NEČTE z přírůstku skóre: skóre je snímek ZAČÁTKU kola,
            # takže TD z 8. kola se objeví až jako přírůstek v „kole 1" další
            # půle -- rozdělení pak vypadá posunuté o jedno kolo a v kole 1
            # vyrobí špičku, která tam nepatří (25.08.). Pole `touchdown`
            # označuje kolo, ve kterém TD PADL, a `active_team` říká kdo.
            if t.get('touchdown') and t.get('active_team') == side:
                out[f'td|{race}|{t["turn"]}'] += 1
            if t.get('active_team') != side:
                continue
            rp, ap = t.get('required_pace', -1), t.get('achievable_pace', -1)
            if rp is None or ap is None or rp < 0 or ap < 0:
                continue
            ok = ap >= rp
            out[f'pace_n|{race}|{t["turn"]}'] += 1
            if ok:
                out[f'pace_ok|{race}|{t["turn"]}'] += 1
            mine = t['home_players'] if me_home else t['away_players']
            car = [p for p in mine if p.get('has_ball')]
            if car:
                ma = car[0]['ma']
                out[f'ma_n|{race}|{ma}'] += 1
                if ok:
                    out[f'ma_ok|{race}|{ma}'] += 1
            # (4) cena ztráty: kolo končí turnoverem a míč jsme drželi my
            if t.get('turnover') and car:
                nxt = logs[i + 1] if i + 1 < len(logs) else t
                bx = nxt.get('ball_x', -1)
                if bx is not None and bx >= 0:
                    dist = abs(bx - our_ez)
                    left = max(0, 8 - t['turn'])
                    opp = t['away_players'] if me_home else t['home_players']
                    fast = max((p['ma'] for p in opp if p['state'] == STANDING), default=0)
                    out[f'loss_n|{race}'] += 1
                    out[f'loss_dist|{race}'] += dist
                    # stihne to? potřebné tempo <= jeho MA + 2 GFI
                    if left and dist / left <= fast + 2:
                        out[f'loss_reachable|{race}'] += 1
    return out


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else 'crosses_20260821_data'
    files = sorted(glob.glob(os.path.join(root, '*', 'g*.json.gz')))
    if not files:
        print(f"⛔ ŽÁDNÉ HRY v {root} — NEPROBĚHLO")
        raise SystemExit(2)
    print(f"korpus: {root}   souborů: {len(files)}")
    os.nice(19)
    with Pool(3) as pool:
        tot = collections.Counter()
        for r in pool.imap_unordered(one, files, chunksize=100):
            if r:
                tot += r
    races = sorted({k.split('|')[1] for k in tot if k.startswith('pace_n|')})
    g = tot['games']
    print(f"her: {g}\n")

    print("=== (1) V KTERÉM KOLE PADÁ TD ===")
    print(f"{'rasa':<12}" + ''.join(f'{k:>6}' for k in range(1, 9)) + f"{'celkem':>9}{'/nástup':>9}{'7-8':>7}")
    for r in races:
        row = [tot.get(f'td|{r}|{k}', 0) for k in range(1, 9)]
        s = sum(row)
        late = 100 * (row[6] + row[7]) / s if s else 0
        nast = tot.get(f'sides|{r}', 0) or 1        # nástupy té rasy, ne hry korpusu
        print(f"{r:<12}" + ''.join(f'{v:>6}' for v in row) + f"{s:>9}{s/nast:>8.3f}{late:>6.0f}%")

    print("\n=== (2) DOKONČITELNOST: podíl kol s `dosažitelné >= potřebné` ===")
    print(f"{'rasa':<12}" + ''.join(f'{k:>7}' for k in range(1, 9)))
    for r in races:
        cells = []
        for k in range(1, 9):
            n = tot.get(f'pace_n|{r}|{k}', 0)
            cells.append(f"{100*tot.get(f'pace_ok|{r}|{k}',0)/n:>6.0f}%" if n else "     -")
        print(f"{r:<12}" + ''.join(cells))

    print("\n=== (3) DOKONČITELNOST PODLE MA NOSIČE ===")
    print(f"{'rasa':<12}{'MA':>4}{'kol':>9}{'dokončitelné':>15}")
    for r in races:
        for ma in sorted({int(k.split('|')[2]) for k in tot if k.startswith(f'ma_n|{r}|')}):
            n = tot[f'ma_n|{r}|{ma}']
            print(f"{r:<12}{ma:>4}{n:>9}{100*tot.get(f'ma_ok|{r}|{ma}',0)/n:>14.1f}%")

    print("\n=== (4) CENA ZTRÁTY MÍČE ===")
    print(f"{'rasa':<12}{'ztrát':>8}{'⌀vzdálenost k naší EZ':>24}{'soupeř by DOŠEL':>18}")
    for r in races:
        n = tot.get(f'loss_n|{r}', 0)
        if not n:
            continue
        print(f"{r:<12}{n:>8}{tot[f'loss_dist|{r}']/n:>24.1f}"
              f"{100*tot.get(f'loss_reachable|{r}',0)/n:>17.0f}%")
    print("\n⚠️ (4) je HORNÍ mez ceny: 'soupeř by došel' počítá jen vzdálenost a MA,")
    print("   ne to, že mu v cestě stojíme. Skutečná cena je nižší.")


if __name__ == '__main__':
    main()
