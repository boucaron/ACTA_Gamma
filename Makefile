# Top-level convenience wrapper for the ACTA Gamma targets.
# `make all` builds the core in dependency order: acta_db -> acta_cli ->
# acta_runner. The Qt 6 GUI is optional and is NOT built by default:
# `make gui` builds it (requires Qt 6; errors if qmake6 is missing).
# `make test-live*` runs the LIVE backend check (scripts/live_backend_check.sh)
# against a real server - it is NEVER part of `make test`, and it exits 0 with
# "SKIP: no backend at <url>" when no server answers. See docs/building.md
# "Live backend check".
# See docs/building.md.

QMAKE6 := $(shell command -v qmake6 2>/dev/null)

.PHONY: all db cli runner gui test test-live test-live-llamacpp test-live-openai clean

all: db cli runner

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

# ---- LIVE backend check (scripts/live_backend_check.sh) -------------------
# Never part of `make test`: these need a real server. Each dialect has its
# OWN base_url variable, because the two dialects are normally two different
# servers. Override: LIVE_BASE_openai=http://host:port LIVE_MODEL=<id>
# LIVE_KEY=<bearer key> (omit it for a keyless localhost server).
# Exit codes of the script: 0 usable (or SKIP when no server answers),
# 1 a server answered and a hard step failed, 2 usage/build error.
LIVE_BASE_llamacpp ?= http://127.0.0.1:8080
LIVE_BASE_openai   ?= http://127.0.0.1:8080
LIVE_MODEL         ?=
LIVE_KEY           ?=
LIVE_ARGS = $(if $(LIVE_MODEL),--model $(LIVE_MODEL),) $(if $(LIVE_KEY),--key $(LIVE_KEY),)

# Both dialects, each against its own base. Red only when NEITHER dialect is
# usable at its base_url: when both bases point at the same OpenAI-dialect
# server (the default here), the llamacpp one exits 1 BY DESIGN - the model is
# mis-registered for that server, not a tool bug (docs/building.md).
test-live: cli runner
	@bash scripts/live_backend_check.sh --dialect llamacpp --base-url $(LIVE_BASE_llamacpp) $(LIVE_ARGS); rc_l=$$?; \
	echo ""; \
	bash scripts/live_backend_check.sh --dialect openai --base-url $(LIVE_BASE_openai) $(LIVE_ARGS); rc_o=$$?; \
	echo ""; \
	echo "== make test-live summary (0 = usable or SKIP, 1 = a server answered and failed)"; \
	echo "   llamacpp  $(LIVE_BASE_llamacpp)  exit $$rc_l"; \
	echo "   openai    $(LIVE_BASE_openai)  exit $$rc_o"; \
	if [ "$$rc_l" != 0 ] && [ "$$rc_o" != 0 ]; then \
		echo "   neither dialect is usable at its base_url"; exit 1; \
	fi; \
	echo "   at least one dialect is usable (the refusal of the other dialect at the same server is expected)"

test-live-llamacpp: cli runner
	bash scripts/live_backend_check.sh --dialect llamacpp --base-url $(LIVE_BASE_llamacpp) $(LIVE_ARGS)

test-live-openai: cli runner
	bash scripts/live_backend_check.sh --dialect openai --base-url $(LIVE_BASE_openai) $(LIVE_ARGS)
