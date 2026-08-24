#!/bin/bash
# Fable audit metrů, ČÁST 2 — spuštění ODPOJENĚ (24.08.2026)
# Poučeno z 21.08.: agent tehdy umřel na session limit, ne na oprávnění,
# ALE navíc mu byly ignorovány permissions.allow (workspace není "trusted")
# ⇒ oprávnění se tady předávají VÝSLOVNĚ přes --allowedTools, ne přes settings.
cd /home/jan/claude/bloodbowl || exit 1

OUT=evidence/fable_metric_audit_part2_20260824.md
LOG=evidence/fable_metric_audit_part2_20260824.log

PROMPT="Jsi Fable a děláš audit pro projekt Blood Bowl.

TVOJE ZADÁNÍ JE V SOUBORU: evidence/fable_brief_metric_audit_part2_20260824.md
Přečti ho CELÝ jako první a řiď se jím doslova.

Povinné vstupy, přečti oba před prací:
  1) evidence/fable_metric_audit_20260821.md   (useknutá ČÁST 1 — je to TVŮJ VSTUP, NEOPAKUJ ji)
  2) evidence/fable_brief_metric_audit_20260821.md  (původní zadání, oddíly 2b, 2c, 3, 4 platí doslova)

VÝSTUP piš PRŮBĚŽNĚ do $OUT — ne až na konci.
Vytvoř ten soubor jako PRVNÍ akci (kostra + pracovní deník s [ ] položkami),
a po KAŽDÉ dokončené podúloze do něj dopiš, co jsi zjistil, a odškrtni [x].
Předchozí pokus byl useknut po 18 minutách a zachránilo ho JEN to, že psal průběžně.
Poslední řádek souboru, až budeš úplně hotov, musí být: HOTOVO

TVRDÁ OMEZENÍ: nepřestavuj engine, needituj nic v engine/, nespouštěj dlouhé běhy.
Je to audit ČTENÍM kódu."

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
