# acta_cli contract — per-action stdout table (T1 / source for `--tools` §11)

This table is the single source of truth for the per-action stdout schema.
It is the data from which the machine-readable `--tools` JSON (T3, spec §11)
will be generated — do not hand-write `--tools` against this table.

Wire-format decisions settled here (T1):

- **Root folder is `null` in every JSON emit** (`model_to_json`,
  `skill_to_json`, `emit_ok_folder`, `model_folder move`). `0` survives
  only in `--table` (plain-text) output and in flag values
  (`--folder_id 0` / `--parent_id 0` mean "root").
- **Lifecycle transitions emit a success line** instead of silence:
  `{"id":N,"status":"<new-status>"}` via `emit_ok_transition`;
  `set-raw` does not change status and emits `{"id":N}`.
- **Restore emits `{"id":N,"restored":true}`** everywhere, via the shared
  `emit_ok_restored` atom (model, skill, model_folder, skill_folder,
  context, exec).
- **Folder moves use the `parent_id` key** on the wire
  (`emit_ok_parent`), `null` for root; entity moves use `folder_id`
  (`emit_ok_folder`), `null` for root.
- **Delete emits `{"deleted":true}`** everywhere, via `emit_deleted`
  (model, skill, model_folder, skill_folder, context, exec).

## Common shapes

| Shape | Meaning |
|-------|---------|
| `{"id":N}` | create / update success (bare `N` with `--id_only`) |
| `{"id":N,"folder_id":null\|M}` | `model move` / `skill move` success |
| `{"id":N,"parent_id":null\|M}` | `model_folder move` / `skill_folder move` success |
| `{"id":N,"restored":true}` | restore success |
| `{"deleted":true}` | `model_folder delete` / `skill_folder delete` success |
| `{"id":N,"status":"<s>"}` | exec transition success (`s` ∈ running, cancelled, completed, failed); `set-raw` echoes the unchanged current status, which can also be `pending` |
| `{"status":"ok"}` | `db exec` success |
| `{"version":"<ver>"}` | `db version` success |
| `[ {…}, … ]` / `[]` | list success (empty list → `[]`) |
| bare integer | `--count` on list, and `count` actions |

Output modifiers (all entities): `--id_only` (bare `N` where noted),
`--table` (columnar / plain instead of JSON), `--fields <csv>`,
`--no_nulls`, and on every `list` action `--stream` (P5). `get`/
`get-latest` return the full entity JSON object (subject to those
modifiers). `--id_only` is a single-row modifier for `create` /
`get`-style actions only; every `list` action rejects it with exit 4
(it used to be silently ignored, printing full JSON rows).

`db exec` is mutating-only: a statement whose first keyword is
`SELECT` (after leading whitespace, stray `;`, and `--` / `/* */`
comments) is rejected with exit 4 before execution, so the "no
SELECT" help claim is enforced, not just documented.

Stream output (P5): `--stream` turns any `list` action's array output
into NDJSON — one JSON object per line, no array wrapper, empty result
emits nothing. The lister pages internally (chunks of at most
`ACTA_DB_MAX_PAGE` rows) until the filter is exhausted, so bulk export
needs no manual `--offset` loop; a user `--offset` is still honoured as
the starting point and `--limit` caps the total emitted. `--fields` /
`--no_nulls` apply per row. `--stream` is mutually exclusive with
`--count`, `--table` and `--id_only` (conflict → exit 4).
`exec list --stream` keeps the light/full projection choice (`--full`).

Output destinations (P3): `--out <path>` writes the entire stdout
payload to `<path>` instead of stdout (errors/warnings stay on
stderr; the flag is ignored with `--version` / `--help` / `--tools`).
`--raw_out <field>` prints one field's raw (unescaped) value with no
JSON wrapper — `context get` / `exec get` only; it takes precedence
over `--id_only` / `--table` / `--fields`, null values produce no
output, and an unknown field is a CLI usage error (exit 10).

