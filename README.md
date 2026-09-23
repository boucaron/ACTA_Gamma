# ACTA Gamma

![Logo](assets/logo.jpg)

**LLMs as actions, not agents.**

A small, stateless runner for versioned skills and reproducible LLM actions.

This is not an agent framework. It is a runner that makes one LLM call and records the result.

> **You (or your program) decide what happens. The runner makes one LLM call and records the outcome. The LLM only does the work it's asked to do.**

The C components build with plain `make` on Windows (MinGW/MSYS2), Linux (gcc/clang), and macOS (Xcode clang); the Qt 6 GUI additionally needs `qmake6` on any of those platforms.

## Current status

Early prototype / POC. The core pipeline is complete: entity model and persistence, versioned skills/models, execution lifecycle with `execution_log`, the standalone runner, the GUI, soft-delete/restore, `sweep`, and the `db backup --to` atomic snapshot. Retry is manual by design (a failed execution is reset explicitly with `exec reset` or the GUI Retry button), and streaming responses are out of scope by design — an execution is a single, non-interactive call. These are product decisions, not missing features. Full detail in [`docs/status.md`](docs/status.md); the issue tracker is [`docs/known_issues.md`](docs/known_issues.md).

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

Observation — the LLM's output for this execution (its response, recorded verbatim as the execution's `raw_response`).

