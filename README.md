# ACTA Gamma

**LLMs as actions, not agents.**

ACTA Gamma is a small runner for executing **one controlled LLM action at a time** and recording the outcome.

You provide:

* a **model** — where the LLM runs
* a **skill** — a versioned prompt describing the action
* a **context** — an immutable snapshot of input data

ACTA Gamma combines them into an execution, makes one LLM call, and records the result and execution history in SQLite.

> **You decide what happens next. ACTA Gamma makes one LLM call and records what happened.**

This is deliberately **not an agent framework**.

The project is currently an early prototype / POC. See [`docs/status.md`](docs/status.md).

---

## Why ACTA Gamma?

LLMs are often embedded in increasingly autonomous systems: agents, tools, planners, memory, queues and workflows.

ACTA Gamma takes the opposite approach.

It treats an LLM as a **deterministic-flow component**:

```text
Context + Skill + Model
          │
          ▼
      One LLM call
          │
          ▼
       Result
          │
          ▼
    Audit trail
```

The surrounding application decides what happens before and after the call.

This makes ACTA Gamma useful for tasks such as:

* classification
* extraction
* review
* analysis
* transformation
* auditing
* document processing
* other repeatable LLM operations

---

## Core concepts

### Model

A registered LLM endpoint and model identifier.

ACTA Gamma currently uses a local **llama.cpp `llama-server` router** serving GGUF models.

### Skill

A versioned prompt template describing the action to perform.

A skill may also define an output schema for structural validation.

### Context

An immutable snapshot of input data.

The context is the user message sent to the model. Once created, it can be reused by multiple executions.

### Revision

Skills and models are versioned.

Editing one creates a new immutable revision rather than changing the revision already used by an execution.

### Execution

An execution binds:

```text
Context + Skill revision + Model revision
```

and records the resulting LLM call.

Executions move through states such as:

```text
pending → running → completed
                  ↘ failed
```

They can be replayed by creating another execution with the same inputs.

Replay provides **input determinism**, not output determinism: the same request inputs do not guarantee identical model output.

---

## What ACTA Gamma does — and does not

| Does                              | Does not                             |
| --------------------------------- | ------------------------------------ |
| Run one-shot LLM actions          | Act as an autonomous agent           |
| Version skills and models         | Maintain conversational state        |
| Keep immutable input snapshots    | Delegate or orchestrate work         |
| Record results and execution logs | Provide automatic retries            |
| Replay executions                 | Provide streaming responses          |
| Support multiple local models     | Manage the model server              |
| Provide CLI and GUI interfaces    | Provide users, permissions or queues |
| Store everything in SQLite        | Require a database server            |

For the design rationale and non-goals, see [`docs/PointOfView.md`](docs/PointOfView.md).

---

# Quick start

## 1. Build

Install the required dependencies for your platform, then:

```bash
make all
```

Run the test suites with:

```bash
make test
```

The optional Qt GUI is built separately:

```bash
make gui
```

See [`docs/building.md`](docs/building.md) for platform-specific dependencies and build instructions.

---

## 2. Start llama.cpp

ACTA Gamma currently uses `llama-server` in router mode.

For example:

```bash
llama-server --models-dir models -c 2048
```

Place your GGUF models in `models/`.

The server is expected at:

```text
http://127.0.0.1:8080
```

A small example model can be downloaded with:

```bash
huggingface-cli download \
  Qwen/Qwen2.5-0.5B-Instruct-GGUF \
  --include "*q4_k_m.gguf" \
  --local-dir models/
```

The `-c` value is the backend's token context window; adjust it for your model.

See [`docs/llamacpp_server_contract.md`](docs/llamacpp_server_contract.md) for the backend contract.

---

# Minimal example

Initialize a new database:

```bash
acta_cli db init
```

Register a model:

```bash
acta_cli model create --json \
'{
  "name": "llama-local",
  "backend": "openai",
  "base_url": "http://127.0.0.1:8080",
  "model_identifier": "qwen3-8b"
}'
```

Create a skill:

