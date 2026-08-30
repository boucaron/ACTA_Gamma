# acta_db_cli — Code Review

Review of the C CLI (`acta_db_cli/`, ~9k LOC). Conducted in parts:

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

### Nitpicks

- Hard-coded offsets in `parse_globals` (`a[4]`, `a[6]`, `a[8]`, `a[9]`,
  `a[11]`) are correct today but fragile. A uniform
  `const char *eq = strchr(a, '=');` after `flag_prefix_match` covers every
  flag in one code path.
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
    *Partially resolved (S1/V1):* the emit-helper half is done — the shared
    atoms `emit_ok_id` / `emit_ok_folder` / `emit_deleted` in `cli_util.h`
    are adopted by all 9 entity files (`08ebc26`–`829fd5a`). The spec table
    remains open (S3); evidence to settle it: `model_folder restore` →
    hand-rolled `{"id":N,"restored":true}` (`model_folder.c:707`) vs
    `{"id":N}` on `model`/`skill`/`skill_folder` restore.

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
    *Partially resolved (S1/V1):* the atom half landed — the common atom set
    (`parse_id_positional`, `parse_nonneg_int_flag`, `parse_offset_limit`,
    `require_flag`, `load_row_or_notfound`, `emit_*`) lives in `cli_util.h`
    and is adopted by all 9 entity files (`08ebc26`–`829fd5a`), collapsing
    the id-parse / flag / pagination / not-found / success-emit blocks to
    one-liners. The field-descriptor + shared-driver half was considered and
    excluded (V3), and the per-verb kernels (V2) and per-entity contract
    suite (V4) were dropped; the per-verb control-flow skeleton remains
    hand-written per file, divergence prevented by review.

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

1. **Global parse layer untested** — the layer that owns the input-source class and all the flag-shadowing issues; orphaned `tests_parse_globals.c` is the seed for that suite. *(P5 #3)*

**Structural (S1) — resolved:** the copy-paste family (P4 #10) was fixed
atom-first and stopped there: the common atom set landed in `cli_util.h`
and was adopted by all 9 entity files (`08ebc26`–`829fd5a`), canonicalizing
id/flag/offset-limit parsing, fetch+not-found, and success emits. The
field-descriptor + shared-driver form (V3) was excluded; per-verb kernels
(V2) and the per-entity contract suite (V4) were dropped. Per-verb skeletons
remain hand-written per file, divergence prevented by review.
