#!/usr/bin/env python3
"""Průběžný ČITELNÝ log z odpojeného `claude -p --output-format stream-json`.

Proč: `-p` v textovém režimu nevypíše NIC, dokud neskončí -- u několikahodinové
analýzy tedy není poznat, jestli běží, zasekla se, nebo umřela. stream-json
průběžný je, ale je to JSON. Tenhle převodník ho tailuje a píše řádky, které
jde číst zvenčí (`tail -f`), aniž by se muselo do sezení.

Použití:  python3 fable_log_reader.py <vstup.jsonl> <výstup.log>
"""
import json, os, sys, time

def brief(s, n=160):
    s = " ".join(str(s).split())
    return s if len(s) <= n else s[:n] + " …"

def render(d):
    t = d.get("type")
    if t == "system" and d.get("subtype") == "init":
        return [f"[init] model={d.get('model','?')} cwd={d.get('cwd','?')}"]
    if t == "assistant":
        out = []
        for b in d.get("message", {}).get("content", []):
            if b.get("type") == "text" and b.get("text", "").strip():
                out.append("[text] " + brief(b["text"], 400))
            elif b.get("type") == "tool_use":
                nm = b.get("name", "?")
                inp = b.get("input", {}) or {}
                arg = (inp.get("command") or inp.get("file_path")
                       or inp.get("pattern") or inp.get("prompt") or "")
                out.append(f"[tool] {nm}: {brief(arg, 200)}")
        return out
    if t == "user":
        out = []
        for b in d.get("message", {}).get("content", []):
            if b.get("type") == "tool_result":
                c = b.get("content")
                if isinstance(c, list):
                    c = " ".join(x.get("text", "") for x in c if isinstance(x, dict))
                mark = "⛔" if b.get("is_error") else "→"
                out.append(f"  {mark} {brief(c, 200)}")
        return out
    if t == "result":
        u = d.get("usage", {}) or {}
        return ["", f"[HOTOVO] {d.get('subtype')} | {d.get('num_turns','?')} kol"
                    f" | {d.get('duration_ms',0)//1000} s"
                    f" | in {u.get('input_tokens','?')} out {u.get('output_tokens','?')}",
                "", brief(d.get("result", ""), 4000)]
    return []

def main(src, dst):
    pos = 0
    with open(dst, "w", buffering=1) as out:
        out.write(f"# čitelný log z {src} — sleduj `tail -f {dst}`\n")
        idle = 0
        while True:
            if os.path.exists(src):
                with open(src) as f:
                    f.seek(pos)
                    new = f.read()
                    pos = f.tell()
                if new:
                    idle = 0
                    for line in new.splitlines():
                        line = line.strip()
                        if not line.startswith("{"):
                            if line: out.write(f"[stderr] {brief(line)}\n")
                            continue
                        try: d = json.loads(line)
                        except json.JSONDecodeError: continue
                        for r in render(d):
                            out.write(time.strftime("%H:%M:%S ") + r + "\n" if r else "\n")
                        if d.get("type") == "result":
                            out.write("# konec\n"); return
                else:
                    idle += 1
            else:
                idle += 1
            if idle > 4 * 60 * 60:      # 4 h ticha = konec
                out.write("# ⚠️ 4 h bez nového řádku — končím sledování\n"); return
            time.sleep(1)

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
