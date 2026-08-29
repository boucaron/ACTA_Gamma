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
>   `commands_dispatch`). Removed: `vdbg` (cli.h), the unused
>   `VLOG(lvl, gopts, ...)` and dead `VERBOSE`/`gopts_local` macros
>   (cli_util.h), and the per-TU `vlog_gopts` + local `#define VLOG`
>   copies in the 10 command files (former P1 #5, P3 #11, and the
>   VLOG part of P4 #13).
> - The duplicate "did you mean" mechanisms (former P1 #4) are
>   resolved: dead `action_err` (common-prefix, zero call sites)
>   deleted; all 10 unknown-action blocks centralized into the
>   `unknown_action()` helper (`cli_util.h`); suggestion threshold is
>   length-relative (`d<=1` for names <= 3 chars, else `d<=2`).
>   Help-pointer entity names normalized to `actagamma_db <entity>`
>   (`execution_log` was reporting itself as `db`).
> - `db version` now reports `sqlite3_libversion()` (former P3 #1 is
>   resolved; it no longer appears in Part 3 below).
> - `db.c` error output now follows the single-line JSON contract: the
>   centralized `finish_db_error(rc, what)` helper (`cli_util.h`) emits
>   `{"error":"ACTA_DB_ERR_*","code":<rc>,"message":"..."}` as stderr line 1
>   and returns the mapped exit code; all 9 `db exec` error paths and
>   `db`'s unknown-action path route through it (Part 3 #6, marked
>   resolved below). The P4 #6 systemic work (silent lib-failure paths in
>   the other entities) still stands.

---

## Part 1: entry point, arg parsing, dispatch

### Bugs (worth fixing)

1. **`cmd_args_next_positional` misparses around boolean flags** (`src/argparse.c`)
   The "skip flag + its value" heuristic treats the *next non-flag token* as the
   flag's value unconditionally. If a handler uses a boolean flag followed by a
   positional (e.g. `--draft --title X`), the positional gets silently eaten.
   **Fix:** the parser needs to know which flags take values (e.g. a
   name→has_value table passed by the handler), not a positional heuristic.

