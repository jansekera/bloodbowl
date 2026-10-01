# Jedna brána před „hotovo“ i před commitem: make check
# Jednotlivě: make check-php · make check-front · make check-cpp
# C++ se staví v engine/build (CMake už nakonfigurovaný). bb_engine_py = Python modul pro měření —
# `make bb_engine` ho NEPŘESTAVÍ, proto se staví výslovně (jinak měření běží na starém kódu).

JOBS ?= 6

.PHONY: check check-php check-front check-cpp check-lint

check: check-lint check-php check-front check-cpp
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
