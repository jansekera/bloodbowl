import sys, pickle, math, collections
d = pickle.load(open(sys.argv[1], "rb"))
MER = ["prolom", "posun", "td", "turnover", "nosic_srazen", "mic_ztracen", "rohu", "rohy_srazeny", "nasi_srazeni", "cas"]
def tab(rows, nazev):
    rows = [r for r in rows if r["ramena"]["vse"]["mel_mic"]]
    print(f"\n## {nazev} — {len(rows)} tahů s míčem na začátku\n")
    print("| na 100 tahů | vše | bez P229 | bez P228+P229 | vše − bez (± SE) | vše − bez229 (± SE) |")
    print("|---|---:|---:|---:|---:|---:|")
    for m in MER:
        def val(r, a):
            v = r["ramena"][a][m]; return 0 if v is None else v
        n = len(rows)
        if n == 0: continue
        avg = {a: 100 * sum(val(r, a) for r in rows) / n for a in ("vse", "bez229", "bez")}
        def diff(b):
            ds = [100 * (val(r, "vse") - val(r, b)) for r in rows]
            mu = sum(ds) / n; sd = math.sqrt(sum((x - mu) ** 2 for x in ds) / max(1, n - 1))
            return f"{mu:+.1f} ± {sd / math.sqrt(n):.1f}"
        print(f"| {m} | {avg['vse']:.1f} | {avg['bez229']:.1f} | {avg['bez']:.1f} | {diff('bez')} | {diff('bez229')} |")
tab(d, "všichni")
for n in sorted({r["nasi"] for r in d}):
    tab([r for r in d if r["nasi"] == n], n)
