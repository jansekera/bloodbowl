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
#   ① leak → ② jmenovatel → ③ n_nonzero → ④ teprve pak delta.
#   Když ① neprojde, delta se NEVYTISKNE. Nedá se přečíst od konce.
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


def summarize(out, name, thr):
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

    # ① leak — bez něj se delta nečte
    missing = [x for x in sh if x["leak"] is None]
    if missing:
        L.append("    ① ⛔ ŘÁDEK `MOVED WITHOUT THE ARM ACTING` CHYBÍ v %d/%d shardech"
                 % (len(missing), len(sh)))
        L.append("       ⇒ binárka tu kontrolu neumí (stará?). VERDIKT SE NEVYNÁŠÍ.")
        return L, 2
    leak = sum(x["leak"] for x in sh)
    bad = [x["path"].split("/")[-2] for x in sh if x["leak"]]
    L.append("    ① MOVED WITHOUT THE ARM ACTING: %d %s"
             % (leak, "(0 = čisté, delta se SMÍ číst)" if leak == 0
                else "⛔ v " + ",".join(bad) + " — rameno změnilo běh JINUDY, DELTA SE NEČTE"))
    if leak:
        return L, 3

    # ② jmenovatel
    acted, of = sum(x["acted"] for x in sh), sum(x["of"] for x in sh)
    L.append("    ② arm acted: %d/%d (%.1f %%)%s"
             % (acted, of, 100.0 * acted / of,
                "" if acted == of else "  ⚠️ neúplný jmenovatel — část párů rameno neměřila"))

    # ③ n_nonzero
    if any(x["nz"] is None for x in sh):
        L.append("    ③ n_nonzero: ⚠️ CHYBÍ v logu (harness starší než 17.08.)"
                 " — nevíme, kolik párů vůbec neslo informaci")
    else:
        nz = sum(x["nz"] for x in sh)
        L.append("    ③ n_nonzero: %d/%d (%.1f %%) — jen tyhle páry nesou informaci"
                 % (nz, pairs, 100.0 * nz / pairs))

    # ④ teprve teď delta
    n = len(sh)
    mean = sum(x["delta"] for x in sh) / n
    pooled = math.sqrt(sum(x["se"] ** 2 for x in sh)) / n
    sd = math.sqrt(sum((x["delta"] - mean) ** 2 for x in sh) / (n - 1)) if n > 1 else 0.0
    emp = sd / math.sqrt(n) if n > 1 else 0.0
    neg = sum(1 for x in sh if x["delta"] < 0)
    L.append("    ④ DELTA SLOUČENĚ: %+.4f ± %.4f SE (%+.2f σ), 95%% CI [%+.4f; %+.4f]"
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
    verdict = ("ŠKODÍ" if mean <= -thr else "POMÁHÁ" if mean >= thr else "NEROZHODNUTO")
    L.append("       PRÁH ±%.4f (vstup běhu, ne konstanta ve zdrojáku) ⇒ **%s**" % (thr, verdict))
    if verdict == "NEROZHODNUTO":
        L.append("       ⚠️ NEROZHODNUTO se zapisuje JAKO NEROZHODNUTO, ne jako potvrzení zamítnutí.")
    return L, 0


if __name__ == "__main__":
    out, thr = sys.argv[1], float(os.environ.get("THRESHOLD", "0.015"))
    rc = 0
    for name in sys.argv[2:]:
        lines, r = summarize(out, name, thr)
        print("\n".join(lines))
        rc = max(rc, r)
    sys.exit(rc)
