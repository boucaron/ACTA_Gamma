# Session notes — Strata backend + WSL (Linux) build

Handoff document. Date: 2026-10-12. Branch state: **all changes uncommitted** (working tree).

Goal of the work: let `acta_runner` use **Strata** (a local OpenAI-compatible
llama.cpp/ggml-derived server already installed on this Windows machine) as a
model backend, and build/run ACTA Gamma natively **inside WSL** reaching Strata
through **WSL2 NAT** (no mirrored networking).

---

## 1. Environment facts (verified)

| Item | Value |
|---|---|
| Windows | 10.0.26300.9457, host name `amdjb` |
| WSL | WSL 2.4.13, kernel 5.15.167.4-microsoft-standard-WSL2 |
| WSL distro | `insightcae` (Ubuntu 22.04 jammy), user `user`, HOME `/home/user` |
| WSL networking | **NAT** (`.wslconfig` only sets `memory=`), interface `vEthernet (WSL (Hyper-V firewall))` |
| Windows host IP from WSL | **172.28.160.1** (default-route gateway); LAN IP `192.168.178.56` also works |
| `127.0.0.1:8080` from WSL | refused (WSL loopback ≠ Windows loopback) — expected under NAT |
| First connect WSL→host | can time out once (NAT/Hyper-V firewall cold path); a retry succeeds |
| Strata install | `C:\Users\admin\Downloads\Strata-main` (data: `..\Strata-data`), started by `run-iq3_xxs.bat` → `serve/server.py --engine strata --config strata-iq3_xxs.json --port 8080` |
| Strata model id | `qwen3.8-flash-next-iq3_xxs` |
| Strata binding | `0.0.0.0:8080` (netstat LISTENING) → reachable from WSL via the host IP |
| Strata `/health` | `{"status":"ok","max_context":204800,"model":"qwen3.8-flash-next-iq3_xxs","images":false,"api_key":false,"loaded":true,"service":"strata"}` |
| Strata `/v1/models` | entry has `id`, `status.value`, `meta.n_ctx` — **no top-level `max_context`** |
| Strata structured output | accepts `response_format` `json_object` **and** OpenAI `json_schema` envelope (`{"type":"json_schema","json_schema":{"name","strict","schema"}}`) |
| WSL packages installed | `build-essential`, `libsqlite3-dev`, `libcjson-dev 1.7.15`, `libcurl4-openssl-dev 7.81` (passwordless sudo works) |
| WSL tools present | gcc 11.4, make, rsync, tar, apt-get, python3 (`/opt/insightcae/python3.6/bin/python3`) |
| WSL tools missing | `curl`, `ip`, `pkg-config` (use `curl.exe` from PATH: `/mnt/c/Program Files/Git/mingw64/bin/curl.exe`) |
| MSYS2 side | `jq` segfaults (avoid it); `libcjson-1.dll` lives in `/c/msys64/mingw64/bin` |

---

## 2. What is DONE

### 2.1 New scripts (untracked, `scripts/`)

| File | Purpose | Status |
|---|---|---|
| `scripts/probe_strata.sh` | Probes the exact surface `acta_runner` uses: `GET /health`, `GET /v1/models` (+ model-id presence), `POST /v1/chat/completions`, `POST` with `response_format json_object`, and a cross-side WSL reachability check. Runs from **either** MSYS2 bash or inside WSL (auto-detects WSL, derives the host IP, falls back to `curl.exe`, retries the first GET). Exit 0/1/2. | works, exit 0 from both sides |
| `scripts/wsl_strata_demo.sh` | WSL-side end-to-end demo: `db init` → model → 2 skills (plain + `output_schema`) → context → 2 executions → `check` → `run 1` → `run 2` → `exec get` → `log list` | works; run 1 completes, run 2 fails (issue A below) |

Probe output (from inside WSL, NAT):

```
== Strata probe: http://172.28.160.1:8080
1 /health                        status=ok model=qwen3.8-flash-next-iq3_xxs max_context=204800
2 /v1/models                     served: qwen3.8-flash-next-iq3_xxs
  model id present               qwen3.8-flash-next-iq3_xxs
3 chat/completions               Hello!
4 response_format json_object    {"label":"negative","confidence":0.5} [finish=stop]
```

### 2.2 WSL build tree

