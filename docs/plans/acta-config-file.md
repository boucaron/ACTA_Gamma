# Plan — optional config file: API key, database path, max_chars, default timeout

Status: **done — work items 1 (the JSON config parser), 2
(the key precedence policy), 3 (DB-path resolution), 4 (max_chars /
timeout resolution), 5 (the POSIX permission gate; the Windows DACL
check remains a separate follow-up) and 6 (the GUI surface) are
implemented: the shared `acta_conf` helper
(`acta_conf_parse`, `acta_conf_api_key_status`, `acta_conf_read`,
`acta_conf_default_path`, and the resolution helpers
`acta_conf_resolve_max_chars` / `acta_conf_resolve_timeout` with the
built-in defaults) lives in `acta_db` (`conf.h` / `conf.c`); the key
policy is wired into the runner's `cmd_run`; the config-file `db` step is
in `acta_db_resolve_db_path` (both `acta_dbpath.c` copies) and
`MainWindow::defaultDbPath` (Qt reader), wired into both `main.c` entry
points and the GUI bootstrap, with the `acta_cli/tests/dbpath` suite
extended for the file step; `cmd_run` now resolves `timeout` (--timeout
flag → file → built-in 300 s) and `max_chars` (file → built-in 100,000
chars) and passes both into the pipeline — `run_execution` gained the
`max_chars` limit parameter (the preflight size check that consumes it
lands with `docs/plans/max-chars-size-check.md`); `acta_conf_read` now
runs the POSIX permission gate (a file whose mode has group/other read
bits set is refused fail-closed before its contents are read; on Windows
the st_mode bit check is not run — meaningless under MSYS2/MinGW, the
NTFS DACL check is a separate follow-up, first cut documented
best-effort), mirrored by the GUI's Qt reader, and the dbpath suite pins
the contract mode 0600; `RunnerWorker::runInThread` now does the same
resolution as `cmd_run` — key via `acta_conf_api_key_status` with the
Qt-parsed file key (absent = NULL, present-but-empty = ""), `timeout`
file → built-in 300 s, and `max_chars` file → built-in 100,000 chars
passed as the `run_execution` limit parameter — a malformed config file
(or permission-gate failure) is a fail-closed hard error before any
claim, and the Qt reader is shared with the GUI bootstrap via
`confreader.h`; work item 7 (the tests) is also implemented: the
`acta_cli/tests/conf` suite pins the unit contract
(`acta_conf_parse` fail-closed rules, `acta_conf_api_key_status`
precedence — env-set-wins even when the env var is empty, file fallback,
missing file + unset env = the existing hard error — `acta_conf_read`
missing/malformed/bad-permissions behaviour, `acta_conf_default_path`,
and the `max_chars`/`timeout` file-over-builtin resolution), and
`acta_runner/tests/run/test_conf.c` pins the end-to-end `cmd_run`
behaviour (file fallback sends the Bearer header with the file key,
env-set-wins, empty-env-wins with no Authorization header, 0644
refusal fail-closed before any claim on POSIX, malformed/unknown-key/
wrong-type hard error before any claim; the stub server gained an
`Authorization`-header capture for that) — the DB-path order
with/without the file was already pinned by the `dbpath` suite
(work item 3); work item 8 (docs) is also implemented: the README
"Environment variables and the per-machine config file" section
(key precedence, DB-path order including the file step, and the
config-file contract), `docs/cli_spec.md`'s DB-file paragraph (the file
step in the resolution order, the fail-closed hard-error rule, and the
lockstep list), `docs/runner_contract.md` decisions 4 (auth: env →
file fallback, fail-closed) and 5 (timeout: flag → file → built-in),
and the `max_chars` limit-source line in
`docs/plans/max-chars-size-check.md` (file → built-in default, resolved
by the shared helper). All work items are now implemented, and the
consumer side — the preflight size check that consumes the resolved
`max_chars` limit — has also shipped with
`docs/plans/max-chars-size-check.md`. The trigger section below remains
the rationale for why the file exists at all.

## Context

Four per-machine settings today have no file source:

