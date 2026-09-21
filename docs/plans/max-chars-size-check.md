# Plan — `max_chars` size check at the execution level

Status: **done.** Companion of
`docs/plans/acta-config-file.md` (the per-machine override of the
limit). Landed: `run_execution` (carried by `cmd_run` and the GUI
worker) takes the `max_chars` limit parameter, the limit is resolved per
the file-over-builtin order by the shared helper
(`acta_conf_resolve_max_chars`, built-in default 100,000 chars; the
file override, `docs/plans/acta-config-file.md` work item 4), and the
preflight size check consumes the limit (work item 1). The docs
(README "How a run is assembled" paragraph; `docs/runner_contract.md`
decision 8 + pipeline step 3) landed with work item 3, and the tests
(`acta_runner/tests/run/test_run.c` scenarios 16/17) landed with work
item 4.

## Context

- There is **no size limit anywhere** in ACTA Gamma today: a context is a
  SQLite `TEXT` blob of any size — a 50 MB log can be created, bound to a
  skill and a model, and run.
- The only enforcement is the backend's context window, and it is enforced
  by the backend **at the chat call**: the execution goes
  `pending → running → failed` with the backend's (usually opaque 400)
  error in the audit trail, after the claim and after the preflight
  HTTP calls.
- Preflight already reads the served `max_context` from `/v1/models` and
  records it in the `preflight_passed` log line — but it does not compare
  it to anything.

## Why chars, not tokens

Token cost varies per model (and per content), so a cross-model
token estimate is not a trustworthy gate. A plain **char count** of the
prompt actually sent is deterministic, model-agnostic, and trivial to
explain: "the prompt sent was N chars; the limit is M chars". It is a
**guard**, not a window-fit guarantee — for prompts under the limit, the
backend's `max_context` remains the final arbiter, and the served value
keeps being recorded in preflight for audit and comparison.

## Why the check is at the execution level

The prompt that is sent is assembled per execution:
`system = skill.prompt_template` (from the bound skill revision) and
`user = context.content` (from the bound context). Both halves can
individually be large (a giant template, a giant context, or both), so
the check must happen where both are known — the execution's preflight,
**after** the resolve step. Checking contexts alone (at `context create`
or pre-resolve) would miss oversized skill prompts; checking only after
the chat call is what happens today and is what this plan removes.

## Target contract

- **Location:** `run_execution` (shared pipeline, `acta_runner/src/run.c`),
  in the preflight step, after the context/skill/model are resolved and
  before the catalog fetch and the chat call. The GUI benefits
  automatically (same `run_execution`).
- **Rule:** `total_chars = strlen(prompt_template) + strlen(context.content)`
  (the assembled prompt is exactly these two strings; JSON framing
  overhead is negligible and not counted). If `total_chars > max_chars` →
  fail the execution **before any chat call**:
  `EXIT_INVALID`, message
  `prompt too large: N chars total (context X + skill prompt Y) exceeds max_chars Z`.
- **Lifecycle:** the normal `pending → running → failed` with an
  `execution_failed` log row — same shape as the other preflight failures,
  with a deterministic local cause instead of a backend 400.
- **Limit source:** config file `"max_chars"` → built-in default
  (**100,000 chars**, `ACTA_CONF_DEFAULT_MAX_CHARS`): resolved by the
  shared helper `acta_conf_resolve_max_chars()`
  (`docs/plans/acta-config-file.md`, work item 4 — implemented), and the
  resolved value is passed into `run_execution`'s limit parameter by
  `cmd_run` and the GUI worker. No CLI flag in this plan (a
  `--max-chars` flag is a possible follow-up).
- **Relation to `max_context`:** unchanged audit behavior — the served
  `max_context` is still read and recorded in `preflight_passed`; it is
  not used by the check.

## Work items

1. **Done —** `acta_runner/src/run.c`: the check in preflight
   (post-resolve, before any backend call), consuming the resolved
   `max_chars` limit (built-in default `ACTA_CONF_DEFAULT_MAX_CHARS` =
   100,000 chars, via `acta_conf_resolve_max_chars`), with the failure
   path and message above.
2. **Done —** Config-file override — with `docs/plans/acta-config-file.md`
   (shared resolution helper: `acta_conf_resolve_max_chars`, file →
   built-in 100,000 chars, passed into `run_execution`).
3. **Done —** Docs:
   - README "How a run is assembled" — a "prompt size limit" paragraph
     (what is counted, where the check happens, limit source, that it is a
     guard and the backend `max_context` is the final arbiter);
   - `docs/runner_contract.md` — decision 8 (prompt size limit: rule,
     exit code, message, limit source) plus the step-3 preflight line.
4. **Done —** Tests (`acta_runner/tests/run/test_run.c`): the
   `scenario()` helper gained a `max_chars` argument (the lowered-limit
   test hook, no 100,001-char fixture) and all existing call sites pass
   `ACTA_CONF_DEFAULT_MAX_CHARS`. Scenario 16: 23-char prompt
   (`"SYS-TEMPLATE"` 12 + `"CTX-CONTENT"` 11) against a limit of 20 →
   fails preflight with `EXIT_INVALID`, **no** `preflight_passed` /
   `llm_request` log rows (no `/v1/chat/completions` request reaches the
   stub), `execution_failed` row, and the exact message. Scenario 17:
   the same prompt against a limit of 23 (total == limit is not over)
   → proceeds to the call and completes. File header scenario list
   updated to 1–17 (also fixing the pre-existing off-by-one numbering
   of the `config api_key` / `empty context` blocks).
5. The GUI needs no separate work (same pipeline function).

## Deliberately out of scope

- No per-model char limit (the limit is global; per-model windows stay in
  the served `max_context` audit data).
- No token estimation of any kind.
- No per-execution override of the limit.
- No streaming/chunking of oversized contexts.
