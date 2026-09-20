# Plan — remove `api_key` from the model `configuration` blob

Status: **done** — all four work items complete: `acta_runner` (`3a0b072`), `acta_runner` test (`d0abfe6`), `acta_gui` (`9f02c60`), docs (`4c3fe27`). Verification: build + all C suites green (incl. `test_run.c` scenario 14 pinning a configuration carrying `api_key` as `EXIT_INVALID` unknown key), wire checks (old-style DB model fails with the unknown-key error; same model with the key removed and `$OPENAI_API_KEY` set completes), GUI smoke with `$OPENAI_API_KEY` only.

Review point: "the `api_key` is a real secret stored in a plaintext SQLite
file." Owner decision: the key is an environment variable (`$OPENAI_API_KEY`)
plus the runner's `--api_key` flag — it must never live in the DB. Remove
`api_key` from the model `configuration` contract, the code paths that read
it, and all doc references to it.

## Target contract

- `models.configuration` known keys: `temperature`, `max_tokens`, `top_k`,
  `supports_response_format` only.
- Auth resolution, highest first:
  - `acta_runner run`: `--api_key` flag → `$OPENAI_API_KEY`.
  - `acta_gui` (in-process pipeline, no `--api_key` flag): `$OPENAI_API_KEY`.
- A `configuration` JSON carrying an `api_key` key is now an **unknown key**
  and fails the execution with `EXIT_INVALID` (existing unknown-key contract)
  — the DB value is never read, so a stale secret in an old DB file stops
  being used the moment the new runner is built.

## Work items

1. **`acta_runner/src/run.c`** — drop the `configuration.api_key` parse block
   (the `cJSON_GetObjectItem(cfg, "api_key")` string-check + assignment) and
   remove `"api_key"` from the `known[]` unknown-key list; update the
   "Known keys" comment block (list without `api_key`; note that auth is
   `--api_key` / `$OPENAI_API_KEY` only).
2. **`acta_runner/tests/run/test_run.c`** — pin the new behavior: a model
   `configuration` containing `api_key` fails the execution with
   `EXIT_INVALID` (unknown model configuration key), so the old
   secret-in-DB path is regression-locked as a hard error.
3. **`acta_gui/src/runnerWorker.cpp`** — update the resolution-order comment
   (the GUI passes `$OPENAI_API_KEY` only; the `configuration.api_key`
   fallback disappears with item 1). No functional change: the env lookup
   already takes precedence and is passed as the override into
   `run_execution`.
4. **Docs** — remove every reference to `api_key` inside the model
   configuration blob:
   - `README.md` "The runner reads the keys …" — drop `api_key` from the list.
   - `README.md` "Environment variables" — resolution order becomes
     `acta_runner run`: `--api_key` flag → `$OPENAI_API_KEY`;
     `acta_gui`: `$OPENAI_API_KEY`.
   - `docs/runner_contract.md` "The model `configuration` JSON keys the
     runner understands" — drop the `api_key` entry.
   - `docs/runner_contract.md` Decision 4 "Auth" — resolution order is
     `--api_key` flag → `$OPENAI_API_KEY`; a `configuration` carrying
     `api_key` is rejected as an unknown key (`EXIT_INVALID`).

## Verification

- Build all components; run all C test suites (runner suite incl. the new
  `api_key`-as-unknown-key scenario, CLI and DB suites unchanged and green).
- Wire check: create a model whose `configuration` carries an `api_key`
  value, run an execution → `EXIT_INVALID` naming the unknown key; run the
  same model with the key removed and `$OPENAI_API_KEY` set → completes.
- GUI smoke: run with `$OPENAI_API_KEY` only (no key in any DB configuration)
  → completes.

## Compatibility notes

- Dev phase, no users: DB files from before this change that store
  `api_key` in a model `configuration` will now **fail** the execution with
  the unknown-key error — that is the intended behavior (the secret is no
  longer read from the DB). The fix is to drop the key from the
  `configuration` blob and use `$OPENAI_API_KEY`.
- `--api_key` flag, `$OPENAI_API_KEY`, the Bearer-header transport
  (`acta_runner/src/backend.c`) and the `llama_smoke` test utility are
  unchanged; `docs/known_issues.md` #8 (truncation fix) and
  `docs/llamacpp_server_contract.md` (401 handling) describe transport
  facts, not DB storage, and stay as-is.
