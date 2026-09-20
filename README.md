# ACTA Gamma

![Logo](assets/logo.jpg)

**LLMs as actions, not agents.**

A small, stateless LLM execution engine for versioned skills and reproducible analysis.

> **The engine decides what happens. The LLM only does the work it's asked to do.**

The C components build with plain `make` on Windows (MinGW/MSYS2), Linux (gcc/clang), and macOS (Xcode clang); the Qt 6 GUI additionally needs `qmake6` on any of those platforms.

## What is ACTA Gamma?

ACTA Gamma treats an LLM as a single controlled action in a larger deterministic flow.

```text
Context + Skill + Model
              │
              ▼
         One LLM Call
              │
              ▼
         Observation
```

The engine controls the execution. The LLM does not orchestrate itself, maintain state, delegate work, or decide what happens next. ACTA Gamma is deliberately not an agent framework — the full point of view is in [`docs/PointOfView.md`](docs/PointOfView.md).

Terms used throughout this README: a **skill** is a versioned prompt template with an optional output schema — it is not a tool, function, or agent capability; a **context** is a named, immutable snapshot of input data (a document, a code file, a log excerpt) — it is not the model's prompt window.

## Core ideas

* **Stateless** — every execution is independent and one-shot.
* **Versioned skills** — prompts and output schemas are revisioned.
* **Immutable contexts** — the exact input can be retained for replay.
* **Multi-model** — the backend serves several models; any model served by the llama.cpp router can be registered as a model record.
* **Auditable** — executions retain prompts, raw responses, results, errors, and execution events.
* **Replayable** — a replay reproduces the request inputs exactly when it reuses the same context, skill revision, and model revision (output equivalence additionally depends on backend determinism — see [Revisions and lifecycle](#revisions-and-lifecycle)).
* **Generic** — suitable for review, analysis, classification, extraction, auditing, and similar tasks.

## What it does — and deliberately does not

| Does | Deliberately does **not** (on purpose) |
|---|---|
| Runs **one** versioned, replayable, auditable LLM action against an immutable context | Not an agent: no self-orchestration, no conversational state, no delegation, no workflow composition between skills (a higher-level program chains the executions — [`PointOfView.md`](docs/PointOfView.md)) |
| Versioned skills & models; immutable revision snapshots; immutable contexts | No automatic retries (owner decision — retry is manual: `exec reset` / GUI Retry) and no streaming responses (owner decision — [`status.md`](docs/status.md)) |
| Standalone runner + GUI (Run/Cancel), soft-delete lifecycle, stale-execution `sweep` | No users, permissions, organizations, queues, vector DBs, datasets — serverless: one private SQLite file ([`DBDesign.md`](docs/DBDesign.md)); no hard delete / purge either |
| Multi-model via a llama.cpp `llama-server` router; any OpenAI-compatible backend | No server manager mode — the backend is user-launched and user-managed ([`runner_contract.md`](docs/runner_contract.md)) |

## Architecture

```text
                 ┌──────────────┐
                 │    Context   │
                 └──────┬───────┘
                        │
                 ┌──────▼───────┐
                 │  Skill @ n   │
                 └──────┬───────┘
                        │
                 ┌──────▼───────┐
                 │   Model @ n  │
                 └──────┬───────┘
                        │
                 ┌──────▼───────┐
                 │  Execution   │
                 └──────┬───────┘
                        │
             ┌──────────┴──────────┐
             ▼                     ▼
        Observation              Audit
```

## Revisions and lifecycle

The whole lifecycle: create/update the parent, revisions are snapshotted automatically, executions reference revision ids. Concretely:

- **How revisions are created.** A DB trigger inserts a new revision row (per-parent sequence 1, 2, 3, …) every time the parent row is created, updated, or soft-deleted (`acta_cli skill create` / `skill update` / `skill delete`, same for `model`). The soft-delete trigger snapshots a final revision carrying `deleted_at`. There is no separate "snapshot" command.
- **Immutability.** Revision rows cannot be edited or deleted — they can only be read (`skill_revision get` / `get-latest` / `list` / `count`, same for `model_revision`). Contexts are likewise immutable (a trigger rejects updates), which is what makes replay inputs exact.
- **Execution binding.** An execution binds to explicit `skill_revision_id` and `model_revision_id`, and its user message is exactly `context.content`. The request inputs are exactly reproducible with all three inputs: the context, the skill revision, and the model revision.
- **Replay caveat.** Output equivalence additionally depends on backend determinism and the model weights behind the model's `base_url`, which the system does not track.
- **No promote / deprecate.** There is deliberately no `active` or `current` flag: the "current" revision is simply the latest one, and choosing what to run is done by pointing the execution at the revision id you want.
- **Editing creates, not modifies.** Updating a skill or model parent inserts a new immutable revision; it does not modify the existing one, and existing executions keep pointing at the revision they were bound to.

## Soft-delete (trash) lifecycle

Rows are never hard-deleted: `delete` sets a `deleted_at` timestamp and `restore` clears it (there is no purge — a new DB file is the clean-state path). `model`, `model_folder`, `skill`, `skill_folder`, `context`, and `exec` all have `delete` / `restore` actions; `list` / `count` default to live-only rows everywhere, with `--include_deleted` / `--deleted` to opt back in on the entity, revision, and context/exec listers (the folder listers are live-only with no opt-in flag). Deleted rows are skipped by `get` / `get-latest` (context, model, skill, exec, model_revision — skill_revision has no deleted filter), `context create` refuses deleted contexts, `exec reset` refuses deleted executions, and the runner's claim step ignores deleted executions. The GUI surfaces the same lifecycle as trash views in the context and execution panels with per-row Delete/Restore (Retry disabled for deleted executions).

## How a run is assembled

The runner assembles the chat call from the bound revisions:

- `system` = `skill.prompt_template` (from the bound skill revision)
- `user`   = `context.content` (from the bound context)

There is no per-execution prompt field: the skill's prompt template is the only instruction source, and the context is the user message content. An empty context content fails the execution. (The `executions.prompt` column remains in the schema as a legacy, never-written field; rows created before its removal may still carry a value, and `exec get` keeps returning it.)

## Implementation

The implementation is C/C++ on top of SQLite:

| Component | Language | Description |
|---|---|---|
| `acta_db/` | C11 | SQLite persistence library (`libacta_db`) — skills, skill folders, skill revisions, models, model folders, model revisions, contexts, executions, execution logs ([schema design: `docs/DBDesign.md`](docs/DBDesign.md)) |
| `acta_cli/` | C11 | Command-line client (`acta_cli`) over `acta_db` (uses cJSON for output) |
| `acta_runner/` | C11 | Standalone LLM execution runner (`acta_runner`) — drives pending executions against the model's OpenAI-compatible backend: claim → resolve → preflight → chat call → record → complete/fail, with `execution_log` phase rows (uses curl + cJSON) |
| `acta_gui/` | C++ / Qt 6 (Core, Widgets) | Desktop GUI: manage skills, models, contexts, review executions, and run them (the in-app "Run" button runs the runner's pipeline in-process — it directly compiles and reuses the runner's own source files `acta_runner/src/run.c` and `acta_runner/src/backend.c` via its qmake project, no `acta_runner` binary needed; there is a single pipeline codebase, not a second copy of the pipeline logic). While a run is in flight the button toggles into **Cancel**, which cooperatively cancels the in-flight pipeline and transitions the row to `cancelled` |

The backend is a llama.cpp `llama-server` running in **router mode** (launched without a model, e.g. with `--models-dir` pointing at local GGUF files): an OpenAI-compatible endpoint that serves several models and routes each request to the matching model instance. A model record stores `backend`, `base_url`, `model_identifier`, and a JSON configuration blob. The runner reads the keys `api_key`, `temperature`, `max_tokens`, `top_k`, and `supports_response_format`; any deviation from that contract — an unknown key (typo), a wrong value type, or a malformed blob — fails the execution with `EXIT_INVALID` instead of silently falling back to backend defaults.

**Preflight** (the `preflight` step of the pipeline) verifies the backend before the chat call: `GET /health` must return 200 (503 means the model is still loading → execution `failed`), `GET /v1/models` must list the model record's `model_identifier` (if not, the execution fails with the ids the server actually serves), and the matched entry's `max_context` is read. As a best-effort audit step, the router's model catalog (`GET /`, models.json format) records the matched model's launch args and meta (`n_ctx`, `n_params`, `size`, `ftype`, …) into the execution timeline, so the server-instance configuration is part of the audit trail — the same model id can be served under different server flags. Success is logged as `preflight_passed`.

**Output-schema validation** is post-hoc: it runs only when the backend did not apply the schema itself, i.e. when `supports_response_format` is false (the schema is then checked against the raw response after the call). If the response does not match the skill's `output_schema`, the execution **fails** with a `validation_failed` log row (`EXIT_INVALID`) — there is no "complete with a flag" mode. The validator is a hand-rolled subset check, not full JSON Schema.

Concurrency: the SQLite connection uses WAL journal mode, and the runner's claim step is an optimistic `UPDATE … WHERE status = 'pending'` (checked for affected rows), so two runner processes cannot claim the same execution. Sequential use is the normal pattern; parallel runners are safe for claiming (the claim step guarantees two processes cannot grab the same execution), but interleaving two runners over the same batch is not supported, since per-execution ordering is not guaranteed.

### Building

See [`docs/building.md`](docs/building.md) for dependencies, platform-specific setup (MinGW/MSYS2, Linux, macOS), the per-component `make` steps, and the top-level wrapper (`make all`, `make test`).

## Quick start

Three steps before the example below:

1. **Install dependencies** — SQLite, curl, cJSON (plus Qt 6 for the GUI): one command block per platform in [`docs/building.md`](docs/building.md).
2. **Build** — from the repo root: `make all` (or per-component `make`; `make test` runs all C test suites).
3. **Start the backend** — a llama.cpp `llama-server` in router mode, e.g. `llama-server --models-dir models -c 2048` on `127.0.0.1:8080` (canonical startup: [`docs/llamacpp_server_contract.md`](docs/llamacpp_server_contract.md) §1).

## Environment variables

* **`OPENAI_API_KEY`** — default API key for the backend's HTTP calls (preflight and chat). Resolution order:
  * `acta_runner run`: `--api_key` flag → `$OPENAI_API_KEY` → the `api_key` key inside the model's JSON configuration blob
  * `acta_gui` (in-process pipeline; there is no `--api_key` flag): `$OPENAI_API_KEY` → the `api_key` key inside the model's JSON configuration blob
* **`ACTA_DB`** — database file path used by `acta_cli` and `acta_runner` when `--db` is not given (fallback: `./acta.db`). The GUI does **not** read `$ACTA_DB` — its default is the platform app-data directory, and the file can be chosen in its *Choose database file* dialog (see [Your first session in the GUI](#your-first-session-in-the-gui)).

## Minimal end-to-end example

Against the running `llama-server` router from the quick start:

```sh
# 1. Register the model
acta_cli model create --json '{"name":"llama-local","backend":"openai","base_url":"http://127.0.0.1:8080","model_identifier":"qwen3-8b"}'

# 2. Create a versioned skill (prompt template + optional output schema)
acta_cli skill create --json '{"name":"sentiment","prompt_template":"Classify the sentiment of the input. Reply with JSON: {\"label\": \"positive\"|\"negative\", \"confidence\": number}"}'

# 3. Create an immutable context (the input snapshot)
# ("hash" is optional here: when omitted it is derived as SHA-256 of the content)
acta_cli context create --json '{"type":"text","content":"The build system shipped on time and the release went smoothly."}'

# 4. Create an execution binding context + skill revision + model revision
# (each entity above got id 1 — first rows in a fresh database —
# so every "1" below is the corresponding row id)
acta_cli exec create --json '{"context_id":1,"skill_revision_id":1,"model_revision_id":1}'

# 5. Run it (hard per-call HTTP timeout: --timeout, default 300 s)
acta_runner run 1

# 6. Inspect the result and the audit trail
acta_cli exec get 1
acta_cli log list 1
```

Stale-run cleanup: if a runner process dies mid-flight, `acta_runner sweep --stale-seconds N` fails executions left in `running` whose newest activity (latest `execution_log` row, or `started_at`) is older than `N` seconds (`--stale-seconds` is required, positive integer). A failed execution is retried manually with `acta_cli exec reset <id>` (`failed → pending`) or the GUI Retry button.

## Your first session in the GUI

Prefer not to use the command line? Once the backend is running (see Quick start), launch `acta_gui` — on first start it creates the `acta.db` database file for you (schema applied automatically; no setup step). Note the default location is the platform app-data directory (`QStandardPaths::AppDataLocation` — e.g. `%LOCALAPPDATA\boucaron\ACTA Gamma\acta.db` on Windows, `~/.local/share/boucaron/ACTA Gamma/acta.db` on Linux) — not `./acta.db` next to the binary, and the GUI does not read `--db` or `$ACTA_DB`. The CLI resolves its DB as `--db` → `$ACTA_DB` → `./acta.db`. To make both use the same database, either pick the CLI's file in the GUI's *Choose database file* dialog (the choice is remembered in QSettings and reused on next start) or point `--db` / `$ACTA_DB` at the GUI's file. Then:

1. **Model** — Models panel → *New…* → give it a name, the backend (`openai`), the router's address, and the model id (the GGUF file's name in your `--models-dir` folder).
2. **Skill** — Skills panel → *New…* → a name and the prompt template — the instruction describing the action.
3. **Context** — Contexts panel → *New…* → a type, and paste the content (a document, a code file, a log…).
4. **Execution** — Executions panel → *New…* → pick the context, the skill and the model, then press **Run**.
5. **Watch it** — the row moves `pending → running → completed` (or `failed`). While it runs, **Run** becomes **Cancel**. **Log** shows the phase timeline, **Details** shows the prompt sent, the raw response and the result; **Retry** re-runs a failed execution.

## CLI ergonomics

For the high-volume payload data (context `content`; execution `raw_response` / `result` / `error`) the CLI has dedicated flags: light-projection listers with `--full`, file in/out (`--out`, `--raw_out`, `--content_file`, `--raw_file`, `--result_file`), NDJSON `--stream`, global output shaping (`--fields`, `--no_nulls`, `--table`, `--count`, `--id_only`, `--pretty`), `--db` (default `$ACTA_DB`, else `./acta.db`), and the machine-readable `--tools` schema (version 3). `db exec` is the developer-facing static-SQL escape hatch (DDL / migrations — never `SELECT`, never user-composed input). The full wire format, per-action flag tables, and error contracts are in [`docs/cli_spec.md`](docs/cli_spec.md).

## Current status

Early prototype / POC. See [`docs/status.md`](docs/status.md) for what is done and what is not yet implemented, and [`docs/known_issues.md`](docs/known_issues.md) for the issue tracker.

Components and their reference docs:

| Component | Contract / design doc |
|---|---|
| `acta_db/` | [`docs/DBDesign.md`](docs/DBDesign.md) — schema, triggers, state machine, soft-delete rules |
| `acta_cli/` | [`docs/cli_spec.md`](docs/cli_spec.md) — wire format, per-action flags, error contracts |
| `acta_runner/` | [`docs/runner_contract.md`](docs/runner_contract.md) — pipeline; [`docs/llamacpp_server_contract.md`](docs/llamacpp_server_contract.md) — backend contract |
| `acta_gui/` | build in [`docs/building.md`](docs/building.md); design decisions in [`docs/status.md`](docs/status.md) |
| project-wide | [`docs/PointOfView.md`](docs/PointOfView.md) — philosophy and non-goals |

## License

BSD Zero Clause License (BSD-0-Clause).

## Third-party code

The SHA-256 implementation in `acta_cli/src/sha256.c` is taken from
[Brad Conte's crypto-algorithms](https://github.com/B-Con/crypto-algorithms/tree/master)
(`sha256.c`). This code is released into the public domain free of any
restrictions. The author requests acknowledgement if the code is used,
but does not require it. This code is provided free of any liability and
without any quality claims by the author.

Two external libraries are linked at build time (not vendored; point the
Makefile overrides `CJSON_DIR` / `CJSON_LIB` / `CURL_INC` / `CURL_LIB` at a
custom build if needed — see [`docs/building.md`](docs/building.md)):

- **cJSON** ([DaveGamble/cJSON](https://github.com/DaveGamble/cJSON), MIT) —
  used by `acta_cli/src/json.c` for the CLI's JSON input/output layer and
  by `acta_runner/src/run.c` for request/response handling; the GUI compiles
  `run.c` and reuses it.
- **libcurl** ([curl](https://curl.se/), MIT-style "curl" license with an
  explicit patent grant) — used only by `acta_runner/src/backend.c`, the
  minimal wrapper around the curl easy interface for the preflight and chat
  HTTP calls; the GUI compiles `backend.c` and reuses it.