Linux build lives in **`~/ACTA_Gamma`** (`/home/user/ACTA_Gamma`) inside WSL, rsynced from
`/mnt/c/msys64/home/admin/github/boucaron/ACTA_Gamma`. Native ELF binaries produced:
`acta_cli/acta_cli`, `acta_runner/acta_runner`, `acta_db/libacta_db.a`, `libacta_db.so`.

Sync + build + demo commands (run from MSYS2 bash):

```bash
# sync (keep acta_test_ref.db — the CLI test harness needs it)
MSYS2_ARG_CONV_EXCL='*' wsl.exe -d insightcae -e bash -lc \
 'rsync -a --include="acta_test_ref.db" \
    --exclude=".tmp/" --exclude=".tmp_build/" --exclude="build/" --exclude="*.o" \
    --exclude="*.exe" --exclude="*.db" --exclude="*.log" --exclude="out" \
    --exclude="acta_db_out" --exclude="*.a" --exclude="*.so" --exclude="test/" \
    --exclude="tmp/" --exclude="MYTEST/" \
    /mnt/c/msys64/home/admin/github/boucaron/ACTA_Gamma/ ~/ACTA_Gamma/'

# build
MSYS2_ARG_CONV_EXCL='*' wsl.exe -d insightcae -e bash -lc 'cd ~/ACTA_Gamma && make all'

# end-to-end demo against Strata over NAT
MSYS2_ARG_CONV_EXCL='*' wsl.exe -d insightcae -e bash -lc 'cd ~/ACTA_Gamma && bash scripts/wsl_strata_demo.sh'
```

`MSYS2_ARG_CONV_EXCL='*'` + `//mnt/c/...` are required, otherwise MSYS bash mangles
paths passed to `wsl.exe`.

### 2.3 Bugs found (via ASAN) and FIXED — 10 files, all verified on both platforms

| # | File | Bug | Fix |
|---|---|---|---|
| 1 | `acta_db/Makefile` | no feature-test macro: `strdup()` implicitly declared under `-std=c11` on glibc → returns `int`, pointer truncated on x86-64 | added `-D_POSIX_C_SOURCE=200809L` to `CFLAGS` |
| 2 | `acta_db/Makefile` | `libacta_db.so` link fails on x86-64 Linux (`relocation R_X86_64_PC32 … recompile with -fPIC`) | added `CFLAGS += -fPIC` |
| 3 | `acta_db/src/db.c` ×2 | `sqlite3_free(db->path)` on a `db_strdup()`/`malloc()` pointer → `free(): invalid pointer`, every CLI command aborted | `free(db->path)` in `acta_db_close()` and `acta_db_force_close()` |
| 4 | `acta_cli/src/cli_main.c`, `acta_runner/src/main.c` | `if (const char *note = …)` — declaration in a condition with a qualifier: GCC 11 (Linux) rejects it, MinGW accepted it | hoisted to `const char *note = …; if (note)` |
| 5 | `acta_runner/src/backend.c` | `CURLE_OPERATION_TIMED` never existed in any curl; the `#else` branch (curl < 8.8) could not compile (Ubuntu 7.81) | version test `>= 0x073200` → `CURLE_OPERATION_TIMEDOUT`, else `CURLE_OPERATION_TIMEOUTED` |
| 6 | `acta_runner/src/check.c` | **use-after-free**: `strdup(m->base_url / m->model_identifier)` after `acta_db_model_free(m)` → garbage JSON (`"model":"\ufffd5_…"`) and a false `verdict:"server unreachable"` | copy before releasing the row; hoisted `model_t *m`, `if (m) acta_db_model_free(m);` after the copies |
| 7 | `acta_runner/src/sweep.c` | **use-after-free** in `last_activity()`: kept `rows[n-1]->created_at` past `acta_db_execution_log_list_free()` → stale rows misclassified as fresh (5 test failures) | copy the timestamp into a local `char last_log[32]` before freeing the list |
| 8 | `acta_runner/tests/stub_server.c` | `send(..., 0)` → POSIX **SIGPIPE** kills the test process when the runner closes the socket (exit 141 on Linux) | `SEND_FLAGS` = `MSG_NOSIGNAL` on POSIX, `0` on Windows |
| 9 | `acta_cli/tests/model/model_test_helpers.c` ×2, `model_test_misc.c` ×1 | `free(errmsg)` on a `sqlite3_exec` message (sqlite3-allocated) → abort in `test_model` | `sqlite3_free(errmsg)` |

