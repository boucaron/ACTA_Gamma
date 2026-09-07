# Help Me !

## Start Devel

### Deps

- C compiler (MinGW/MSYS2 or gcc/clang)
- SQLite 3
- Qt 6 (Core, Widgets) — GUI only
- cJSON — CLI, runner + GUI
- curl — runner + GUI
- llama.cpp — local backend server (llama-server in router mode), runtime only

### MinGW64 (MSYS2)

```sh
pacman -Syu
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-make
pacman -S mingw-w64-x86_64-sqlite
pacman -S mingw-w64-x86_64-qt6
pacman -S mingw-w64-x86_64-curl
pacman -S mingw-w64-x86_64-cjson
```

### Build

See `README.md` for the per-component steps:

```sh
cd acta_db          && make            # libacta_db + unit tests
cd ../acta_cli   && make            # acta_cli CLI
cd ../acta_runner   && make            # acta_runner (needs a running
                                       # OpenAI-compatible backend)
cd ../acta_gui    && qmake6 "CONFIG+=debug" acta_gui.pro -o Makefile
make
```

### Linux (Debian/Ubuntu)

Easy peasy:

```sh
sudo apt install build-essential make \
    libsqlite3-dev libcjson-dev libcurl4-openssl-dev qt6-base-dev
```

### macOS

```sh
xcode-select --install                 # Xcode command-line tools (clang, make)
brew install sqlite cjson curl qt
brew install llama.cpp                # local backend server (llama-server)
```

### Tests

```sh
cd acta_db       && make test   # C unit tests
cd ../acta_cli && make test  # per-entity CLI tests
cd ../acta_runner  && make test  # pipeline tests against a local stub backend
```