```bash
acta_cli skill create --json \
'{
  "name": "sentiment",
  "prompt_template": "Classify the sentiment of the input. Reply with JSON: {\"label\": \"positive\"|\"negative\", \"confidence\": number}",
  "output_schema": {
    "type": "object",
    "required": ["label", "confidence"],
    "properties": {
      "label": {"type": "string"},
      "confidence": {"type": "number"}
    }
  }
}'
```

Create an immutable context:

```bash
acta_cli context create --json \
'{
  "type": "text",
  "content": "The build system shipped on time and the release went smoothly."
}'
```

Create an execution:

```bash
acta_cli exec create --json \
'{
  "context_id": 1,
  "skill_revision_id": 1,
  "model_revision_id": 1
}'
```

For a keyless local `llama-server`:

```bash
export OPENAI_API_KEY=""
```

Optionally check the backend without making an LLM call:

```bash
acta_runner check 1
```

Run the execution:

```bash
acta_runner run 1
```

Inspect the result:

```bash
acta_cli exec get 1
acta_cli log list 1
```

The complete CLI contract and all available options are documented in [`docs/cli_spec.md`](docs/cli_spec.md).

---

# What gets recorded?

An execution records the important parts of the operation:

```text
context
skill revision
model revision
status
timestamps
raw response
validated result
error, if any
execution log
```

The execution log records the major phases of the run, making it possible to inspect what happened rather than only seeing the final answer.

For example:

```text
execution_started
context_loaded
prompt_resolved
preflight_passed
llm_request
llm_response
execution_completed
```

The exact execution and logging contract is documented in [`docs/runner_contract.md`](docs/runner_contract.md).

---

# Revisions and replay

Skills and models are versioned.

An execution never points at a mutable skill or model directly. It points at a specific revision.

That means an old execution remains tied to the exact skill/model definitions it originally used.

A replay can reuse:

```text
same context
same skill revision
same model revision
```

and therefore reproduce the same request inputs.

This is useful for auditing, comparison and experimentation.

See:

* [`docs/DBDesign.md`](docs/DBDesign.md)
* [`docs/examples/replay.md`](docs/examples/replay.md)
* [`docs/examples/create-and-revise.md`](docs/examples/create-and-revise.md)

---

# CLI

The main commands are:

| Command                               | Purpose                            |
| ------------------------------------- | ---------------------------------- |
| `acta_cli db init`                    | Initialize a database              |
| `acta_cli db migrate`                 | Apply database migrations          |
| `acta_cli db backup --to <path>`      | Create an atomic database snapshot |
| `acta_cli model create`               | Register a model                   |
| `acta_cli skill create`               | Create a versioned skill           |
| `acta_cli context create`             | Create an immutable context        |
| `acta_cli exec create`                | Create an execution                |
| `acta_runner check <id>`              | Check backend/model availability   |
| `acta_runner run <id>`                | Execute one run                    |
| `acta_runner run --pending`           | Run pending executions             |
| `acta_cli exec reset <id>`            | Reset a failed execution           |
| `acta_runner sweep --stale-seconds N` | Recover stale executions           |
| `acta_cli exec get <id>`              | Inspect an execution               |
| `acta_cli log list <id>`              | Inspect its execution log          |

All binaries support:

```bash
--help
--version
```

For the complete command syntax, JSON format, flags and error contracts, see [`docs/cli_spec.md`](docs/cli_spec.md).

---

# GUI

The optional Qt 6 GUI provides a convenient interface for human users.

It covers the same core workflow:

```text
Model
  ↓
Skill
  ↓
Context
  ↓
Execution
  ↓
Run
  ↓
Result + Log
```

The GUI is intended as a convenience layer. Scripted and programmatic use should use `acta_cli` and `acta_runner`.

Build it with:

```bash
make gui
```

See [`docs/building.md`](docs/building.md) for GUI build requirements.

---

# Architecture

ACTA Gamma is intentionally small:

```text
                    ┌─────────────────┐
                    │     acta_gui    │
                    │     Qt 6        │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │    acta_runner   │
                    │   one LLM call   │
                    └────────┬────────┘
                             │
                    OpenAI-compatible
                         HTTP API
                             │
                    ┌────────▼────────┐
                    │   llama-server  │
                    │     llama.cpp   │
                    └─────────────────┘

                    ┌─────────────────┐
                    │     acta_cli    │
                    └────────┬────────┘
                             │
                    ┌────────▼────────┐
                    │     acta_db     │
                    │     SQLite      │
                    └─────────────────┘
```

