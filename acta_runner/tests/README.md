# acta_runner tests (planned, phase 2)

Layout mirrors `acta_db_cli/tests/`: one directory per concern, one
binary per suite.

Planned suites:

- `tests/argparse/`  — pass-1/pass-2 parsing smoke tests (global flag
  extraction, positional skipping, boolean vs value flags,
  `--name=value` inline form, unknown-flag rejection).
- `tests/run/`       — `run` action against a scratch DB: pending claim
  path, non-pending rejection, not-found, `--pending` batch loop,
  `--max`/`--timeout` parsing.
- `tests/backend/`   — LLM call path against a local mock
  OpenAI-compatible HTTP stub (chat/completions echo + error cases):
  success → complete + log rows, HTTP error → fail + log rows,
  timeout → fail.

No test code yet: the scaffold (`make`, `--help`, `--version`, DB open,
pending validation) is exercised by hand until phase 2 lands.