Input from file (P4): `--content_file <path>` (`context create`) reads
the payload as raw file content, no JSON escaping; `--result_file
<path>` (`exec complete`) and `--raw_file <path>` (`exec set-raw`) are
the same for their respective fields. Each is mutually exclusive with
the corresponding inline flag (`--content` / `--result` / `--raw`) —
combining them is an invalid-argument error (exit 4) — and an
unreadable file is likewise an error (exit 4). Unlike `--from_file` /
`--json`, the file content is **not** a JSON body: it is the field
value itself, so arbitrarily large payloads work without escaping.

Unknown JSON keys: the JSON sources (`--json` / `--stdin` / `--from_file`)
are parsed against exactly the documented per-action input keys;
any top-level key outside that set — including a misspelling with the
wrong case — is rejected with exit 4 (`invalid JSON body`), not
silently ignored.

Schema flags: `--tools` emits the machine-readable JSON schema (T3,
`src/tools.c`); `--tools --compact` emits a plain-text one-line-per-command
rendering (~7 KB) of the same static table — positionals, flags (with `*`
= required), JSON keys, input mode, aliases — intended for LLM/agent
in-context use. The full JSON output stays the source of truth; compact
is derived from the same `tool_table`, so it cannot drift from it.

`success` is structured (schema version 2, P1): a JSON object
`{"kind": "json" | "json_object" | "json_array" | "bare_int" | "plain_text"`
(`,"keys": [ … ]` when `kind` is `"json"` — the exact wire keys from the
per-action table above; `,"note": "…"` optional, carrying the
`--table` / `--count` variants). The Common-shapes table remains the
wire-format source of truth; `--tools` renders it structurally, not as a
prose string.

Exit codes: `0` ok, `1` not found, `2` SQL error, `3` OOM, `4` invalid
argument / missing flag / missing required field / duplicate / FK
violation / invalid DB file, `10` CLI usage error (unknown entity,
unknown action, unknown option, bad `--verbose`, too few positionals,
missing flag value, unexpected positional — an extra positional left
unconsumed by a successful action, e.g. `context list help`, is
rejected with `unexpected argument: '<tok>'`), `11` DB open failed. Errors: single JSON line on
stderr `{"error":"ACTA_DB_ERR_*"|"ACTA_CLI_ERR","code":<n>,"message":"..."}`
where the exit code is authoritative and `code` = −exit (the T2
invariant, exit code canonical with `code` = −exit everywhere);
the `error` name keeps per-cause granularity — e.g.
`ACTA_DB_ERR_DUPLICATE` is `code:-4` with exit `4`, and a DB-open failure
is `code:-11` with exit `11`. The `message` carries a human-readable
detail: on a DB failure it is `<op> failed: <detail>` (e.g. `execution
create failed: FOREIGN KEY constraint failed: executions.context_id`),
where `<detail>` is the last error recorded by `acta_db`, falling back
to the connection's `sqlite3_errmsg` (`acta_db_errmsg`) — it degenerates
to `(no detail)` only when no error message was recorded at all.

## db

| Command | Positionals | Flags | Input | stdout on success |
|---------|-------------|-------|-------|-------------------|
| `db exec` | `sql` (or one of the flags) | `--sql`, `--file` (≤64 KiB), `--sql_stdin` (≤64 KiB) — mutually exclusive, first wins | positional / flag, never JSON | `{"status":"ok"}` (`--table` → `ok`) |
| `db version` | — | `--table` | — | `{"version":"<ver>"}` (`--table` → `SQLite <ver>`) |

## context

