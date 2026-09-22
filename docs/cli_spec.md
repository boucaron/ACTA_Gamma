# acta_cli contract — per-action stdout table (source for `--tools`)

This table is the single source of truth for the per-action stdout schema.
It is the data from which the machine-readable `--tools` JSON
(`src/tools.c`) is generated — do not hand-write `--tools` against this table.

Wire-format decisions settled here:

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
| `{"target":"<path>","bytes":<N>,"quick_check":"ok"}` | `db backup` success |
| `[ {…}, … ]` / `[]` | list success (empty list → `[]`) |
| bare integer | `--count` on list, and `count` actions |

DB file: the global `--db <path>` selects the database; when omitted, resolution is `--db` → `$ACTA_DB` → the `"db"` key of the per-machine config file `ACTA Gamma.conf` (same app-data directory as the default DB file) → the platform app-data location — the same file the GUI uses: `%APPDATA%\ACTA Gamma\acta.db` (Windows), `~/.local/share/ACTA Gamma/acta.db` (Linux; `$XDG_DATA_HOME/ACTA Gamma/acta.db` if set) → `./acta.db` only as a last resort when the platform base directory is unresolvable. The config file is consulted even when `$ACTA_DB` is set: a readable-but-malformed file (or, on POSIX, a file whose mode gives read access to group or other — the file must be `0600`) is a fail-closed hard error (`ACTA_CLI_ERR`, code −10, exit 10), even though a *valid* file's `db` value loses to `--db` / `$ACTA_DB`, so a stray file can never silently retarget a run that named its DB. The GUI (`MainWindow::defaultDbPath`) applies the same file step between its remembered dialog choice and the platform default, and exits on a malformed file. This paragraph is the single source of truth for the default: `acta_dbpath.c` (CLI/runner), `MainWindow::defaultDbPath` (GUI), and `acta_conf_default_path()` (the file's location) must stay in lockstep with it (contract: `docs/plans/acta-config-file.md`).

Output modifiers (all entities): `--id_only` (bare `N` where noted),
`--table` (columnar / plain instead of JSON), `--fields <csv>`,
`--no_nulls`, and on every `list` action `--stream`. `get`/
`get-latest` return the full entity JSON object (subject to those
modifiers). `--id_only` is a single-row modifier for `create` /
`get`-style actions only; every `list` action rejects it with exit 4
(it used to be silently ignored, printing full JSON rows).

`db exec` is mutating-only: a statement whose first keyword is
`SELECT` (after leading whitespace, stray `;`, and `--` / `/* */`
comments) is rejected with exit 4 before execution, so the "no
SELECT" help claim is enforced, not just documented.

`db backup` makes an atomic, consistent snapshot of the open database
into `--to <target>` via the SQLite backup C API (`sqlite3_backup_*` —
the target is passed to the C API, never interpolated into SQL text).
It has the same consistency guarantee as `VACUUM INTO` but runs
against the open connection: no need to close the GUI / CLI / runner
first, and the WAL state is folded into the snapshot. The target is
validated strictly — non-empty, no quote / semicolon / backslash
characters, not equal to the DB path itself, a path the process can
create, and not already existing (no silent overwrite) — and any
validation failure is a CLI usage error (exit 10). After the copy the
backup is reopened on its own connection and `PRAGMA quick_check` is
run; a backup that does not check is reported as failure and nothing is
left behind. Snapshot failure is the standard error JSON with a
non-zero rc.

Stream output: `--stream` turns any `list` action's array output
into NDJSON — one JSON object per line, no array wrapper, empty result
emits nothing. The lister pages internally (chunks of at most
`ACTA_DB_MAX_PAGE` rows) until the filter is exhausted, so bulk export
needs no manual `--offset` loop; a user `--offset` is still honoured as
the starting point and `--limit` caps the total emitted. `--fields` /
`--no_nulls` apply per row. `--stream` is mutually exclusive with
`--count`, `--table` and `--id_only` (conflict → exit 4).
`exec list --stream` keeps the light/full projection choice (`--full`).

Output destinations: `--out <path>` writes the entire stdout
payload to `<path>` instead of stdout (errors/warnings stay on
stderr; the flag is ignored with `--version` / `--help` / `--tools`).
`--raw_out <field>` prints one field's raw (unescaped) value with no
JSON wrapper — `context get` / `exec get` only; it takes precedence
over `--id_only` / `--table` / `--fields`, null values produce no
output, and an unknown field is a CLI usage error (exit 10).

Input from file: `--content_file <path>` (`context create`) reads
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

Schema flags: `--tools` emits the machine-readable JSON schema
(`src/tools.c`); `--tools --compact` emits a plain-text one-line-per-command
rendering (~7 KB) of the same static table — positionals, flags (with `*`
= required), JSON keys, input mode, aliases — intended for LLM/agent
in-context use. The full JSON output stays the source of truth; compact
is derived from the same `tool_table`, so it cannot drift from it.

`success` is structured (the schema `version` field is 4): a JSON object
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
missing flag value, unexpected positional — a positional beyond the
action's declared count (e.g. `context list help`) is rejected before
the action runs, with no stdout payload, as `unexpected argument:
'<tok>'`), `11` DB open failed. Errors: single JSON line on
stderr `{"error":"ACTA_DB_ERR_*"|"ACTA_CLI_ERR","code":<n>,"message":"..."}`
where the exit code is authoritative and `code` = −exit (the exit code
is canonical, `code` = −exit everywhere);
the `error` name keeps per-cause granularity — e.g.
`ACTA_DB_ERR_DUPLICATE` is `code:-4` with exit `4`, and a DB-open failure
is `code:-11` with exit `11`. The `message` carries a human-readable
detail: on a DB failure it is `<op> failed: <detail>` (e.g. `execution
create failed: FOREIGN KEY constraint failed: executions.context_id`),
where `<detail>` is the last error recorded by `acta_db`, falling back
to the connection's `sqlite3_errmsg` (`acta_db_errmsg`) — it degenerates
to `(no detail)` only when no error message was recorded at all.

