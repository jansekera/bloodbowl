# Jedna brána před „hotovo“ i před commitem: make check
# Jednotlivě: make check-php · make check-front · make check-cpp
# C++ se staví v engine/build (CMake už nakonfigurovaný). bb_engine_py = Python modul pro měření —
# `make bb_engine` ho NEPŘESTAVÍ, proto se staví výslovně (jinak měření běží na starém kódu).

JOBS ?= 6

.PHONY: check check-all check-php check-front check-cpp check-lint check-smoke check-py check-e2e

check: check-lint check-php check-front check-cpp check-smoke
	@echo "== check: vše zelené"

check-php:
	vendor/bin/phpstan analyse --no-progress
	vendor/bin/phpunit

check-front:
	npx tsc --noEmit -p .
	npx vitest run

check-lint:
	vendor/bin/php-cs-fixer fix --dry-run --diff
	npx eslint frontend/src

check-cpp:
	$(MAKE) -C engine/build -j$(JOBS) bb_tests mcts_cli bb_engine_py
	engine/build/bb_tests --gtest_brief=1

# Kouř enginu: 6 celých her greedy × greedy (~2,5 s). Chytí pád nebo zacyklení (timeout 120 s), které unit testy nevidí.
# Pevné semínko ⇒ stejný běh pokaždé; výsledek her se nekontroluje (pravidlová oprava se neměří zlepšením).
check-smoke: check-cpp
	timeout 120 engine/build/mcts_cli --home=greedy --away=greedy --games=6 --home-roster=wood-elf --away-roster=orc --seed=42 > /dev/null
	@echo "== smoke: 6 her bez pádu"

# Delší sady — před dávkou commitů / jednou za den, ne před každým commitem (make check-all ≈ 4 min).
check-all: check check-py check-e2e
	@echo "== check-all: vše zelené"

# Python nástroje (měření, trénink) — venv na Pythonu 3.8, viz requirements-py38.txt (P118).
check-py: check-cpp
	venv/bin/python -m pytest python/tests -q -p no:cacheprovider

# Prohlížeč: registrace → týmy → najímání → zápas proti AI. Vlastní server na :8000, po testu ho ukončí.
check-e2e:
	@php -S localhost:8000 -t public >/tmp/bb-e2e-server.log 2>&1 & echo $$! > /tmp/bb-e2e-server.pid; \
	for i in $$(seq 1 30); do curl -s -o /dev/null localhost:8000/ && break; sleep 0.3; done; \
	timeout 600 node e2e_human_vs_ai_smoke.mjs; rc=$$?; kill $$(cat /tmp/bb-e2e-server.pid); exit $$rc

