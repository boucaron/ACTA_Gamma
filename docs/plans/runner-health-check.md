# Plan: quick runner health check without consuming tokens

Status: proposal (not started)

## Context

Today the only way to find out whether the backend is usable is to start a
real execution — `acta_runner run <id>` — which claims the row
(`pending → running`), resolves, and performs a `POST /v1/chat/completions`
call: an actual inference, i.e. tokens and model compute, with a side
effect on the execution row (and, on failure, a `failed` state that needs
`exec reset` or `sweep` cleanup). There is no way to ask "is the server
up, is my model loaded, what context window is it serving?" without paying
for a completion and without touching the audit database.

The stub server used by the test suite already proves the cheap surface
exists: `GET /health` and `GET /v1/models` — both token-free. The runner
already calls them in preflight; this plan promotes them into a standalone
operator command.

## Goal

A read-only `acta_runner check` command that verifies the backend and a
specific model **without any chat completion — zero tokens, zero
inference, zero execution rows** — and gives a clear, distinct verdict.

## Design

**Command surface:**

```
acta_runner check <model-record-id>            # DB mode
acta_runner check --base-url <url> --model-identifier <id>   # standalone mode
```

- **DB mode** (default): `<model-record-id>` is a `models` row id; the
  runner reads `base_url` + `model_identifier` from the record. No
  execution is created, claimed, or logged — the DB is opened read-only
  for the model row.
- **Standalone mode**: no DB lookup (the `--base-url` /
  `--model-identifier` flags are mutually exclusive with the positional id);
  useful to probe a server before any model is registered.

**Check sequence (all token-free):**

1. `GET /health` —
   - `200` → ok;
   - `503` → verdict `model still loading` (same meaning as the run
     preflight);
   - connection refused / DNS / timeout → verdict `server unreachable`.
2. `GET /v1/models` —
   - `model_identifier` present in the catalog → verdict `ok`, report the
     catalog entry's `max_context` (and `args`/`meta` when present);
   - absent → verdict `model not served` (the identifier is not one of the
     router's models — the 404 a run would hit);
   - endpoint error → verdict `catalog unreachable`.

   The catalog check happens only after `/health` succeeds (a 503 server
   is "loading", not "unknown model").

**Result contract:**

- stdout on success:
  `{"ok":true,"model":"<identifier>","max_context":<n>,"base_url":"<url>"}`
  (plain-text one-liner is fine for the POC; the JSON keeps it scriptable).
- stdout on failure: the same shape with `"ok":false` and a `verdict`
  field: one of `model still loading`, `server unreachable`,
  `model not served`, `catalog unreachable`.
- Exit codes (reuse the runner's existing code space, no new numbers):
  - `0` — all checks passed;
  - `EXIT_HTTP` (12) — `/health` non-200, model not in catalog, or
    HTTP-level failure;
  - `EXIT_TIMEOUT` (13) — the HTTP call hit `--timeout`;
  - `EXIT_NOT_FOUND` — unknown `<model-record-id>` (DB mode only).
- Timeout resolution is the existing one (`--timeout` flag → config
  `"timeout"` → built-in default 600 s); `/health` responds fast, so no
  separate default is needed.

**Non-goals (by design):**

- No `POST /v1/chat/completions` — ever. The whole point is a
  zero-token check.
- No `execution_log` rows, no execution state change, no DB writes of any
  kind.
- No streaming, no retry loop (consistent with the product decisions).

## Files

- `acta_runner/src/main.c`: new `check` action in the usage block +
  dispatch.
- `acta_runner/src/check.c` (new, or a section in `run.c`): the two HTTP
  calls reuse `backend.c` (`backend.c` already wraps `GET /health` and the
  catalog fetch for preflight — factor the shared request/response path,
  no new curl surface).
- `acta_gui`: no change (the GUI already shows run failures; a future
  "Check backend" button is out of scope here).

## Docs

- `docs/runner_contract.md`: new section "check action" — sequence,
  verdicts, exit codes, "no tokens, no DB writes" guarantee; note it is
  the token-free pre-flight for a `run --pending` batch.
- `README.md`:
  - Quick start: one line — `acta_runner check 1` verifies the server and
    model before any run, without consuming tokens.
  - CLI ergonomics or the sweep paragraph: recommend `check` before a
    batch and as the first triage step after a `failed` backend call.
- `docs/status.md`: Done note when landed.

## Test plan

The existing in-process stub server already covers everything needed:

- `/health` 200 + model in catalog → `ok`, exit 0, `max_context` reported;
- `/health` 503 → `model still loading`, `EXIT_HTTP`;
- `/health` 200 + model not in catalog → `model not served`, `EXIT_HTTP`;
- connection refused (no server) → `server unreachable`, `EXIT_HTTP`;
- slow `/health` + small `--timeout` → `EXIT_TIMEOUT`;
- unknown `<model-record-id>` on a scratch `:memory:` DB →
  `EXIT_NOT_FOUND`;
- **token-free assertion:** the stub counts `/v1/chat/completions`
  requests; the suite asserts the count is 0 in every `check` scenario
  (the property the whole plan is about), and that no `execution_log`
  rows are written.

`make test` green, including the new suite in `acta_runner/tests/check/`.

## Open questions

1. Should `check` also verify the `max_chars` preflight would pass for a
   given skill+context (a "would-run" dry run, still no tokens)? That
   needs reading the bound revisions — possible, but it widens the command
   beyond "is the backend up"; defer to a follow-up if wanted.
2. Should the GUI get a "Check backend" button that calls the same
   in-process pipeline? Nice-to-have; separate plan.

## Rollout (single logical change)

1. Factor the shared `/health` + catalog request path from `run.c` into
   `backend.c` (no behavior change to the pipeline).
2. New `check` action: DB lookup or standalone flags, two token-free
   calls, verdict + exit codes.
3. Tests: new `acta_runner/tests/check/` suite with the zero-`/v1/chat/
   completions` assertion.
4. Docs: runner_contract, README, status.
5. `make test` + `make gui` green.
