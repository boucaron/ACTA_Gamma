# Building

## Dependencies

- C compiler (MinGW/MSYS2 or gcc/clang)
- SQLite 3
- Qt 6 (Core, Widgets) — GUI only
- cJSON — acta_db (config-file parser), CLI, runner + GUI
- curl — runner + GUI
- llama.cpp — local backend server (`llama-server` in router mode), runtime only

The C targets build with plain `make` on Windows (MinGW/MSYS2), Linux (gcc/clang), and macOS (Xcode clang); the Qt 6 GUI additionally needs `qmake6` on any of those platforms.

## MinGW64 (MSYS2)

```sh
pacman -Syu
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-make
pacman -S mingw-w64-x86_64-sqlite
pacman -S mingw-w64-x86_64-qt6
pacman -S mingw-w64-x86_64-curl
pacman -S mingw-w64-x86_64-cjson
```

## Linux (Debian/Ubuntu)

```sh
sudo apt install build-essential make \
    libsqlite3-dev libcjson-dev libcurl4-openssl-dev qt6-base-dev
```

## macOS

```sh
xcode-select --install                 # Xcode command-line tools (clang, make)
brew install sqlite cjson curl qt
brew install llama.cpp                # local backend server (llama-server)
```

## Build

Per component:

```sh
cd acta_db          && make            # libacta_db + unit tests
cd ../acta_cli      && make            # acta_cli CLI
cd ../acta_runner   && make            # acta_runner (needs a running
                                       # OpenAI-compatible backend)
cd ../acta_gui      && qmake6 "CONFIG+=debug" acta_gui.pro -o Makefile
make
```

Or, from the repository root, build the core in dependency order with the
 top-level Makefile — the Qt 6 GUI is **optional and not built by default**:

```sh
make all     # acta_db -> acta_cli -> acta_runner (core; the GUI is not built)
make gui     # GUI only; explicit, errors if qmake6 is not installed
make test    # all three C test suites
make -C acta_runner test-e2e    # optional: dead-runner end-to-end suite
                                # (real runner processes; ~10-15 s)
make clean   # cleans the three C targets; the GUI is cleaned too when
                                # acta_gui/Makefile exists (warning otherwise)
```

(The GUI can still be built directly in `acta_gui/` with
 `qmake6 "CONFIG+=debug" acta_gui.pro -o Makefile && make`; `make gui`
generates that Makefile for you when it is missing.)

## Tests

```sh
cd acta_db       && make test   # C unit tests
cd ../acta_cli   && make test   # per-entity CLI tests
cd ../acta_runner && make test   # pipeline tests against a local stub backend
```

The runner also offers `make test-e2e` (dead-runner end-to-end suite, spawns real `acta_runner` child processes, ~10-15 s) and `make smoke` (manual check of `tests/llama_smoke` against a LIVE llama.cpp `llama-server`; `llama_smoke` takes the key as a CLI argument, or `-` for no auth). For real use — `acta_runner run` or the GUI — the API key must come from `$OPENAI_API_KEY` or, when that variable is unset, the `"api_key"` key of the config file (`ACTA_Gamma.conf` in the app-data directory; no key anywhere → hard error, empty key → warning; see the README *Environment variables and the per-machine config file* section).

## Build notes

- `CC ?= cc` picks up gcc/clang unchanged on Linux and Xcode clang on macOS; `EXEEXT` is empty under POSIX make and `.exe` under Windows (mingw) make, so the same Makefiles build on both.
- The Makefiles link `-lsqlite3`, `-lcjson`, and `-lcurl` straight from the system packages above.
- If a dependency lives elsewhere, override the paths: `make CJSON_DIR=/opt/cjson/include CJSON_LIB=/opt/cjson/lib/libcjson.a` (CLI/runner), `make CURL_INC=... CURL_LIB=...` (runner).

## GUI–runner source coupling (hard constraint)

The GUI's qmake project (`acta_gui/src/src.pro`, via the
`acta_gui/acta_gui.pro` subdirs wrapper) compiles the runner's own
source files `acta_runner/src/run.c`, `acta_runner/src/backend.c`,
`acta_runner/src/argparse.c` (the `cmd_args_*` helpers used by `cmd_run`)
and `acta_runner/src/deathmark.c` (the death-marker claim/release helpers
used by `run_execution`) directly, so the in-app **Run** button executes
the same pipeline as `acta_runner` without building the `acta_runner`
binary. There is a single
pipeline codebase, not a second copy: the atomic `pending → running` claim
and the state-transition logic are shared, not duplicated — both paths use
the single `acta_db_execution_start` in `acta_db`.

**Constraint: the runner sources the qmake project compiles (`run.c`,
`backend.c`, `argparse.c` and `deathmark.c` in `acta_runner/src/`) may not
be moved or renamed without updating `acta_gui/src/src.pro`.** The qmake
project references them by path; moving them (e.g. into a `pipeline/`
directory) breaks the GUI build — the C targets still build fine, only the
`qmake6`/`make` step in `acta_gui/` fails.
