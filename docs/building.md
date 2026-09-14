# Building

## Dependencies

- C compiler (MinGW/MSYS2 or gcc/clang)
- SQLite 3
- Qt 6 (Core, Widgets) — GUI only
- cJSON — CLI, runner + GUI
- curl — runner + GUI
- llama.cpp — local backend server (`llama-server` in router mode), runtime only

The C targets build with plain `make` on Windows (MinGW/MSYS2) and Linux (gcc/clang); the Qt 6 GUI additionally needs `qmake6` on either platform.

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

Or, from the repository root, build everything in dependency order with the
 top-level Makefile — the Qt 6 GUI is built too when `qmake6` is on `PATH`:

```sh
make all     # acta_db -> acta_cli -> acta_runner, then the GUI
                                # (GUI skipped with a warning if qmake6 is missing)
make gui     # GUI only; errors if qmake6 is not installed
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

The runner also offers `make test-e2e` (dead-runner end-to-end suite, spawns real `acta_runner` child processes, ~10-15 s) and `make smoke` (manual check of `tests/llama_smoke` against a LIVE OpenAI-compatible server).

## Build notes

- `CC ?= cc` picks up gcc/clang unchanged on Linux and Xcode clang on macOS; `EXEEXT` is empty under POSIX make and `.exe` under Windows (mingw) make, so the same Makefiles build on both.
- The Makefiles link `-lsqlite3`, `-lcjson`, and `-lcurl` straight from the system packages above.
- If a dependency lives elsewhere, override the paths: `make CJSON_DIR=/opt/cjson/include CJSON_LIB=/opt/cjson/lib/libcjson.a` (CLI/runner), `make CURL_INC=... CURL_LIB=...` (runner).
