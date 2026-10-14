# Building

## Dependencies

- C compiler (MinGW/MSYS2 or gcc/clang)
- SQLite 3
- Qt 6 (Core, Widgets) — GUI only
- cJSON — acta_db (config-file parser), CLI, runner + GUI
- curl — runner + GUI
- a backend **server**, runtime only: a llama.cpp `llama-server` in router
  mode (the `llamacpp` dialect) or any strict OpenAI-compatible server
  (the `openai` dialect — Strata, vLLM, …). ACTA ships no server; see
  [`llamacpp_server_contract.md`](llamacpp_server_contract.md) and
  [`openai_dialect_contract.md`](openai_dialect_contract.md).

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

## WSL2 (Linux build reaching a backend hosted on the Windows machine)

The C targets build natively in WSL (verified on the `insightcae` distro,
Ubuntu 22.04, gcc 11.4, `libcjson 1.7.15`, `libcurl 7.81`, sqlite 3.37.2).
Nothing about the Makefiles is Windows- or Linux-specific: `CC ?= cc` and
`EXEEXT` handle both.

```sh
sudo apt install build-essential make \
    libsqlite3-dev libcjson-dev libcurl4-openssl-dev   # qt6-base-dev for the GUI
```

A convenient workflow is to keep editing in the Windows tree and rsync it into
WSL (run from MSYS2 bash; `MSYS2_ARG_CONV_EXCL='*'` is required or MSYS bash
mangles the paths handed to `wsl.exe`):

```bash
MSYS2_ARG_CONV_EXCL='*' wsl.exe -d <distro> -e bash -lc \
 'rsync -a --include="acta_test_ref.db" \
    --exclude=".tmp/" --exclude=".tmp_build/" --exclude="build/" \
    --exclude="*.o" --exclude="*.exe" --exclude="*.db" --exclude="*.log" \
    --exclude="out" --exclude="acta_db_out" --exclude="*.a" --exclude="*.so" \
    --exclude="test/" --exclude="tmp/" --exclude="MYTEST/" \
    /mnt/c/<path>/ACTA_Gamma/ ~/ACTA_Gamma/ && cd ~/ACTA_Gamma && make all'
```

- `--include="acta_test_ref.db"` **before** `--exclude="*.db"`: copies the
  `acta_cli` test fixture along with the sources. It is a convenience, not a
  requirement — `acta_cli/Makefile` regenerates the `.db` from the tracked
  `acta_test_ref.sql` whenever it is missing or stale (see **Tests**) — but
  dropping the include means the first `make test` rebuilds it. `make test` in
  `acta_cli/` must run with cwd `acta_cli/` either way.
- The `*.o *.a *.so *.exe build/ out/` excludes matter: a Windows tree full of
  PE objects would otherwise be relinked into ELF binaries (and fail).

**Reaching a backend that runs on Windows.** Under the default **NAT** WSL
networking, `127.0.0.1` inside WSL is *not* the Windows loopback — a server
bound to `0.0.0.0:8080` on Windows is reached through the Windows host address
of the `vEthernet (WSL …)` adapter, i.e. the WSL default-route gateway
(`172.28.160.1` on this machine; `awk '$2=="00000000"{print $3}'
/proc/net/route` decoded, or the LAN IP if the server binds broadly). So the
model record's `base_url` must be `http://<windows-host-ip>:8080`, never
`http://127.0.0.1:8080`. `scripts/probe_strata.sh` derives that address
automatically and runs from either side.

- The **first** WSL→Windows connect can time out (Hyper-V firewall cold path);
  a retry succeeds. `scripts/probe_strata.sh` retries its GETs for exactly this
  reason.
- Minimal distros often lack `curl`, `ip` and `pkg-config`. `curl.exe` from a
  mounted Windows toolchain works, and the scripts fall back to `python3` for
  JSON parsing and reachability checks.
- `scripts/wsl_strata_demo.sh` runs the whole flow (init → model → skills →
  context → executions → `check` → `run`) from WSL against a Windows-hosted
  server, and is the quickest way to prove a WSL build + NAT path end to end.
