# acta_cli — Code Review

Review of the C CLI (`acta_cli/`, ~9k LOC). Conducted in parts:

| Part | Scope | Status |
|------|-------|--------|
| 1 | Entry point, arg parsing, dispatch (`main.c`, `argparse.c/h`, `cli.h`, `cli_util.h`, `commands.h/c`) | ✅ done |
| 2 | JSON layer (`json.h`, `json.c`) + input-source plumbing | ✅ done |
| 3 | Small entities (`db.c`, `context.c`) | ✅ done |
| 4 | Big entities (`model.c`, `skill.c`, `*_folder.c`, `*_revision.c`) | ✅ done |
| 5 | `execution.c`, `execution_log.c`, Makefile, tests | ✅ done |

---

## Part 1: entry point, arg parsing, dispatch

### Design / consistency issues

1. **`cmd_args_flag` doc contradicts implementation** (`include/argparse.h`, `src/argparse.c`)
   Header claims the call "advances past it" and describes a
   value/boolean protocol; the implementation never advances `pos`, and returns
   `NULL` both for "flag absent" and "boolean flag present" — callers cannot
   distinguish the two. Additionally `cmd_args_has_flag` scans from `it->pos`
   while `cmd_args_flag` scans from 0. The truncated comment block
   (`if (!v) { // not present`) shows this API was half-refactored.
   **Fix:** commit to one protocol (e.g. `flag()` never advances; return a
   small struct `{present, value}`), fix the docs, and reconcile the scan
   ranges.

2. **`parse_globals` conflates three error classes** (`src/argparse.c`)
   OOM on the `rest` malloc, a missing value for `--db`/`--json`/etc., and
   "fewer than 2 positionals" all return `EXIT_CLI` (10) with no message;
   `main.c` then prints "missing entity and/or action" — misleading for the
   other two cases. OOM should be `EXIT_ALLOC` (3); missing flag values need
   their own message naming the flag.

3. **`--result` / `--error` silently drop their value** (`src/argparse.c`
   flag-spec table, `cmd_args_flag`; `src/commands/execution.c`)
   In `entity_flag_specs` both `result` and `error` are marked boolean
   (`has_value = 0`), and `cmd_args_flag(ga, "result", 0)` never consumes
   the following token — so `exec complete 42 --result "answer"` stores
   `NULL` and exits 0. Only the `--result=...` form works, contradicting
   the documented example (`exec complete 42 --result "answer text"`).
   Data-loss bug with exit 0; same root cause as #1 (the absent/present/
   boolean conflation).

4. **`resolve_input_source` leaks the full stdin payload to stderr**
   (`include/commands.h`)
   A leftover `fprintf(stderr, "DEBUG read_stdin_all: %zu bytes: '%s'")`
   (marked "remove after diagnosis") runs on every `--stdin` use and dumps
   the entire payload — potentially megabytes of context content — into
   stderr, polluting the "line 1 is the JSON contract" stream.

5. **Error contract: two namespaces, broken code/exit invariant**
   (`src/main.c`, `cli_util.h`, `src/argparse.c`)
   `main.c` documents that the exit code "always matches the `code` field";
   that holds for `ACTA_DB_ERR_*` lines (`code` = raw negative rc, so
   `|code| == exit`) but not for `ACTA_CLI_ERR` lines, which emit
   `code: -10` while exiting `EXIT_INVALID` (4). Agents branching on
   `(code, exit)` need one namespace and one table.
   *Resolved (T2):* Option A from [`t2_analysis.md`](t2_analysis.md) —
   the exit code is canonical and `code` = −exit everywhere. All
   CLI-usage errors (unknown entity/action/option, bad `--verbose`,
   missing flag value, too few positionals) emit `ACTA_CLI_ERR`/
   `code:-10`/exit `10` (new `emit_cli_error` atom in `cli_util.h`; the
   shared `unknown_action` now emits the JSON line and returns `EXIT_CLI`);
   `parse_globals` distinguishes OOM (exit 3) / missing flag value /
   too-few-positionals and emits its own JSON line; the `--verbose` clamp
   line is VLOG-only; `ACTA_DB_ERR_DUPLICATE`/`FK`/`INVALID_DB` keep their
   names but carry `code:-4` with exit `4` (`finish_db_error` prints
   `code` as `map_rc_to_exit(rc)` negated); DB open failure emits
   `code:-11`/exit `11` (`emit_db_open_error`, `EXIT_DB_OPEN`) — the spec's
   exit 11 is now reachable. Contract pinned in
   `tests/cli_util/cli_util_test_error_contract.c`; `cli_spec.md`
   documents the invariant.