`git diff --stat` (uncommitted): 10 files, +53 / −28.

### 2.4 Test status after the fixes

| Suite group | Linux (WSL `~/ACTA_Gamma`) | Windows (MinGW) |
|---|---|---|
| `acta_db` | **ALL TESTS PASSED** (rc 0) | not re-run this session |
| `acta_cli` (16 suites: cli_util, conf, context, db, dbpath, exec, execution_log, gparse, help, json, model, model_folder, model_revision, skill, skill_folder, skill_revision, tools) | **16/16 pass** | not re-run this session |
| `acta_runner` | **11/11 suites, 446 checks, 0 failures** | 10/10 re-run: run 102/0, sweep 43/0, check 38/0, pending 29/0, autopreflight 37/0, api_key 16/0, conf 36/0, deathmark 20/0, deleted 12/0, shadow 35/0 (+ argparse 47/0) |

ASAN was used to find #3–#7, #9: `make CC="cc -fsanitize=address -O0 -g" all` / `… -C acta_cli test`.
Remaining ASAN finding: a 16-byte leak **inside libsqlite3** on `db init` (not ACTA code).

### 2.5 End-to-end against Strata

| Path | Result |
|---|---|
| Windows build, `base_url http://127.0.0.1:8080` | `check` ok, `run` completed, validated JSON result |
| Windows build, `base_url http://172.28.160.1:8080` | ok (Strata binds 0.0.0.0) |
| **WSL build, `base_url http://172.28.160.1:8080`** | `check` → `{"ok":true,…,"max_context":0}`; `run` (plain skill) → **completed**, HTTP 200 ~1.5 s, real summary stored |
| WSL build, skill **with** `output_schema` | **failed**: `HTTP 400 response_format.json_schema needs a schema object` (issue A) |

Scratch DBs used: `/tmp/acta_strata.db` (Windows side), `~/ACTA_Gamma/acta_wsl.db` (WSL side).

---

## 3. What REMAINS

> **Progress update (same day, later session):** issues **A** and **B** are now
> implemented — Phases 1–3 of the dialect plan are done and green on Windows and
> WSL. See [`docs/session_dialect_progress.md`](session_dialect_progress.md)
> for the state, the remaining Phase 3 asserts, and the next resume prompt.
> The four open decisions below (§3 A/B, J) were answered in
> [`docs/plans/backend_dialects.md`](plans/backend_dialects.md) §9.

> The design for issues A and B is now planned in
> [`docs/plans/backend_dialects.md`](plans/backend_dialects.md) — read it before
> touching A/B. Decision already taken there (§3.2): the dialect comes from
> `model_revision.backend`; `--dialect` is only a `check`-standalone option and
> a logged `run <id>` override.

