#!/usr/bin/env python3
# ============================================================================
# SLOUČENÍ SHARDŮ JEDNÉ NOCI                                     (18.08.2026)
#
# ⚑ PROČ VZNIKL
#   Noc 17.→18.08. doběhla čistě (8/8 shardů, žádný FAIL) a přesto SKONČILA
#   BEZ VÝSLEDKU: `chain.log` končil `NIGHT DONE`, 6 000 párů leželo jako
#   osm čísel po ±0,019 -- tedy osm JEDNOTLIVĚ NEPRŮKAZNÝCH výsledků -- a
#   sloučenou deltu (−0,0248 ± 0,0068) musel ráno spočítat člověk.
#   Táž rodina chyby jako audit měřicího aparátu: SNÍMEK SE VYDÁVÁ ZA STAV.
#   A je to přesně krok, ve kterém si unavené čtení vybere shard, který se hodí.
#
#   Druhá vada téže noci: řádek, který předregistrace označuje za NEJCENNĚJŠÍ
#   ranní čtení (`MOVED WITHOUT THE ARM ACTING`), se do `chain.log` vytáhnout
#   MĚL, ale grep hledal `^  ARM `, což je řádek jen pro mode 4 (Dauntless).
#   Výsledkem byl falešný poplach „(harness nic netiskl — stará binárka?)"
#   nad testem, který ve skutečnosti proběhl a byl čistý 8/8.
#
# ⚑ POŘADÍ VÝPISU JE POŘADÍ ČTENÍ Z PŘEDREGISTRACE, ne pořadí zajímavosti:
#   (1) leak → (2) jmenovatel → (3) n_nonzero → (4) teprve pak delta.
#   Když (1) neprojde, delta se NEVYTISKNE. Nedá se přečíst od konce.
# ============================================================================
import re, sys, glob, math, os

# n_nonzero přibylo do harnessu až 17.08. — u starších logů smí chybět, ale MUSÍ
# se to říct nahlas, ne spadnout na „log se nedá přečíst" (to je táž vada, jakou
# tenhle skript opravuje: falešný poplach místo informace).
RE_SUM   = re.compile(r"SUMMARY matchup \d+ \([^)]*\), (\d+) pairs \(\d+ games\)"
                      r"(?:, n_nonzero (\d+))?")
RE_ARM   = re.compile(r"arm acted in (\d+)/(\d+) pairs; pairs that moved: (\d+); "
                      r"MOVED WITHOUT THE ARM ACTING: (\d+)")
RE_DELTA = re.compile(r"PAIRED delta chess as \w+: ([-+][\d.]+) \+- ([\d.]+)")


def read_shard(path):
    t = open(path).read()
    s, a, d = RE_SUM.search(t), RE_ARM.search(t), RE_DELTA.search(t)
    if not (s and d):
        return None
    return dict(path=path,
                pairs=int(s.group(1)),
                nz=int(s.group(2)) if s.group(2) else None,
                acted=int(a.group(1)) if a else None,
                of=int(a.group(2)) if a else None,
                leak=int(a.group(4)) if a else None,
                delta=float(d.group(1)), se=float(d.group(2)))


