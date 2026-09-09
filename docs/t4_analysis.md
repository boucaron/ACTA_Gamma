# T4 — Test `--tools`: analysis

Analysis for the `--tools` test item in
[`cli_active_action.md`](cli_active_action.md). T3 is done
(`src/tools.c`, 69 entries, `--pretty` implemented and its leading-comma
JSON bug fixed — see [`t3_analysis.md`](t3_analysis.md) §0), so T4 is
unblocked.

Scope (from the plan): the `--tools` output is **valid JSON** (parsed
with the project's own json layer); it covers **all 10 entities and the
full action set** (count check: 69); each entry's flags/positionals
match what `parse_globals` + dispatch actually accept — ideally by
feeding generated commands through the raw-argv path, which also
exercises the S2 global-parse seam. Added from T3's post-implementation
audit: **both compact and `--pretty` modes** must be validated (pretty
was invalid JSON before the T3 fix).

## 1. Current state (audited)

| Item | State |
|------|-------|
| `tests/tools/` | does not exist — `--tools` output is consumed by no test; the emitted schema is currently unpinned |
| `stest_run_argv` (`tests/helpers/test_helpers`) | raw-argv path: `parse_globals` → `apply_flag_aliases` → `cmd_args_init` → `cmd_args_validate` → handler. Returns `parse_globals`/`cmd_args_validate` rc when the argv is rejected, so **`rc == EXIT_CLI (10)` ⇔ the argv was rejected by the parse layer**; any other rc proves acceptance. Stdin can be fed via `stdin_blob`; stdout is captured |
| `stest_capture_begin/end` | dup2-based stdout capture — the standard way to grab emitter output |
| JSON layer | `json_validate(blob)` (json.h) = the project's own well-formedness check (cJSON underneath); there is **no** serialize API, only parse — fine, T4 parses. The J1 include-cycle (`json.c` → `commands.h`) does not block this test |
| `stest_run_argv` vs `--tools` | it **cannot** run `--tools`: `main.c` early-exits before dispatch on `show_tools`, and the runner has no such branch (it would dispatch with `argc - 2 < 0`). The suite must call `tools_print(stdout, pretty)` directly and capture stdout |
| `targs_*` builders (`test_helpers.c`) | known leaky (in-code comment) — not used; the suite builds raw `argv` arrays directly, which is what `stest_run_argv` expects |
| Makefile | per-directory test targets, wildcard `$(wildcard tests/<dir>/*.c)`; a new `tests/tools/` suite is additive (new section + entry in `ALL_TEST_TARGETS` + `clean` line) |
| ref DB | `stest_init(&ctx, "acta_test_ref.db")` — same convention as every existing suite; the DB is a temp copy, so mutating handlers are safe |

## 2. Decisions

### D1 — Location and shape

One file, `tests/tools/tools_test_main.c`, with the suite function +
`main()` (the `cli_util_test_main.c` convention; other suites split into
several files only because they grew). Makefile: `TOOLS_TEST_SRCS/OBJS/TARGET`
section, `test_tools$(EXEEXT)`, added to `ALL_TEST_TARGETS` and `clean`.

### D2 — How the output is obtained

Call `tools_print(stdout, pretty)` directly under
`stest_capture_begin/end`, **for both modes** (0 = compact, 1 = pretty).
Not via `stest_run_argv` (see §1 — the runner has no `--tools` branch).

### D3 — JSON validity via the project's own layer

`json_validate(blob)` must return 0 for **both** outputs — that is the
project's own json layer. Structural walks use the underlying cJSON API
(the layer `json.c` is built on; no dependency change).

### D4 — Cross-check: consume the parsed output, do not duplicate the table

The test does **not** re-declare the 69-entry data. It parses the
emitted JSON and **generates one command per entry from the entry's own
data**:

- positionals: each *required* positional → dummy value `"1"`;
- flags: each *required* flag → `--<name>` plus value `"x"` when
  `has_value`;
- run through `stest_run_argv` with the entity's `cmd_*` handler;
- assert `rc != EXIT_CLI` — i.e. `parse_globals` +
  `apply_flag_aliases` + `cmd_args_validate` accept it. This pins the
  T3 "every per-action flag is a subset of `entity_flag_specs`"
  invariant for all 69 entries at once, and exercises the S2 seam.

Dummy values may fail inside the handler (e.g. `atoi("x")` → exit 4);
that is fine — a non-10 rc is exactly "argv accepted". `rc` is only
asserted to `EXIT_OK` where success is deterministic: the 10 `help`
actions (usage text) and the input-source smoke runs (D5).

For the 8 `flags|json` entries, a second run uses a `--json` blob built
from the entry's `json_keys.required` keys (all values `"x"`) — also
`rc != EXIT_CLI`.

### D5 — Input-source smoke runs

Two extra runs pin the global input sources end-to-end (S2 territory):
`context create` with `--stdin` (blob fed as `stdin_blob`) and with
`--from_file` (via `stest_write_input`), each asserting `rc == EXIT_OK`.

### D6 — Hardcoded expectations (spec is the source of truth)

Only the small spec-side sets are hardcoded in the test — the action
inventory, not the schema data:

- **69** total entries (59 actions + 10 help);
- per-entity action sets exactly as in
  [`cli_spec.md`](cli_spec.md) (table below);
- exactly the **8** JSON-capable commands carry `json_keys`:
  `context.create`, `model.create`, `model_folder.create`,
  `skill_folder.create`, `skill.create`, `skill.update`, `exec.create`,
  `log.create`;
- global section: 14 `global_flags`, `entity_aliases`
  (`exec → execution`, `log → execution_log`), 3 `input_sources`,
  7 `exit_codes` (`0,1,2,3,4,10,11`), error `stream = stderr` and
  `invariant = "code == -exit"`;
- per-entry invariants: `command == "<entity>.<action>"`; `input` is
  one of `flags | flags|json | positional|flags | positional | none`;
  `input == "none"` ⇔ the 10 help actions + `db.version` (pure-output);
  `positionals`/`flags` are arrays whose elements carry `name`
  (+ `required`/`type` / `has_value`/`required`, booleans).

### D7 — Shape checks

- compact: exactly one trailing newline, no other newline (one line);
- pretty: starts `{\n`, second line indented two spaces.

## 3. Expected per-entity action sets (from `cli_spec.md`)

| Entity | Actions (incl. `help`) | n |
|--------|------------------------|---|
| db | `exec`, `version`, `help` | 3 |
| context | `create`, `get`, `list`, `count`, `help` | 5 |
| model | `create`, `get`, `update`, `delete`, `restore`, `move`, `list`, `count`, `help` | 9 |
| model_folder | `create`, `get`, `list`, `count`, `rename`, `delete`, `restore`, `move`, `help` | 9 |
| model_revision | `get`, `get-latest`, `list`, `count`, `help` | 5 |
| skill | `create`, `get`, `update`, `delete`, `restore`, `move`, `list`, `count`, `help` | 9 |
| skill_folder | `create`, `get`, `list`, `count`, `rename`, `delete`, `restore`, `move`, `help` | 9 |
| skill_revision | `get`, `get-latest`, `list`, `count`, `help` | 5 |
| exec | `create`, `get`, `start`, `cancel`, `complete`, `fail`, `set-raw`, `list`, `count`, `help` | 10 |
| log | `create`, `get`, `list`, `count`, `help` | 5 |
| **Total** | | **69** |

("All expected found" + "total == 69" ⇒ exact match, no extra entries.)

## 4. Risks / watch-outs

- **rc 4 vs 10**: handler-side type conversions (`atoi("x")`) fail with
  exit 4 — that must *not* count as an acceptance failure; only
  `EXIT_CLI` does.
- **M3 quirk**: the generated `skill_folder list` command carries no
  optional positional; the validator-accepts-but-ignored
  `--parent_id` quirk is *not* tested here (S4 scope).
- **M4/M5**: the suite consumes the table, which follows the spec for
  those two cells — it inherits that choice and cannot contradict it;
  fixing M4/M5 later only changes the data, not the test logic.
- **Entity-handler map in the test**: a 10-entry `entity → cmd_*` table
  duplicates `commands.c`'s `entity_table` — dispatch wiring, not
  schema data; acceptable.
- **Memory**: per-entry `argv` flag-name strings are `strdup`'d and
  freed; `cJSON_Delete` on both parsed roots; ref DB temp copy removed
  by `stest_teardown`.

## 5. Implementation

1. `acta_cli/tests/tools/tools_test_main.c` (new) — the suite per
   D1–D7.
2. `acta_cli/Makefile` — `TOOLS_TEST_SRCS/OBJS/TARGET` section +
   `test_tools` rule, added to `ALL_TEST_TARGETS` and `clean`.
3. Build + `make test` (pending — this round is doc + code only).

## 6. Unblocks / next

Closes the T chain (T1→T4). S2 (dedicated global-parse suite) remains,
but this suite's 69× raw-argv cross-check plus the `--stdin`/
`--from_file` smoke runs already exercise most of that seam; J1–J3
unchanged.
