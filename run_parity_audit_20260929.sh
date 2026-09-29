#!/bin/bash
# Pondělní audit parity PHP x C++ (E29) -- spuštění ODPOJENĚ (29.09.2026)
cd /home/jenda/claude/blood-bowl || exit 1
LOG=evidence/parity_audit_20260929.log
PROMPT="Děláš audit pro projekt Blood Bowl.

TVOJE ZADÁNÍ JE V SOUBORU: evidence/fable_brief_parity_audit_20260929.md
Přečti ho CELÉ jako první a řiď se jím doslova.

VÝSTUP piš PRŮBĚŽNĚ do evidence/parity_audit_20260929.md -- soubor založ jako PRVNÍ akci,
po KAŽDÉM commitu do něj dopiš výsledek. Poslední řádek, až budeš úplně hotov: HOTOVO

TVRDÁ OMEZENÍ: neměň žádný kód, nic needituj v src/ ani engine/, nekompiluj, nespouštěj hry.
Smíš psát jen do evidence/parity_audit_20260929.md. Je to audit ČTENÍM."
exec claude -p "$PROMPT" \
  --model claude-opus-5-5 \
  --effort high \
  --permission-mode acceptEdits \
  --allowedTools "Read" "Grep" "Glob" "Write" "Edit" "TodoWrite" \
                 "Bash(grep:*)" "Bash(rg:*)" "Bash(sed:*)" "Bash(awk:*)" \
                 "Bash(head:*)" "Bash(tail:*)" "Bash(cat:*)" "Bash(wc:*)" \
                 "Bash(ls:*)" "Bash(find:*)" "Bash(sort:*)" "Bash(uniq:*)" \
                 "Bash(cut:*)" "Bash(git show:*)" "Bash(git log:*)" "Bash(git diff:*)" \
  --disallowedTools "Bash(make:*)" "Bash(cmake:*)" "Bash(g++:*)" "Bash(git commit:*)" "Bash(git push:*)" "Bash(php:*)" \
  < /dev/null >> "$LOG" 2>&1