- `scripts/live_backend_check.sh` is the make-wired version of that proof: it
  takes `--dialect` explicitly, works from either side (`curl.exe` is used when
  the distro has no native `curl`, the host IP is derived from `/proc/net/route`),
  and asserts the audit keys afterwards instead of just printing the rows.
  `make test-live-openai LIVE_BASE_openai=http://<windows-host-ip>:8080` is the
  one command that re-proves a WSL build against a Windows-hosted server.
  All three scripts are listed in the table under **Live backend check**.

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

`acta_cli` seeds every suite from the fixture `acta_cli/acta_test_ref.db`, which
is **git-ignored**: the tracked `acta_test_ref.sql` (a plain `BEGIN TRANSACTION
… COMMIT` dump) is its source of truth, and the `acta_cli` Makefile regenerates
the `.db` from it whenever it is missing or older than the `.sql` — a fresh
clone therefore needs nothing but `make test`. Edit the `.sql`, never the `.db`.
The rule needs the `sqlite3` command-line tool (`make SQLITE3=/path/to/sqlite3
test` to point at another one); without it the build stops with a named error
instead of letting all 17 suites fail on a missing file. The CLI suites must run
with `cwd = acta_cli/` — each one copies the fixture into `acta_cli/tmp/`.

The runner also offers `make test-e2e` (dead-runner end-to-end suite, spawns real `acta_runner` child processes, ~10-15 s) and the two **live** targets — `make smoke`, which **builds only** `tests/llama_smoke` (the C live check of either dialect), and `make smoke-live SMOKE_DIALECT=…`, which builds **and runs** it; `make test-live*` runs the same kind of check through `scripts/live_backend_check.sh` and adds the full pipeline. They are never part of `make test`: see **Live backend check** below. For real use — `acta_runner run` or the GUI — the API key must come from `$OPENAI_API_KEY` or, when that variable is unset, the `"api_key"` key of the config file (`ACTA_Gamma.conf` in the app-data directory; no key anywhere → hard error, empty key → warning; see the README `# Configuration` section).

## Live backend check (`make test-live*`, `make smoke-live`)

Nothing in `make test` ever touches a real server — the pipeline suites speak to
the in-tree stub (`acta_runner/tests/stub_server.c`). The live checks are
separate targets, and they are **never** part of `make test`.

| Live entry point | What it is | Why it exists |
|---|---|---|
| `scripts/live_backend_check.sh` — `make test-live`, `make test-live-llamacpp`, `make test-live-openai` | the generic live check in **either** dialect: probe phase (the HTTP surface the runner uses) + pipeline phase (scratch DB: `db init` → model → 2 skills → context → 2 executions → `check` → `run` ×2 → status, `raw_response` and audit-key assertions) | the make-wired entry point: any `base_url`, either dialect, both platforms |
| `acta_runner/tests/llama_smoke.c` — `make smoke` (**build only**), `make smoke-live` (build **and** run) | the C live check of one dialect; it builds its request bodies through `src/dialect.c`, so what it sends is what `run.c` sends | the check in the runner's own language, with no CLI and no DB involved |
| `scripts/probe_strata.sh` | the Strata investigation record (health / models / chat / both envelopes / the WSL cross-side check) | kept untouched: `docs/archive/session_strata_backend.md` §2.5 cites it as a verification record |
| `scripts/wsl_strata_demo.sh` | the WSL-over-NAT end-to-end demo | kept untouched: cited by the same doc; the quickest proof of a WSL build + NAT path |

### `scripts/live_backend_check.sh`

```sh
bash scripts/live_backend_check.sh --dialect openai --base-url http://127.0.0.1:8080 \
    --key - --model <id>
# the same through make (builds cli + runner first):
make test-live-openai LIVE_MODEL=<id>
make test-live-llamacpp LIVE_BASE_llamacpp=http://127.0.0.1:8081 LIVE_MODEL=<id>
```