### Nitpicks

- Hard-coded offsets in `parse_globals` (`a[4]`, `a[6]`, `a[8]`, `a[9]`,
  `a[11]`) are correct today but fragile. A uniform
  `const char *eq = strchr(a, '=');` after `flag_prefix_match` covers every
  flag in one code path.
- `--tools` prints `[]` (TODO) yet help advertises it as "full command
  reference" — implement it or remove it from help text. The machine-readable
  tool schema is the single biggest gap for agentic use; T3 generates
  `--tools` from the per-action table in
  [`cli_spec.md`](cli_spec.md) (now the single source of truth) rather than
  hand-writing it.
- The per-action `create` usage snippets (6 entities) show
  `cat x.json | acta_cli <entity> create --json`, but a bare `--json` with no
  value is unparseable: as the last token, `parse_globals` returns `EXIT_CLI`
  and `main.c` prints "missing entity and/or action". The working stdin form
  is `--stdin`; fix the examples (see S4).
- `--pretty` is parsed (`argparse.c`) and advertised in `--help` ("2-space
  indent JSON") but honored by no emitter — dead flag; implement it or drop
  it from the help text (same class as the `--tools` TODO).
  *Resolved (T3):* implemented — `tools_print(FILE*, int pretty)` honors it,
  scoped to `--tools` (2-space indent, still valid JSON); pinned in the T4
  suite (`--pretty` output validated via `json_validate` + shape walk).
- Naming drift: `--from_file` selects the JSON input source, while `--file`
  selects the SQL file in `db exec` — two names for the same "read from file"
  concept.
- `--verbose 99` clamps with a plain-text stderr line ("--verbose: level
  clamped to 3") — fine for humans, but it is a non-JSON diagnostic outside
  the error-line contract.
- `main.c` re-checks `gopts.argc < 2` although `parse_globals` already
  guarantees ≥ 2. Harmless, but the manual `free(rest)` in 3 early-exit paths
  plus the final `free(gopts.argv)` is the kind of ownership protocol a future
  edit can double-free. Consider `g->argv = NULL` after freeing, unconditionally.

---

## Part 2: JSON layer (`json.h` / `json.c`) and input-source plumbing

### Bugs (worth fixing)

5. **Include cycle / wrong layering: `json.c` includes `commands.h`**
   just to get the entity structs (`model_t`, `skill_t`, …), while
   `commands.h` includes `json.h`. The entity structs are defined in
   `commands.h`, which is a dispatch-layer header. Move the structs to a
   dedicated `entities.h` (owned by the db-lib side or the CLI) so
   `json.c → entities.h` and no cycle exists.

### Design / consistency issues

6. **Parse functions silently accept nearly-empty payloads**
   `json_parse_model("{}")` returns 0 with every field NULL/0 — no required-field
   checking in the parse layer, and unknown/typo'd keys are ignored (no
   `"extra fields"` diagnostic). This is only safe because callers re-validate
   (`model.c` checks `name`/`backend` non-empty, etc.). That works, but it means
   every new command must remember to re-check; a typo'd key (`"nam"`) degrades
   to a missing-field error at the call site, which is the best achievable here,
   but consider returning the parsed-into struct plus a "which keys were seen"
   diagnostic at `VLOG(2)` to make typo hunting cheap.

7. **`jget_int` truncates and wraps** (`json.c`)
   `(int)item->valuedouble` maps `3.7 → 3` and silently wraps ids beyond
   `INT_MAX`. Use a range check (`item->valuedouble >= INT_MIN && <= INT_MAX`)
   or `cJSON_GetNumberValue`, and decide whether floats should be an error.

8. **`>0 else 0` clamping conflates three states**
   `(id > 0) ? id : 0` is correct for "0 = root / no parent" semantics, but it
   also maps negative ids to 0 instead of erroring. Negative ids are almost
   certainly user error — clamp-to-root hides it. Flag negatives as invalid.

9. **NULL conflation in string getters**
    `dup_or_null`/`jget_str` return NULL for both "key absent" and OOM. Same
    ambiguity as the absent/boolean-flag conflation in Part 1 #1. Low risk
    (OOM is fatal in practice) but a
    `*ok` out-param or errno-style convention would make the contract explicit.

10. **Opaque `-1` parse errors**
    All parse failures return -1 with no detail. `cJSON_GetErrorPtr()` is right
    there — at minimum `VLOG(1, ...)` the error pointer + offset on failure so
    `--verbose` users can debug malformed input.

### Nitpicks

- Asymmetric API: parse exists for 7 entities, serialize for 4 (+1 array), and
  neither exists for `model_revision` / `skill_revision`. Fine if intentional,
  but the `/* ... one per entity ... */` comment makes it look accidental.
- `json_parse_context` maps the JSON key `"hash"` to `content_hash` while every
  other key is a straight name match — inconsistent wire format; prefer
  `"content_hash"`.
- `model.c`'s flag path (the `else` branch) uses the shared strict
  `parse_folder_id` helper (cli_util.h, `strtol` + endptr + ERANGE) for
  `folder_id` in create/update/move — good; the JSON path goes through
  `jget_int` instead, so the same input can be validated differently
  depending on which input mode the user chose.

---

## Part 3: small entities — `db.c`, `context.c`

### Design / consistency issues

3. **Success-output shapes are inconsistent across entities**
   `context create` → `{"id":N}`; `db exec` → `{"status":"ok"}`; `db version`
   → `{"version":"..."}`. Fine if the spec defines per-action shapes, but there
   is no single place that documents them; `--table` variants add a third
   shape each. Worth a spec table (action → stdout schema).
   *Resolved (T1):* the cross-entity per-action table now exists in
   [`docs/cli_spec.md`](cli_spec.md) — the single source of truth for the
   per-action stdout schema (all 10 entities, plus the error line and exit
   codes). The `db` shapes documented in `db_usage()` match it, and a code
   audit confirmed every entity emits exactly the documented shapes (the
   same table also closes P4 #7–8, P5 #4 and Agentic #3).

### Nitpicks

- `context` list/count duplicate the `context_query_t q = {...}` construction
  twice; a `build_query(flags)` helper would also keep flag names in one
  place when filters are added.
- `ctx_table` row uses `fprintf(f, " %4s  ", idb)` while the header uses
  `" %4s  %-10s ..."` — aligned today, but the two formats are maintained
  independently and will drift; generate the row from the same width table.
- `context create` via flags prints a *warning* (non-fatal) when all four
  flags are missing, then a *hard error* for `type` — the warning is pure
  noise since the error follows immediately; drop one.

---

## Part 4: big entities — model, skill, folders, revisions

### Systemic patterns (affect most of the family)

6. **Help text bugs, family-wide**
    - `model get --live` is documented as "Include soft-deleted rows"; the
      flag selects the *unfiltered* fetch — wording is backwards and should
      be "fetch even if soft-deleted".
    *Resolved (J2):* stale item — the current `model get` help (both
    `model_usage` and the `usage_get` snippet) already reads
    `--include_deleted  Return the row even if soft-deleted`; `--live` was
    the old flag name. No code change needed.

### Design / consistency

7. **Root-folder representation differs by output path**
    `model_to_json` emits `"folder_id":null` for root; `model move`'s
    success line emits `"folder_id":0`; `skill move` emits 0. Pick one wire
    representation per entity (and document it; spec §?).
    *Resolved (T1):* root folder is `null` in **every** JSON emit — the
    `model_to_json`/`skill_to_json`/revision JSON and the
    `emit_ok_folder`/`emit_ok_parent` move success lines all emit `null`
    for root; `0` survives only in `--table` (plain-text) output and in flag
    values. Documented in [`cli_spec.md`](cli_spec.md).

8. **Success shapes keep multiplying**
    `{"id":N}` / `{"id":N,"folder_id":M}` / `{"deleted":true}` /
    `{"id":N,"restored":true}` / bare `N`. Same ask as Part 3 #3: a single
    per-action output table in the spec, enforced by one emit helper.
    *Resolved (S1/V1 + T1):* both halves are done. Emit-helper half: the
    shared atoms `emit_ok_id` / `emit_ok_folder` / `emit_deleted`
    (`08ebc26`–`829fd5a`) plus the later `emit_ok_transition` (exec
    lifecycle) and `emit_ok_restored` (unified restore lines) cover every
    success path in all 9 entity files. Spec-table half:
    [`cli_spec.md`](cli_spec.md) documents the per-action shapes, and the
    restore-shape drift is gone — `model`/`skill`/`model_folder`/
    `skill_folder` restore all emit `{"id":N,"restored":true}` via
    `emit_ok_restored`.

9. **`usage_*` snippets are static per file, `*_usage` are not declared
    anywhere**
    `model_usage`, `ctx_usage`, `model_folder_usage`, … are non-static
    ("the dispatch layer can call this") but no header declares them, so the
    dispatch layer *cannot* call them (implicit declaration is an error in
    C99+). Either declare them in `commands.h` (and then
    `acta <entity> help`/`--help` can be wired centrally) or make them static.
    `db_usage` was resolved by making it static (only `cmd_db` calls it); the
    other 9 entities carry the issue. Note the `usage_create` name is reused
    (statically) across 8 files — fine, but a `usage_<entity>_<action>` naming
    convention would let the declarations coexist.
    *Resolved (J3):* all nine (`model_usage`, `ctx_usage`, `skill_usage`,
    `skill_folder_usage`, `skill_rev_usage`, `model_folder_usage`,
    `model_revision_usage`, `exec_usage`, `execution_log_usage`) are now
    `static`, matching the `db_usage` precedent; none was declared in a
    header and each is used only in its own file. The `commands.h`
    declaration route can be revisited if a central `acta <entity> --help`
    is ever wired.

10. **Structural duplication is the dominant cost in this family**
    Each of the 8 files re-implements: the
    usage text + per-action snippets, the id-parse block (×1–5), the
    JSON-vs-flags input split, the `goto cleanup` free list, the
    to-json/table/empty-`[]` emit triplet, and the unknown-action suggest
    block. That's why this family is ~5.4k lines for what is mostly CRUD.
    A small per-entity descriptor (field table: name, JSON key, required,
    type, flag name) + shared `create/get/list/count/delete/restore/move`
    drivers would collapse most of it *and* make the model/skill divergence
    impossible to repeat.
    *Partially resolved (S1/V1):* the atom half landed — the common atom set
    (`parse_id_positional`, `parse_nonneg_int_flag`, `parse_offset_limit`,
    `require_flag`, `load_row_or_notfound`, `emit_*`) lives in `cli_util.h`
    and is adopted by all 9 entity files (`08ebc26`–`829fd5a`), collapsing
    the id-parse / flag / pagination / not-found / success-emit blocks to
    one-liners. The field-descriptor + shared-driver half was considered and
    excluded (V3), and the per-verb kernels (V2) and per-entity contract
    suite (V4) were dropped; the per-verb control-flow skeleton remains
    hand-written per file, divergence prevented by review.

---

## Part 5: execution, log, Makefile, tests

### Design / consistency

3. **Test architecture: in-process handler calls, global parse layer
   untested** (tests/ overall)
   The suites build `cmd_args_t`/`global_opts_t` via `targs_*` helpers and
   call `cmd_*` directly — fast and well-structured, but the *entire* class
   of bugs found in Parts 1–4 (`--json` blob ignored + stdin hang, `--stdin`
   eaten by `parse_globals`, bare `--json` not parseable, dead local
   `--count`) lives in the argv layer the tests skip. A suite that feeds raw
   `argv` through `parse_globals` + `commands_dispatch` (or spawns the built
   binary) would have caught all of them. This is the highest-value coverage
   gap in the project.
   *Resolved (S2):* `tests/gparse/gparse_test_main.c` is the dedicated
   raw-argv suite; it exercises the seam end to end (rejections, value
   consumption, input sources, unknown entity/action via dispatch).

4. **Exec lifecycle transitions emit empty stdout on success**
   (`src/commands/execution.c`)
   `start`, `cancel`, `complete`, `fail`, and `set-raw` print nothing on
   success — only the exit code confirms the transition. `db exec` is the
   only action whose stdout schema is documented in its usage. S3 must
   decide the transition success shape (e.g. `{"id":N,"status":"..."}`) so
   scripts and agents can confirm outcomes by parsing stdout.
   *Resolved (T1):* all four transitions emit `{"id":N,"status":"<s>"}`
   via the shared `emit_ok_transition` atom (bare `N` with `--id_only`);
   `set-raw` re-fetches the row and echoes the unchanged current status.
   Documented in [`cli_spec.md`](cli_spec.md).

### Makefile

4. **Every new test suite needs ~7 manual edits**
   New-suite checklist today: `SRCS/OBJS/TARGET` vars, `ALL_TEST_TARGETS`,
   the `test` recipe line, the `clean` list, a `-Itests/<suite>` flag, the
   header comment, (and the suite's own `*_test_main.c`). Generate it:
   ```make
   TEST_DIRS := $(wildcard tests/*)
   TEST_BINS := $(foreach d,$(TEST_DIRS),$(notdir $(d))_test)
   ```
   (or keep explicit targets but derive them) — one `tests/foo/` directory
   should "just work".

5. **`libacta_db.a` consumed with no dependency on it**
   `LDFLAGS += ../acta_db/libacta_db.a …` — if the lib is missing/stale you
   get a confusing link failure, and `make` will never rebuild it. Add:
   ```make
   *Resolved (`787b518`):* the Makefile now builds `../acta_db/libacta_db.a`
   via a rule keyed on the lib sources and lists it as a prerequisite on the
   app and all test targets. Residual: the libraries still sit in `LDFLAGS`
   rather than `LDLIBS` — they only link correctly because `LDFLAGS` happens
   to expand after the objects; a user-supplied `LDFLAGS` would put them
   before and break static linking.

---

## Agentic usage — what is missing

The CLI is already pipe-safe (stdout carries data, stderr carries the
contract), but for LLM/script drivers the following are missing:

1. **`--tools` is a stub** — the machine-readable tool/command schema
   (entities, actions, positionals, flags, required fields, input JSON
   keys, success stdout shapes, exit codes, error shape) is a TODO that
   prints `[]`. Until it exists, an agent must scrape help prose and guess
   the contract. T3 generates it from the per-action table in
   [`cli_spec.md`](cli_spec.md) (now the single source of truth).
   *Resolved (T3–T4 + M4–M6):* `src/tools.c` holds the 69-entry schema
   (renderer + `--pretty`), its contract suite is green in `make test`, and
   the three table follow-ups are closed (M4 `--all` on `skill
   list`/`count`, M5 optional `skill_folder move --parent_id`, M6 per-entry
   `aliases` `["execution"]` / `["execution_log"]`).
2. **One unified error contract** — see P1 #5: two namespaces
   (`ACTA_DB_ERR_*` raw rc vs `ACTA_CLI_ERR` `-10`) and a code/exit-code
   invariant that does not hold for CLI errors. One table, one namespace.
   *Resolved (T2):* the exit code is canonical, `code` = −exit
   everywhere, and the error shape is settled — `ACTA_DB_ERR_*` names for
   library failures, `ACTA_CLI_ERR`/`code:-10`/exit `10` for every
   argv/usage error, `code:-11`/exit `11` for DB open failure. Full
   analysis and the chosen direction in
   [`t2_analysis.md`](t2_analysis.md); `cli_spec.md` documents the
   invariant, so T3 `--tools` can state the error shape truthfully.
3. **Stable, documented success shapes** — ✅ *resolved* (S3 / P3 #3 /
   P4 #7–8 / P5 #4, via T1): the per-action stdout table, the root-folder
   wire representation (`null` in every JSON emit), and the restore-shape
   drift are settled and implemented; transitions emit
   `{"id":N,"status":"<s>"}` instead of silence. All in
   [`cli_spec.md`](cli_spec.md).
4. **Discoverability** — `--help` anywhere in argv short-circuits to
   top-level help (pass 1), so per-entity help is only reachable as
   `acta_cli <entity> help`; the undeclared `*_usage` functions (P4 #9 /
   J3) block wiring a central `acta_cli <entity> --help`. Entity names
   `exec` / `log` differ from the docs' "execution" / "execution_log" —
   the fuzzy "Did you mean" covers typos, but doc-driven agents will emit
   `acta_cli execution list`.
   *Partially resolved (J3 + M6):* the `--tools` schema now states the
   aliases literally (per-entry `aliases`), and the `*_usage` functions are
   settled (static, J3). Wiring a central `acta_cli <entity> --help` remains
   an open design choice.
5. **`db exec` is an unguarded escape hatch** — arbitrary mutating SQL
   (no SELECT) with no scope limit; fine as an escape hatch, but it is the
   one surface where an agent can do anything, so document it as such (or
   scope it).

---

# Summary (unresolved, by severity)

1. **`--result` / `--error` silently drop their value** — `exec complete 42 --result "text"` stores `NULL` and exits 0; the documented example is the broken form. Data-loss bug. *(P1 #3)*
2. **DEBUG stdin leak** — `resolve_input_source` (`include/commands.h`) dumps the full stdin payload to stderr on every `--stdin` use; one-line removal. *(P1 #4)*
3. **Broken `create --json` usage examples** — 6 entity snippets show `cat x.json | acta_cli <entity> create --json`; bare `--json` is unparseable, the working form is `--stdin`. *(P1 nitpick, S4)*
4. **Global parse layer untested** — ✅ *resolved (S2)*: dedicated suite `tests/gparse/gparse_test_main.c` feeds raw argv through the `main.c` seam (parse_globals → aliases → validate → handler/dispatch); pins too-few positionals, missing flag values, value consumption, unknown options, `--verbose` clamp, the three input sources + exclusion, and unknown entity/action via `commands_dispatch`. Green in `make test`. *(P5 #3)*
5. **`--tools`** — ✅ *resolved* (T1–T4): the per-action stdout table and the wire-format decisions (transition shape, root-folder `null`, restore drift) are settled and implemented, documented in [`cli_spec.md`](cli_spec.md) (T1, done); the 69-entry schema is generated from that table (`src/tools.c`, T3, done) and its contract test suite is green in `make test` (T4, done). *(T3 — T2, its blocker, is now done)*
6. **Error-contract unification** — ✅ *resolved* (T2, Option A from [`t2_analysis.md`](t2_analysis.md)): one namespace (`ACTA_DB_ERR_*` names for library failures, `ACTA_CLI_ERR`/`code:-10` for argv/usage errors), `code` = −exit everywhere, and the spec's exit 11 (DB open failed) reachable via `emit_db_open_error`. *(P1 #5)
7. **Parse-layer inconsistencies** — `cmd_args_flag` protocol (root cause of #1), `parse_globals` error conflation, bare `--json`. *(S4)*
8. **JSON-layer + help + usage-declaration residue** — J1 (include cycle, `jget_int` truncation, clamping, NULL conflation, opaque `-1`) **open**; J2 (`model get --live` wording) ✅ *resolved* (stale item — current help wording already correct, no code change); J3 (`*_usage` declarations) ✅ *resolved* (all nine made static, matching `db_usage`).

**Structural (S1) — resolved:** the copy-paste family (P4 #10) was fixed
atom-first and stopped there: the common atom set landed in `cli_util.h`
and was adopted by all 9 entity files (`08ebc26`–`829fd5a`), canonicalizing
id/flag/offset-limit parsing, fetch+not-found, and success emits. The
field-descriptor + shared-driver form (V3) was excluded; per-verb kernels
(V2) and the per-entity contract suite (V4) were dropped. Per-verb skeletons
remain hand-written per file, divergence prevented by review.