- **The API key.** `$OPENAI_API_KEY` is the **only** key source (owner
  decision, `docs/runner_contract.md` decision 4; history in the git log —
  `api_key` was removed from the model `configuration` blob and the
  `--api_key` CLI flag). For a single user on one machine this is the right
  answer: no key in the DB, no key in argv, one conventional secret
  channel. Set it in `.bashrc` and move on.
- **The database path.** Resolution is `--db` → `$ACTA_DB` → the shared
  app-data default (`%APPDATA%\ACTA Gamma\acta.db` /
  `~/.local/share/ACTA Gamma/acta.db`, the same file for GUI, CLI, and
  runner) → `./acta.db` last resort (single source of truth:
  `docs/cli_spec.md`). The GUI additionally has a *Choose database file*
  dialog (remembered in QSettings) for non-default setups.
- **The prompt size limit (`max_chars`).** The size check now ships in the
  runner's preflight (`docs/plans/max-chars-size-check.md`, done):
  `strlen(prompt_template) + strlen(context.content)` is compared against
  the limit before any backend call, and the execution fails
  `EXIT_INVALID` when over. It is a char count, not tokens (token cost
  varies per model); the built-in default is 100,000 chars, and the file
  holds the per-machine override of that limit.
- **The default per-call timeout.** `acta_runner run --timeout` (default
  300 s) and the GUI's run timeout are flag/default only. A per-machine
  default belongs in the file for service-style deployments.

The constraint: all four settings live in the launching shell/process
context (`.bashrc` exports, a flag, a dialog choice). If ACTA ever runs
beyond that context — a shared server, a service unit without a shell
environment, several users on one machine — then "export it in `.bashrc`"
stops being the sane operational story. A config file with explicit file
permissions, read by all three binaries, would be the more operationally
sound source.

## Relationship and build order (with `max-chars-size-check.md`)

This plan is the parent of `docs/plans/max-chars-size-check.md` along the
`max_chars` axis. Build order:

1. Ship the `max_chars` check first with the **built-in** default (100,000
   chars) — self-contained, no file, no new I/O, no permission machinery.
2. Land this config file next; its `max_chars` override plugs into the same
   `run_execution` limit parameter the check introduced.

The "shared resolution helper" named in both plans is **one component**
(`acta_conf`), read by all three binaries (`acta_cli`, `acta_runner`,
`acta_gui`), returning `{api_key, db, max_chars, timeout}`. Precedence is
**per-setting, not a single uniform ladder**:

| setting | precedence (top wins) | file's role |
|---------|------------------------|-------------|
| `api_key` | `$OPENAI_API_KEY` → file | fallback (never a second channel) |
| `db` | `--db` → `$ACTA_DB` → file → app-data default → `./acta.db` | middle |
| `max_chars` | file → built-in default | top override (no env/flag exists) |
| `timeout` | `--timeout` → file → built-in default | middle |

## Trigger (when this becomes worth doing)

Any of:

- ACTA Gamma is run as a service (systemd / launchd / scheduled task) where
  per-user shell environments do not exist;
- several users share one machine (and therefore one default DB path) but
  need different backend keys;
- the backend is remote (non-localhost) and the key is a real secret that
  should live in a `0600` file the operator controls, not in a login
  shell's exported environment;
- the operator wants a per-machine `max_chars` or default timeout that
  applies to every run without a flag on the command line.

Until then: do nothing. The current key, DB-path, and timeout contracts
stay as-is.

## Target contract (implemented)

- A single per-machine file, e.g. `ACTA Gamma.conf` in the same app-data
  directory as the default DB file, holding **at most**:
  - the API key;
  - the database file path;
  - `max_chars` — the maximum total chars of the prompt sent
    (`skill.prompt_template` + `context.content`);
  - `timeout` — the default per-call HTTP timeout (seconds).
- **Format: JSON.** A single JSON object with at most the four keys above:
  `"api_key"` (string), `"db"` (string), `"max_chars"` (positive integer),
  `"timeout"` (positive integer). Parsed with the same fail-closed style as
  the model `configuration` blob: not-an-object, unknown key, wrong type,
  or malformed JSON → hard error; the known-keys list mirrors the `known[]`
  check in `run.c`. Chosen over INI because the JSON parser and the
  unknown-key rejection pattern already exist in all three binaries (`cJSON`
  in the runner, `acta_cli/src/json.c` in the CLI, `QJsonDocument` in the
  GUI); INI would require a custom parser in each and weaker type safety.