2. **`cli_error` emits invalid JSON when the message contains quotes/backslashes**
   (`src/main.c`)
   The message is `vfprintf`'d raw into a JSON string; a db path containing `"`
   or `\` breaks the one-line JSON error contract that tools parse.
   **Fix:** reuse the existing `json_str` escaping helper (note the header
   dependency direction: `cli_util.h` → `main.c` already includes it).

### Design / consistency issues

3. **`cmd_args_flag` doc contradicts implementation** (`include/argparse.h`, `src/argparse.c`)
   Header claims the call "advances past it" and describes a
   value/boolean protocol; the implementation never advances `pos`, and returns
   `NULL` both for "flag absent" and "boolean flag present" — callers cannot
   distinguish the two. Additionally `cmd_args_has_flag` scans from `it->pos`
   while `cmd_args_flag` scans from 0. The truncated comment block
   (`if (!v) { // not present`) shows this API was half-refactored.
   **Fix:** commit to one protocol (e.g. `flag()` never advances; return a
   small struct `{present, value}`), fix the docs, and reconcile the scan
   ranges.

4. **Dead/inconsistent error-code plumbing in `cli_error`** (`src/main.c`)
    - `exit_code` parameter is ignored (`(void)exit_code`).
    - Callers mix string codes (`"ACTA_CLI_ERR"`) with numeric `c_code` values
      of different provenance (`-10` CLI codes vs. raw library rc in
      `ACTA_DB_OPEN_FAIL`). Decide the schema once (spec §7.1 exists — enforce
      it here).

5. **`parse_globals` conflates three error classes** (`src/argparse.c`)
    OOM on the `rest` malloc, a missing value for `--db`/`--json`/etc., and
    "fewer than 2 positionals" all return `EXIT_CLI` (10) with no message;
    `main.c` then prints "missing entity and/or action" — misleading for the
    other two cases. OOM should be `EXIT_ALLOC` (3); missing flag values need
    their own message naming the flag.

6. **Duplicate declaration of `cmd_skill`** at the bottom of
    `include/commands.h` — copy-paste leftover.

### Nitpicks

- Hard-coded offsets in `parse_globals` (`a[4]`, `a[6]`, `a[8]`, `a[9]`,
  `a[11]`) are correct today but fragile. A uniform
  `const char *eq = strchr(a, '=');` after `flag_prefix_match` covers every
  flag in one code path.
- `version_print` hard-codes `"libacta_db 0.1.0, sqlite 3.x.x"` instead of
  using `ACTA_DB_CLI_VERSION` / a lib version macro.
- `map_rc_to_exit`'s `default:` maps *any* unknown library error to
  `EXIT_SQL` — an IO-style code would be misreported as SQL.
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
- Consistent single-line JSON error shape (once #2 is fixed).

---

## Part 2: JSON layer (`json.h` / `json.c`) and input-source plumbing

### Bugs (worth fixing)

1. **`--json <blob>` is ignored — all create commands read stdin instead**
   (copy-pasted in 9 places: `model.c:470`, `skill.c:460,669`, `context.c:220`,
   `execution.c:483`, `execution_log.c:335`, `model_folder.c:389`,
   `skill_folder.c:392`)
   ```c
   if (gopts->json_input) {
       char *blob = read_stdin_all();   /* ← blob in gopts->json_input unused */
   ```
   The JSON the user passed on the command line is silently discarded and the
   program instead blocks on stdin — which *hangs* for an interactive user with
   no pipe. Fix: use `gopts->json_input` (validate with `json_validate`, then
   parse); only fall back to stdin/file for the other two sources.

2. **`--stdin` and `--from_file` are dead flags**
   `gopts->from_stdin` and `gopts->from_file` are parsed by `parse_globals` and
   documented in `--help`, but no command handler ever references them. All
   three input sources (blob / stdin / file) collapsed into one mis-wired
   branch (see #1). Either implement the selection logic once (a
   `resolve_input_source()` helper returning the blob + ownership flag) or drop
   the flags from the parser and help text.

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

9. **Client-supplied server-authoritative fields**
   `json_parse_execution` accepts `status` and `created_at` from the input blob;
   `json_parse_context` accepts `content_hash` and `created_at`. If a create
   handler forwards the parsed struct straight to the db layer, a caller can
   forge execution status, timestamps, and content hashes. Verify in Parts 3/5
   that create paths drop/override these fields; the safer contract is a
   `json_parse_*_create()` that excludes server-owned fields, or zeroing them
   after the generic parse.

10. **NULL conflation in string getters**
    `dup_or_null`/`jget_str` return NULL for both "key absent" and OOM. Same
    ambiguity as the absent/boolean-flag conflation in Part 1 #3. Low risk
    (OOM is fatal in practice) but a
    `*ok` out-param or errno-style convention would make the contract explicit.

11. **Opaque `-1` parse errors**
    All parse failures return -1 with no detail. `cJSON_GetErrorPtr()` is right
    there — at minimum `VLOG(1, ...)` the error pointer + offset on failure so
    `--verbose` users can debug malformed input.

12. **Stale header comment** (`json.h`)
    "JSON serialize/parse stubs. Real implementation will use cJSON …
    signatures are placeholders" — the file *is* the cJSON implementation now.
    The comment describes a previous life of the file.

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
  depending on which input mode the user chose. (The copy-pasted strtol
  block that was once unique to model.c has been extracted to `cli_util.h`;
  the atoi sites it tracked have since been converted to the strict
  helpers as well.)

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

1. **`db exec` does not support the documented positional-argument form** (`db.c`)
   Help and the "no SQL source" error both advertise
   `acta db exec "INSERT INTO ..."` (positional), but the resolver only checks
   `--sql` / `--file` / `--stdin` — `cmd_args_next_positional` is never called.
   The documented form fails with "no SQL source provided".

2. **`db exec --stdin` is dead: the global parser eats `--stdin`** (`db.c`)
   `parse_globals` pulls `--stdin` into `gopts->from_stdin` and removes it from
   the command argv, so `cmd_args_flag(ga, "stdin", 0)` can never see it.
   Same class as Part 2 #2: entity-level flags must not collide with global
   flag names. Rename the entity flag (e.g. `--sql_stdin`) or check
   `gopts->from_stdin` here.

3. **`context create` leaks `created_at` on the JSON path** (`context.c`)
   `json_parse_context` mallocs `ctx.created_at` when the blob carries the key,
   but `cleanup_create` only frees `type/content/content_hash/metadata`.
   Any JSON body containing `"created_at"` leaks.

4. **`context get <missing-id>` returns exit 0 with empty stdout** (`context.c`)
   ```c
   if (!c) return EXIT_OK;   /* not found: silent success */
   ```
   A not-found lookup prints nothing and exits 0 — indistinguishable from
   success to any caller/script; the spec's `EXIT_NOT_FOUND` (1) + JSON error
   line is the documented contract (and `acta_db_context_get` signaling
   `err==OK, c==NULL` is exactly the "not found" case). Compare: `list` at
   least prints `[]`.

5. **`context list --count` local flag is dead** (`context.c`)
   `--count` is a *global* flag, so `parse_globals` already consumes it into
   `gopts->count`; the local `s_count = cmd_args_flag(ga, "count", 0)` is
   unreachable and dead. The code happens to work via `gopts->count`, but the
   dead branch misleads readers about where the flag is handled.

### Design / consistency issues

6. **Error output contract violation in `db.c`** — ✅ **fixed**
   Every error in `db exec` / unknown-action is human text on stderr
   ("Error: SQL execution failed (rc=%d)…"), while the rest of the CLI
   (and Part 1's design) emits single-line JSON `"{\"error\":...}`. Pick one
   contract and keep it — scripts parsing stderr JSON will choke on db errors.
   *Resolved:* all `db` error paths now use the centralized
   `finish_db_error(rc, what)` helper (`cli_util.h`), which emits the
   single-line JSON error and returns the mapped exit code; the
   unknown-action path prints the JSON line first, then the shared
   human suggestion block. See `cli_active_action.md` (plan #1).

7. **Success-output shapes are inconsistent across entities**
   `context create` → `{"id":N}`; `db exec` → `{"status":"ok"}`; `db version`
   → `{"version":"..."}`. Fine if the spec defines per-action shapes, but there
   is no single place that documents them; `--table` variants add a third
   shape each. Worth a spec table (action → stdout schema).
   *Partially resolved:* the `db` shapes (`db exec` →
   `{"status":"ok"}`, `db version` → `{"version":"<version>"}`) plus the
   JSON error line are now documented in `db_usage()` help; the
   cross-entity spec table remains open (see P4 #11).

8. **Bare `--json` is not a valid invocation, yet help teaches it** (`context.c` usage)
   `ctx_usage` shows `... | acta context create --json` (boolean, stdin
   semantics), but `parse_globals` defines `--json <blob>` as a *value* flag:
   a trailing bare `--json` returns `EXIT_CLI` (missing value), and a bare
   `--json` mid-argv silently eats the next token as the blob. The usage text
   and the parser describe two different features (this is the root cause of
   Part 2 #1/#2: the intended trichotomy blob/stdin/file was never built).

9. **Client-supplied `content_hash` and `created_at` in `context create`**
    The hash is a client claim the DB never verifies (by design perhaps), but
    `created_at` from the JSON blob is forwarded into
    `acta_db_context_create` — confirm the lib ignores it (Part 2 #9 carries
    into this entity concretely).

### Nitpicks

- `db exec` accepts a 0-byte SQL file / empty stdin and calls
  `acta_db_exec(db, "")` — reject empty SQL with a clear error instead of
  relying on the lib's reaction.
- `db exec` help example ends with `VALUES ('Ada');` (trailing semicolon) —
  verify the lib accepts a trailing `;` for a "single statement"; if not, fix
  the example (first-run users will hit it).
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

### Bugs (worth fixing)

1. **`skill update` destroys data: partial struct passed to a full-replace API** (`skill.c` + `acta_db/include/skill.h`)
   The db lib is explicit:
   > "Optional fields: NULL in struct → SQL NULL in column, **overwriting prior value** … Populate the full struct. To change a single field: 1. `acta_db_skill_get` 2. modify the one field 3. call `acta_db_skill_update` with the **fully-populated** struct."
   The CLI does neither: `skill_t s = {0}` + only the passed flags, then
   `acta_db_skill_update(db, &s)`. Consequences for
   `skill update 5 --name X --prompt_template Y` (the two required fields
   only):
   - `description`, `output_schema` → SQL NULL (wiped),
   - `folder_id` 0 → **moved to root folder**.
   Compare `model update`, which correctly fetches the live row and
   shallow-merges. Either do the same fetch-and-merge in `skill update`, or
   make the update API NULL-means-unchanged (COALESCE) — as written it is a
   silent data-loss path in both flag and JSON modes.

2. **`model move` without `--folder_id` silently moves to root** (`model.c`)
   `f_folder_id` missing → `folder_id = 0` → move to root, while the usage
   text says "Required: `--folder_id <int>`". `skill move` (same operation)
   correctly errors on the missing flag — model move is the odd one out.

3. **Garbage in `model update` VLOG** (`model.c`)
   ```c
   has_folder ? (folder_val == 0 ? "root" : (char[]){'0'+folder_val,0}) : ...
   ```
   The compound literal builds a 2-byte string whose first byte is
   `'0' + folder_val` — for any folder id ≥ 10 this is a non-printable
   garbage byte printed to stderr. (Only surfaces at `-v 1`, but trivially
   wrong.) Use `%d`.

4. **`model_folder create` leaks 3 fields on the JSON path** (`model_folder.c`)
   `cleanup_mf_create` frees only `mf.name`, but `json_parse_model_folder`
   also mallocs `created_at`, `updated_at`, `deleted_at`. (Skill create's
   cleanup frees all seven — it's the reference implementation for the family.)

### Systemic patterns (affect most of the family)

5. **Silent not-found: `get` returns exit 0 with empty stdout when the row
   doesn't exist** — `context`, `model`, `skill`, `model_folder`,
   `skill_folder`, and both `*_revision` files all do
   `if (!row) { VLOG(1, "not found..."); return EXIT_OK; }`.
   Widespread now (Part 3 #4 was the first instance). Should be
   `EXIT_NOT_FOUND` (1) + the JSON error line.

6. **Library failures exit non-zero with *nothing* on stderr**
   Every `if (rc != ACTA_DB_OK) { VLOG(...); return map_rc_to_exit(rc); }`
   path (get/update/delete/restore/move/list/count, all entities) prints no
   error JSON — the single-line-JSON-on-stderr contract (Part 1) is only
   implemented for *input-validation* errors. `db.c` at least prints human
   text; these print nothing but an exit code. Centralize this:
   one `finish_db_error(rc, what)` that emits the JSON line and returns the
   mapped code.

7. **Dead local `--count` flag** — every `list` action reads
    `cmd_args_flag(ga, "count", 0)` / `cmd_args_has_flag(ga, "count")` even
    though `parse_globals` already consumed `--count` into `gopts->count`
    (Part 3 #5). Dead in model, skill, context, model_folder, skill_folder,
    and both revisions.

8. **Update/rename paths skip the empty-string checks that create has**
    `model create` rejects `name == ""`; `model update --name ""` and
    `model_folder rename 7 --name ""` accept it. Apply the same check to
    update/rename.

9. **Help text bugs, family-wide**
    - `--json` is documented as a bare flag ("read from stdin") — still the
      Part 2/3 issue: the parser requires `--json <blob>`.
    - `model get --live` is documented as "Include soft-deleted rows"; the
      flag selects the *unfiltered* fetch — wording is backwards and should
      be "include soft-deleted rows in the result" → "fetch even if
      soft-deleted".

### Design / consistency

10. **Root-folder representation differs by output path**
    `model_to_json` emits `"folder_id":null` for root; `model move`'s
    success line emits `"folder_id":0`; `skill move` emits 0. Pick one wire
    representation per entity (and document it; spec §?).

11. **Success shapes keep multiplying**
    `{"id":N}` / `{"id":N,"folder_id":M}` / `{"deleted":true}` /
    `{"id":N,"restored":true}` / bare `N`. Same ask as Part 3 #7: a single
    per-action output table in the spec, enforced by one emit helper.

12. **`usage_*` snippets are static per file, `*_usage` are not declared
    anywhere**
    `model_usage`, `ctx_usage`, `db_usage`, `model_folder_usage`, … are
    non-static ("the dispatch layer can call this") but no header declares
    them, so the dispatch layer *cannot* call them (implicit declaration is
    an error in C99+). Either declare them in `commands.h` (and then
    `acta <entity> help`/`--help` can be wired centrally) or make them static.
    Note the `usage_create` name is reused (statically) across 8 files — fine,
    but a `usage_<entity>_<action>` naming convention would let the
    declarations coexist.

13. **Structural duplication is the dominant cost in this family**
    Each of the 8 files re-implements: the
    usage text + per-action snippets, the id-parse block (×1–5), the
    JSON-vs-flags input split, the `goto cleanup` free list, the
    to-json/table/empty-`[]` emit triplet, and the unknown-action suggest
    block. That's why this family is ~5.4k lines for what is mostly CRUD.
    A small per-entity descriptor (field table: name, JSON key, required,
    type, flag name) + shared `create/get/list/count/delete/restore/move`
    drivers would collapse most of it *and* make the model/skill divergence
    (#1, #8) impossible to repeat.

### Positives

- `model.c` is the best file in the CLI: robust id parsing, fetch-and-merge
  update (the correct pattern), required-field validation on create, and
  the `json_owned` cleanup discipline.
- The family's uniformity (same section banners, same action order, same
  cleanup pattern) is a real virtue — it's what makes the review above
  transferable file-to-file.
- `skill create`'s cleanup list is complete (all 7 malloc-able fields) —
  the standard the other cleanups should meet.
- Read-only revision entities are appropriately minimal (get / get-latest /
  list / count, no mutations).

---

## Part 5: execution, log, Makefile, tests

### Bugs (worth fixing)

1. **`exec create` and `log create` leak `created_at` on the JSON path**
   `json_parse_execution` / `json_parse_execution_log` both malloc
   `created_at` from the blob, but `cleanup_exec_create` frees only
   `prompt` + `status`, and `execution_log`'s cleanup frees
   `level/event/message/metadata` — `created_at` is never freed. Same leak
   family as Parts 2/4 (context, model_folder); the complete free list in
   `skill create` is again the reference.

2. **`exec create` advertises `--status`, which is silently ignored** (`execution.c`)
   The help lists `--status <str>  pending | running | …` for create, the CLI
   validates the enum, and then `acta_db_execution_create` does the right
   thing: *"e->status is deliberately ignored: a new execution is always
   created 'pending'"* (good — this resolves the forgery concern raised in
   Part 2 #9). But the CLI still accepts and validates the flag, so
   `exec create … --status completed` looks like it worked and didn't.
   Either reject `--status` on create with "status is managed by the
   lifecycle actions" or mark it "(ignored)" in the help.

### Design / consistency

3. **`exec` positional ids are now strict** (`parse_positive_id` in
   `get`/`start`/`cancel`/`complete`, and `execution_log.c:535,649` for
   `--execution_id` filters), but not-found `get` still → silent
   `EXIT_OK` (`execution.c:663–664`), so Part 4 #5 still applies here.
   The `exec create` flag ids and the `exec list`/`count` query filters
   (`--context_id` etc.) are now strict (`parse_id_flag`); only the
   not-found issue above remains in this file.

4. **Test architecture: in-process handler calls, global parse layer
   untested** (tests/ overall)
   The suites build `cmd_args_t`/`global_opts_t` via `targs_*` helpers and
   call `cmd_*` directly — fast and well-structured, but the *entire* class
   of bugs found in Parts 1–4 (`--json` blob ignored + stdin hang, `--stdin`
   eaten by `parse_globals`, bare `--json` not parseable, dead local
   `--count`) lives in the argv layer the
   tests skip. A suite that feeds raw `argv` through `parse_globals` +
   `commands_dispatch` (or spawns the built binary) would have caught all of
   them. This is the highest-value coverage gap in the project.

5. **Test scaffolding leftovers** (`tests/skill/skill_test_helpers.h`)
   - Comment: `ADAPT: the 4 functions below must construct … Replace the
     body with your actual API.` — scaffold instruction left in the header.
   - Fallback defines contradict the real headers: `#define EXIT_INVALID 2`
     (real value in `cli.h` is 4; 2 is `EXIT_SQL`). Dead today (the
     `#ifndef` is shadowed by the included `cli.h`) but it encodes a wrong
     mental model; delete it.


### Makefile

6. **Every new test suite needs ~7 manual edits**
   New-suite checklist today: `SRCS/OBJS/TARGET` vars, `ALL_TEST_TARGETS`,
   the `test` recipe line, the `clean` list, a `-Itests/<suite>` flag, the
   header comment, (and the suite's own `*_test_main.c`). Generate it:
   ```make
   TEST_DIRS := $(wildcard tests/*)
   TEST_BINS := $(foreach d,$(TEST_DIRS),$(notdir $(d))_test)
   ```
   (or keep explicit targets but derive them) — one `tests/foo/` directory
   should "just work".

7. **`libacta_db.a` is consumed with no dependency on it**
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

8. **POSIX-only assumptions, Windows artifacts in the tree**
   Targets have no `.exe`, `test` runs `./$(TEST_TARGET)`, `clean` uses
   `rm -f` — yet the tree contains `actagamma_db.exe`, `test_*.exe` and
   `.o` files, i.e. it *was* built under Windows somehow (patched makefile or
   separate toolchain). Either document "POSIX only" or add `$(EXEEXT)` and
   a `del`-equivalent path. Related hygiene: the root `.gitignore` covers
   `*.db` but not `*.o`, `*.exe`, `*.a` — the tree currently carries
   `hello.c` (stray file), `acta.db`, dozens of `.o`, and 9 `.exe` binaries.
   All untracked, so not a commit bug, but one `make clean` + a few
   `.gitignore` lines away from a clean tree.

9. **Shared test framework lives inside one suite**
    `HELPERS_OBJ := tests/skill/skill_test_helpers.o` is linked into every
    other suite, and every suite's `-I` path includes `tests/skill` — the
    "shared" framework is owned by the skill suite's directory. Move to
    `tests/helpers/` (or `tests/common/`) so the ownership is visible.
    (Minor inconsistency: the skill target gets the helper via its own
    wildcard instead of `HELPERS_OBJ`.)

10. **`test` stops at the first failing suite**
    Each recipe line is one shell; make aborts on first non-zero. Fine for
    CI red-early, but for local dev a `for t in …; do ./$t || fail=1; done`
    (or `make -k`) reports the full damage in one run.

### Positives

- **The lib defends the state machine** (`acta_db_execution_create` forces
  `status='pending'`, transitions only via start/complete/fail/cancel) —
  the one server-authoritative-field risk from Part 2 turned out to be
  handled correctly; the CLI just shouldn't advertise the dead flag.
- Test volume is proportionate: ~10.8k lines of tests vs ~9.2k of app.
- Per-suite binaries isolate failures and keep link units small;
  `APP_OBJS_NO_MAIN` (link app minus `main.o` into tests) is a clean way to
  test handlers in-process without an HTTP/shell layer.
- Reference-DB pattern (`acta_test_ref.db` + `.sql` committed, copied per
  run) gives suites hermetic, reproducible state.
- Non-fatal assert counters + per-suite `main` aggregation give good failure
  summaries already.

---

# Summary (top 9 by severity)

1. **`skill update` data loss** — partial struct to a full-replace API (wipes fields, moves to root). *(P4 #1)*
2. **`--json <blob>` ignored, reads stdin, hangs interactively** — 9 call sites; `--stdin`/`--from_file` dead flags. *(P2 #1–2)*
3. **Silent not-found: `get` → exit 0, empty stdout** — all 8 entities. *(P4 #5)*
4. **`db exec` positional form unimplemented, `--stdin` dead.** *(P3 #1–2)*
5. **DB failures exit non-zero with no stderr output** — JSON error contract only implemented for input validation. *(P4 #6)*
6. **`cli_error` emits unescaped JSON** — paths with `"`/`\` break the error contract. *(P1 #2)*
7. **`model move` without `--folder_id` → silent move to root** (help says required). *(P4 #2)*
8. **JSON-path `created_at` leaks** in context / model_folder / exec / log creates. *(P2/P4/P5)*
9. **Global parse layer untested** — the layer that owns bugs #2–3 and all the flag-shadowing issues; orphaned `tests_parse_globals.c` is the seed for that suite. *(P5 #4)*

**Structural recommendation:** the copy-paste family (P4 #13) is where most
bugs live. A per-entity *field descriptor* (name, JSON key, flag, type,
required, root-vs-null semantics) plus shared `create/get/list/count/
update/delete/restore/move` drivers would fix the divergences structurally
(atoi vs strtol, leak lists, silent not-found, dead `--count`, success
shapes) rather than file-by-file.

