# ACTA Gamma

![Logo](assets/logo.jpg)

**LLMs as actions, not agents.**

A small runner that makes one LLM call and records the result — each execution is stateless and one-shot; the SQLite file is a versioned, auditable store behind it.

This is not an agent framework. It is a runner that makes one LLM call and records the result.

> **You (or your program) decide what happens. The runner makes one LLM call and records the outcome. The LLM only does the work it's asked to do.**

The C components build with plain `make` on Windows (MinGW/MSYS2), Linux (gcc/clang), and macOS (Xcode clang); the Qt 6 GUI additionally needs `qmake6` on any of those platforms.

**You need:** a C compiler (gcc/clang), SQLite, curl, cJSON (plus Qt 6 only for the optional GUI), and a running llama.cpp `llama-server` in router mode serving at least one GGUF model. Everything else is in this repo — if you just want a run on screen, skip to [Quick start](#quick-start) (step 2 there covers the optional `make gui` build).

## Current status

Early prototype / POC: the core pipeline is complete (entity model and persistence, versioned skills/models, execution lifecycle with `execution_log`, the standalone runner, the GUI, soft-delete/restore, `sweep`, and the `db backup --to` atomic snapshot); the project is at an early productization stage. Retry is manual by design (a failed execution is reset explicitly with `exec reset` or the GUI Retry button), and streaming responses are out of scope by design — an execution is a single, non-interactive call. These are product decisions, not missing features. Full detail in [`docs/status.md`](docs/status.md); the issue log ([`docs/known_issues.md`](docs/known_issues.md)) records the issues found and fixed so far (all currently closed).

## What is ACTA Gamma?

ACTA Gamma treats an LLM as a single controlled action in a larger deterministic flow:

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

In agent frameworks, versioning, immutability, and audit trails are typically a separate layer added on top. Here they are part of the *same* operation: a result is only as good as your ability to prove what produced it, and a replay six months later is the same operation.

The runner drives the execution. The LLM does not orchestrate itself, maintain state, delegate work, or decide what happens next. ACTA Gamma is deliberately not an agent framework — at its core it is a runner over an OpenAI-compatible endpoint plus a SQLite audit log — the full point of view is in [`docs/PointOfView.md`](docs/PointOfView.md).

Terms used throughout this README: a **skill** is a versioned prompt template with an optional output schema — it is not a tool, function, or agent capability; a **context** is a named, immutable snapshot of input data (a document, a code file, a log excerpt) — it is not the model's prompt window.

## Core ideas

* **Stateless** — every execution is independent and one-shot.
* **Versioned skills** — prompts and output schemas are revisioned.
* **Immutable contexts** — the exact input can be retained for replay.
* **Multi-model** — the backend serves several models; any model served by the llama.cpp router can be registered as a model record.
* **Auditable** — executions retain raw responses, results, errors, and execution events (the resolved prompt is recorded in the execution log).
* **Replayable** — a replay reproduces the request inputs exactly when it reuses the same context, skill revision, and model revision. That is **input determinism, not output determinism** — resending the same inputs does not guarantee the same output (see [`PointOfView.md`](docs/PointOfView.md), "Replay caveat"; worked example: [`docs/examples/replay.md`](docs/examples/replay.md)).
* **Generic** — suitable for review, analysis, classification, extraction, auditing, and similar tasks.

## What it does — and deliberately does not

| Does | Deliberately does **not** (on purpose) |
|---|---|
| Runs **one** versioned, replayable, auditable LLM action against an immutable context | Not an agent: no self-orchestration, no conversational state, no delegation, no workflow composition between skills (a higher-level program chains the executions — [`PointOfView.md`](docs/PointOfView.md)) |
| Versioned skills & models; immutable revision snapshots; immutable contexts | No automatic retries (operator decision — retry is manual: `exec reset` / GUI Retry) and no streaming responses (operator decision — [`status.md`](docs/status.md)) |
| Standalone runner + GUI (Run/Cancel), soft-delete lifecycle, stale-execution `sweep`, atomic DB snapshot (`acta_cli db backup --to <path>`) | No users, permissions, organizations, queues, vector DBs, datasets — no server process: one private local SQLite file ([`DBDesign.md`](docs/DBDesign.md)); no batch mode for "one skill over N contexts": that is N `exec create` + N `run` commands, looped by the operator's script or a higher-level program (`acta_runner run --pending [--max N]` does loop over pending executions, but each execution is still one separate call); no hard delete / purge; no prompt-injection defense — the context reaches the model verbatim as the user message, so sanitizing untrusted content is the operator's job |
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

In the diagram, **Observation** is the LLM's response recorded verbatim as the execution's `raw_response`; **Audit** is the `execution_log` phase rows (including the resolved prompt) plus the raw response and the recorded result. `Skill @ n` and `Model @ n` mean "the skill/model revision *n* bound to this execution."

## Revisions, executions, and soft delete

The short version of the lifecycle:

- **Revisions.** Skill folders and model folders are optional organizational groupings for list display; they do not affect execution. Creating, updating, or soft-deleting a skill or model parent auto-snapshots a new immutable revision row via a DB trigger — there is no separate "snapshot" command, and no `active`/`current` flag: the "current" revision is simply the latest one. Editing *creates, not modifies*: existing executions keep pointing at the revision they were bound to.
- **Execution binding.** An execution binds to explicit `skill_revision_id` and `model_revision_id`, and its user message is exactly `context.content` — so a replay (a second `exec create` with the same three ids, optionally linked via `--parent_execution_id`) resends byte-identical request inputs.
- **States.** `pending → running → completed`, with `failed` (manual reset via `exec reset`) and `cancelled` (allowed from `pending` or `running`).
- **Concurrency.** The claim is atomic: `start()` is a single conditional `UPDATE … WHERE id=? AND status='pending'`, so two processes can never both own the same execution — the loser's claim matches zero rows and it exits with an error. The database runs in WAL mode; concurrent readers are fine, but simultaneous *writers* on the same file are **not supported in the current implementation** — single-writer is the expected usage pattern. A 5 s `busy_timeout` is set on every open (the `ACTA_DB_BUSY_TIMEOUT_MS` constant in `acta_db/src/db.c`), so a transient contention (a WAL checkpoint, a concurrent `db backup`) is absorbed by waiting up to 5 s instead of failing mid-pipeline. If a write still fails with `SQLITE_BUSY` after the timeout, `acta_db` reports the wrapped diagnostic — `SQLITE_BUSY: concurrent write detected; this database is single-writer by design — wait for the other process to finish and rerun` — and the operator's action is to rerun the command. Details: [`docs/DBDesign.md`](docs/DBDesign.md).
- **Soft delete.** Rows are never hard-deleted: `delete` sets a `deleted_at` timestamp, `restore` clears it, listers are live-only by default (`--include_deleted` / `--deleted` to opt back in), and a new DB file is the clean-state path.

The full treatment — triggers, the state machine, soft-delete rules, and the durability/backup guidance — is in [`docs/DBDesign.md`](docs/DBDesign.md); worked examples are in [`docs/examples/create-and-revise.md`](docs/examples/create-and-revise.md) (revision snapshots) and [`docs/examples/playground.md`](docs/examples/playground.md) (soft-delete tour, no backend needed).

## How a run is assembled

The runner builds the chat call from the bound revisions: `system` = the skill's `prompt_template`, `user` = `context.content` — there is no per-execution prompt field, and an empty context content fails the execution at preflight, before any backend call. Preflight enforces a deterministic `max_chars` guard on the two strings before any backend call (default 100,000, configurable): the count is the **UTF-8 byte length** of `prompt_template` + `context.content` (equivalently `strlen` in C, which counts bytes), not Unicode codepoints and not tokens — multibyte text (CJK, emoji) consumes more bytes per character, and the backend's own context window (the router's `-c`, in tokens) is a separate, final constraint. Full pipeline — claim, resolve, preflight, call, validate, record — and the backend contract are in [`docs/runner_contract.md`](docs/runner_contract.md); the llama.cpp router surface is in [`docs/llamacpp_server_contract.md`](docs/llamacpp_server_contract.md).

## Implementation

The implementation is C/C++ on top of SQLite:

| Component | Language | Description |
|---|---|---|
| `acta_db/` | C11 | SQLite persistence library (`libacta_db`) — skills, skill folders, skill revisions, models, model folders, model revisions, contexts, executions, execution logs ([schema design: `docs/DBDesign.md`](docs/DBDesign.md)) |
| `acta_cli/` | C11 | Command-line client (`acta_cli`) over `acta_db` (uses cJSON for output) |
| `acta_runner/` | C11 | Standalone LLM execution runner (`acta_runner`) — drives pending executions against the model's OpenAI-compatible backend, with `execution_log` phase rows (uses curl + cJSON) |
| `acta_gui/` | C++ / Qt 6 (Core, Widgets) | Desktop GUI: manage skills, models, contexts, review executions, and run them (the in-app **Run** button runs the runner's pipeline in-process — single pipeline codebase, not a second copy; the source coupling is a hard build constraint, see [`docs/building.md`](docs/building.md), "GUI–runner source coupling"). While a run is in flight the button toggles into **Cancel**, which cooperatively cancels the in-flight pipeline — the pipeline runs on a worker thread, Cancel sets a flag checked between pipeline steps, and a response that arrives after cancel is discarded — and transitions the row to `cancelled` |

The backend is a llama.cpp `llama-server` running in **router mode** (launched without a model, e.g. with `--models-dir` pointing at local GGUF files) — it is the **only** supported backend. llama.cpp **0.4.x** (router mode) is the tested baseline and a hard requirement: the OpenAI-compatible surface is pinned to that version's behavior, the runner does no version negotiation, and the router API is not guaranteed stable across major versions; dropping a new GGUF into `--models-dir` while the server runs is not covered by the contract. The pipeline talks to it through the OpenAI-compatible HTTP surface (`GET /health`, `GET /v1/models`, `POST /v1/chat/completions`), but that surface is the interface, not a portability promise; ACTA Gamma is not a generic OpenAI client. The backend is assumed to be a **local, unauthenticated, plain-HTTP** endpoint — do not point `base_url` at a remote host; the API key (if any) travels in cleartext.

### Building

See [`docs/building.md`](docs/building.md) for dependencies, platform-specific setup (MinGW/MSYS2, Linux, macOS), the per-component `make` steps, and the top-level wrapper (`make all`, `make test`).

## Quick start

Three steps before the example below:

1. **Install dependencies** — SQLite, curl, cJSON (plus Qt 6 for the GUI): one command block per platform in [`docs/building.md`](docs/building.md).
2. **Build** — from the repo root: `make all` (add `make gui` if you want the desktop app — it is optional and not part of `make all`), or per-component `make`; `make test` runs all C test suites.
3. **Start the backend** — a llama.cpp `llama-server` in **router mode** (launched **without** `-m`: every GGUF in `--models-dir` becomes a served model and each request is routed to the matching one):

   ```sh
   llama-server --models-dir models -c 2048
   ```

   This serves every GGUF in `models/` at `http://127.0.0.1:8080` — it is the llama.cpp router, not a generic OpenAI endpoint (full contract: [`docs/llamacpp_server_contract.md`](docs/llamacpp_server_contract.md) §1). One model is fine: point `--models-dir` at a folder containing that one GGUF — GGUF model files are downloadable, e.g., from Hugging Face; if you don't have llama.cpp yet, [`docs/building.md`](docs/building.md) covers install/build. The `-c 2048` above is a **demo value**: it is the backend's *token* context window and is deliberately much smaller than the runner's `max_chars` *byte* guard — a context can pass preflight and still be rejected by the backend. Set `-c` to match your model's supported context length.

   Before any run you can verify the backend and a model **without consuming tokens** — `acta_runner check <model-record-id>` does the two token-free preflight calls (`GET /health`, `GET /v1/models`) and reports `ok` / the served `max_context`, or a distinct verdict (`model still loading`, `server unreachable`, `model not served`, `catalog unreachable`), with no execution row and no DB write ([`docs/runner_contract.md`](docs/runner_contract.md), "check action").

## Environment variables and the per-machine config file

* **`OPENAI_API_KEY`** — the API key for the backend's HTTP calls, used identically by `acta_runner` and `acta_gui`. Resolution: `$OPENAI_API_KEY` (if set — even to the empty string, which is enough for a keyless localhost server) → the config file's `"api_key"` key. There is no CLI flag, and the key is never stored in the database. If the key is present in neither source, the run does not start; an empty key sends no `Authorization` header. Note the precedence: a set environment variable (even empty) **shadows** the config file's `"api_key"` — a stray `export OPENAI_API_KEY=` in your shell profile silently disables a key stored in the config file. This shadowing is not silent: when `OPENAI_API_KEY` is set (even to the empty string) and the config file provides a non-empty `"api_key"`, the binaries print one line — `warning: OPENAI_API_KEY is set (empty or not) and shadows the config file "api_key"; the effective key is the environment value` (to stderr in `acta_runner`; in the run result message in `acta_gui`). If the key must not persist on disk, set it in your shell profile instead of putting it in `ACTA_Gamma.conf`. Full policy: [`docs/runner_contract.md`](docs/runner_contract.md), decision 4.
* **`ACTA_DB`** — database file path used by `acta_cli` and `acta_runner` when `--db` is not given. Resolution order: `--db` → `$ACTA_DB` → the config file's `"db"` → the **same** app-data file as the GUI (`%APPDATA%\ACTA_Gamma\acta.db` on Windows, `~/.local/share/ACTA_Gamma/acta.db` on Linux, or `$XDG_DATA_HOME/ACTA_Gamma/acta.db`) → `./acta.db` as a last-resort fallback. The GUI does **not** read `--db` or `$ACTA_DB`; its *Choose database file* dialog covers non-default setups (see [Your first session in the GUI](#your-first-session-in-the-gui)). The resolution contract is in [`docs/cli_spec.md`](docs/cli_spec.md).
* **`ACTA_Gamma.conf`** — the per-machine config file: a flat JSON object with at most the four keys below, in the same app-data directory as the default DB file. All three binaries read it through the same helper; a malformed file or an unknown key is a fail-closed hard error. It may hold an `api_key` at rest, so treat it as sensitive — keeping it private is the owner's job, not the app's: on POSIX the app refuses a file that is not user-only readable (mode `0600`, hard error), but on Windows the mode bits are meaningless and a warning is printed and the file is read anyway (so on a shared Windows machine, an `api_key` in the file can be read by other users).

  | Key | Type | Meaning |
  |---|---|---|
  | `"api_key"` | string | key fallback for `$OPENAI_API_KEY` |
  | `"db"` | string | database path step |
  | `"max_chars"` | positive integer | max total prompt size in **UTF-8 bytes** (`skill.prompt_template` + `context.content`); default 100,000 |
  | `"timeout"` | positive integer, seconds | default per-call HTTP timeout; default 600 s (the `--timeout` flag still wins per run) |

## Minimal end-to-end example

Against the running `llama-server` router from the quick start (fresh `acta.db` — by default the platform app-data location, see [Environment variables](#environment-variables-and-the-per-machine-config-file); so every id is `1`).

**Fresh databases need the schema first.** `acta_cli`/`acta_runner` never create the schema themselves: run `acta_cli db init` — it applies the canonical schema (`acta_db/schema.sql`, embedded statically in the CLI; the GUI embeds the same file, and launching the GUI once applies it as well) and records the schema version in `PRAGMA user_version` (0.1 → 1). After that, schema changes are migrated in place by `acta_cli db migrate`: it applies the pending repo-static migrations — one static DDL file per version, `acta_db/migrations/<version>.sql` (the version ledger is in [`docs/DBDesign.md`](docs/DBDesign.md)) — in ascending order, each in its own transaction, and records the applied version. A `user_version = 0` file with no user tables migrates from 0.1; one with user tables is not an ACTA Gamma database and fails closed. A fresh file without the schema fails with `no such table`.

```sh
# 1. Register the model
# ("backend" is a protocol family: "openai" = OpenAI-compatible HTTP,
#  served here by the llama-server router — currently the only value;
#  model_identifier = the GGUF file's name in your --models-dir, WITHOUT the .gguf
#  extension (e.g. qwen3-8b for models/qwen3-8b.gguf) — verify with GET /v1/models)
acta_cli model create --json '{"name":"llama-local","backend":"openai","base_url":"http://127.0.0.1:8080","model_identifier":"qwen3-8b"}'

# 2. Create a versioned skill (prompt template + optional output schema)
# output_schema is a JSON-schema-style object (type/required/properties/items are checked;
# extra fields in the response are allowed). Only those keywords are enforced —
# other JSON Schema keywords (enum, pattern, minimum, …) are not. Omit it for free-text outputs.
acta_cli skill create --json '{"name":"sentiment","prompt_template":"Classify the sentiment of the input. Reply with JSON: {\"label\": \"positive\"|\"negative\", \"confidence\": number}","output_schema":{"type":"object","required":["label","confidence"],"properties":{"label":{"type":"string"},"confidence":{"type":"number"}}}}'

# 3. Create an immutable context (the input snapshot)
# (for large inputs, `context create` accepts --content_file <path>)
# type is a free-form string, not an enum — use whatever labels your contexts need
# (e.g. "text", "code", "log"), or filter by it later with `context list --type <T>`
acta_cli context create --json '{"type":"text","content":"The build system shipped on time and the release went smoothly."}'

# 4. Create an execution binding context + skill revision + model revision
acta_cli exec create --json '{"context_id":1,"skill_revision_id":1,"model_revision_id":1}'

# 5. Provide the API key: set the environment variable, or add an
# "api_key" key to ACTA_Gamma.conf (see above).
# For a keyless localhost llama-server, an empty value is intentional —
# it just means no Authorization header is sent.
export OPENAI_API_KEY=""

# 6. Optional: verify the backend and model before the run, without
#    consuming tokens (no execution row, no DB write; exit 0 on ok)
acta_runner check 1

# 7. Run it (blocks until the execution reaches a terminal state; exit 0 on success,
#    non-zero on failure; hard per-call HTTP timeout: --timeout, default 600 s)
acta_runner run 1

# 8. Inspect the result and the audit trail
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

`raw_response` is the model's text verbatim; `result` is the recorded, schema-checked output; the log is the phase timeline — one row per event, with `prompt_resolved` carrying the exact prompt that was sent. If the response does not match the skill's `output_schema`, the execution is `failed` with a `validation_failed` log row — there is no "completed with a flag" mode. `parent_execution_id` links a replay to the execution it replays (optional on `exec create`).

A failed backend call (server down, connection error, or the `--timeout` exceeded) leaves the execution in `failed` with the error recorded in `error`; recovery is the manual reset cycle: `acta_runner run 1` (fails) → `acta_cli exec reset 1` (`failed → pending`) → `acta_runner run 1` again (or the GUI Retry button, which does both). If a runner process dies mid-flight, `acta_runner sweep --stale-seconds N` fails executions left in `running` whose last activity — the newest of its `execution_log` rows and `started_at` — is older than N — run it manually when you suspect a crash or a hung run; it is not a daemon and nothing runs it for you. Pick N larger than the longest legitimate run you may have in flight (e.g. `--timeout 600` → `--stale-seconds 650` or more) so a live run is never swept. Before a `run --pending` batch — and as the first triage step after a `failed` backend call — run `acta_runner check <model-id>`: it verifies the server and the served model with **zero tokens and no execution row**, so a dead backend is diagnosed before any row is claimed and left `failed` ([`docs/runner_contract.md`](docs/runner_contract.md), "check action"). Note the limit: sweep judges staleness by last activity, not by process liveness — a runner that is alive but stuck in a hanging HTTP call can look stale; if you run very long calls, use a larger N or check the runner process before sweeping (details in [`docs/runner_contract.md`](docs/runner_contract.md), decision 6).

For worked examples against an *existing* database — exploring the DB, revising skills, replaying runs, and running five versioned skills over the same context — see [`docs/examples/`](docs/examples/README.md).

## Your first session in the GUI

The GUI is a convenience layer for humans: paste inputs, watch a run, read results without terminal JSON. Programs and scripted use should use the CLI (`acta_cli` + `acta_runner`), which is the complete surface; every GUI operation has a CLI equivalent.

Once the backend is running (see [Quick start](#quick-start)), launch `acta_gui` (built with `make gui` — the GUI is optional and not part of `make all`). On first start it creates the `acta.db` database file for you in the platform app-data directory (the same file `acta_cli` / `acta_runner` resolve to out of the box — see [Environment variables and the per-machine config file](#environment-variables-and-the-per-machine-config-file)); its *Choose database file* dialog remains for non-default setups. Then:

1. **Model** — Models panel → *New…* → a name, the backend (`openai`), the router's address, and the model id (the GGUF file's name in your `--models-dir` folder, without the `.gguf` extension).
2. **Skill** — Skills panel → *New…* → a name and the prompt template — the instruction describing the action.
3. **Context** — Contexts panel → *New…* → a type, and paste the content (a document, a code file, a log…).
4. **Execution** — Executions panel → *New…* → pick the context, the skill and the model, then press **Run**.
5. **Watch it** — the row moves `pending → running → completed` (or `failed`). While it runs, **Run** becomes **Cancel**. **Log** shows the phase timeline (including the resolved prompt), **Details** shows the raw response and the result; **Retry** re-runs a failed execution.

## CLI ergonomics

All three binaries support `--version` and `--help`. The CLI offers file in/out (`--content_file`, `--out`, `--raw_out`), NDJSON `--stream`, output shaping (`--fields`, `--no_nulls`, `--table`, `--count`, `--id_only`, `--pretty`), light-projection listers with `--full` for the high-volume blob fields, and the machine-readable `--tools` JSON schema (e.g. `acta_cli skill list --table --fields name,revision`). The CLI takes no user-supplied SQL: `db init` applies the canonical embedded schema to a fresh file (already schema'd file → no-op; partial/foreign file → fail closed, exit 4). The full wire format, per-action flag tables, and error contracts are in [`docs/cli_spec.md`](docs/cli_spec.md).

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