- **Permissions are part of the contract, split by platform.**
  - **POSIX:** `0600` (owner read/write only). The resolver refuses a file
    whose mode has group/other read bits set (`st.st_mode & (S_IRGRP |
    S_IROTH)`), fail closed — the real enforcement gate.
  - **Windows (MSYS2/MinGW):** the Unix mode bits are meaningless —
    `_stat64` reports `0666` for every regular file regardless of
    `icacls`/`chmod`, so the POSIX bit check cannot be used there (it would
    refuse every file). The real check is the NTFS DACL (security
    descriptor): refuse if `Users`/`Everyone` have read access. That is a
    **separate, not-yet-implemented work item** (Win32 `GetFileSecurity`
    from MinGW). In the first cut the resolver does **not** enforce the
    permission guarantee on Windows; a file created in the user profile is
    single-user by default, but this is documented as best-effort, not
    verified.
- **Key precedence:** `$OPENAI_API_KEY` (if set) wins over the file; the
  file is a fallback, not a second channel. Unset env + missing/unreadable
  file = the existing `ACTA_RUNNER_ERROR` "OPENAI_API_KEY is not set" hard
  error.
- **DB-path precedence:** `--db` → `$ACTA_DB` → **config file** → shared
  app-data default → `./acta.db` last resort. The file is consulted only
  after explicit flags/vars, so a stray file can never silently retarget a
  run that named its DB. The GUI's *Choose database file* dialog (QSettings)
  stays above the file: an explicit operator choice in the GUI wins.
- **`max_chars` precedence:** config file → built-in default
  (100,000 chars; see `docs/plans/max-chars-size-check.md`).
  No CLI flag in this plan (a `--max-chars` flag is a possible follow-up).
- **`timeout` precedence:** `--timeout` flag → config file → built-in
  default (300 s). The file supplies the default, never a per-run
  override.
- **One resolution path:** the file is read by all three binaries
  (`acta_cli`, `acta_runner`, `acta_gui`) through the same helper, so the
  three-way "same default file out of the box" guarantee
  (`docs/cli_spec.md`) extends to "same file, same key, same defaults
  out of the box".
- **Scope of the file:** these four settings only. Backend URL, model id,
  and configuration stay in the DB model record — the file holds
  per-machine operator settings, not entity data.
- The key policy is applied at the same entry points as today (`cmd_run`
  in `acta_runner`, `runnerWorker` in the GUI), **not** inside
  `run_execution`; the `max_chars` value, by contrast, is consumed inside
  the pipeline (execution-level preflight, see the feature plan) from the
  value resolved by the shared helper.
- Windows note: `_putenv` cannot set/remove env vars reliably (see the
  `test_api_key` suite notes), which is exactly why a file-based source is
  a cleaner fit there than env tricks.

## Work items

1. **Done — JSON parser for the file.** Implemented as the shared `acta_conf`
   helper in `acta_db` (`acta_db/include/conf.h`, `acta_db/src/conf.c`):
   a single object with at most `api_key` (string), `db` (string),
   `max_chars` (positive integer), `timeout` (positive integer); rejects
   not-an-object, unknown key, wrong type (string vs integer), and malformed
   JSON with the same fail-closed error style as the model `configuration`
   blob (mirror of the `known[]` check in `run.c`). Exposed via `acta_db.h`;
   cJSON added to the `acta_db` build. Read by `acta_cli` and `acta_runner`
   (the GUI uses an equivalent Qt reader — work item 6). Not yet wired into
   resolution (work items 2–4).
2. **Done —** Key precedence policy helper shared by
   `acta_runner` and `acta_gui` (mirror of `runner_api_key_status`).
   `acta_conf_api_key_status()` in `acta_db` (`conf.h` / `conf.c`) —
   `$OPENAI_API_KEY` (if set, even empty) wins over the file's
   `api_key`; canonical one-line messages for unset / empty / ok; plus
   `acta_conf_read()` (file missing/unreadable = fallback unavailable,
   readable-but-malformed = fail-closed hard error) and
   `acta_conf_default_path()` (`ACTA Gamma.conf` next to the default DB
   file). Wired into `cmd_run` (`acta_runner/src/run.c`); the
   `runnerWorker` side landed with work item 6 and the tests with work
   item 7.
