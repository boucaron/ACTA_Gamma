# Plan — remove the `--api_key` flag from `acta_runner`

Status: **planned**.

Follow-up to [`drop-model-config-api-key.md`](drop-model-config-api-key.md):
the key is no longer a configuration-blob key, but the runner still accepts
a `--api_key` CLI flag. Owner decision: drop the flag — `$OPENAI_API_KEY`
becomes the **only** API-key source, for both the runner and the GUI.

## Rationale

- Secrets in argv are an anti-pattern: process arguments are readable via
  `ps` / `/proc` and land in shell history; environment variables are the
  conventional secret channel and this tool already uses one.
- The flag is redundant: every use of `--api_key` is expressible as
  `OPENAI_API_KEY=… acta_runner run …`, including per-run overrides.
- A single source (`$OPENAI_API_KEY`) is a simpler, easier-to-audit
  contract and removes one of the three spellings of the same thing
  (server `--api-key`, runner `--api_key`, env `OPENAI_API_KEY`).

## Target contract

- Auth source, the only one: `$OPENAI_API_KEY` (runner and GUI).
- `acta_runner run`: no `--api_key` flag; a `--api_key` on the command line
  is an unrecognized option (standard unknown-option error, exit 10).
- Unresolved key (env var unset/empty) → no `Authorization` header sent;
  the call succeeds against a keyless server and fails with `401`
  `authentication_error` (execution `failed`) against a server started with
  `--api-key` — unchanged transport behavior, see
  `docs/llamacpp_server_contract.md`.
- Unchanged: `$OPENAI_API_KEY` semantics, the Bearer-header transport in
  `acta_runner/src/backend.c`, `acta_gui` (already env-only), `llama_smoke`
  (test utility; its positional key argument stays).

## Work items

1. **`acta_runner/src/run.c`** — drop the `--api_key` flag: the
   `cmd_args_flag(ga, "api_key", 1)` lookup and the `api_key = …`
   assignment (keep the `getenv("OPENAI_API_KEY")` fallback as the sole
   source), the `--api_key` line from the usage/help text, and the
   file-header pipeline spec line that mentions `--api_key` / env order.
2. **`acta_runner/src/argparse.c`** — remove the `{ "api_key", 1 }` entry
   from the runner's flag spec (so `--api_key` is rejected as an unknown
   option).
3. **`acta_runner/src/main.c`** — drop the `--api_key <key>` help line
   ("API key override (default: $OPENAI_API_KEY)").
4. **`acta_runner/tests/argparse/test_argparse.c`** — switch the
   value-flag test from `--api_key` to a remaining value flag (e.g.
   `--timeout`) so the argwalking test no longer pins the removed flag.
5. **Docs** — `docs/runner_contract.md` Decision 4 "Auth" and the
   implementation-notes bullet: resolution source is `$OPENAI_API_KEY`
   only (a `--api_key` flag no longer exists); `README.md`
   "Environment variables": drop the `--api_key` flag from the resolution
   order for `acta_runner run` (env var only, as in the GUI), keep the
   "never stored in the database" note.

## Verification

- Build + all C test suites green (runner suite incl. `test_run.c`
  scenario 14; argparse suite with the re-pointed value-flag test).
- Wire checks: `acta_runner run --api_key x …` → unknown-option error
  (exit 10); `OPENAI_API_KEY=k acta_runner run <id>` against the stub
  server → completes with the Bearer header sent.
- GUI smoke: unchanged (already env-only).

## Compatibility notes

- Scripts passing `--api_key` must switch to
  `OPENAI_API_KEY=… acta_runner run …`; the old flag now errors instead of
  silently working. Dev phase, no users — accepted.
