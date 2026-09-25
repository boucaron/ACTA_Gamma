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