def summarize(out, name, thr, facts=None):
    logs = sorted(glob.glob(os.path.join(out, name + "_s*", "run.log")))
    sh = [x for x in (read_shard(p) for p in logs) if x]
    L = []
    if not sh:
        return ["  %s: ⛔ ŽÁDNÝ ČITELNÝ SHARD (%d logů) — výsledek NEEXISTUJE" % (name, len(logs))], 1
    if len(sh) != len(logs):
        L.append("  %s: ⚠️ %d z %d logů se nedalo přečíst — sloučení je NEÚPLNÉ"
                 % (name, len(logs) - len(sh), len(logs)))

    pairs = sum(x["pairs"] for x in sh)
    L.append("  %s — %d shardů, %d párů" % (name, len(sh), pairs))

    # (1) leak — bez něj se delta nečte
    missing = [x for x in sh if x["leak"] is None]
    if missing:
        L.append("    (1) ⛔ ŘÁDEK `MOVED WITHOUT THE ARM ACTING` CHYBÍ v %d/%d shardech"
                 % (len(missing), len(sh)))
        L.append("       ⇒ binárka tu kontrolu neumí (stará?). VERDIKT SE NEVYNÁŠÍ.")
        return L, 2
    leak = sum(x["leak"] for x in sh)
    bad = [x["path"].split("/")[-2] for x in sh if x["leak"]]
    L.append("    (1) MOVED WITHOUT THE ARM ACTING: %d %s"
             % (leak, "(0 = čisté, delta se SMÍ číst)" if leak == 0
                else "⛔ v " + ",".join(bad) + " — rameno změnilo běh JINUDY, DELTA SE NEČTE"))
    if leak:
        return L, 3

    # (2) jmenovatel
    acted, of = sum(x["acted"] for x in sh), sum(x["of"] for x in sh)
    L.append("    (2) arm acted: %d/%d (%.1f %%)%s"
             % (acted, of, 100.0 * acted / of,
                "" if acted == of else "  ⚠️ neúplný jmenovatel — část párů rameno neměřila"))

    # (3) n_nonzero
    if any(x["nz"] is None for x in sh):
        L.append("    (3) n_nonzero: ⚠️ CHYBÍ v logu (harness starší než 17.08.)"
                 " — nevíme, kolik párů vůbec neslo informaci")
    else:
        nz = sum(x["nz"] for x in sh)
        L.append("    (3) n_nonzero: %d/%d (%.1f %%) — jen tyhle páry nesou informaci"
                 % (nz, pairs, 100.0 * nz / pairs))

    # (4) teprve teď delta
    n = len(sh)
    mean = sum(x["delta"] for x in sh) / n
    pooled = math.sqrt(sum(x["se"] ** 2 for x in sh)) / n
    sd = math.sqrt(sum((x["delta"] - mean) ** 2 for x in sh) / (n - 1)) if n > 1 else 0.0
    emp = sd / math.sqrt(n) if n > 1 else 0.0
    neg = sum(1 for x in sh if x["delta"] < 0)
    L.append("    (4) DELTA SLOUČENĚ: %+.4f ± %.4f SE (%+.2f σ), 95%% CI [%+.4f; %+.4f]"
             % (mean, pooled, mean / pooled if pooled else 0,
                mean - 1.96 * pooled, mean + 1.96 * pooled))
    L.append("       shardy: " + " ".join("%+.4f" % x["delta"] for x in sh))
    L.append("       %d/%d shardů záporných" % (neg, n))
    if n > 1:
        # Empirická SE mezi shardy je kontrola SLOUČITELNOSTI, ne lepší odhad:
        # když je VÝRAZNĚ větší než sdružená, shardy si neodpovídají a průměr
        # není to, co si o něm myslíme.
        L.append("       empirická SE mezi shardy %.4f vs sdružená %.4f ⇒ %s"
                 % (emp, pooled,
                    "overdisperze ⚠️ SHARDY SI NEODPOVÍDAJÍ, sloučení je podezřelé"
                    if emp > 1.5 * pooled else "bez overdisperze, sloučení legitimní"))
    if facts is not None:
        facts.update({"delta": mean, "n_nonzero": nz / pairs if not any(x["nz"] is None for x in sh) else None,
                      "leak": float(leak), "arm_acted": acted / of})
        for k in [k for k, v in facts.items() if v is None]:
            del facts[k]
    # ⭐ T2.16 (19.08.): VERDIKT SE NESMÍ VYNÁŠET Z BODOVÉHO ODHADU SAMOTNÉHO.
    #
    # ⚑ PROČ. Noc 18.->19.08. (P9c) dala +0,0017 ± 0,0060, tedy 95% CI
    #   [-0,0100; +0,0134] proti prahu ±0,015 -- CELÉ CI leží UVNITŘ prahu.
    #   To NENÍ „nemáme sílu": efekt velikosti prahu je vyloučen v OBOU směrech
    #   (TOST projde). Dosavadní kód na to řekl „NEROZHODNUTO" -- tedy TOTÉŽ
    #   SLOVO, jakým odpovídá na běh, který má SE tak velkou, že neví nic.
    #
    #   A ta dvě „nerozhodnuto" nesou OPAČNÉ PŘÍKAZY K AKCI:
    #     * CI přesahuje práh  ⇒ přidej páry a běh zopakuj;
    #     * CI uvnitř prahu    ⇒ ZASTAV, rameno předpovězený efekt nedodává.
    #   Cena záměny je další 14hodinová noc, která nic nového nezjistí.
    lo, hi = mean - 1.96 * pooled, mean + 1.96 * pooled
    if mean <= -thr:
        verdict = "ŠKODÍ"
    elif mean >= thr:
        verdict = "POMÁHÁ"
    elif -thr < lo and hi < thr:
        verdict = "EKVIVALENCE"
    else:
        verdict = "NEROZHODNUTO — MÁLO SÍLY"
    L.append("       PRÁH ±%.4f (vstup běhu, ne konstanta ve zdrojáku) ⇒ **%s**" % (thr, verdict))

    if verdict == "EKVIVALENCE":
        L.append("       celé 95%% CI [%+.4f; %+.4f] leží UVNITŘ prahu ⇒ efekt velikosti"
                 " prahu je vyloučen v OBOU směrech." % (lo, hi))
        L.append("       ⇒ PŘÍKAZ: ZASTAV. Přidávat páry nemá smysl — rameno"
                 " předpovězený efekt NEDODÁVÁ.")
        # ⚠️ Co tenhle běh NEVYLUČUJE, se musí říct nahlas: ekvivalence proti
        #    prahu není nula. Bez téhle věty se z „EKVIVALENCE" čte „nula".
        L.append("       ⚠️ NENÍ to nula: efekt až do %+.4f (mez CI) tenhle běh"
                 " vyloučit NEUMÍ. \u201eNeškodí\u201c ANO, \u201eje k ničemu\u201c NE."
                 % (hi if abs(hi) > abs(lo) else lo))
    elif verdict == "NEROZHODNUTO — MÁLO SÍLY":
        L.append("       95%% CI [%+.4f; %+.4f] PŘESAHUJE práh ⇒ tenhle běh neumí"
                 " rozhodnout ani \u201epomáhá/škodí\u201c, ani \u201eefekt tam není\u201c." % (lo, hi))
        # Kolik párů by chybělo, aby CI ještě padlo dovnitř prahu. Platí PŘI
        # NEZMĚNĚNÉM bodovém odhadu -- je to rozpočtový odhad, ne předpověď.
        room = thr - abs(mean)
        if room > 0 and pooled > 0:
            need = pairs * (1.96 * pooled / room) ** 2
            L.append("       na rozhodnutí by při NEZMĚNĚNÉM odhadu bylo potřeba"
                     " ~%d párů (dnes %d, tj. %.1f×)" % (round(need), pairs, need / pairs))
        else:
            L.append("       bodový odhad je na prahu nebo za ním ⇒ počet párů"
                     " na rozhodnutí se odsud spočítat nedá")
        L.append("       ⚠️ NEROZHODNUTO se zapisuje JAKO NEROZHODNUTO, ne jako potvrzení zamítnutí.")
    return L, 0



