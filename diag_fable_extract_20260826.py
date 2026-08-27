#!/usr/bin/env python3
"""Vytáhne klíčové řádky z výstupů diag_fable_drives_20260826.py pro srovnání korpusů/ras."""
import re
import sys

for f in sys.argv[1:]:
    print("#####", f)
    lines = open(f).read().splitlines()
    out = []
    sec = None
    for i, l in enumerate(lines):
        if l.startswith("==="):
            sec = l[4:8]
        if i < 3:
            out.append(l)
        elif sec in ("2b. ", "4. O", "3c. ", "3d. ", "3e. ", "4b. ", "1. P") and (l.startswith("[A]") or l.startswith("[D1]") or l.startswith("[D (") or l.startswith("A ") or l.startswith("    ") or l.startswith("TD") or l.startswith("tempo") or l.startswith("použ")):
            if sec == "1. P" and l.startswith("    "):
                continue
            out.append("%s| %s" % (sec.strip(". "), l))
        elif sec == "3. S" and (l.startswith("[A]") or l.startswith("[D1]")):
            out.append("3| " + l)
            out.append("3| " + lines[i + 2])
        elif sec == "3b. " and "nosič s událostí" in l:
            out.append("3b| " + l)
    print("\n".join(out))
    print()