Refusal reasons: decisions made in C code without
any failing SQL statement — illegal exec state transitions, `delete`
from `running`, `reset` / `restore` of the wrong row class, delete /
restore of a missing or already-deleted row, the folder-move cycle
guard, and the folder-delete guards (live sub-folders, live assigned
models) — each records its own detail in `last_error` via
`db_set_error`, so they surface as `<op> failed: <reason>` (e.g.
`execution start failed: execution 3 is 'completed'; start requires
status 'pending'`, `model_folder move failed: cannot move model_folder
6 into its own subtree: parent 8 is a descendant of 6`) instead of
`(no detail)`. The exit-code split for refusals: a refused *operation
on an existing row* (bad state, cycle, guard) is
`ACTA_DB_ERR_INVALID` → exit 4; a refusal because the *row is missing
or already deleted* is `ACTA_DB_ERR_NOT_FOUND` → exit 1.

`restore` is row-class-strict per entity: `context`, `exec` and
`skill_folder` restore refuse a live (non-deleted) row with exit 1
(`context restore failed: context 1 is not deleted (nothing to
restore)`, `execution restore failed: execution 1 is not deleted
(nothing to restore)`, `skill_folder restore failed: skill_folder 1
does not exist`), while `model`, `skill` and `model_folder` restore
treat an already-live row as a no-op success (exit 0, same
`{"id":N,"restored":true}` line). A missing row is always exit 1
(`<row> does not exist`).

Versioning side effect (model / skill): every mutation of a tracked
field automatically snapshots a new immutable revision row via the DB
triggers in `acta_gui/db/schema.sql` — so `model update` / `skill
update` (and `model move` / `skill move`, `folder_id` being a tracked
field) append a `model_revision` / `skill_revision` row with
`revision = max(revision) + 1` whenever a tracked value actually
changes; `create` snapshots the initial revision and `delete` a final
`deleted_at`-carrying one, and a no-op update (no tracked field
changed) snapshots nothing. Revision rows are read via the
`model_revision` / `skill_revision` actions (design: `docs/DBDesign.md`).

## db

| Command | Positionals | Flags | Input | stdout on success |
|---------|-------------|-------|-------|-------------------|
| `db exec` | `sql` (or one of the flags) | `--sql`, `--file` (≤64 KiB), `--sql_stdin` (≤64 KiB) — mutually exclusive, first wins | positional / flag, never JSON | `{"status":"ok"}` (`--table` → `ok`) |
| `db backup` | — | `--to <target>*` (must not exist), `--table` | — | `{"target":"<path>","bytes":<N>,"quick_check":"ok"}` (`--table` → `<target> <N> bytes`) |
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
| `model_revision list <model_id>` | `model_id` | `--offset`, `--limit`, `--include_deleted` / `--deleted`, `--count`, `--table`, `--stream`, `--fields`, `--no_nulls` | — | `[ … ]` / `[]`; `--count` → bare int; `--stream` → NDJSON |
| `model_revision count <model_id>` | `model_id` | `--include_deleted` / `--deleted` | — | bare int |

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

> **`skill list` / `skill count` — `--all`:** the default scope is
> already all folders, so `--all` alone is a no-op (identical rows /
> count); it is kept as the explicit form and wins over `--folder_id`
> when both are given.

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
| `exec create` | — | `--context_id*`, `--skill_revision_id*`, `--model_revision_id*`, `--parent_execution_id` | flags or JSON (same keys) | `{"id":N}` (row always created `pending`) |
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

- **No `prompt` input or output** — `exec create` takes no `--prompt`
  flag and no `prompt` JSON key: a JSON body carrying `prompt` (any case)
  is rejected as an unknown key, exit 4 (`invalid JSON body`). The
  `executions` table has no prompt column, so `exec get` / `exec list`
  never emit a `prompt` key, `--fields prompt` selects nothing and
  `--raw_out prompt` is an unknown-field error. The user message is
  always `context.content` (see `docs/runner_contract.md`).
- **State machine** — the lifecycle transitions and their allowed source
  states; every other transition is refused:

  | Action | Allowed from | Result status |
  |---|---|---|
  | `start` | `pending` | `running` |
  | `cancel` | `pending`, `running` | `cancelled` |
  | `complete` | `running` | `completed` |
  | `fail` | `running` | `failed` |
  | `reset` | `failed` | `pending` (clears `error`, `raw_response`, `started_at`, `completed_at`) |
  | `set-raw` | any | unchanged (echoes the current status, which can be `pending`) |
  | `delete` | any **except** `running` and already-deleted | soft-deleted (refused from `running` → exit 4; already deleted → exit 1) |
  | `restore` | deleted rows only | status untouched (live or missing row → exit 1) |

  Consequences for callers: `complete` / `fail` are only reachable
  *through* `running` (start first, from `pending`); `cancelled` is a
  dead end (no restart, no complete); `reset` only un-sticks `failed`.
- **Replay** — there is no dedicated replay action. Replay an execution by
  creating a new one with the same inputs — `exec create` with the same
  `context_id` / `skill_revision_id` / `model_revision_id` — plus
  `--parent_execution_id <id>` to link the
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
  projection (`acta_db_execution_query_light`): `raw_response`,
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
usage (unknown entity/action → exit 10).
