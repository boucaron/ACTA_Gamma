# acta_cli — content / execution "download" analysis

Exploration-only review of how the payload data (`context.content` and the
execution blobs `prompt` / `raw_response` / `result` / `error`) can be
retrieved through `acta_cli`, in the context of the recently added **light
listers** (`acta_db_context_query_light`, `acta_db_execution_query_light`).
Companion to `cli_spec.md` (T1 stdout contract). Findings verified against
the running binary; P1–P4 applied in source.

## How you can download the data today

**Context content**

- `context get <id>` — always the **full** row (includes `content`).
  Single-item download: `context get 5 --fields content` (JSON-escaped).
- `context list --full [--type T] [--hash H] --offset N --limit M` — full
  pages including the `content` blob. Page ceiling `ACTA_DB_MAX_PAGE`
  (10 000); bulk download = loop over `--offset`.
- Without `--full`, the lister uses the light projection: `content` is
  `null` in every row.

**Execution payload**

- `exec get <id>` — full row: `prompt`, `raw_response`, `result`, `error`.
- `exec list --full [--status S] [--context_id N] ...` — full pages with
  those 4 blob fields; light by default otherwise.
- `log list <execution_id>` — phase-log messages (small, no light
  projection).

So the capability exists; the light helpers introduced a schema/detection
gap.

## Gaps found (verified)

1. **`--full` missing from the machine-readable schema (`tools.c`).**
   `f_ctx_list` and `f_exec_list` list 9/12 flags but neither included
   `--full`. Verified: `--tools --compact` printed
   `context.list flags:type,hash,offset,limit,include_deleted,count,table,
   fields,no_nulls` — no `full`. Since `--tools` / `--tools --compact` are
   the interface meant for LLM agents in-context, an agent following the
   schema has no way to learn that `--full` fetches the content from a
   list. (The `--deleted` alias for `--include_deleted` is also absent
   from the schema — minor.)
2. **Silent-null trap.** `context list --fields content` (no `--full`) →
   `[{"content":null},{"content":null}]`, exit 0; `exec list
   --fields raw_response,result` → all null, exit 0. No warning, no error
   — the exact failure mode an agent hits if it follows gap 1.
3. **No raw/file output mode.** Everything is JSON on stdout; no
   `--out <path>` / `--save`, and no unescaped single-field output
   (requires piping through `jq -r`). `--table` truncates columns
   (`tcol`), so it cannot be used for downloads.
4. **Input-side asymmetry.** `context create` accepts content only via
   flag / JSON stdin / `--from_file` (64 KiB cap on `--file` /
   `--sql_stdin`); there is no `--content_file <path>` to ingest a large
   file directly. Same story for `exec set-raw --raw` /
   `exec complete --result` with large payloads (resolved by P4).

## Proposals (priority order)

- **P1 — Fix the schema (APPLIED, source only).**
  - `tools.c`: added `{ "full", 0, 0 }` to `f_ctx_list` and
    `f_exec_list`; new `suc_ctx_list` / `suc_exec_list` success objects
    whose `note` documents the light default and the `--full` escape
    hatch (`context.list`: "light projection by default: 'content' is
    null in every row; --full fetches the content blob; --count -> bare
    int"; `exec.list` analog naming `prompt`, `raw_response`, `result`,
    `error`); `context.list` / `exec.list` entries now reference them
    (flag counts 9→10, 12→13). `full` is already in
    `entity_flag_specs` (argparse.c), so the T3 "flags ⊆
    entity_flag_specs" invariant holds.
  - `tests/tools/tools_test_main.c`: per-entry assertion that
    `context.list` and `exec.list` advertise `--full` in their flags and
    carry a `success.note` mentioning it.
  - Not compiled, not committed; `cli_spec.md` already documents
    `--full`, so no spec change needed. **Pending: rebuild + run
    `test_tools`.**
- **P2 — Warn on the silent-null trap (APPLIED, source only).** In the
  list paths of `cmd_context` / `cmd_execution`: if `--fields` names a
  light-omitted field (`content` / `prompt, raw_response, result,
  error`) and `--full` is absent, a one-line stderr warning is printed
  (exit code unchanged). Warning-only, not auto-upgrade — auto-upgrading
  would change the cost semantics the light projection was designed
  around. **Pending: rebuild + a run of
  `context list --fields content` / `exec list --fields raw_response`
  to observe the warning.**
- **P3 — Download ergonomics (APPLIED, source only).**
  - `--out <path>` (global): the entire stdout payload is written to
    `<path>` instead of stdout. Implemented in `main.c` by redirecting
    fd 1 around `commands_dispatch` (`dup`/`dup2`), so every entity's
    emit path is covered without touching the handlers; stderr
    (errors, warnings, VLOG) is untouched. Ignored with
    `--version` / `--help` / `--tools`. Unopenable path → exit-10
    `ACTA_CLI_ERR`.
  - `--raw_out <field>` (global, `context get` / `exec get` only):
    prints one field's raw (unescaped) value, no JSON wrapper; takes
    precedence over `--id_only` / `--table` / `--fields`; null values
    produce no output; unknown field → exit-10 `ACTA_CLI_ERR` listing
    the supported fields. Context fields: `id, type, content,
    content_hash, metadata, created_at, deleted_at`; exec fields:
    `id, context_id, skill_revision_id, model_revision_id,
    parent_execution_id, prompt, raw_response, result, status, error,
    created_at, started_at, completed_at, deleted_at`. Any other
    entity/action using `--raw_out` → exit-10 (checked in `main.c` before
    dispatch).
  - Registered in `parse_globals` (pass 1, value-taking),
    `global_opts_t`, `help_print`, per-action help (`context get`,
    `exec get`), and the `--tools` schema (`global_flags`: 15 → 17,
    compact header line). `tools_test` expected count updated 15 → 17.
  - `cli_spec.md` extended with an "Output destinations (P3)"
    paragraph. **Pending: rebuild + manual runs (see test list in
    commit conversation).**
- **P4 — Large-payload input (APPLIED, source only).**
  - `--content_file <path>` (`context create`): the payload is the raw
    file content — not a JSON body — so no JSON escaping and no flag /
    64 KiB limit; `--type` is still supplied as a flag.
  - `--raw_file <path>` (`exec set-raw`) and `--result_file <path>`
    (`exec complete`): same semantics for the raw response / result
    field.
  - Each flag is mutually exclusive with its inline counterpart
    (`--content` / `--raw` / `--result`; `--content_file` is also
    exclusive with `--json` / `--stdin` / `--from_file`): combining them
    → exit-4 `ACTA_DB_ERR_INVALID`; unreadable file → exit-4.
    `set-raw` still requires one of `--raw` / `--raw_file`.
  - Implemented on top of the existing `read_file_all` helper
    (`include/commands.h`); registered in `entity_flag_specs`
    (argparse.c), the `--tools` schema (`f_ctx_create` 4 → 5 flags,
    `f_exec_complete` 1 → 2, `f_raw` 1 → 2), and the per-action help
    (`context create`, `exec complete`, `exec set-raw`).
  - `cli_spec.md` extended with an "Input from file (P4)" paragraph
    and the three updated flag cells. **Pending: rebuild + manual runs
    (create a large blob file, `context create --content_file`, `exec
    set-raw --raw_file`, `exec complete --result_file`, plus the
    conflict cases → exit 4).
- **P5 — Bulk export (optional).** A `list --stream` NDJSON mode or an
  `export` action to avoid the `--offset` loop; only worth it if bulk
  export becomes a real workflow.