| # | Task | Where | Notes / decision needed |
|---|---|---|---|
| **A** | **Strata `response_format` envelope** (the blocker for skills with `output_schema`) | `acta_runner/src/run.c` ~lines 796, 967–974 | ACTA sends the llama.cpp shape `{"type":"json_schema","schema":{…}}` (documented in `docs/llamacpp_server_contract.md` §4). Strata needs the OpenAI shape `{"type":"json_schema","json_schema":{"name":…,"strict":true,"schema":{…}}}` — verified working by hand. Options: (a) always send the OpenAI shape, (b) branch on `model.backend` (`openai` vs `strata`), (c) add a `model.configuration` key e.g. `response_format_style`. Pick one and update `docs/runner_contract.md` + `docs/llamacpp_server_contract.md` |
| **B** | `max_context` is 0 for Strata | `acta_runner/src/backend.c` ~line 349 | ACTA reads `max_context` from the matched `/v1/models` entry; Strata exposes `meta.n_ctx` there and `max_context` in `/health`. Add fallbacks so the `max_chars` guard gets the real ceiling (204 800). Also `preflight_passed` records `"catalog":null`: the catalog fetch is `GET /` (llama.cpp-specific); Strata serves `GET /v1/models` (with `meta`) and `GET /props` |
| **C** | `output_schema` silently dropped | `acta_cli/src/json.c` (`JF_STR`), README example | `skill create --json '{"output_schema": { … }}` stores `null`; it must be a JSON **string**. Either accept objects or reject loudly; fix the README example |
| **D** | Docs: WSL build path | `docs/building.md` | Add a WSL section: distro/deps (`build-essential libsqlite3-dev libcjson-dev libcurl4-openssl-dev`), rsync-to-`~` workflow, **host-IP rule** (`172.28.160.1`, never `127.0.0.1` under NAT), cold-path retry, missing `curl`/`ip`/`pkg-config` in `insightcae` |
| **E** | Docs: Strata backend contract | new `docs/strata_server_contract.md` + README | README currently says "the current backend is a local llama.cpp `llama-server`"; add Strata as a supported backend and its differences (`meta.n_ctx`, OpenAI `json_schema`, `/health` fields, `/v1/responses`, `/v1/messages`) |
| **F** | `acta_runner check <id>` semantics | `README.md`, `docs/cli_spec.md` | It takes a **model record id**, not an execution id (`check 4` → `model 4 not found`). Document explicitly |
| **G** | Commit the 10 fixed files | git | Nothing committed yet; also decide whether `scripts/` is committed (`.gitignore` has `**/Makefile` but the four Makefiles are tracked) |
| **H** | GUI on WSL | `acta_gui` | Not attempted: no Qt 6 in `insightcae`; GUI compiles runner sources directly |
| **I** | Full `make test` in one shot | both platforms | Repeatedly interrupted by tool timeouts on WSL; verified suite-by-suite instead. Windows full `make test` never run this session |
| **J** | Decide the backend model | design | Does ACTA need a real `backend: "strata"` implementation, or is `backend: "openai"` + the two fixes (A, B) enough? `model.backend` is free text in the DB today and the runner ignores it entirely |

---

## 4. Known gotchas (do not rediscover them)

| Gotcha | Workaround |
|---|---|
| MSYS `make` on Windows: `Cannot create temporary file in C:\WINDOWS\: Permission denied` | use `mingw32-make` with `TMP='C:/msys64/tmp' TEMP='C:/msys64/tmp'` and `PATH=/c/msys64/mingw64/bin:$PATH` |
| This agent shell has no `/mingw64` mount → `acta_cli.exe`/`acta_runner.exe` exit **127** | `export PATH=/c/msys64/mingw64/bin:$PATH` (provides `libcjson-1.dll`) |
| `jq` segfaults under MSYS2 | parse JSON with `python3` |
| `wsl.exe` output is UTF-16 / CR-terminated; MSYS mangles `/mnt/...` args | pipe through `tr -d '\r'`, prefix `//mnt/c/...`, set `MSYS2_ARG_CONV_EXCL='*'` |
| `acta_cli` test harness | must run with **cwd = `acta_cli/`** and needs the fixture `acta_cli/acta_test_ref.db` (rsync `--exclude="*.db"` must not drop it — or let `acta_cli/Makefile` regenerate it from the tracked `acta_test_ref.sql`) |
| `acta_runner` Makefile test rule prefixes `PATH='/mingw64/bin:…'` | on this shell that path is missing → run the built test binaries directly |
| Stale objects | after changing `CFLAGS`, run `make clean` (or per-dir `make -C acta_db clean`), otherwise old non-PIC `.o` files are relinked |
| rsync from `/mnt/c` also copies Windows `.o/.a/.so` unless excluded | exclude `build/ *.o *.a *.so *.exe out acta_db_out test/ tmp/ MYTEST/` |
| `acta_runner check` | takes a **model record id** |
| `exec create` ids | sequential: the next execution after 1..3 is **4**, not 5 |

---

## 5. Resume prompt for the next session