| Command | Positionals | Flags | Input | stdout on success |
|---------|-------------|-------|-------|-------------------|
| `context create` | — | `--type*`, `--content*` / `--content_file*` (exactly one), `--hash`, `--metadata` | flags or JSON `{type*, content*, hash, metadata}` (`--content_file` is flag-only) | `{"id":N}` |
| `context get <id>` | `id` | `--include_deleted` / `--deleted` | — | context JSON object |
| `context delete <id>` | `id` | — | — | `{"deleted":true}` |
| `context restore <id>` | `id` | — | — | `{"id":N,"restored":true}` |
| `context list` | — | `--type`, `--hash`, `--offset`, `--limit`, `--include_deleted` / `--deleted`, `--full`, `--count`, `--table`, `--stream`, `--fields`, `--no_nulls` | — | `[ … ]` / `[]`; `--count` → bare int; `--stream` → NDJSON |
| `context count` | — | `--type`, `--hash`, `--include_deleted` / `--deleted` | — | bare int |

> **`context create` — `hash` default:** when `--hash` (or the JSON key `hash`) is omitted, the hash is derived as the **SHA-256 of `content`, lowercase hex** — the same rule the GUI applies (`QCryptographicHash::toHex` in `contextDialog.cpp`). An explicitly supplied hash is stored as-is. Wire key is `hash` (not `content_hash`).

> **`context list` — light by default:** without `--full`, the lister uses the light projection (`acta_db_context_query_light` / `_with_deleted_light`): the `content` blob column is not fetched and `content` is `null` in every row. `--full` switches to the full lister and returns `content`. `context get` always returns the full row.

## model

| Command | Positionals | Flags | Input | stdout on success |
|---------|-------------|-------|-------|-------------------|
| `model create` | — | `--name*`, `--backend*`, `--model_identifier*`, `--folder_id`, `--description`, `--base_url`, `--configuration` | flags or JSON (same keys) | `{"id":N}` |
| `model get <id>` | `id` | `--include_deleted` / `--deleted` | — | model JSON object |
| `model update <id>` | `id` | `--name`, `--folder_id`, `--description`, `--backend`, `--base_url`, `--model_identifier`, `--configuration` (≥ 1 required) | — | `{"id":N}` |
| `model delete <id>` | `id` | — | — | `{"deleted":true}` |
| `model restore <id>` | `id` | — | — | `{"id":N,"restored":true}` |
| `model move <id>` | `id` | `--folder_id*` (0 = root) | — | `{"id":N,"folder_id":null\|M}` |
| `model list` | — | `--folder_id`, `--offset`, `--limit`, `--include_deleted` / `--deleted`, `--count`, `--table`, `--stream`, `--fields`, `--no_nulls` | — | `[ … ]` / `[]`; `--count` → bare int; `--stream` → NDJSON |
| `model count` | — | `--folder_id`, `--include_deleted` / `--deleted` | — | bare int |

## model_folder

| Command | Positionals | Flags | Input | stdout on success |
|---------|-------------|-------|-------|-------------------|
| `model_folder create` | — | `--name*`, `--parent_id` (0/omitted = root) | flags or JSON `{name*, parent_id}` | `{"id":N}` |
| `model_folder get <id>` | `id` | — | — | folder JSON object |
| `model_folder list` | — | `--parent_id`, `--offset`, `--limit`, `--count`, `--table`, `--stream`, `--fields`, `--no_nulls` | — | `[ … ]` / `[]`; `--count` → bare int; `--stream` → NDJSON |
| `model_folder count` | — | `--parent_id` | — | bare int |
| `model_folder rename <id>` | `id` | `--name*` | — | `{"id":N}` |
| `model_folder delete <id>` | `id` | — | — | `{"deleted":true}` |
| `model_folder restore <id>` | `id` | — | — | `{"id":N,"restored":true}` |
| `model_folder move <id>` | `id` | `--parent_id*` (0 = root) | — | `{"id":N,"parent_id":null\|M}` |

## model_revision