The main components are:

| Component      | Role                          |
| -------------- | ----------------------------- |
| `acta_db/`     | SQLite persistence library    |
| `acta_cli/`    | Command-line interface        |
| `acta_runner/` | LLM execution pipeline        |
| `acta_gui/`    | Optional Qt desktop interface |

See [`docs/DBDesign.md`](docs/DBDesign.md) and [`docs/runner_contract.md`](docs/runner_contract.md) for the detailed component contracts.

---

# Configuration

The most important configuration options are:

### `OPENAI_API_KEY`

API key used for backend HTTP calls.

For a keyless local `llama-server`, an empty value is valid:

```bash
export OPENAI_API_KEY=""
```

### `ACTA_DB`

Optional database path override.

### `ACTA_Gamma.conf`

Optional per-machine configuration file.

It can contain settings such as:

```json
{
  "api_key": "...",
  "db": "...",
  "max_chars": 100000,
  "timeout": 600
}
```

Configuration precedence and security rules are documented in [`docs/runner_contract.md`](docs/runner_contract.md) and [`docs/cli_spec.md`](docs/cli_spec.md).

---

# Important limitations

ACTA Gamma is intentionally narrow.

In particular:

* it does **not** protect against prompt injection
* output-schema validation checks structure, not semantic correctness
* it does **not** automatically retry failed executions
* it does **not** stream model responses
* it does **not** manage the llama.cpp server
* the current backend is a local llama.cpp `llama-server`
* the SQLite design is intended for a private local database, not a multi-user server

The context is passed to the model as input; applications handling untrusted data are responsible for deciding how that data should be treated.

These are design constraints rather than hidden features. See [`docs/PointOfView.md`](docs/PointOfView.md) and [`docs/status.md`](docs/status.md).

---

# Documentation

The README intentionally stays high-level. The detailed contracts live here:

| Document                                                               | Contents                                           |
| ---------------------------------------------------------------------- | -------------------------------------------------- |
| [`docs/building.md`](docs/building.md)                                 | Dependencies and platform-specific builds          |
| [`docs/cli_spec.md`](docs/cli_spec.md)                                 | CLI commands, flags, JSON and errors               |
| [`docs/runner_contract.md`](docs/runner_contract.md)                   | Execution pipeline and runner behavior             |
| [`docs/DBDesign.md`](docs/DBDesign.md)                                 | SQLite schema, revisions, lifecycle and migrations |
| [`docs/llamacpp_server_contract.md`](docs/llamacpp_server_contract.md) | llama.cpp backend contract                         |
| [`docs/PointOfView.md`](docs/PointOfView.md)                           | Design philosophy and non-goals                    |
| [`docs/status.md`](docs/status.md)                                     | Current project status and product decisions       |
| [`docs/known_issues.md`](docs/known_issues.md)                         | Known issues                                       |
| [`docs/examples/`](docs/examples/)                                     | Worked examples and tutorials                      |

---

# Current status

**Early prototype / POC.**

The core pipeline is implemented:

* SQLite persistence
* versioned skills and models
* immutable contexts
* execution lifecycle
* execution logging
* standalone runner
* CLI
* optional Qt GUI
* replay
* soft delete / restore
* stale execution recovery
* schema migrations (`db migrate`)
* token-free backend health check (`acta_runner check`)
* auto preflight before a run is claimed
* death marker for clean process exits mid-run
* database backup

The project is still at an early productization stage.

See [`docs/status.md`](docs/status.md) for the current status and deliberate product decisions.

---

# License

BSD Zero Clause License (BSD-0-Clause).

---

# Third-party code

ACTA Gamma uses:

* [cJSON](https://github.com/DaveGamble/cJSON) — MIT
* [libcurl](https://curl.se/libcurl/) — curl license
* a public-domain SHA-256 implementation derived from Brad Conte's `crypto-algorithms`

See [`docs/building.md`](docs/building.md) and the source tree for details.
