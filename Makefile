# Top-level convenience wrapper for the ACTA Gamma targets.
# Builds in dependency order: acta_db -> acta_cli -> acta_runner,
# then the Qt 6 GUI. The GUI is skipped with a warning when qmake6
# is not on PATH; `make gui` always requires Qt 6 and errors if qmake6
# is missing (see docs/building.md).

QMAKE6 := $(shell command -v qmake6 2>/dev/null)

.PHONY: all db cli runner gui test clean

all: db cli runner
	@if [ -n "$(QMAKE6)" ]; then \
		$(MAKE) gui; \
	else \
		echo "WARNING: qmake6 not found - skipping GUI build (install Qt 6, see docs/building.md; 'make gui' builds it)"; \
	fi

db:
	$(MAKE) -C acta_db

cli:
	$(MAKE) -C acta_cli

runner:
	$(MAKE) -C acta_runner

gui:
	@if [ -z "$(QMAKE6)" ]; then \
		echo "ERROR: qmake6 not found - install Qt 6 (see docs/building.md) and re-run"; \
		exit 1; \
	fi
	cd acta_gui && [ -f Makefile ] || $(QMAKE6) "CONFIG+=debug" acta_gui.pro -o Makefile
	$(MAKE) -C acta_gui

test: db cli runner
	$(MAKE) -C acta_db test
	$(MAKE) -C acta_cli test
	$(MAKE) -C acta_runner test

clean:
	$(MAKE) -C acta_db clean
	$(MAKE) -C acta_cli clean
	$(MAKE) -C acta_runner clean
	@if [ -f acta_gui/Makefile ]; then \
		$(MAKE) -C acta_gui clean; \
	else \
		echo "WARNING: acta_gui/Makefile not found - skipping GUI clean (run 'make gui' first to generate it)"; \
	fi
