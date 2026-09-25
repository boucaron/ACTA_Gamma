# Task continuation: remove `db exec` / add `db init`

Status: **done.** All code and doc edits are complete (sessions 1-6) and
the user verified the full build + test run green
(`make all`, `make test` — every suite PASS; `make gui` +
`acta_gui/db/smoke_test.sh` per the plan). Committed; see the git log.

## Done (session 1)

1. **Schema file moved** `acta_gui/db/schema.sql` → `acta_db/schema.sql` (via `git mv`, staged).
2. **Generated CLI header** `acta_db/include/schema_sql.h` created from `acta_db/schema.sql`
   (one C string literal `ACTA_SCHEMA_SQL[]`; schema has no `"` or `\`, so each
   line is a quoted literal ending in an escaped newline). Verified byte-exact.
3. **`acta_db/Makefile`** edited: added `SCHEMA_H := include/schema_sql.h`, a rule
   `$(SCHEMA_H): schema.sql` (generator), added it to `all`, and to `clean`.
4. **GUI resource** `acta_gui/assets.qrc`: `db/schema.sql` now
   `<file alias="db/schema.sql">../acta_db/schema.sql</file>` (keeps
   `:/db/schema.sql`; rcc regenerates at `make gui`).
5. **`acta_db/include/db.h`**: `acta_db_exec` comment updated — sole callers are
   GUI first-launch and CLI `db init`, both applying the static embedded schema.
   Added declarations `char **acta_db_user_tables(db_t*, int*, int*)` and
   `void acta_db_user_tables_free(char**, int)`.
6. **`acta_db/src/db.c`**: added `<stdlib.h>`, `<string.h>`; implemented
   `acta_db_user_tables` + `acta_db_user_tables_free` (SELECT from
   `sqlite_master`, heap NULL-terminated array).
7. **`acta_cli/src/commands/db.c`** — patched in session 2 (the patch script was
   NOT applied in session 1): `#include "schema_sql.h"`; `usage_exec` →
   `usage_init`; `db_usage` actions line + `usage_init(f)`;
   `db_help_for_action` `"exec"` → `"init"`; removed
   `sql_first_statement_is_select` + `exec_output` helpers; `db_actions`
   `"exec"` row → `"init"`; the exec branch (positional/`--sql`/`--file`/
   `--sql_stdin`, 64 KiB cap, SELECT guard) replaced with an `init` branch:
   rejects positionals/`--sql`/`--file`/`--sql_stdin`/global `--stdin`
   (exit 4), lists user tables via `acta_db_user_tables`, classifies fresh /
   already schema'd (no-op) / partial-or-foreign (fail closed, exit 4), and on
   the fresh case runs `acta_db_exec(db, ACTA_SCHEMA_SQL)`. Output
   `{"status":"ok"}` / `--table` → `ok`.

## Done (session 2)

8. **`acta_cli/src/tools.c`**
   - `db.exec` entry removed; `db.init` entry added
     (`"db.init","db","init",NULL,0, desc, NULL,0, f_table,1,"none",NULL,0,NULL,0,&suc_db_init`);
     `suc_db_init` added, `suc_db_exec` / `f_db_exec[]` / `p_sql[]` removed.
   - `--tools` schema `version` bumped 4 → 5 (`jf_num`, header comment);
     `tools_print_compact` header string `v3` → `v4`.
   - "75 entries = 65 actions + 10 help actions" comment → 74 / 64.
9. **`acta_cli/tests/db/db_test.c`**
   - All `db exec` scenarios removed (positional, `--sql`, `--file`,
     `--sql_stdin`, KI-1, KI-5, verbose smoke) and the KI banner section.
   - New `db init` scenarios: `test_init_fresh` (scratch
     `ACTA_DB_OPEN_CREATE` file → schema applied, `{"status":"ok"}`, 9 user
     tables), `test_init_fresh_table` (`--table` → `ok`), `test_init_noop`
     (REF_DB, exit 0), `test_init_partial` (one canonical table → exit 4),
     `test_init_foreign` (unrelated table → exit 4), `test_init_rejects_input`
     (positional / `--sql` / `--file` / `--sql_stdin` / global `--stdin` →
     exit 4), `test_exec_removed` (`db exec` in every form → unknown-action
     exit 10).
   - `test_help` expects `init` (not `exec`); unknown-action suggestion now
     uses `"initt"` (one char off `init`). `run_db_test_all` updated.
10. **`acta_cli/tests/tools/tools_test_main.c`**
    - entry count 75 → 74 (all comment mentions + the
      `cJSON_GetArraySize(tools)` assertion); `EXP_DB` =
      `{ "init","version","backup","help" }`; `version` assertion 4 → 5
      ("schema v5"); the `input "none"` ⇔ pure-output invariant extended to
      include `db.init` (db.init's `success` is checked by the generic
      per-entry `kind "json"` + non-empty keys check).
11. **Runner/db test seed paths** `acta_gui/db/schema.sql` →
    `acta_db/schema.sql` in: `acta_runner/tests/README.md`,
    `acta_runner/tests/run/{test_api_key,test_conf,test_deadrunner,test_deleted,
    test_pending,test_run,test_sweep}.c`, `acta_db/tests/test_common.h`,
    `acta_db/tests/test_model_folder.c`.
12. **`acta_db/Makefile` rule fixed and verified** (the session-1 rule was
    broken and had never been executed): collapsed the multi-line recipe to one
    line; make was expanding `$$0` → must be written `$$0` in the recipe
    (it generated a header of bare `"\n"` lines, dropping every schema line);
    `$@` replaced by the literal `include/schema_sql.h`; line-1 comment
    matches the already-generated header (`Source of truth: acta_db/schema.sql.`).
    Verified: `rm include/schema_sql.h && make include/schema_sql.h` →
    byte-identical to the session-1 header.
13. **Docs**
    - `README.md`: "Fresh databases need the schema first" → `acta_cli db init`
      (canonical file `acta_db/schema.sql`; GUI path mentioned); the `db exec`
      warning sentence in "CLI ergonomics" removed (replaced with a one-line
      note that the CLI takes no user-supplied SQL).
    - `docs/cli_spec.md`: wire-format row `db exec` → `db init`; the
      "mutating-only" paragraph rewritten to describe `db init`; schema path
      → `acta_db/schema.sql`; `## db` table: `db exec` row → `db init` row
      (no positionals, `--table`, input `—`); schema `version` field note 4 → 5.
    - `docs/DBDesign.md`: canonical path → `acta_db/schema.sql` (line 5);
      evolution policy (line 11) now names the versioned-schema-file / `db init`
      path; "One raw-SQL escape hatch" (line 14) → only caller GUI
      first-launch (+ `db init` on a fresh file); "schema changes via `db exec`"
      → "via the versioned schema file / `db init`" (lines 18, 41); line 127
      "via `db exec"` mention dropped; line 489 `db exec` → `db init` on a
      fresh file (backup the data-carrying file first).
    - `docs/status.md`: Done note added (what changed, test pinning,
      `--tools` version bump 4 → 5, 74 entries, seed paths).
    - `docs/known_issues.md`: left as-is (closed-issue log).

## Done (session 3 — verification + leftover cleanup)

All 13 items above were re-verified against the files; every edit had
actually landed. Two small leftovers fixed:

14. **`acta_cli/tests/db/db_test.c`** — stale comment in
    `test_unknown_action_suggestion`: `"exect" is one char off "init"` →
    `"initt" is one char off "init"` (the code already used `"initt"`).
15. **`acta_cli/include/json.h:31`** — the flagged optional cosmetic
    cleanup done: `/* ---- raw pass-through (for db exec results etc.) ---- */`
    → `/* ---- raw pass-through ---- */`.
16. **`schema_sql.h` re-verified byte-exact** against `acta_db/schema.sql`
    (240 quoted line-literals, decoded content identical).
17. **Grep audit completed**: no user-facing `db exec` / `db.exec` left
    except (a) intentional historical mentions in `docs/plans/*`,
    `docs/known_issues.md`, `docs/status.md`, `docs/DBDesign.md`
    ("(`db exec` was removed)"), and (b) the intentional test-pinning
    comment in `db_test.c` ("db exec no longer exists"). Untracked
    session junk (`acta_cli/hw.c`, `tools.json`, `test_db_out`, `tmp/`,
    `acta_runner/MYTEST/`, …) left in place, not product code.

## Done (session 4 — build-failure fix, no compilation)

User ran `make all`; the first failure was logged in `acta_cli/out`:
compiling `src/commands/db.c` died at `sqlite3.h:188` with
`expected ',' or ';' before 'extern'`. Root-caused by reading (no
compilation): the generated `acta_db/include/schema_sql.h` was missing the
terminating `;` of the declaration — the Makefile rule emitted
`echo 'static const char ACTA_SCHEMA_SQL[] =';` then the awk literal loop
then `echo '';`/`#endif`, so the file ended
`...\n"` with no `;`. The unterminated initializer swallowed all of the
following translation unit up to the first real code token after the
`#define` block in `<sqlite3.h>` — line 188
(`SQLITE_API SQLITE_EXTERN const char sqlite3_version[];`) — where the
parser tripped on `extern`. The sqlite3.h location was a pure red herring;
the defect was in the generated header (item 2 / item 12 above both
verified content byte-exact but not C-validity).

18. **Fix, two places**
    - `acta_db/Makefile`: the `$(SCHEMA_H)` rule now emits `echo ';'` after
      the awk literal loop, so every regeneration is a valid C
      declaration.
    - `acta_db/include/schema_sql.h`: the missing `;` added after the last
      string literal (current file fixed without regeneration; the next
      `make` regenerates it identically).
    - Note (no code change): the generator has no guard against a future
      `"` or `\` in `schema.sql` — the "no double-quote or backslash"
      assumption in both the rule's and the header's comments would then
      silently emit a broken header again.

## Done (session 5 — first full test run, three failing suites fixed)

First full `make test` run after the session-4 fix; everything was green
except three suites, each with a distinct cause:

19. **`acta_db/src/db.c` — `acta_db_user_tables` empty-result bug**
    (test_db FAILs `db_test.c:60` / `:67` / `:89`: `db init` on a fresh
    file printed `{"error":"ACTA_DB_ERR_INVALID","code":0,"message":"cannot
    list user tables: (no detail)"}`). With zero user tables the
    row loop never allocates `names`, so the function returned `NULL`
    with `*err = ACTA_DB_OK` — the `init` branch's `err != ACTA_DB_OK ||
    tables == NULL` test treated a valid empty result as a failure (and
    the `ACTA_DB_OK` = 0 `err` explains the `"code":0` in the error JSON).
    Fix: when the result set is empty, `calloc` a one-slot array so the
    `names[count] = NULL` terminator lands at `names[0]` — an empty
    table list is a valid result, distinct from NULL-on-failure.
20. **`acta_cli/tests/help/help_test.c` — two stale `exec` pins** the
    session-3 audit missed (it checked user-facing strings, not test
    pins): the H1 case-table row
    `{ cmd_db, "exec", "== exec", "== version" }` →
    `{ cmd_db, "init", "== init", "== version" }` (running `db help exec`
    now rc 10, unknown action), and the H2 full-help pin
    `TEST_CONTAINS(..., "== exec")` → `"== init"`.
21. **The tools entry count was a miscount — 74 → 75**: `db init`
    *replaces* `db exec` (one action in, one out), so the table keeps
    75 entries = 65 actions + 10 help; the session-2 "74 / 64" figure
    (item 8, item 10, item 13) was wrong. Corrected in:
    `acta_cli/src/tools.c` table comment ("75 entries = 65 actions +
    10 help actions"); `tools_test_main.c` (the
    `cJSON_GetArraySize(tools)` assertion 74 → 75 + all comment
    mentions); `EXP_EXEC` `n_actions` 12 → 13 (the `EXP_EXEC` literal
    always carried 13 entries incl. `help` — with `n_actions` 12 the
    coverage loop silently skipped the last one, `help`); and
    `docs/status.md` ("75 entries").

## Done (session 6 — segfault in `acta_db_user_tables`)

The session-5 re-run got every suite green except `test_db` and
`test_tools`, which produced **no output at all** and killed `make test`
(SIGSEGV; the stdout buffer is lost on crash, so the suites looked
empty). GDB backtrace: `names[count] = strdup(name)` in
`acta_db_user_tables` (`acta_db/src/db.c`), first call with a
non-empty result — `test_init_fresh` after `db init` re-lists the 9
schema'd tables; `test_tools`'s in-process cross-check runs `db.init`
on a schema'd DB, same path.

Root cause: the function never allocated its initial buffer — `names`
started `NULL` and only grew via `realloc` when `count == capacity`
(16), which is never reached from `count = 0` with `names == NULL`, so
the first row wrote `names[0]` on NULL. The bug could only surface once
item 19 let an empty result pass (fresh-file `db init` now succeeds and
the follow-up list call returns 9 rows).

22. **`acta_db/src/db.c` — `acta_db_user_tables`**: up-front
    `malloc(capacity * sizeof *names)` with `failed = (names == NULL)`
    and the row loop guarded by `!failed`; this subsumes the session-5
    `calloc` zero-length workaround (deleted) — the terminator
    `names[count] = NULL` now always lands, empty list ≢ error, and the
    failure path frees a possibly-partial buffer correctly. The
    `db.h` contract comment now states the zero-length-but-valid
    empty result explicitly.
    (Leftover `acta_test_init_fresh.db{-shm,-wal}` files from the
    crashed run removed from `acta_cli/`.)

## Remaining — verified by the user (all green)

1. **Build + test** — verified green after the session-6 fix:
   - `make all` (item 18 fix)
   - `make test` — every suite PASS (`test_db`, `test_help` and
     `test_tools` were the last failures — items 19-22)
   - `make gui` (regenerates the `:/db/schema.sql` Qt resource from the
     moved file) + `acta_gui/db/smoke_test.sh`
2. ~~**Grep audit** for leftover user-facing `db exec`~~ — **done in
   session 3** (see above); the `json.h:31` cosmetic cleanup was applied.

## Done (session 7 — `db init` bootstrap unreachable from the CLI)

While running the schema-migration plan's leftover operator checks
(`db backup --to` + `db migrate` on the backed-up file), a gap in the
finished `db init` work was found: **the "fresh file" path is
unreachable from the CLI binary.**

23. **The gap.** `main.c` opened the DB with `ACTA_DB_OPEN_EXISTING`
   for every action, and `acta_db_open` in `EXISTING` mode requires
   **at least one user table** (its anti-0-byte guard). So
   `acta_cli db init --db fresh.db` exits 11 for both a **nonexistent**
   file and a **valid-but-empty** (zero-table) file — verified
   empirically with the prebuilt binary. The fresh-file behavior
   (schema applied, `user_version = 1`) was only ever reachable
   in-process: the GUI first launch and the `db_test.c` scenarios,
   which open their scratch handles with `ACTA_DB_OPEN_CREATE`. That
   contradicts the shipped contract: README ("Fresh databases need the
   schema first: run `acta_cli db init`"), `usage_init` ("Fresh file
   (no user tables): the canonical schema is applied"), and the
   `remove-db-exec` plan's goal 1 ("Bootstrapping a fresh database for
   CLI-only users"). The in-process `cmd_db` pins and the `acta_db`
   open-mode pins were all green — the broken layer was the wiring
   around them, which no test exercised (test binaries link
   `APP_OBJS_NO_MAIN` and pre-open their own handles).
24. **Fix, `acta_cli/src/main.c`.** Open-mode selection before
   `acta_db_open`: `db init` opens with `ACTA_DB_OPEN_CREATE` (the
   bootstrap path — may create/adopt a fresh file); every other action
   keeps `ACTA_DB_OPEN_EXISTING` (missing file still exits 11).
   Fail-closed on a corrupt non-SQLite file is preserved: `sqlite3_open`
   succeeds, the user-table listing fails, and the `init` branch's
   existing `err != ACTA_DB_OK || tables == NULL` path errors out. No
   doc change was needed — README / `cli_spec.md` / `usage_init`
   already state the contract the fix now actually meets.
25. **Testable seam: `cli_main`.** To pin the wiring in-process, the
   full CLI flow (parse → early-exit → resolve → open → dispatch →
   close) moved from `main.c` into a new `acta_cli/src/cli_main.c`
   (`cli_main` + `cli_error`, which moved with it — declared in
   `cli.h`); `main.c` is now a thin `main()` → `cli_main()` wrapper.
   `cli_main.o` is picked up by the `SRCS` wildcard and links into
   every test binary (`APP_OBJS_NO_MAIN` excludes only `main.o`).
26. **Tests, `acta_cli/tests/db/db_test.c`.** New
   `test_cli_main_bootstrap` (registered in `run_db_test_all`), driving
   `cli_main` in-process under `stest_capture` (dup2 on fd 1, same
   pattern as the `test_check` capture fix):
   - **(a)** `db init --db <nonexistent>` → rc 0, `{"status":"ok"}`;
     the file now exists with **9 user tables** and
     `PRAGMA user_version = 1` (the bootstrap that previously exited 11);
   - **(b)** `db init --db <existing zero-table valid file>` → rc 0,
     schema applied — the exact case `ACTA_DB_OPEN_EXISTING` rejects;
   - **(c)** regression: `db version --db <nonexistent>` → rc
     **11** with empty stdout (JSON error line on stderr) — every other
     action still requires an existing database.

Verified green by the user: `make all` + `make test` (every suite,
`test_db` now includes the three `cli_main` scenarios).

## Notes / decisions made
- `db init` uses short-circuit idempotency (does NOT re-run CREATE TABLE on an
  already schema'd file; only a fresh file runs the schema), because the
  shipped `schema.sql` uses bare `CREATE TABLE` (not `IF NOT EXISTS`).
- Canonical table set for the check: model_folders, models, model_revisions,
  skill_folders, skills, skill_revisions, contexts, executions, execution_logs.
  All 9 present ⇒ already schema'd (no-op), even if extra tables exist.
- `db init` supports `--table` (prints `ok`) for parity with `db version`/`backup`.
- `db init` takes no SQL input: positional / `--sql` / `--file` /
  `--sql_stdin` / global `--stdin` all rejected with exit 4; `db exec` in any
  form is an unknown action (exit 10).

---

# Task continuation 2: token-free runner health check (`acta_runner check`)

Source plan: `docs/plans/runner-health-check.md` (plan 1 of the three open
plans; plan 2 — schema migration — is now done, see "Task continuation 3"
below; plan 3 — Windows config permissions — is still a proposal, not
started).

Status: **verified; committed.** Session 1 was edit-only; session 2 ran
`make all` / `make test` / `make gui` and fixed the failures found (see
"Done (session 2)"), then everything was green and committed.

## Done (session 1, edit-only)

1. **`acta_runner/src/backend.h`** — added `preflight_result_t` (10 codes:
   `PREFLIGHT_OK`, `PREFLIGHT_CANCELED`, `PREFLIGHT_HEALTH_{TIMEOUT,
   TRANSPORT, NOT_200}`, `PREFLIGHT_MODELS_{TIMEOUT, TRANSPORT, NOT_200,
   UNPARSEABLE}`, `PREFLIGHT_MODEL_NOT_SERVED`), `backend_preflight_t`
   (`brc`, `http_status`, `max_context`, `available_ids[512]`) and
   `int backend_preflight(base_url, model_id, api_key, timeout_sec, out)`.
2. **`acta_runner/src/backend.c`** — `#include <cjson/cJSON.h>`; static
   `build_url` copy; `backend_preflight()` implementation: GET /health
   (timeout / transport / non-200 classified, 503 carried in
   `http_status`), then GET /v1/models **only after** /health is 200;
   scans the whole `data` array for `model_id`; fills `max_context` and
   `available_ids`. `api_key` is forwarded to `backend_request` — the run
   pipeline still sends the key on its preflight GETs (decision 4
   unchanged); `check` passes NULL (keyless by design).
3. **`acta_runner/src/run.c`** — the inline /health + /v1/models preflight
   blocks replaced by one `backend_preflight()` call; the classification
   maps to the **exact same** `FAIL` messages as before (same exit codes,
   same wording — pinned by `test_run.c` scenarios 2/3/5). The best-effort
   catalog `GET /` stays inline (not part of the token-free preflight).
4. **`acta_runner/src/check.c`** (new) — `cmd_check(ga, gopts, db)`:
   - `--timeout` must be a positive integer; timeout resolution is the
     existing `--timeout` → config file `"timeout"` → built-in 600 s. The
     config file is read **only** when the flag is absent (the flag wins
     outright); a readable-but-malformed file is a fail-closed hard error
     like everywhere else.
   - DB mode: positional `<model-record-id>`; read-only
     `acta_db_model_get_live` (soft-deleted rows → not found); unknown id →
     `EXIT_NOT_FOUND`; missing `base_url` / `model_identifier` →
     `EXIT_INVALID`. No execution is created, claimed or logged.
   - Standalone mode: `--base-url` + `--model-identifier` (mutually
     exclusive with the positional; one flag alone → `EXIT_INVALID`); the
     `db` handle is NULL and must not be touched.
   - Result contract: one JSON line on stdout — success
     `{"ok":true,"model":...,"max_context":N,"base_url":...}`; failure
     `{"ok":false,"model":...,"base_url":...,"verdict":...}` with verdict
     `model still loading` / `server unreachable` (incl. /health
     timeout + transport + any non-200-non-503) / `model not served` /
     `catalog unreachable` (models timeout → `EXIT_TIMEOUT`, other
     catalog failures → `EXIT_HTTP`).
   - Exit codes 0 / 1 / 4 / 12 / 13 — the existing runner code space, no
     new numbers. No `POST /v1/chat/completions`, no DB writes, no
     streaming, no retry loop.
5. **`acta_runner/include/runner_util.h`** — `cmd_check` declaration with
   the db-NULL-in-standalone-mode contract documented.
6. **`acta_runner/src/argparse.c`** — `runner_flag_specs` gained
   `{ "base-url", 1 }, { "model-identifier", 1 }` (known value flags for
   `cmd_args_validate` / `cmd_args_flag` / positional skipping).
7. **`acta_runner/src/main.c`** — `check` dispatch in
   `commands_dispatch`; **standalone-mode branch before DB resolution**:
   `check` with both `--base-url` and `--model-identifier` is dispatched
   with a NULL `db` handle (DB path is neither resolved nor opened, so a
   machine without any database file can still probe a server); help text
   (check action lines + `check flags:` section) and the file header
   comment updated.
8. **`acta_runner/tests/stub_server.{h,c}`** — `stub_config_t` gained
   `models_status` (default 200; non-200 → canned error body) and
   `max_context` (0 = field omitted from the /v1/models entry); new
   `stub_server_chat_requests()` counter (reset on `stub_server_start`,
   incremented per POST /v1/chat/completions) — the zero-token assertion
   hook. Existing tests are unaffected (zeroed configs → default
   behavior).
9. **`acta_runner/tests/check/test_check.c`** (new, port 8921) — 9
   scenarios: (1) DB-mode success (exit 0; stdout carries `ok:true`,
   `model`, `max_context:162000`, `base_url`), (2) /health 503 →
   `model still loading` + EXIT_HTTP, (3) model not in catalog →
   `model not served` + EXIT_HTTP, (4) catalog endpoint 500 →
   `catalog unreachable` + EXIT_HTTP, (5) connection refused (no stub) →
   `server unreachable` + EXIT_HTTP, (6) slow /health (delay 2500) +
   `--timeout 1` → EXIT_TIMEOUT, (7) unknown `<model-record-id>` →
   EXIT_NOT_FOUND, (8) standalone mode with **NULL db handle** → exit 0,
   (9) argument conflicts (one standalone flag only / flags + positional /
   no args) → EXIT_INVALID. **Every** scenario asserts
   `stub_server_chat_requests() == 0` and that no execution rows exist
   (the zero-token / no-DB-writes guarantee). Stdout verdicts are
   captured with dup/dup2 + mkstemp (POSIX + MSYS2/MinGW-safe, no
   freopen).
10. **`acta_runner/Makefile`** — new `tests/check/test_check` suite
    (`CHK_SRCS` = test_check.c + stub_server.c + src/check.c +
    src/backend.c + src/argparse.c, `TEST_LDLIBS`), added to the `test`
    target (runs after `test_conf`) and to `clean`; compile rule with the
    `tests/stub_server.h` dependency; header comment updated.
11. **Docs**
    - `docs/runner_contract.md`: status line gains "`check` token-free
      health action shipped"; "What landed" bullet for `src/check.c` +
      the `backend_preflight` factoring; `tests/check/test_check.c`
      bullet; new **`## check action (token-free backend health check)`**
      section (surface, check sequence, result contract, verdicts, exit
      codes, guarantees).
    - `README.md`: quick-start paragraph (verify the backend before any
      run, zero tokens); the end-to-end example inserts step 6
      `acta_runner check 1` (old steps 6/7 renumbered 7/8); the
      dead-runner/sweep paragraph recommends `check` before a
      `run --pending` batch and as the first triage step after a `failed`
      backend call.
    - `docs/status.md`: Done bullet for `acta_runner check`, explicitly
      marked "implementation landed, full `make test` verification
      pending".
    - `docs/plans/runner-health-check.md`: status → "implemented
      (edit-only session — no compilation/test run yet; verification
      pending)"; new **Implementation notes** section recording the
      resolutions: non-200-non-503 /health → `server unreachable`;
      `check` is keyless; standalone mode is detected in `main.c` before
      DB resolution; the config file is read only when `--timeout` is
      absent; stub extensions (`models_status`, `max_context`,
      `stub_server_chat_requests`).

## Done (session 2 — verification + fixes)

User ran the builds (session 1 stayed edit-only); three failures found
and fixed, then everything green:

23. **`acta_runner/src/check.c:215` — compile error** (first `make all`):
    `too many arguments to function 'emit_runner_error'; expected 2, have
    3` — the `"no base_url or model_identifier"` path passed the already
    formatted `msg` plus a stray `"%s"`. Fixed: `emit_runner_error(
    EXIT_INVALID, msg)` (2-arg inline in `runner_util.h:194`).
24. **`acta_runner/src/run.c:734` — truncation warning**: the
    `PREFLIGHT_MODEL_NOT_SERVED` FAIL wrote two unbounded `%s` (up to 511
    bytes each, from `model_identifier` / `available_ids`) into
    `errmsg[512]`. Fixed with precision specifiers
    (`"execution model '%.200s' not served by server (available:
    %.200s)"` — worst case 453 < 512), matching the existing `%.300s`
    style; test-pinned substrings (`not served by server`) unchanged.
25. **`acta_runner/tests/check/test_check.c` — every functional check
    failed (23/38) on the first `make test`**: the stdout capture used
    `mkstemp(NULL)`, which on the MSYS2 host resolved to the unwritable
    system temp dir (`C:\WINDOWS`) — `cap_begin` failed, so
    `run_check_capture` returned -1 without ever calling `cmd_check`
    (no stderr from `cmd_check` in the log; the standalone zero-chat /
    no-DB-row guarantees still passed). Fixed: the capture file is now
    created with a **CWD-relative `mkstemp` template**
    (`.test_check.XXXXXX`, unlinked on both success and failure paths);
    no `freopen`.
    (A pipe-based capture was tried first but abandoned: this mingw-w64
    CRT's `_pipe` has a 3-arg UCRT-style signature.) After the fix:
    `make all`, `make test` (every suite, `test_check` included) and
    `make gui` all green; docs status lines updated (`docs/status.md`,
    `docs/plans/runner-health-check.md`). Committed.

## Deferred (plan open questions, NOT part of this task)

- a `check` "would-run" `max_chars` dry run;
- a GUI "Check backend" button.

---

# Task continuation 3: schema migrations (`acta_cli db migrate`)

Source plan: `docs/plans/schema-migration.md` (plan 2; follow-up to the
`remove-db-exec` task — fills the "no operator path for evolving an
existing database" gap left by the `db exec` removal).

Status: **verified; committed.** Session 1 was edit-only; session 2 ran
the builds and fixed the failures found (see "Done (session 2)"), then
everything was green and committed.

## Done (session 1, edit-only)

1. **`acta_db/migrations/0.1.sql`** (new) — verbatim copy of
   `acta_db/schema.sql`; the versioning path starts from a copy of the
   canonical schema (9 baseline tables, indexes, triggers).
2. **`acta_db/Makefile`** — new `MIGRATIONS_H := include/migrations_sql.h`
   generation rule (one C string literal per migration file, same escaping
   as `schema_sql.h`), added to `all` and `clean`; atomic write
   (`> .tmp && mv`, `|| rm -f` on failure) so a partial file can never be
   observed. The header comment avoids any `*/` sequence (the glob is
   written `migrations/ 0.*.sql`) and the comment block is explicitly
   closed before `#ifndef`.
3. **`acta_db/include/migrations_sql.h`** (new, generated) —
   `typedef struct { int version; const char *sql; } acta_migration_t;`
   + `static const acta_migration_t ACTA_MIGRATIONS[]` (one row per
   version, `{0, NULL}` sentinel); payload verified byte-exact against
   `schema.sql`.
4. **`acta_db/include/db.h` / `src/db.c`** — new `acta_db_schema_version`
   (read `PRAGMA user_version`) and `acta_db_set_schema_version` (set it);
   `acta_db_open`'s informational open-time note now appends
   `" Schema version: 0.N (PRAGMA user_version=N). Informational note,
   not an error."` when the recorded version is nonzero (combined with
   the pragma-degradation notes; ownership handled so the note buffer is
   freed exactly once — see the session-2 UAF fix); `acta_db_exec` doc
   lists all three DDL callers (GUI first launch, `db init`, `db
   migrate`).
5. **`acta_cli/src/commands/db.c`** — `usage_migrate` + `migrate` in
   `db_usage` / `db_help_for_action` / `db_actions`; `db_migrate_apply()`
   (declared in `commands.h`, defined here, callable from tests with
   synthetic fixtures): each pending migration in its own `BEGIN…COMMIT`,
   `PRAGMA user_version` set AFTER the commit (it is a no-op inside a
   transaction), failing migration rolls back itself and the file keeps
   its prior version, `*out_version` set on both success and failure
   (failure → last successfully applied version), fail message
   `"migration 0.N failed: <detail>"`. The `init` branch sets
   `user_version = 1` after a fresh schema apply and adopts legacy
   `user_version = 0` files that are already fully schema'd. The `migrate`
   branch: rejects any SQL input (positional / `--sql` / `--file` /
   `--sql_stdin` / global `--stdin` → exit 4); `user_version = 0` with no
   user tables → migrate from 0.1; `user_version = 0` with user tables →
   fail closed ("not an ACTA Gamma database"); success
   `{"status":"ok","schema_version":"0.N"}` (`--table` → `ok`).
6. **`acta_cli/include/commands.h`** — `db_migrate_apply` declaration +
   `migrations_sql.h` include.
7. **`acta_cli/src/tools.c`** — `db.migrate` entry (`input: "none"`,
   success keys `status` + `schema_version`); 75 → 76 entries; `--tools`
   schema `version` 5 → 6; compact header `v4` → `v5`.
8. **Tests** — `acta_cli/tests/db/db_test.c`: migrate fresh / no-op /
   foreign / failing-migration fixture / ordering / input-rejection
   scenarios + `db help` lists `migrate`; `acta_cli/tests/tools/
   tools_test_main.c`: 76 entries, 5 `db` actions, `db.migrate` in the
   input-"none" invariant, `version` 6; `acta_db/tests/test_db.c`:
   `user_version` set/read cycle + open-note reporting tests.
9. **`acta_cli/Makefile`** — the generated acta_db headers
   (`schema_sql.h`, `migrations_sql.h`) are now explicit prerequisites of
   `ACTA_DB_LIB` with sub-make regeneration recipes, so a `make clean`
   is self-healing even when `acta_cli` is built on its own.
10. **GUI** — `MainWindow::createDatabase` sets `user_version = 1` after
    the schema application (warning on failure, non-fatal);
    `acta_gui/db/smoke_test.sh` sets `PRAGMA user_version=1` after schema
    application.
11. **Docs** — `docs/DBDesign.md` (versioning paragraph replaces "No
    schema versioning", version ledger table with the 9 baseline tables,
    sole-callers and durability wording), `docs/cli_spec.md` (Common-
    shapes row, `db migrate` paragraph, `## db` table row), `README.md`
    (fresh-database paragraph), `acta_cli/Makefile` comment.

## Done (session 2 — verification + fixes)

User ran the builds (session 1 stayed edit-only); the failures found and
fixed, then everything green:

12. **Generated-header comment broke C parsing** (first `make all`,
    `commands.o`): the header's first comment contained the glob
    `migrations/*.sql` — the `*/` inside it terminated the comment early,
    swallowing `#ifndef`/`#define`, so `#endif` had no matching `#if`
    (`'/*' within comment` warnings pointed at it). The user hand-fixed
    the header first; the fix then landed in the Makefile recipe itself:
    the comment is reworded (glob written `migrations/ 0.*.sql`) so no `*/`
    sequence occurs inside it, and the comment block is explicitly closed
    with `*/` before `#ifndef`. A first regeneration attempt was missing
    that closing `*/` (caught before the build); the regenerated header is
    byte-exact against `schema.sql`.
13. **`acta_db` test crash, exit 127** (`test_db`, SIGTRAP in
    `acta_db_close` via sqlite heap validation; gdb backtrace): the
    `acta_db_open` note-combining code freed the version note buffer it had
    just assigned to `last_error` (use-after-free) whenever the
    pragma-degradation note was NULL; the test's `strstr` reads then hit
    freed memory (two false `ASSERT FAILED` lines) and `acta_db_close`'s
    `sqlite3_free(db->last_error)` tripped `RtlValidateHeap`. Fixed in
    `acta_db/src/db.c`: when the pragma note is NULL the code takes
    ownership of the version note instead of freeing it (and guards
    `mprintf` NULL-on-OOM). Verified with gdb + a direct `sqlite3`
    `PRAGMA user_version` read of the leftover test file (the set itself
    had always persisted correctly).
14. **`test_db` single assertion** (`db_test.c:337`,
    `test_migrate_failing_migration`: `uv == 1 (got -1, want 1)`):
    `db_migrate_apply` did not set `*out_version` on the failure path, so
    the test's `-1` init value survived. Fixed: `*out_version` is now set
    on failure too (the version the file is currently at — the last
    successfully applied migration, i.e. the prior version); doc comments
    in `db.c` and `commands.h` state the exact contract.

After the fixes: `make all`, `make test` (every suite PASS) and `make gui`
all green; docs status lines updated; committed.

## Deferred (plan open questions, NOT part of this task)

- whether migrations ever include `UPDATE` (data) statements — DDL only
  for now;
- semver-like version strings (`0.10`) — the integer `PRAGMA
  user_version` with a minor-bump convention is the chosen design; a
  `schema_version TEXT` table row would be the alternative;
- a future `db schema-version` action printing the version ledger.
