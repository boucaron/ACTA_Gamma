# acta_db_cli — Code Review

Review of the C CLI (`acta_db_cli/`, ~9k LOC). Conducted in parts:

| Part | Scope | Status |
|------|-------|--------|
| 1 | Entry point, arg parsing, dispatch (`main.c`, `argparse.c/h`, `cli.h`, `cli_util.h`, `commands.h/c`) | ✅ done |
| 2 | JSON layer (`json.h`, `json.c`) + input-source plumbing | ✅ done |
| 3 | Small entities (`db.c`, `context.c`) | ✅ done |
| 4 | Big entities (`model.c`, `skill.c`, `*_folder.c`, `*_revision.c`) | ✅ done |
| 5 | `execution.c`, `execution_log.c`, Makefile, tests | ✅ done |

> **Post-review changes (doc re-checked against the tree):**
> - Flag names were standardized to **underscores** (`--context_id`,
>   `--no_nulls`, `--id_only`, `--from_file`, `--include_deleted`, …);
>   dashed spellings below are updated.
> - Findings fixed since the review are no longer listed (strict
>   `--verbose` parsing, bare-`atoi` sites, negative `folder_id`/
>   `parent_id` handling, `--id_only` help mismatch). The strict id
>   helpers live in `cli_util.h`: `parse_positive_id`, `parse_nonneg_int`,
>   `parse_folder_id`.
> - Verbose logging consolidated to a single macro: `VLOG(lvl, fmt, ...)`
>   (`cli.h`), gated on `cli_gopts` (defined in `commands.c`, set once in
>   `commands_dispatch`).
> - The duplicate "did you mean" mechanisms are resolved: dead `action_err`
>   deleted; all unknown-action blocks centralized into the `unknown_action()`
>   helper (`cli_util.h`); suggestion threshold is length-relative
>   (`d<=1` for names <= 3 chars, else `d<=2`). Help-pointer entity names
>   normalized to `actagamma_db <entity>`.
> - `db version` reports `sqlite3_libversion()`; `version_print` (global
>   `--version`) does too.
> - The global error layer (`main.c`) is contract-clean: `cli_error` is a
>   thin wrapper over `finish_db_error` — JSON-escaped messages, the raw
>   library rc in `"code"`, exit code = `map_rc_to_exit(rc)` (former P1 #2 /
>   P1 #4). `map_rc_to_exit` covers the full `ACTA_DB_ERR_*` set; unknown rc
>   falls back to `EXIT_INVALID`, not `EXIT_SQL`.
> - `db.c` error output follows the single-line JSON contract: the
>   centralized `finish_db_error(rc, what)` helper (`cli_util.h`) emits
>   `{"error":"ACTA_DB_ERR_*","code":<rc>,"message":"..."}` as stderr line 1
>   and returns the mapped exit code; all 9 `db exec` error paths and
>   `db`'s unknown-action path route through it. The P4 #3 systemic work
>   (silent lib-failure paths in the other entities) still stands.
> - `db exec` supports the positional-argument form, reads SQL from stdin
>   via `--sql_stdin` (renamed from `--stdin`, which `parse_globals` eats),
>   rejects empty SQL, and a trailing `;` is verified accepted.
> - `db_usage` is static (only `cmd_db` calls it); the other entities'
>   `*_usage` still carry the declaration issue (P4 #9).
> - Input sources fixed (former P2 #1–2): a shared `resolve_input_source()`
>   (`commands.h`) now feeds every create handler + `skill update` —
>   `--json <blob>` → `--from_file` → `--stdin`; no source → flag mode;
>   conflicting / empty / unreadable → canonical JSON error +
>   `EXIT_INVALID` (`e58d956`). Per-suite input-source regression cases run
>   raw argv through `parse_globals` via the shared `stest_run_argv` helper
>   (`fd262ca`). Residual: a *bare* `--json` (no value) is still unparseable
>   — the parser was left unchanged on purpose (see S4 in
>   `cli_active_action.md`); help text no longer teaches it.
> - `skill update` data loss (former P4 #1) fixed: fetch-and-merge on the
>   live row, exactly the `model update` pattern; not-found now
>   `EXIT_NOT_FOUND` from the live-row fetch (`928c66f`).
> - Empty `name` rejected on the remaining update/rename paths (`model
>   update`, `model_folder rename`; `skill update`/`skill_folder rename`
>   already had it) — same contract as create (former P4 #5) (`341de4b`).
> - `exec create` now *rejects* `--status` (flag or JSON body) with a
>   canonical JSON error instead of validating it and silently letting the
>   lib drop it (former P5 #1; the lib's forced-`pending` comment remains
>   authoritative) (`cffe8fe`).
> - `json.h` header comment rewritten to describe the implemented parse
>   layer (former P2 #12) (`9f19a6a`). The `json.c` serialize stubs and
>   `json_print_table` remain open (P2 #3–4 / W4).
> - Server-authoritative field forgery (former P2 #9 / P3 #5) verified
>   closed at the lib: the create INSERTs take no client `created_at`, and
>   exec status is forced `pending`; `content_hash` is accepted by design.
> - Part 5 #3 (untested global parse layer) is narrowed, not closed: the
>   `stest_run_argv` in-process raw-argv path now exercises
>   `parse_globals` + handler in every JSON-input suite, but a dedicated
>   `parse_globals`/`commands_dispatch` suite is still missing (S2).

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

### Nitpicks

- Hard-coded offsets in `parse_globals` (`a[4]`, `a[6]`, `a[8]`, `a[9]`,
  `a[11]`) are correct today but fragile. A uniform
  `const char *eq = strchr(a, '=');` after `flag_prefix_match` covers every
  flag in one code path.
- Unknown-*entity* error has no suggestion list while unknown-*action* does —
  inconsistent UX for a one-line addition.
- `--tools` prints `[]` (TODO) yet help advertises it as "full command
  reference" — implement it or remove it from help text.
- `main.c` re-checks `gopts.argc < 2` although `parse_globals` already
  guarantees ≥ 2. Harmless, but the manual `free(rest)` in 3 early-exit paths
  plus the final `free(gopts.argv)` is the kind of ownership protocol a future
  edit can double-free. Consider `g->argv = NULL` after freeing, unconditionally.

### Positives

- Clean two-pass parsing design (globals vs. per-command).
- Strict stdout/stderr separation: JSON on stdout, all diagnostics on stderr —
  pipe-safe.
- Dispatch table + `entity_fn` callback is the right shape for per-entity
  files.
- `main` lifecycle is orderly: open → dispatch → close → `force_close`
  fallback with a warning.
- Consistent single-line JSON error shape across CLI and db error paths
  (`cli_error` / `finish_db_error`).

---

## Part 2: JSON layer (`json.h` / `json.c`) and input-source plumbing

### Bugs (worth fixing)

3. **Serialize half of the API is unimplemented stubs** (`json.c`)
   `json_serialize_{model,skill,context,execution}`, `json_serialize_model_array`
   all `return NULL` with a TODO, and the header documents `NULL` as the error
   value — callers cannot distinguish "unimplemented" from "error". Output is in
   fact hand-emitted per command (`fputs`/`json_str`/`tcol` sprinkled through
   `commands/*.c`), so this header API is aspirational. Decide: either implement
   the serializers (which would let every `create`/`get`/`update` share one
   emit path) or remove the declarations so the code doesn't advertise an API
   that always fails.

4. **`json_print_table` is an unimplemented no-op** (`json.c`)
   Declared in `json.h` ("print rows as aligned columns") with a TODO body that
   does nothing. Table output is actually done with `tcol` from `cli_util.h` —
   two table mechanisms. Remove the stub or implement it on top of `tcol`.

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

10. **NULL conflation in string getters**
    `dup_or_null`/`jget_str` return NULL for both "key absent" and OOM. Same
    ambiguity as the absent/boolean-flag conflation in Part 1 #1. Low risk
    (OOM is fatal in practice) but a
    `*ok` out-param or errno-style convention would make the contract explicit.

11. **Opaque `-1` parse errors**
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

### Positives

- Uniform, correct parse pattern: every path `cJSON_Delete(root)`s on exit,
  including all error branches (no leaks).
- `cJSON_GetObjectItemCaseSensitive` + `cJSON_IsString`/`IsNumber` guards make
  type-coercion behavior explicit rather than relying on cJSON's implicit
  conversions.
- Keeping stdout pipe-safe (JSON out, everything else stderr) is maintained in
  this layer.

---

## Part 3: small entities — `db.c`, `context.c`

### Bugs (worth fixing)

1. **`context get <missing-id>` returns exit 0 with empty stdout** (`context.c`)
   ```c
   if (!c) return EXIT_OK;   /* not found: silent success */
   ```
   A not-found lookup prints nothing and exits 0 — indistinguishable from
   success to any caller/script; the spec's `EXIT_NOT_FOUND` (1) + JSON error
   line is the documented contract (and `acta_db_context_get` signaling
   `err==OK, c==NULL` is exactly the "not found" case). Compare: `list` at
   least prints `[]`. Widespread now (see Part 4 #2).

2. **`context list --count` local flag is dead** (`context.c`)
   `--count` is a *global* flag, so `parse_globals` already consumes it into
   `gopts->count`; the local `s_count = cmd_args_flag(ga, "count", 0)` is
   unreachable and dead. The code happens to work via `gopts->count`, but the
   dead branch misleads readers about where the flag is handled. (Family-wide:
   Part 4 #4.)

### Design / consistency issues

3. **Success-output shapes are inconsistent across entities**
   `context create` → `{"id":N}`; `db exec` → `{"status":"ok"}`; `db version`
   → `{"version":"..."}`. Fine if the spec defines per-action shapes, but there
   is no single place that documents them; `--table` variants add a third
   shape each. Worth a spec table (action → stdout schema).
   *Partially resolved:* the `db` shapes (`db exec` →
   `{"status":"ok"}`, `db version` → `{"version":"<version>"}`) plus the
   JSON error line are now documented in `db_usage()` help; the
   cross-entity spec table remains open (see P4 #8).

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

### Positives

- `db exec` is contract-clean: positional / `--sql` / `--file` /
  `--sql_stdin` sources (mutually exclusive, first wins), empty SQL rejected,
  all error paths emit the single-line JSON error via `finish_db_error`,
  success shapes documented in `db_usage()`.
- `context` is the best-structured command file: single `goto cleanup_create`
  with ownership tracking (`json_owned`), required-field table with per-source
  (flag vs JSON key) hinting, `strtol` validation on numeric flags, and the
  "no recognised flags" early nudge.
- Consistent `err` out-param handling around every lib call (free-on-error
  paths included).
- `--fields`/`--no_nulls` filtering is implemented uniformly in
  `ctx_to_json` and shared by `get` and `list`.
- `list` empty → `[]` (valid JSON) rather than nothing.

---

## Part 4: big entities — model, skill, folders, revisions

### Systemic patterns (affect most of the family)

2. **Silent not-found: `get` returns exit 0 with empty stdout when the row
   doesn't exist** — `context`, `model`, `skill`, `model_folder`,
   `skill_folder`, and both `*_revision` files all do
   `if (!row) { VLOG(1, "not found..."); return EXIT_OK; }`.
   Widespread now (Part 3 #1 was the first instance). Should be
   `EXIT_NOT_FOUND` (1) + the JSON error line.

3. **Library failures exit non-zero with *nothing* on stderr**
   Every `if (rc != ACTA_DB_OK) { VLOG(...); return map_rc_to_exit(rc); }`
   path (get/update/delete/restore/move/list/count, all entities) prints no
   error JSON — the single-line-JSON-on-stderr contract (Part 1) is only
   implemented for *input-validation* errors. `db.c` emits the JSON error
   line via `finish_db_error` (the reference implementation); these print
   nothing but an exit code. Centralize: route these paths through
   `finish_db_error(rc, what)`.

4. **Dead local `--count` flag** — every `list` action reads
    `cmd_args_flag(ga, "count", 0)` / `cmd_args_has_flag(ga, "count")` even
    though `parse_globals` already consumed `--count` into `gopts->count`
    (Part 3 #2). Dead in model, skill, context, model_folder, skill_folder,
    and both revisions.

6. **Help text bugs, family-wide**
    - `model get --live` is documented as "Include soft-deleted rows"; the
      flag selects the *unfiltered* fetch — wording is backwards and should
      be "fetch even if soft-deleted".

### Design / consistency

7. **Root-folder representation differs by output path**
    `model_to_json` emits `"folder_id":null` for root; `model move`'s
    success line emits `"folder_id":0`; `skill move` emits 0. Pick one wire
    representation per entity (and document it; spec §?).

8. **Success shapes keep multiplying**
    `{"id":N}` / `{"id":N,"folder_id":M}` / `{"deleted":true}` /
    `{"id":N,"restored":true}` / bare `N`. Same ask as Part 3 #3: a single
    per-action output table in the spec, enforced by one emit helper.

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

### Positives

- `model.c` is the best file in the CLI: robust id parsing, fetch-and-merge
  update (the correct pattern), required-field validation on create, and
  the `json_owned` cleanup discipline. `model move` correctly errors on the
  missing required `--folder_id` (unlike the old silent move-to-root).
- The family's uniformity (same section banners, same action order, same
  cleanup pattern) is a real virtue — it's what makes the review above
  transferable file-to-file.
- `skill create`'s cleanup list is complete (all 7 malloc-able fields) —
  the standard the other cleanups should meet.
- Read-only revision entities are appropriately minimal (get / get-latest /
  list / count, no mutations).

---

## Part 5: execution, log, Makefile, tests

### Design / consistency

2. **`exec` positional ids are strict** (`parse_positive_id` in
   `get`/`start`/`cancel`/`complete`, and `execution_log.c` for
   `--execution_id` filters), but not-found `get` still → silent
   `EXIT_OK`, so Part 4 #2 still applies here. The `exec create` flag ids and
   the `exec list`/`count` query filters (`--context_id` etc.) are strict
   (`parse_id_flag`); only the not-found issue above remains in this file.

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

5. **`libacta_db.a` is consumed with no dependency on it**
   `LDFLAGS += ../acta_db/libacta_db.a …` — if the lib is missing/stale you
   get a confusing link failure, and `make` will never rebuild it. Add:
   ```make
   ../acta_db/libacta_db.a:
   	$(MAKE) -C ../acta_db
   ```
   and list it (order-only) on the link targets. (Also: libraries belong in
   `LDLIBS`, not `LDFLAGS` — currently they only link correctly because
   `LDFLAGS` happens to expand after the objects; a user-supplied
   `LDFLAGS` would put them before and break static linking.)

6. **POSIX-only assumptions, Windows artifacts in the tree**
   Targets have no `.exe`, `test` runs `./$(TEST_TARGET)`, `clean` uses
   `rm -f` — yet the tree contains `actagamma_db.exe`, `test_*.exe` and
   `.o` files, i.e. it *was* built under Windows somehow (patched makefile or
   separate toolchain). Either document "POSIX only" or add `$(EXEEXT)` and
   a `del`-equivalent path. Related hygiene: the root `.gitignore` covers
   `*.db` but not `*.o`, `*.exe`, `*.a` — the tree currently carries dozens
   of `.o` and 9 `.exe` binaries. All untracked, so not a commit bug, but one
   `make clean` + a few `.gitignore` lines away from a clean tree.

7. **Shared test framework lives inside one suite**
   `HELPERS_OBJ := tests/skill/skill_test_helpers.o` is linked into every
   other suite, and every suite's `-I` path includes `tests/skill` — the
   "shared" framework is owned by the skill suite's directory. Move to
   `tests/helpers/` (or `tests/common/`) so the ownership is visible.
   (Minor inconsistency: the skill target gets the helper via its own
   wildcard instead of `HELPERS_OBJ`.)

8. **`test` stops at the first failing suite**
   Each recipe line is one shell; make aborts on first non-zero. Fine for
   CI red-early, but for local dev a `for t in …; do ./$t || fail=1; done`
   (or `make -k`) reports the full damage in one run.

### Positives

- **The lib defends the state machine** (`acta_db_execution_create` forces
  `status='pending'`, transitions only via start/complete/fail/cancel) —
  the one server-authoritative-field risk from Part 2 turned out to be
  handled correctly, and the CLI now rejects the dead flag (`cffe8fe`).
- Test volume is proportionate: ~10.8k lines of tests vs ~9.2k of app.
- Per-suite binaries isolate failures and keep link units small;
  `APP_OBJS_NO_MAIN` (link app minus `main.o` into tests) is a clean way to
  test handlers in-process without an HTTP/shell layer.
- Reference-DB pattern (`acta_test_ref.db` + `.sql` committed, copied per
  run) gives suites hermetic, reproducible state.
- Non-fatal assert counters + per-suite `main` aggregation give good failure
  summaries already.

---

# Summary (unresolved, by severity)

1. **Silent not-found: `get` → exit 0, empty stdout** — all 8 entities. *(P4 #2)*
2. **DB failures exit non-zero with no stderr output** — JSON error contract only implemented for input validation (`db.c` is the reference). *(P4 #3)*
3. **Global parse layer untested** — the layer that owns the input-source class and all the flag-shadowing issues; orphaned `tests_parse_globals.c` is the seed for that suite. *(P5 #3)*

**Structural recommendation:** the copy-paste family (P4 #10) is where most
bugs live. A per-entity *field descriptor* (name, JSON key, flag, type,
required, root-vs-null semantics) plus shared `create/get/list/count/
update/delete/restore/move` drivers would fix the divergences structurally
(silent not-found, dead `--count`, success shapes) rather than file-by-file.
