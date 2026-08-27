#!/bin/bash
# Fable audit metrů, ČÁST 2 — spuštění ODPOJENĚ (24.08.2026)
# Poučeno z 21.08.: agent tehdy umřel na session limit, ne na oprávnění,
# ALE navíc mu byly ignorovány permissions.allow (workspace není "trusted")
# ⇒ oprávnění se tady předávají VÝSLOVNĚ přes --allowedTools, ne přes settings.
cd /home/jan/claude/bloodbowl || exit 1

OUT=evidence/fable_movement_parity_20260824.md
LOG=evidence/fable_movement_parity_20260824.log

PROMPT="Jsi Fable a děláš audit pro projekt Blood Bowl.

TVOJE ZADÁNÍ JE V SOUBORU: evidence/fable_brief_movement_parity_20260824.md
Přečti ho CELÉ jako první a řiď se jím doslova.

Text pravidel je rules_bb2016.txt (hrajeme BB2016, NE CRP/LRB6).
Každé tvrzení dolož číslem řádku odtud. Necituj pravidlo z hlavy.

VÝSTUP piš PRŮBĚŽNĚ do evidence/fable_movement_parity_20260824.md -- ne až na konci.
Vytvoř ten soubor jako PRVNÍ akci (kostra + pracovní deník s [ ] položkami),
a po KAŽDÉ dokončené podúloze do něj dopiš, co jsi zjistil, a odškrtni [x].
Poslední řádek souboru, až budeš úplně hotov, musí být: HOTOVO

TVRDÁ OMEZENÍ: neměň žádný kód, needituj nic v engine/, nespouštěj dlouhé běhy.
Je to audit ČTENÍM."

exec claude -p "$PROMPT" \
  --model claude-fable-5 \
  --effort high \
  --permission-mode acceptEdits \
  --allowedTools "Read" "Grep" "Glob" "Write" "Edit" "TodoWrite" \
                 "Bash(grep:*)" "Bash(rg:*)" "Bash(sed:*)" "Bash(awk:*)" \
                 "Bash(head:*)" "Bash(tail:*)" "Bash(cat:*)" "Bash(wc:*)" \
                 "Bash(ls:*)" "Bash(find:*)" "Bash(sort:*)" "Bash(uniq:*)" \
                 "Bash(cut:*)" "Bash(nice:*)" "Bash(python3:*)" \
  --disallowedTools "Bash(make:*)" "Bash(cmake:*)" "Bash(g++:*)" "Bash(git commit:*)" "Bash(git push:*)" \
  < /dev/null >> "$LOG" 2>&1
