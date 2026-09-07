# Top-level convenience wrapper for the ACTA Gamma C targets.
# Builds in dependency order: acta_db -> acta_cli -> acta_runner.
# The Qt GUI is built separately in acta_gui/ (qmake6 + make).

.PHONY: all db cli runner gui test clean

all: db cli runner

db:
	$(MAKE) -C acta_db

cli:
	$(MAKE) -C acta_cli

runner:
	$(MAKE) -C acta_runner

gui:
	$(MAKE) -C acta_gui

test: db cli runner
	$(MAKE) -C acta_db test
	$(MAKE) -C acta_cli test
	$(MAKE) -C acta_runner test

clean:
	$(MAKE) -C acta_db clean
	$(MAKE) -C acta_cli clean
	$(MAKE) -C acta_runner clean