| Command | Positionals | Flags | Input | stdout on success |
|---------|-------------|-------|-------|-------------------|
| `model_revision get <id>` | `id` | — | — | revision JSON object |
| `model_revision get-latest <model_id>` | `model_id` | — | — | revision JSON object |
| `model_revision list <model_id>` | `model_id` | `--offset`, `--limit`, `--count`, `--table`, `--stream`, `--fields`, `--no_nulls` | — | `[ … ]` / `[]`; `--count` → bare int; `--stream` → NDJSON |
| `model_revision count <model_id>` | `model_id` | — | — | bare int |

## skill

| Command | Positionals | Flags | Input | stdout on success |
|---------|-------------|-------|-------|-------------------|
| `skill create` | — | `--name*`, `--prompt_template*`, `--folder_id`, `--description`, `--output_schema` | flags or JSON (same keys) | `{"id":N}` |
| `skill get <id>` | `id` | `--include_deleted` / `--deleted` | — | skill JSON object |
| `skill update <id>` | `id` | `--name`, `--folder_id`, `--description`, `--prompt_template`, `--output_schema` (≥ 1 required) | flags or JSON `{name, prompt_template, folder_id, description, output_schema}` (≥ 1) | `{"id":N}` |
| `skill delete <id>` | `id` | — | — | `{"deleted":true}` |
| `skill restore <id>` | `id` | — | — | `{"id":N,"restored":true}` |
| `skill move <id>` | `id` | `--folder_id*` (0 = root) | — | `{"id":N,"folder_id":null\|M}` |
| `skill list` | — | `--all` (all folders), `--folder_id`, `--offset`, `--limit`, `--include_deleted` / `--deleted`, `--count`, `--table`, `--stream`, `--fields`, `--no_nulls` | — | `[ … ]` / `[]`; `--count` → bare int; `--stream` → NDJSON |
| `skill count` | — | `--all` (all folders), `--folder_id`, `--include_deleted` / `--deleted` | — | bare int |

## skill_folder

| Command | Positionals | Flags | Input | stdout on success |
|---------|-------------|-------|-------|-------------------|
| `skill_folder create` | — | `--name*`, `--parent_id` (0/omitted = root) | flags or JSON `{name*, parent_id}` | `{"id":N}` |
| `skill_folder get <id>` | `id` | — | — | folder JSON object |
| `skill_folder list` | `parent_id` (optional; `all` = all folders) | `--offset`, `--limit`, `--count`, `--table`, `--stream`, `--fields`, `--no_nulls` | positional / flag | `[ … ]` / `[]`; `--count` → bare int; `--stream` → NDJSON |
| `skill_folder count` | `parent_id` (optional; `all` = all folders) | — | positional | bare int |
| `skill_folder rename <id>` | `id` | `--name*` | — | `{"id":N}` |
| `skill_folder delete <id>` | `id` | — | — | `{"deleted":true}` |
| `skill_folder restore <id>` | `id` | — | — | `{"id":N,"restored":true}` |
| `skill_folder move <id>` | `id` | `--parent_id` (0/omitted = root) | — | `{"id":N,"parent_id":null\|M}` |

## skill_revision

| Command | Positionals | Flags | Input | stdout on success |
|---------|-------------|-------|-------|-------------------|
| `skill_revision get <id>` | `id` | — | — | revision JSON object |
| `skill_revision get-latest <skill_id>` | `skill_id` | — | — | revision JSON object |
| `skill_revision list <skill_id>` | `skill_id` | `--offset`, `--limit`, `--count`, `--table`, `--fields`, `--no_nulls` | — | `[ … ]` / `[]`; `--count` → bare int |
| `skill_revision count <skill_id>` | `skill_id` | — | — | bare int |

## exec