> Read **both** `docs/session_strata_backend.md` and
> `docs/plans/backend_dialects.md` in this repo (ACTA Gamma,
> `C:/msys64/home/admin/github/boucaron/ACTA_Gamma`).
>
> Context: ACTA Gamma must support **Strata** (`http://127.0.0.1:8080` on Windows,
> `http://172.28.160.1:8080` from WSL2 NAT, model id `qwen3.8-flash-next-iq3_xxs`)
> as a runner backend, and the project must build and pass its tests natively in
> WSL (`insightcae` Ubuntu 22.04, build tree `~/ACTA_Gamma`, synced with the rsync
> command in the session doc). A previous session fixed 10 portability/memory bugs
> (uncommitted: `acta_db/Makefile`, `acta_db/src/db.c`, `acta_cli/src/cli_main.c`,
> `acta_runner/src/{main,check,sweep,backend}.c`, `acta_runner/tests/stub_server.c`,
> `acta_cli/tests/model/model_test_{helpers,misc}.c`) and added
> `scripts/probe_strata.sh` + `scripts/wsl_strata_demo.sh`. All tests pass on both
> platforms; a plain (no-schema) execution already completes from WSL over NAT.
>
> **Already decided — do not reopen:** the backend *dialect* (`llamacpp` vs
> `openai`) is a property of `model_revision.backend`. `--dialect` exists only
> (a) for `acta_runner check --base-url/--model-identifier` standalone mode
> (default `llamacpp`) and (b) as a logged override on `acta_runner run <id>`
> (recorded in `llm_request` metadata as `dialect` + `dialect_source`, `warn`
> when it disagrees with the row, rejected with `--pending`, unavailable to the
> GUI). Unknown backend value → `EXIT_INVALID` before any HTTP call. Full
> rationale and the dialect matrix: `docs/plans/backend_dialects.md` §2–§3.2.
>
> Now do the remaining work, in this order (phases of the plan doc):
> 1. **Phase 1 — ceiling fix, land it alone:** `acta_runner/src/backend.c`
>    `max_context` fallback: entry `max_context` → entry `meta.n_ctx` →
>    `/health` `max_context`; log which one was used. Today Strata yields
>    `max_context: 0`, which disables the `max_chars` guard.
> 2. **Phase 2 — dialect module:** new `acta_runner/src/dialect.{h,c}`
>    (`dialect_id_t`, `dialect_t` with `fetch_catalog`, `sends_top_k`,
>    `build_response_format`, `max_context_of`), `dialect_from_string()`, unit
>    tests in `acta_runner/tests/dialect/`. No behavior change yet.
> 3. **Phase 3 — wire it:** `run.c` builds `response_format` through the dialect,
>    gates `top_k`, and adds `dialect` + `dialect_source` to `llm_request`
>    metadata; `backend_preflight()` takes the dialect; the `GET /` catalog call
>    is gated on `fetch_catalog`. The `llamacpp` path must stay identical.
> 4. **Phase 4 — CLI surface:** `--dialect` for `check` standalone mode and as
>    the `run <id>` override; reject `run --pending --dialect`; update argparse,
>    usage and help text.
> 5. **Phase 5 — the `openai` envelope (issue A):**
>    `{"type":"json_schema","json_schema":{"name":…,"strict":true,"schema":{…}}}`
>    so a skill with `output_schema` works against Strata (today: HTTP 400
>    `response_format.json_schema needs a schema object`).
> 6. **Phase 6 — tests:** give `tests/stub_server.c` a dialect parameter (rejects
>    the wrong envelope, serves `meta.n_ctx`); add `test_dialect` /
>    `test_openai_dialect`; keep the existing 446 runner checks green; verify
>    end-to-end against live Strata with `scripts/probe_strata.sh`.
> 7. **Phase 7 — docs:** WSL section in `docs/building.md`, a Strata/OpenAI
>    dialect contract doc, README backend wording + a Strata example, the
>    `acta_runner check <model-record-id>` clarification, and the
>    `output_schema`-must-be-a-JSON-string clarification (issue C).
>
> **Ask me before Phase 5/6 — four open decisions** (`backend_dialects.md` §9):
> (1) migration `0.2.sql` rewriting old `backend='openai'` rows to `llamacpp`,
> or non-colliding names `llamacpp` + `openai_strict`; (2) validate `backend` in
> `acta_cli` create/update or only at run time; (3) send `top_k` in `openai`
> mode or omit it; (4) catalog audit in `openai` mode: `"catalog":null` or fetch
> Strata's `/props`.
>
> Verify on **both** platforms after each phase: Windows `mingw32-make` with
> `TMP='C:/msys64/tmp' TEMP='C:/msys64/tmp'` and `PATH=/c/msys64/mingw64/bin:$PATH`;
> WSL via the rsync command in the session doc, running test suites one by one
> (long single-shot runs get killed by the tool timeout). Use ASAN
> (`make CC="cc -fsanitize=address -O0 -g" all`) when touching memory code.
>
> Nothing is committed yet — show me the diff per phase and commit only after I
> approve. Follow the "Known gotchas" table in `docs/session_strata_backend.md`
> instead of re-deriving them.
