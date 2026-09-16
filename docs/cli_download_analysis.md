# acta_cli — content / execution "download" analysis

Exploration-only review of how the payload data (`context.content` and the
execution blobs `prompt` / `raw_response` / `result` / `error`) can be
retrieved through `acta_cli`, in the context of the recently added **light
listers** (`acta_db_context_query_light`, `acta_db_execution_query_light`).
Companion to `cli_spec.md` (T1 stdout contract). Findings verified against
the running binary; fixes are proposed, only P1 applied (source only, not
compiled, not committed).

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
   `exec complete --result` with large payloads.

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
- **P2 — Warn on the silent-null trap (not applied).** In the list paths
  of `cmd_context` / `cmd_execution`: if `--fields` names a light-omitted
  field (`content` / `prompt, raw_response, result, error`) and `--full`
  is absent, print a one-line stderr warning (exit code unchanged).
  Warning-only, not auto-upgrade — auto-upgrading would change the cost
  semantics the light projection was designed around.
- **P3 — Download ergonomics (not applied).** Global `--out <path>`
  writing the exact stdout payload to a file, and a raw single-field
  mode (e.g. `context get 5 --raw_out content > out.txt`) printing the
  value unescaped — a true "download" without `jq`.
- **P4 — Large-payload input (not applied).** `--content_file <path>`
  for `context create` (and symmetrically `--raw_file` for
  `exec set-raw`, `--result_file` for `exec complete`) to bypass the
  flag / 64 KiB limits for big blobs.
- **P5 — Bulk export (optional).** A `list --stream` NDJSON mode or an
  `export` action to avoid the `--offset` loop; only worth it if bulk
  export becomes a real workflow.
