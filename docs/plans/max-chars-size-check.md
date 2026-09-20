# Plan — `max_chars` size check at the execution level

Status: **open — not started.** Companion of `docs/plans/acta-config-file.md`
(the per-machine override of the limit); the check itself can be built
first with the built-in default, and the file override lands with the
config file.

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
- **Limit source:** built-in default (proposed: **100,000 chars**) →
  per-machine override via the config file
  (`docs/plans/acta-config-file.md`). No CLI flag in this plan (a
  `--max-chars` flag is a possible follow-up).
- **Relation to `max_context`:** unchanged audit behavior — the served
  `max_context` is still read and recorded in `preflight_passed`; it is
  not used by the check.

## Work items (TBD — none started)

1. `acta_runner/src/run.c` — the check in preflight (post-resolve), the
   built-in default constant, and the failure path with the message above;
   `run_execution` gains the limit parameter (default = built-in).
2. Config-file override — with `docs/plans/acta-config-file.md` (shared
   resolution helper).
3. Docs:
   - README "How a run is assembled" — a "prompt size limit" paragraph
     (what is counted, where the check happens, limit source, that it is a
     guard and the backend `max_context` is the final arbiter);
   - `docs/runner_contract.md` — a preflight decision line (rule, exit
     code, message).
4. Tests (`acta_runner/tests/run/test_run.c`): a scenario with a prompt
   over the built-in default (or a lowered limit via the test hook) →
   execution fails preflight, **no** `/v1/chat/completions request reaches
   the stub, message as specified; a just-under-limit scenario proceeds
   to the call.
5. The GUI needs no separate work (same pipeline function).

## Deliberately out of scope

- No per-model char limit (the limit is global; per-model windows stay in
  the served `max_context` audit data).
- No token estimation of any kind.
- No per-execution override of the limit.
- No streaming/chunking of oversized contexts.
