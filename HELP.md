# Help Me !

## Start Devel

### Deps

- C compiler (MinGW/MSYS2 or gcc/clang)
- SQLite 3
- Qt 6 (Core, Widgets) — GUI only
- cJSON — CLI + runner
- curl — runner

### MinGW64 (MSYS2)

```sh
pacman -Syu
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-make
pacman -S mingw-w64-x86_64-qt6
pacman -S mingw-w64-x86_64-curl
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

### Linux

Easy peasy

### Tests

```sh
cd acta_db       && make test   # C unit tests
cd ../acta_cli && make test  # per-entity CLI tests
cd ../acta_runner  && make test  # pipeline tests against a local stub backend
```