The workflow is: register a model, create a skill (a versioned prompt), create a context (the input), create an execution binding them, run it, read the result — the commands in the [Minimal end-to-end example](#minimal-end-to-end-example).

In agent frameworks, versioning, immutability, and audit trails are typically a separate layer added on top. Here they are not a "second need" — they are part of the *same* operation. A result is only as good as your ability to prove what produced it: if a result is wrong, you want to show *exactly* which prompt revision, context, and model were sent. A replay six months later is the same operation, and comparing outputs across prompt revisions and models falls out of the same records.

The runner drives the execution. The LLM does not orchestrate itself, maintain state, delegate work, or decide what happens next. ACTA Gamma is deliberately not an agent framework — at its core it is a runner over an OpenAI-compatible endpoint plus a SQLite audit log — the full point of view is in [`docs/PointOfView.md`](docs/PointOfView.md).

Terms used throughout this README: a **skill** is a versioned prompt template with an optional output schema — it is not a tool, function, or agent capability; a **context** is a named, immutable snapshot of input data (a document, a code file, a log excerpt) — it is not the model's prompt window.

## Core ideas

**The workflow** — six setup steps: register a model, create a skill, create a context, create an execution, run, read the result. The execution itself is a single LLM call: no multi-turn conversation, no follow-up steps, no conversational state.

**Why those records exist** — so the result is trustworthy (you can prove exactly what was sent), reproducible (the same operation, six months later), and comparable (across prompt revisions and models):

* **Stateless** — every execution is independent and one-shot.
* **Versioned skills** — prompts and output schemas are revisioned.
* **Immutable contexts** — the exact input can be retained for replay.
* **Multi-model** — the backend serves several models; any model served by the llama.cpp router can be registered as a model record.
* **Auditable** — executions retain raw responses, results, errors, and execution events (the resolved prompt is recorded in the execution log).
* **Replayable** — a replay reproduces the request inputs exactly when it reuses the same context, skill revision, and model revision. That is **input determinism, not output determinism** — resending the same inputs does not guarantee the same output (see [`PointOfView.md`](docs/PointOfView.md), "Replay caveat").
* **Generic** — suitable for review, analysis, classification, extraction, auditing, and similar tasks.

## What it does — and deliberately does not

| Does | Deliberately does **not** (on purpose) |
|---|---|
| Runs **one** versioned, replayable, auditable LLM action against an immutable context | Not an agent: no self-orchestration, no conversational state, no delegation, no workflow composition between skills (a higher-level program chains the executions — [`PointOfView.md`](docs/PointOfView.md)) |
| Versioned skills & models; immutable revision snapshots; immutable contexts | No automatic retries (owner decision — retry is manual: `exec reset` / GUI Retry) and no streaming responses (owner decision — [`status.md`](docs/status.md)) |
| Standalone runner + GUI (Run/Cancel), soft-delete lifecycle, stale-execution `sweep` | No users, permissions, organizations, queues, vector DBs, datasets — no server process: one private local SQLite file ([`DBDesign.md`](docs/DBDesign.md)); no hard delete / purge either; no prompt-injection defense — the context reaches the model verbatim as the user message, so sanitizing untrusted content is the operator's job |
| Multi-model via a llama.cpp `llama-server` router — the only supported backend (see [Implementation](#implementation)) | No server manager mode — the backend is user-launched and user-managed ([`runner_contract.md`](docs/runner_contract.md)) |

## Architecture

```text
┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│    Context   │ │  Skill @ n   │ │   Model @ n  │
└──────┬───────┘ └──────┬───────┘ └──────┬───────┘
       │                │                │
       └────────────────┼────────────────┘
                        ▼
                 ┌──────────────┐
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
- **Replay caveat — input determinism, not output determinism.** A replay resends exactly the same request inputs, but does not guarantee identical outputs, because the model weights, the server instance configuration, and LLM sampling are not pinned. Full treatment in [`PointOfView.md`](docs/PointOfView.md), "Replay caveat".
- **No promote / deprecate.** There is deliberately no `active` or `current` flag: the "current" revision is simply the latest one, and choosing what to run is done by pointing the execution at the revision id you want.
- **Editing creates, not modifies.** Updating a skill or model parent inserts a new immutable revision; it does not modify the existing one, and existing executions keep pointing at the revision they were bound to. `skill update` takes a **partial** payload (at least one field):

  ```sh
  acta_cli skill update 1 --json '{"prompt_template":"Updated prompt…"}'   # inserts revision 2
  ```

  (same for `model update`).

**Execution states** — the full lifecycle in one picture:

```text
   pending ──▶ running ──▶ completed
      ▲  │          │
      │  │          ├──▶ failed ──(exec reset)──▶ pending
      │  │          │
      └──┴──────────┴──▶ cancelled   (cancel is allowed from pending or running)
```

## Soft-delete (trash) lifecycle

Rows are never hard-deleted: `delete` sets a `deleted_at` timestamp and `restore` clears it (there is no purge — a new DB file is the clean-state path). `model`, `model_folder`, `skill`, `skill_folder`, `context`, and `exec` all have `delete` / `restore` actions; `list` / `count` default to live-only rows everywhere, with `--include_deleted` / `--deleted` to opt back in on the entity, revision, and context/exec listers (the folder listers are live-only with no opt-in flag). Deleted rows are skipped by `get` / `get-latest` (context, model, skill, exec, model_revision — skill_revision has no deleted filter), `context create` refuses deleted contexts, `exec reset` refuses deleted executions, and the runner's claim step ignores deleted executions. The GUI surfaces the same lifecycle as trash views in the context and execution panels with per-row Delete/Restore (Retry disabled for deleted executions).

(Durability: the DB file is the data — take a backup with `acta_cli db backup --to <target>` before destructive operations; WAL lifecycle, `PRAGMA integrity_check` / `VACUUM` procedure, the backup frequency guidance, and the manual reference are in [`DBDesign.md`](docs/DBDesign.md), "Data durability and maintenance".)

## How a run is assembled

The runner assembles the chat call from the bound revisions:

- `system` = `skill.prompt_template` (from the bound skill revision)
- `user`   = `context.content` (from the bound context)

There is no per-execution prompt field: the skill's prompt template is the only instruction source, and the context is the user message content. An empty context content fails the execution. The `executions` table has no prompt column.

**Prompt size limit.** The execution's preflight checks `strlen(prompt_template) + strlen(context.content)` against the `max_chars` limit (the per-machine config file's `"max_chars"` key, defaulting to the built-in 100,000 chars — a conservative cross-model floor; raise it in the config file if your model's context window is larger and you send long contexts) and, if the total exceeds it, fails the execution with `EXIT_INVALID` ("prompt too large: N chars total (context X + skill prompt Y) exceeds max_chars Z") **before any backend call**.

- It is a deterministic char-count guard, not a token or context-window calculation — the assembled prompt is exactly these two strings, so a char count is a sufficient, model-agnostic check.
- For prompts *under* the limit, the backend's served `max_context` remains the final arbiter (recorded in the `preflight_passed` log row, but not used by the check itself).

**No instruction/data boundary.** The context is passed verbatim as the user message; ACTA does not delimit, sanitize, or reject context content — the operator is responsible for input sanitization, and prompt-injection defenses are out of scope. Details in [`PointOfView.md`](docs/PointOfView.md), "Context is data, not memory".

## Implementation

The implementation is C/C++ on top of SQLite:

| Component | Language | Description |
|---|---|---|
| `acta_db/` | C11 | SQLite persistence library (`libacta_db`) — skills, skill folders, skill revisions, models, model folders, model revisions, contexts, executions, execution logs ([schema design: `docs/DBDesign.md`](docs/DBDesign.md)) |
| `acta_cli/` | C11 | Command-line client (`acta_cli`) over `acta_db` (uses cJSON for output) |
| `acta_runner/` | C11 | Standalone LLM execution runner (`acta_runner`) — drives pending executions against the model's OpenAI-compatible backend: claim → resolve → preflight → chat call → record → complete/fail, with `execution_log` phase rows (uses curl + cJSON) |
| `acta_gui/` | C++ / Qt 6 (Core, Widgets) | Desktop GUI: manage skills, models, contexts, review executions, and run them (the in-app **Run** button runs the runner's pipeline in-process — single pipeline codebase, not a second copy; the source coupling is a hard build constraint, see [`docs/building.md`](docs/building.md), "GUI–runner source coupling"). While a run is in flight the button toggles into **Cancel**, which cooperatively cancels the in-flight pipeline and transitions the row to `cancelled` |

The backend is a llama.cpp `llama-server` running in **router mode** (launched without a model, e.g. with `--models-dir` pointing at local GGUF files): an OpenAI-compatible endpoint that serves several models and routes each request to the matching model instance. A model record stores `backend`, `base_url`, `model_identifier`, and a JSON configuration blob. The runner reads the keys `temperature`, `max_tokens`, `top_k`, and `supports_response_format`, with exactly these types:

| Key | Type |
|---|---|
| `temperature` | number |
| `max_tokens` | positive number |
| `top_k` | positive number |
| `supports_response_format` | boolean (default `true`) |

Any deviation from that contract — an unknown key (typo), a wrong value type, or a malformed blob — fails the execution with `EXIT_INVALID` instead of silently falling back to backend defaults.

**Which backends are supported.** The backend is the llama.cpp `llama-server` in router mode, tested on llama.cpp 0.4.x — it is the **only** supported backend: nothing else is built, tested, documented, or guaranteed (compatibility with llama.cpp versions later than 0.4.x is not claimed). The pipeline talks to it through the OpenAI-compatible HTTP surface that llama.cpp exposes — `GET /health`, `GET /v1/models`, `POST /v1/chat/completions` — but that surface is the interface, not a portability promise; ACTA Gamma is not a generic OpenAI client. Two pipeline steps are defensive:

* the **catalog fetch** (`GET /`, models.json) is best-effort audit data only — if the catalog is missing or unparseable, `"catalog":null` is recorded and the execution is unaffected;
* **server-side schema application** (`response_format: json_schema`) — if a model record sets `supports_response_format: false`, the runner validates the raw response itself after the call.

**Preflight** (the `preflight` step of the pipeline) verifies the backend before the chat call: `GET /health` must return 200 (503 means the model is still loading → execution `failed`), `GET /v1/models` must list the model record's `model_identifier` (if not, the execution fails with the ids the server actually serves), and the matched entry's `max_context` is read. As a best-effort audit step, the router's model catalog (`GET /`, models.json format) records the matched model's launch args and meta (`n_ctx`, `n_params`, `size`, `ftype`, …) into the execution timeline, so the server-instance configuration is part of the audit trail — the same model id can be served under different server flags. Success is logged as `preflight_passed`.

**Output-schema validation** is post-hoc: it runs only when the backend did not apply the schema itself, i.e. when `supports_response_format` is false (the schema is then checked against the raw response after the call). If the response does not match the skill's `output_schema`, the execution **fails** with a `validation_failed` log row (`EXIT_INVALID`) — there is no "complete with a flag" mode. The validator is a hand-rolled subset check, not full JSON Schema: it validates `type`, `required`, `properties`, and `items` recursively (depth-capped), and ignores `pattern`, `enum`, length constraints, `format`, and `oneOf`/`anyOf` — an `output_schema` that relies on any of those constrains nothing. Full contract: [`docs/runner_contract.md`](docs/runner_contract.md).

Concurrency: the SQLite connection uses WAL journal mode, and the runner's claim step is an optimistic `UPDATE … WHERE status = 'pending'` (checked for affected rows), so two runner processes cannot claim the same execution. Sequential use is the normal pattern. Running two `acta_runner` processes simultaneously is safe for claiming (the atomic claim prevents double-claim), but the per-execution processing order across two processes is not guaranteed — if you need ordering, run them sequentially.

### Building

See [`docs/building.md`](docs/building.md) for dependencies, platform-specific setup (MinGW/MSYS2, Linux, macOS), the per-component `make` steps, and the top-level wrapper (`make all`, `make test`).

## Quick start

Three steps before the example below:

1. **Install dependencies** — SQLite, curl, cJSON (plus Qt 6 for the GUI): one command block per platform in [`docs/building.md`](docs/building.md).
2. **Build** — from the repo root: `make all` (or per-component `make`; `make test` runs all C test suites).
3. **Start the backend** — a llama.cpp `llama-server` in **router mode**: launched **without** `-m`, every GGUF in `--models-dir` becomes a served model and each request is routed to the matching one. Single-model mode (`llama-server -m model.gguf`) loads exactly one model at start and is not what ACTA expects — but one model is fine: point `--models-dir` at a folder containing that one GGUF. E.g. `llama-server --models-dir models -c 2048` on `127.0.0.1:8080` (canonical startup: [`docs/llamacpp_server_contract.md`](docs/llamacpp_server_contract.md) §1).

## Environment variables and the per-machine config file

* **`OPENAI_API_KEY`** — the API key for the backend's HTTP calls, used identically by `acta_runner` and `acta_gui`. Resolution: `$OPENAI_API_KEY` (if set — even to the empty string) → the config file's `"api_key"` key. Both sources are deliberate, not redundant: the env var is the primary channel for scripted/programmatic use; the file key is a per-machine fallback for GUI and no-shell setups (the "env var only" alternative was considered and rejected — see [`docs/runner_contract.md`](docs/runner_contract.md), decision 4). There is no CLI flag, and the key is never stored in the database (a model `configuration` blob carrying an `api_key` key is rejected as an unknown key, `EXIT_INVALID`). If the key is present in neither source, the run does not start; an empty key is a warning and sends no `Authorization` header — acceptable only for a keyless localhost server.
* **`ACTA_DB`** — database file path used by `acta_cli` and `acta_runner` when `--db` is not given. Resolution order: `--db` → `$ACTA_DB` → the config file's `"db"` → the **same** app-data file as the GUI (`%APPDATA%\ACTA_Gamma\acta.db` on Windows, `~/.local/share/ACTA_Gamma/acta.db` on Linux, or `$XDG_DATA_HOME/ACTA_Gamma/acta.db`) → `./acta.db` as a last-resort fallback. A readable-but-malformed config file is a fail-closed hard error, never a silent retarget. The GUI does **not** read `--db` or `$ACTA_DB` — it uses the same default file, and its *Choose database file* dialog covers non-default setups (see [Your first session in the GUI](#your-first-session-in-the-gui)).
* **`ACTA_Gamma.conf`** — the per-machine config file: a flat JSON object with **at most** four keys, in the same app-data directory as the default DB file (renamed from the old space-bearing `ACTA Gamma` directory and `ACTA Gamma.conf` file — move or recreate any existing files):

  | Key | Type | Meaning |
  |---|---|---|
  | `"api_key"` | string | key fallback for `$OPENAI_API_KEY` (above) |
  | `"db"` | string | database path step (above) |
  | `"max_chars"` | positive integer | max total prompt chars (`skill.prompt_template` + `context.content`); default 100,000 |
  | `"timeout"` | positive integer, seconds | default per-call HTTP timeout; default 300 s (the `--timeout` flag still wins per run) |

  All three binaries read it through the same helper (`acta_conf` in `acta_db`). Fail-closed, like the model `configuration` blob: not-an-object, unknown key, wrong type, or malformed JSON → hard error; the file must be `0600` (on Windows the mode check degrades to a warning). Full contract: [`docs/plans/acta-config-file.md`](docs/plans/acta-config-file.md).

## Minimal end-to-end example

Against the running `llama-server` router from the quick start:

```sh
# All create commands take the full payload as --json '{...}'.
# For large inputs, `context create` accepts `--content_file <path>` instead of inline content.

# 1. Register the model
# ("backend" is a protocol family, not a framework name: "openai" =
#  OpenAI-compatible HTTP, served here by the llama-server router)
acta_cli model create --json '{"name":"llama-local","backend":"openai","base_url":"http://127.0.0.1:8080","model_identifier":"qwen3-8b"}'

# 2. Create a versioned skill (prompt template + optional output schema)
acta_cli skill create --json '{"name":"sentiment","prompt_template":"Classify the sentiment of the input. Reply with JSON: {\"label\": \"positive\"|\"negative\", \"confidence\": number}"}'

# 3. Create an immutable context (the input snapshot)
# ("type" is a free-form label with no fixed domain — convention: "text";
#  "hash" is optional here: when omitted it is derived as SHA-256 of the content)
acta_cli context create --json '{"type":"text","content":"The build system shipped on time and the release went smoothly."}'

# 4. Create an execution binding context + skill revision + model revision
# (each entity above got id 1 — first rows in a fresh database —
# so every "1" below is the corresponding row id)
acta_cli exec create --json '{"context_id":1,"skill_revision_id":1,"model_revision_id":1}'

# 5. Provide the API key: set the environment variable, or add an
# "api_key" key to the config file (ACTA_Gamma.conf in the same
# app-data directory as the default DB file). The environment variable
# wins when set (even to the empty string, which is sufficient for a
# keyless localhost server — no Authorization header will be sent); with
# the variable unset, the file key is the fallback.
export OPENAI_API_KEY=

# 6. Run it (hard per-call HTTP timeout: --timeout, default 300 s)
acta_runner run 1

# 7. Inspect the result and the audit trail
acta_cli exec get 1
acta_cli log list 1
```

What the output looks like (abbreviated — real timestamps, and a full `raw_response`, in practice):

```sh
$ acta_cli exec get 1
{"id":1,"context_id":1,"skill_revision_id":1,"model_revision_id":1,
 "status":"completed","error":null,
 "raw_response":"{\"label\": \"positive\", \"confidence\": 0.9}",
 "result":"{\"label\": \"positive\", \"confidence\": 0.9}",
 "created_at":"2026-07-10T09:30:01","started_at":"2026-07-10T09:30:02",
 "completed_at":"2026-07-10T09:30:03",
 "parent_execution_id":null,"deleted_at":null}

$ acta_cli log list 1
[
 {"id":1,"execution_id":1,"level":"info","event":"execution_started","created_at":"…"},
 {"id":2,"execution_id":1,"level":"info","event":"context_loaded","created_at":"…"},
 {"id":3,"execution_id":1,"level":"info","event":"prompt_resolved","created_at":"…"},
 {"id":4,"execution_id":1,"level":"info","event":"preflight_passed","created_at":"…"},
 {"id":5,"execution_id":1,"level":"info","event":"llm_request","created_at":"…"},
 {"id":6,"execution_id":1,"level":"info","event":"llm_response","created_at":"…"},
 {"id":7,"execution_id":1,"level":"info","event":"execution_completed","created_at":"…"}
]
```

`raw_response` is the model's text verbatim; `result` is the recorded, schema-checked output; the log is the phase timeline — one row per event, with `prompt_resolved` carrying the exact prompt that was sent.

Stale-run cleanup: if a runner process dies mid-flight, `acta_runner sweep --stale-seconds N` fails executions left in `running` whose newest activity (latest `execution_log` row, or `started_at`) is older than `N` seconds (`--stale-seconds` is required, positive integer). A failed execution is retried manually with `acta_cli exec reset <id>` (`failed → pending`) or the GUI Retry button.

## Your first session in the GUI

The GUI is a convenience layer for humans: paste inputs, watch a run, read results without terminal JSON. It targets engineers and analysts who operate ACTA Gamma interactively — programs and scripted use should use the CLI (`acta_cli` + `acta_runner`), which is the complete surface; every GUI operation has a CLI equivalent.

Prefer not to use the command line? Once the backend is running (see Quick start), launch `acta_gui` (built with `make gui` — the GUI is optional and not part of `make all`) — on first start it creates the `acta.db` database file for you (schema applied automatically; no setup step). The default location is the platform app-data directory (`%APPDATA%\ACTA_Gamma\acta.db` on Windows, `~/.local/share/ACTA_Gamma/acta.db` on Linux) — not `./acta.db` next to the binary — and `acta_cli` / `acta_runner` resolve to the **same** file out of the box (`--db` → `$ACTA_DB` → the config file's `"db"` → that app-data file; `./acta.db` only as a last resort, with a hint naming such a legacy file when the default DB is missing). The GUI does not read `--db` or `$ACTA_DB`; its *Choose database file* dialog (the choice is remembered in QSettings and reused on next start) remains for non-default setups, e.g. a legacy `./acta.db`. Then:

1. **Model** — Models panel → *New…* → give it a name, the backend (`openai`), the router's address, and the model id (the GGUF file's name in your `--models-dir` folder).
2. **Skill** — Skills panel → *New…* → a name and the prompt template — the instruction describing the action.
3. **Context** — Contexts panel → *New…* → a type, and paste the content (a document, a code file, a log…).
4. **Execution** — Executions panel → *New…* → pick the context, the skill and the model, then press **Run**.
5. **Watch it** — the row moves `pending → running → completed` (or `failed`). While it runs, **Run** becomes **Cancel**. **Log** shows the phase timeline (including the resolved prompt), **Details** shows the raw response and the result; **Retry** re-runs a failed execution.

## CLI ergonomics

For the high-volume payload data (context `content`; execution `raw_response` / `result` / `error`) the CLI has dedicated flags: light-projection listers with `--full`, file in/out (`--out`, `--raw_out`, `--content_file`, `--raw_file`, `--result_file`), NDJSON `--stream`, global output shaping (`--fields`, `--no_nulls`, `--table`, `--count`, `--id_only`, `--pretty`), `--db` (default `$ACTA_DB`, else the app-data file shared with the GUI), and the machine-readable `--tools` JSON schema (currently version 4). `db exec` is the developer-facing static-SQL escape hatch: it executes a single mutating statement (DDL / migrations) whose SQL must be a developer-written literal, never composed from runtime input; a `SELECT` is rejected **before the DB is touched** by a first-statement keyword check (exit 4) — a first-keyword blocklist, not a full parse. The full wire format, per-action flag tables, and error contracts are in [`docs/cli_spec.md`](docs/cli_spec.md).

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
  used by `acta_cli/src/json.c` for the CLI's JSON input/output layer,
  by `acta_db/src/conf.c` for the per-machine config-file parser, and
  by `acta_runner/src/run.c` for request/response handling; the GUI
  compiles `run.c` and reuses it.
- **libcurl** ([curl](https://curl.se/), MIT-style "curl" license with an
  explicit patent grant) — used only by `acta_runner/src/backend.c`, the
  minimal wrapper around the curl easy interface for the preflight and chat
  HTTP calls; the GUI compiles `backend.c` and reuses it.