3. **Done —** DB-path resolution: insert the file into
   `acta_dbpath.c` (both copies) and `MainWindow::defaultDbPath` as the
   step between `$ACTA_DB` and the app-data default; keep the
   `acta_cli/tests/dbpath` suite as the pinning mechanism (it already
   asserts the exact per-platform default string and resolution order).
   Done so far: `acta_db_resolve_db_path(flag, &err_msg)` consults the
   file's `db` (missing/unreadable skipped; readable-but-malformed = hard
   error, `err_msg` malloc'd; empty value = absent) and new
   `acta_db_default_db_path()` pins the lower rungs; both `main.c`
   entry points hard-error on a malformed file, and the legacy
   `./acta.db` hint now fires only when the default path was actually
   used; `MainWindow::defaultDbPath(QString *error)` uses the Qt
   (QJsonDocument) reader — same fail-closed rules as `acta_conf_parse` —
   and the GUI bootstrap exits on a malformed file (dialog choice stays
   on top); the dbpath suite gains the file-step cases (file wins over
   default; flag/env still win over file; absent/empty key; malformed /
   unknown-key / wrong-type = hard error even with `$ACTA_DB` set; missing
   file = default).
4. **Done —** `max_chars` / `timeout` resolution: shared helper with the
   built-in defaults; the runner passes the resolved values into the
   pipeline (`run_execution` gains the limit parameter; `cmd_run` already
   has `--timeout`, the GUI worker takes its timeout from the resolved
   default). `ACTA_CONF_DEFAULT_MAX_CHARS` (100,000) and
   `ACTA_CONF_DEFAULT_TIMEOUT` (300) plus `acta_conf_resolve_max_chars()`
   (file → built-in default; no flag/env exists) and
   `acta_conf_resolve_timeout()` (--timeout flag → file → built-in
   default) in `acta_db` (`conf.h` / `conf.c`); `cmd_run` resolves both
   after the conf read and passes them to `run_execution`, whose
   signature gains the `max_chars` limit parameter (consumed by the
   preflight size check, now shipped —
   `docs/plans/max-chars-size-check.md`, done); usage/help
   text updated; the GUI worker side landed with work item 6.
5. **Done (POSIX) / follow-up (Windows) —** Permission checks,
   platform-split:
   - POSIX: **done** — `stat()` mode bits in `acta_conf_read` (checked
     before the contents are read): a file whose mode has group/other
     read bits set is refused fail-closed (the file must be `0600`,
     owner read/write only); mirrored by the GUI's Qt reader
     (`readActaConfFile`) so all three binaries refuse the same file;
     the dbpath suite's `write_file` now pins the contract mode `0600`.
   - Windows: **not** via `st_mode` (meaningless, always `0666` under
     MSYS2/MinGW). The NTFS DACL check is a **separate follow-up work
     item**; the first cut does not enforce the guarantee on Windows
     (documented as best-effort).
6. **Done —** GUI surface: same resolution as the runner (no separate
   GUI config; dialog choice stays on top). Done: the Qt reader
   (`readActaConfFile` + `ActaConfFile`) extracted from
   `mainwindow.cpp` into `confreader.h` (shared by the GUI bootstrap
   and the worker) and extended to consume `api_key` (with a
   presence flag — the shared key policy distinguishes an absent key
   from a present-but-empty one), `max_chars` and `timeout`;
   `RunnerWorker::runInThread` reads the file at
   `acta_conf_default_path()`, applies `acta_conf_api_key_status`
   (env-set-wins, file fallback), resolves `timeout` (file → built-in
   default; the `timeoutSec` constructor argument is gone, and
   `ExecutionPanel` no longer passes a hard-coded 300) and `max_chars`
   (file → built-in default), and calls the 5-argument
   `run_execution` with both; a readable-but-malformed file (or one
   failing the POSIX permission gate, already in the shared reader)
   fails closed before any claim; `src.pro` lists `confreader.h`.
