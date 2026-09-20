# Plan — remove the `--api_key` flag from `acta_runner` and make `$OPENAI_API_KEY` mandatory to set

Status: **planned**.

Follow-up to [`drop-model-config-api-key.md`](drop-model-config-api-key.md):
the key is no longer a configuration-blob key, but the runner still
accepts a `--api_key` CLI flag. Owner decision:

1. Drop the flag — `$OPENAI_API_KEY` becomes the **only** API-key source,
   for both the runner and the GUI.
2. Key-presence policy for that env var:
   - **not set** → hard error (the run does not start);
   - **set but empty** → warning only (an empty key is acceptable for a
     keyless localhost server, but it is a bad idea in general).

## Rationale

- Secrets in argv are an anti-pattern: process arguments are readable via
  `ps` / `/proc` and land in shell history; environment variables are the
  conventional secret channel and this tool already uses one.
- The flag is redundant: every use of `--api_key` is expressible as
  `OPENAI_API_KEY=… acta_runner run …`, including per-run overrides.
- A single source (`$OPENAI_API_KEY`) is a simpler, easier-to-audit
  contract and removes one of the three spellings of the same thing
  (server `--api-key`, runner `--api_key`, env `OPENAI_API_KEY`).
- Making the variable *required to be set* (even to the empty string)
  turns "forgot the key" from a silent no-auth run into an explicit,
  intentional choice: unset = error, empty = warned.

## Target contract

- Auth source, the only one: `$OPENAI_API_KEY` (runner and GUI).
- `acta_runner run`: no `--api_key` flag; a `--api_key` on the command line
  is an unrecognized option (standard unknown-option error, exit 10).
- Key-presence policy, checked at the entry points (`cmd_run` in the
  runner, `runnerWorker` in the GUI) — **not** inside `run_execution`, so
  the shared pipeline and its direct-call test scenarios are unchanged:
  - unset → runner emits the `ACTA_RUNNER_ERROR` JSON line
    (`"OPENAI_API_KEY is not set; set the environment variable"`, code
    4) and exits 4 before any execution is claimed; the GUI shows the
    same message in the run result (`EXIT_INVALID`) and does not start
    the pipeline.
  - empty → warning: the runner prints
    `warning: OPENAI_API_KEY is empty — no Authorization header will be
    sent (acceptable only for a keyless localhost server; a bad idea in
    general)` on stderr and continues; the GUI surfaces the same warning
    in the run result message. No `Authorization` header is sent; the
    call succeeds against a keyless server and fails with `401`
    `authentication_error` (execution `failed`) against a server started
    with `--api-key` — transport behavior unchanged, see
    `docs/llamacpp_server_contract.md`.
- Unchanged: the Bearer-header transport in `acta_runner/src/backend.c`,
  `llama_smoke` (test utility; its positional key argument stays).

## Work items

1. **`acta_runner/src/run.c`** — drop the `--api_key` flag: the
   `cmd_args_flag(ga, "api_key", 1)` lookup and the `api_key = …`
   assignment (keep the `getenv("OPENAI_API_KEY")` as the sole source),
   the `--api_key` line from the usage/help text, and the file-header
   pipeline spec line that mentions `--api_key` / env order.
2. **`acta_runner/src/argparse.c`** — remove the `{ "api_key", 1 }` entry
   from the runner's flag spec (so `--api_key` is rejected as an unknown
   option).
3. **`acta_runner/src/main.c`** — drop the `--api_key <key>` help line
   ("API key override (default: $OPENAI_API_KEY)").
4. **`acta_runner/include/runner_util.h` (+ the implementation file)** —
   add the shared key-presence policy helper, e.g.
   `runner_api_key_status(const char *key)` → `KEY_OK` /
   `KEY_EMPTY_WARN` / `KEY_UNSET_ERR`, with the canonical error/warning
   message strings, so the runner and the GUI cannot drift.
5. **`acta_runner/src/run.c` (`cmd_run`)** — apply the policy before any
   claim: unset → `ACTA_RUNNER_ERROR` JSON line, exit 4; empty → stderr
   warning, continue.
6. **`acta_gui/src/runnerWorker.cpp`** — apply the same policy before
   `run_execution`: unset → `finished(EXIT_INVALID, <error message>)`, no
   pipeline; empty → warning in the finished message.
7. **`acta_runner/tests/argparse/test_argparse.c`** — switch the
   value-flag test from `--api_key` to a remaining value flag (e.g.
   `--timeout`) so the argwalking test no longer pins the removed flag.
8. **`acta_runner/tests/run/` new test** — pin the policy: unset →
   `cmd_run`-level error (exit 4, error message, no execution claimed);
   empty → warning and a completed execution against the stub server
   (no `Authorization` header). Register the test in
   `acta_runner/Makefile` (`TEST_DIRS`).
9. **Docs** —
   - `docs/runner_contract.md`: Decision 4 "Auth" (source is
     `$OPENAI_API_KEY` only, no `--api_key` flag; unset → error, empty →
     warning + no header) and the implementation-notes bullet.
   - `README.md` "Environment variables": resolution source is
     `$OPENAI_API_KEY` only for both runner and GUI; it **must be set** —
     unset is an error, empty is a warned-but-permitted no-auth mode
     (keyless localhost server only); keep the "never stored in the
     database" note.
   - `README.md` quick start: add the step "set `OPENAI_API_KEY`" (may be
     empty for the keyless example server).

## Verification

- Build + all C test suites green (runner suite incl. `test_run.c`
  scenario 14 and the new key-policy test; argparse suite with the
  re-pointed value-flag test; CLI and DB suites unchanged).
- Wire checks:
  - `acta_runner run --api_key x …` → unknown-option error (exit 10).
  - `unset OPENAI_API_KEY; acta_runner run <id>` → `ACTA_RUNNER_ERROR`
    line, exit 4, no execution claimed.
  - `OPENAI_API_KEY= acta_runner run <id>` → stderr warning, completes
    against the stub/keyless server with no `Authorization` header.
  - `OPENAI_API_KEY=k acta_runner run <id>` → completes with the Bearer
    header sent.
- GUI smoke: unset → error shown in the run result; empty → warning shown,
  run completes.

## Compatibility notes

- Scripts passing `--api_key` must switch to
  `OPENAI_API_KEY=… acta_runner run …`; the old flag now errors instead of
  silently working.
- The previous no-key quick-start flow now requires
  `OPENAI_API_KEY=` (empty) explicitly; that is the intended, explicit
  no-auth choice. Dev phase, no users — accepted.