Invoke it as `bash scripts/…`: `core.fileMode=false` on this box, so a committed
script lands `100644` in git (that is already true of `scripts/probe_strata.sh`)
and is not executable after a fresh clone on Linux.

- `--dialect llamacpp|openai` is **required** — there is deliberately no
  default: the two dialects expect different servers, and a silent default is how
  the older live scripts drifted. `--base-url` / `--model` / `--key` /
  `--timeout` (or the env `ACTA_LIVE_BASE` / `ACTA_LIVE_MODEL` /
  `ACTA_LIVE_KEY` / `ACTA_LIVE_TIMEOUT`), `--probe-only` / `--pipeline-only` to
  run one phase, `--keep-db` to keep the scratch DB, `-h` for the full header.
- **probe phase**: `/health` (one retry — the WSL→Windows NAT cold path drops the
  first packet) → `/v1/models` (the id must be served) → plain chat →
  `response_format json_object` (informational) → the **chosen** dialect's
  `json_schema` envelope (**hard**: a refusal is exactly what blocks a skill with
  `output_schema`) → the **other** dialect's envelope (informational: a refusal
  there is issue A, the expected answer from a server that speaks the chosen
  dialect) → `GET /` (informational, judged by **body shape** — Strata answers
  HTTP 200 with a non-catalog body, so the status code proves nothing and the
  runner records `"catalog":null` either way) → the cross-side reachability note
  (`ACTA_LIVE_CROSS=0` disables it).
- **pipeline phase**: a scratch DB in `$TMPDIR` / `$TEMP` / `$TMP` (else `/tmp`)
  named `acta_live_<dialect>.db` — **never** the repo `acta.db`, removed at exit
  unless `--keep-db`. `db init` → `model create backend=<dialect>` → 2 skills
  (plain + `output_schema` **as a JSON string**, `docs/cli_spec.md` issue C) →
  `context create` → 2 `exec create` → `check <model record id>` → `run` ×2 →
  both rows `completed`, exec 2's `raw_response` parses and satisfies the schema
  (`label`, `confidence`), and the `execution_log` audit keys name the dialect
  that produced them: `preflight_passed.max_context > 0` with
  `max_context_source`, `catalog` (`null` on `openai`), `llm_request.dialect`,
  `dialect_source == model_revision`, `model_backend == <dialect>`, and
  `response_format` false on the plain skill / true on the schema skill.
- **Key policy**: the runner reads the key only from `$OPENAI_API_KEY` (decision
  4), so the script always exports it — `--key VALUE` sets it, keyless sets it to
  `""` (the documented keyless-localhost warning). An **unset** variable would be
  a hard runner error.
- **Exit codes**: `0` usable as a backend — **or** no server answered at all,
  which prints `SKIP: no backend at <url>`: a live target must never be red just
  because a server is switched off. `1` a server answered and a hard step failed.
  `2` usage or build error (missing `--dialect`, `acta_cli`/`acta_runner` not built).
- `make test-live` runs **both** dialects, each against its own
  `LIVE_BASE_llamacpp` / `LIVE_BASE_openai` (two dialects are normally two
  servers), and is red only when NEITHER is usable. With both bases pointing at
  the same OpenAI-dialect server — the defaults — the `llamacpp` run exits 1
  **by design**: the model is mis-registered for that dialect, which is a
  registration fact, not a tool bug. `make test-live-llamacpp` /
  `test-live-openai` are the strict per-dialect targets; `LIVE_MODEL` and
  `LIVE_KEY` are passed through when set.
- `make smoke-live` needs `SMOKE_DIALECT=llamacpp|openai` for the same reason
  (`SMOKE_BASE` / `SMOKE_KEY` / `SMOKE_MODEL` / `SMOKE_PROMPT` /
  `SMOKE_TIMEOUT` otherwise default to localhost, keyless, first id served,
  "Say hello.", 60 s).