7. **Done —** Tests: env-set-wins, file-fallback, bad-permissions
   refusal, missing file + unset env → existing hard error; DB-path
   order with/without the file; `max_chars` / `timeout` file-over-builtin
   order. Implemented as two suites: `acta_cli/tests/conf` (auto
   discovered by the acta_cli Makefile) — unit pins for
   `acta_conf_parse` (at-most-four-keys, unknown key, wrong type,
   fractional/zero/negative ints, malformed JSON, trailing garbage,
   zeroed-struct-on-failure contract), `acta_conf_api_key_status`
   (env-set-wins — even an empty env var; file fallback; missing file +
   unset env = `ACTA_KEY_UNSET_ERR`), `acta_conf_read` (missing file =
   fallback unavailable, not an error; readable-but-malformed =
   fail-closed hard error; POSIX group/other-readable file refused
   fail-closed before its contents are read — 0644/0604/0640/0444 all
   refused, 0600 accepted), `acta_conf_default_path` per-platform
   strings, and `acta_conf_resolve_max_chars`/`acta_conf_resolve_timeout`
   file-over-builtin order (flag over file); and
   `acta_runner/tests/run/test_conf.c` (new `CONF_TARGET` in the
   acta_runner Makefile, in the `test` target) — end-to-end `cmd_run`
   against the scratch `:memory:` DB + stub server, with the platform
   app-data env var pointed at a scratch dir so
   `acta_conf_default_path()` is controlled: missing file + unset env →
   `EXIT_INVALID` with the row still pending (the existing hard error);
   file fallback completes with the Bearer header carrying the FILE
   key; env-set-wins sends the ENV key; empty env var (POSIX) wins with
   no Authorization header; a 0644 file is refused fail-closed before
   the claim (POSIX; the NTFS DACL check is the separate Windows
   follow-up); malformed / unknown-key / wrong-type files are
   fail-closed hard errors before any claim. The stub server gained
   `stub_server_last_auth()` (captures the last `Authorization` header
   value) so the key actually used is observable. The DB-path order
   with/without the file was already pinned by the `dbpath` suite
   (work item 3).
8. **Done —** Docs: README "Environment variables" section, `cli_spec.md`
   (the DB-path and key contracts), `runner_contract.md` decision 4, and
   the `max_chars` contract amended. Implemented as: the README section
   retitled "Environment variables and the per-machine config file"
   (key precedence env → file with the fallback-not-a-second-channel
   rule, DB-path resolution `--db` → `$ACTA_DB` → file `"db"` → app-data
   default → `./acta.db`, the four-key file contract with the 0600 /
   fail-closed rules, and the file's role for `max_chars` / `timeout`);
   the `cli_spec.md` DB-file paragraph amended with the file step, the
   fail-closed hard-error rule (the file is consulted even when
   `$ACTA_DB` is set; `ACTA_CLI_ERR` exit 10), and the lockstep list
   including `acta_conf_default_path()` and `MainWindow::defaultDbPath`;
   `runner_contract.md` decision 4 amended (env-set-wins, file fallback,
   no key anywhere = the existing hard error, fail-closed malformed /
   bad-permission file) and decision 5 amended (`--timeout` → file
   `"timeout"` → built-in 300 s); `max-chars-size-check.md`'s limit
   source amended (file → built-in 100,000 chars via
   `acta_conf_resolve_max_chars`, status updated to partially started
   with the resolution side done).

## Deliberately out of scope

- No multi-user accounts, per-user key isolation, or authorization layer —
  that belongs to a higher-level application built on top (see
  `docs/PointOfView.md`, "What ACTA Gamma is not").
- No key rotation, no key server, no secrets manager integration.
- No per-entity settings in the file (model URLs, per-model limits,
  per-execution overrides) — those belong to the DB or the execution;
  `max_chars` and `timeout` are per-machine defaults.
- Nothing changes until the trigger above is real; the POC contract
  (`$OPENAI_API_KEY` only, current DB-path resolution, `--timeout` flag)
  stays as-is.