| Command | Positionals | Flags | Input | stdout on success |
|---------|-------------|-------|-------|-------------------|
| `exec create` | — | `--context_id*`, `--skill_revision_id*`, `--model_revision_id*`, `--prompt`, `--parent_execution_id` | flags or JSON (same keys) | `{"id":N}` (row always created `pending`) |
| `exec get <id>` | `id` | `--include_deleted` / `--deleted` | — | execution JSON object |
| `exec delete <id>` | `id` | — | — | `{"deleted":true}` |
| `exec restore <id>` | `id` | — | — | `{"id":N,"restored":true}` |
| `exec start <id>` | `id` | — | — | `{"id":N,"status":"running"}` |
| `exec cancel <id>` | `id` | — | — | `{"id":N,"status":"cancelled"}` |
| `exec complete <id>` | `id` | `--result` / `--result_file` (mutually exclusive) | — | `{"id":N,"status":"completed"}` |
| `exec fail <id>` | `id` | `--error` | — | `{"id":N,"status":"failed"}` |
| `exec reset <id>` | `id` | — | — | `{"id":N,"status":"pending"}` |
| `exec set-raw <id>` | `id` | `--raw*` / `--raw_file*` (exactly one) | — | `{"id":N,"status":"<current status, unchanged>"}` |
| `exec list` | — | `--status`, `--context_id`, `--skill_revision_id`, `--model_revision_id`, `--parent_execution_id`, `--include_deleted` / `--deleted`, `--full`, `--offset`, `--limit`, `--count`, `--table`, `--stream`, `--fields`, `--no_nulls` | — | `[ … ]` / `[]`; `--count` → bare int; `--stream` → NDJSON |
| `exec count` | — | same filters as `exec list` (minus `--count`/`--table`/`--fields`/`--no_nulls`) | — | bare int |

Notes on `exec`:

- **Replay** — there is no dedicated replay action. Replay an execution by
  creating a new one with the same inputs — `exec create` with the same
  `context_id` / `skill_revision_id` / `model_revision_id` (and `--prompt`
  if the original had one) — plus `--parent_execution_id <id>` to link the
  new row to the one being replayed; the new row is created `pending` and
  runs normally.
- **Reset** — `exec reset <id>` performs the `failed → pending` reset
  (`acta_db_execution_reset`): it clears `error`, `raw_response`,
  `started_at` and `completed_at` (the execution_log audit trail of the
  previous attempt is preserved) and makes the execution re-runnable. The
  same transition is exposed by the GUI Retry button.
- **Soft delete** — `exec delete <id>` flags a live row (refused from
  `running` and on already-deleted rows — exit 4 / exit 1); `exec
  restore <id>` unflags it with the status untouched. `exec get` /
  `exec list` / `exec count` are live-only by default; `--include_deleted`
  (`--deleted` alias) includes soft-deleted rows. `exec create
  --context_id <deleted>` fails with the standard not-found path.
- **Light by default** — without `--full`, `exec list` uses the light
  projection (`acta_db_execution_query_light`): `prompt`, `raw_response`,
  `result` and `error` are not fetched and are `null` in every row.
  `--full` switches to the full lister and returns them. `exec get`
  always returns the full row.

## log (execution_log)

| Command | Positionals | Flags | Input | stdout on success |
|---------|-------------|-------|-------|-------------------|
| `log create` | — | `--execution_id*`, `--level*` (debug\|info\|warn\|error), `--event*`, `--message`, `--metadata` | flags or JSON (same keys) | `{"id":N}` |
| `log get <id>` | `id` | — | — | log JSON object |
| `log list <execution_id>` | `execution_id` | `--level`, `--offset`, `--limit`, `--count`, `--table`, `--stream`, `--fields`, `--no_nulls` | — | `[ … ]` / `[]`; `--count` → bare int; `--stream` → NDJSON |
| `log count <execution_id>` | `execution_id` | `--level` | — | bare int |

Every entity also has a bare-word `help` action (usage text, no JSON
contract). `help` takes an optional action positional (`X help
<action>` → that action's section only; unknown action → exit 10), and
`--help` is scoped by what follows it: `X --help` → full entity help,
`X <action> --help` → single-action section, bare `--help` → global
usage (unknown entity/action → exit 10). See `docs/cli_help_plan.md`
(P0).