Two output-shape facts the script depends on, worth knowing when writing your
own check: a successful `acta_runner run` prints **nothing** on stdout (the whole
audit lives in `execution_log`, and `log list <exec_id>` returns a JSON array
whose `metadata` is a **string** containing JSON), and a freshly `db init`-ed
scratch DB prints `[warn] db opened with degraded pragma state…` on **stderr**
for every call — informational, never a failure, so only stdout is parsed.

## Build notes

- `CC ?= cc` picks up gcc/clang unchanged on Linux and Xcode clang on macOS; `EXEEXT` is empty under POSIX make and `.exe` under Windows (mingw) make, so the same Makefiles build on both.
- The Makefiles link `-lsqlite3`, `-lcjson`, and `-lcurl` straight from the system packages above.
- If a dependency lives elsewhere, override the paths: `make CJSON_DIR=/opt/cjson/include CJSON_LIB=/opt/cjson/lib/libcjson.a` (CLI/runner), `make CURL_INC=... CURL_LIB=...` (runner).

## GUI–runner source coupling (hard constraint)

The GUI's qmake project (`acta_gui/src/src.pro`, via the
`acta_gui/acta_gui.pro` subdirs wrapper) compiles the runner's own
source files `acta_runner/src/run.c`, `acta_runner/src/backend.c`,
`acta_runner/src/dialect.c`,
`acta_runner/src/argparse.c` (the `cmd_args_*` helpers used by `cmd_run`)
and `acta_runner/src/deathmark.c` (the death-marker claim/release helpers
used by `run_execution`) directly, so the in-app **Run** button executes
the same pipeline as `acta_runner` without building the `acta_runner`
binary. There is a single
pipeline codebase, not a second copy: the atomic `pending → running` claim
and the state-transition logic are shared, not duplicated — both paths use
the single `acta_db_execution_start` in `acta_db`.

**Constraint: the runner sources the qmake project compiles (`run.c`,
`backend.c`, `dialect.c`, `argparse.c` and `deathmark.c` in `acta_runner/src/`)
may not be moved or renamed without updating `acta_gui/src/src.pro`.** The qmake
project references them by path; moving them (e.g. into a `pipeline/`
directory) breaks the GUI build — the C targets still build fine, only the
`qmake6`/`make` step in `acta_gui/` fails. The same rule applies when a new
runner source joins the pipeline: `src/dialect.c` (the backend dialects) had to
be added to `SOURCES` when `run.c` started using it.

`acta_cli` shares the dialect module the same way rather than duplicating it:
`acta_cli/Makefile` compiles `../acta_runner/src/dialect.c` into its own
`src/dialect.o` (`-I../acta_runner/src` for the header), so the backend
vocabulary has exactly one definition in the repository.

The same coupling applies to **headers**. `acta_runner/include/runner_util.h`
includes `"dialect.h"`, and `dialect.h` lives in `acta_runner/src/`; a quoted
include resolves relative to the *including* file, so every consumer of
`runner_util.h` needs `acta_runner/src/` on its include path. The runner's own
Makefile already passes `-Isrc` and `acta_cli` passes `-I../acta_runner/src`,
so `src.pro` must mirror them with
`INCLUDEPATH += $$PWD/../../acta_runner/src` — without it the GUI build stops
in `runnerWorker.cpp` with `fatal error: dialect.h: No such file or directory`
(found when the GUI was first compiled, 2026-10-12; the C targets build fine
either way).

The generated `acta_gui/Makefile` and `acta_gui/src/Makefile{,.Debug,.Release}`
are git-ignored build artifacts. **After any `src.pro` change, delete them and
let qmake regenerate them**: a stale generated Makefile compiles the old
`SOURCES` list and silently omits a newly added source (the tree once carried
Makefiles with zero mentions of `dialect` next to a `src.pro` that listed it).
qmake also creates `src/build/{obj,moc,rcc,ui}` when it generates the
Makefiles, so deleting `src/build/` while keeping the Makefiles makes `uic`
fail with `Could not create output file` — delete the Makefiles too (or
recreate the directories).
