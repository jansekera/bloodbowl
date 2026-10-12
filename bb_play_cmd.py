#!/usr/local/bin/python3.8
"""Pošle příkaz serveru živé partie a vypíše odpověď.

  ./bb_play_cmd.py board
  ./bb_play_cmd.py do 5
  BB_PLAY_DIR=evidence/play_20261012 ./bb_play_cmd.py odp tym

Návratový kód 0 = odpověď přišla, 1 = server do limitu neodpověděl.
"""
import os
import sys
import time

TADY = os.path.dirname(os.path.abspath(__file__))
DIR = os.path.join(TADY, os.environ.get("BB_PLAY_DIR", os.path.join("evidence", "play_20261012")))
CMD, OUT = os.path.join(DIR, "cmd.txt"), os.path.join(DIR, "out.txt")


def main():
    prikaz = " ".join(sys.argv[1:]).strip()
    if not prikaz:
        print(__doc__)
        return 1
    pred = os.path.getmtime(OUT) if os.path.exists(OUT) else 0
    with open(CMD + ".tmp", "w") as f:
        f.write(prikaz + "\n")
    os.replace(CMD + ".tmp", CMD)          # server nikdy neuvidí napůl zapsaný příkaz
    limit = time.time() + float(os.environ.get("BB_PLAY_LIMIT", "180"))
    while time.time() < limit:
        if not os.path.exists(CMD) and os.path.exists(OUT) and os.path.getmtime(OUT) > pred:
            time.sleep(0.1)                # ať je odpověď dopsaná
            with open(OUT) as f:
                sys.stdout.write(f.read())
            return 0
        time.sleep(0.2)
    print(f"server neodpověděl na `{prikaz}` — běží? ({DIR})")
    return 1


if __name__ == "__main__":
    sys.exit(main())