# ============================================================================
# PŘEDPOVĚĎ vs VÝSLEDEK                                          (18.08.2026)
#
# ⚑ PROČ. Předregistrace má jedinou hodnotu: donutí mít nepravdu NAHLAS.
#   Noc 17.→18.08. měla šest předpovědí. Dvě z nich ten běh NEMOHL zodpovědět
#   (`CORPUS=0`) a nikdo to nezkontroloval PŘED spuštěním; a nic je po doběhnutí
#   neporovnalo s výsledkem, takže minutá předpověď `n_nonzero` (62,8 % proti
#   čekaným >80 %) se málem ztratila -- přitom je to informace o rameni, ze které
#   vzešlo P32. Když minutá předpověď nezanechá stopu, necháš si stejný špatný model.
#
# ⚑ FORMÁT souboru (PREREG=cesta), jedna předpověď na řádek:
#     delta      in    -0.015 0.015     # co čekám a proč
#     n_nonzero  >=    0.80
#     leak       ==    0
#     arm_acted  >=    0.99
#     corpus:K9a <     baseline         # potřebuje CORPUS=1 -- jinak se běh NESPUSTÍ
#   `#` je komentář. Metrika s předponou `corpus:` je pro spouštěč signál, že
#   běh musí sbírat korpus; sem se dostane jen jako NEZODPOVĚDITELNÁ.
# ============================================================================
OPS = {
    "==": lambda v, a: abs(v - a[0]) < 1e-9,
    ">=": lambda v, a: v >= a[0],
    "<=": lambda v, a: v <= a[0],
    ">":  lambda v, a: v > a[0],
    "<":  lambda v, a: v < a[0],
    "in": lambda v, a: a[0] <= v <= a[1],
}


def parse_prereg(path):
    out = []
    for raw in open(path):
        line = raw.split("#")[0].strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) < 3:
            out.append((parts[0] if parts else "?", None, None, raw.strip()))
            continue
        metric, op, args = parts[0], parts[1], parts[2:]
        try:
            vals = [float(a) for a in args]
        except ValueError:
            vals = None            # např. `corpus:K9a < baseline`
        out.append((metric, op, vals, raw.strip()))
    return out


def confront(preds, facts):
    """facts: metrika -> hodnota. Vrací (řádky, počet MIMO)."""
    L = ["  --- PŘEDPOVĚĎ vs VÝSLEDEK (z předregistrace, zapsané PŘED během) ---"]
    missed = 0
    for metric, op, vals, raw in preds:
        if metric not in facts or op not in OPS or vals is None:
            L.append(f"    ⛔ NEZODPOVĚDITELNÁ  {raw}")
            L.append("       ⇒ běh na tuhle předpověď neumí odpovědět. To se mělo chytit PŘED startem.")
            missed += 1
            continue
        v = facts[metric]
        ok = OPS[op](v, vals)
        L.append(f"    {'✅ TREFA ' if ok else '❌ MIMO  '}  {metric} {op} "
                 f"{' '.join(f'{x:g}' for x in vals)}   →  změřeno {v:+.4f}")
        if not ok:
            missed += 1
            L.append("       ⚠️ MIMO se zapisuje. Je to informace o rameni, ne selhání běhu.")
    if not preds:
        L.append("    (předregistrace nepředána -- PREREG=cesta)")
    return L, missed

if __name__ == "__main__":
    out, thr = sys.argv[1], float(os.environ.get("THRESHOLD", "0.015"))
    preg = os.environ.get("PREREG", "")
    preds = parse_prereg(preg) if preg and os.path.exists(preg) else []
    rc = 0
    for name in sys.argv[2:]:
        facts = {}
        lines, r = summarize(out, name, thr, facts)
        print("\n".join(lines))
        rc = max(rc, r)
        if preds and r == 0:
            cl, missed = confront(preds, facts)
            print("\n".join(cl))
            if missed:
                print(f"       ⇒ {missed} předpověď/i MIMO nebo nezodpověditelná — patří do zápisu noci.")
    sys.exit(rc)
