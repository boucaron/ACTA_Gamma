# Docs review — inconsistency list (priority-ordered)

Audit of [`README.md`](../README.md) + all `docs/*` files, cross-checked
against the code (`acta_cli/`, `acta_db/`, `acta_runner/`, `acta_gui/`).
Items are ordered by priority. Each item names the conflicting sources and
what the code actually does.

Legend: **H** = high (docs demonstrably wrong / will mislead users),
**M** = medium (doc drift / staleness), **L** = low (hygiene).

---

## H2 — Backend deployment mode contradicts itself (router vs single-model)

- [`README.md`](../README.md) and [`building.md`](building.md): the backend
  is `llama-server` in **router mode** (multi-model, launched without a
  model, `--models-dir`); the README's preflight section even says a
  mismatch "fails the execution with the ids the server actually serves"
  (plural).
- [`llamacpp_server_contract.md`](llamacpp_server_contract.md) §1–3:
  **single-model** server — "One server = one model", `-m <file>` +
  `--alias <name>` for a stable `model_identifier`, "`GET /v1/models`
  returns exactly one model".

The runner preflight itself is mode-agnostic (`GET /health`, `GET
/v1/models` id match, best-effort `GET /` catalog — see
[`runner_analysis.md`](runner_analysis.md) Phase 2 step 3 and decision 2),
but the docs must settle which deployment is canonical and whether the
`GET /` catalog contract (§3a) holds in router mode.

**Resolved:** router mode is settled as canonical (per README and
`building.md`; the runner preflight is mode-agnostic, and §3a's `GET /`
catalog holds in router mode — the router's models.json lists every
directory model). [`llamacpp_server_contract.md`](llamacpp_server_contract.md)
§1–3 rewritten accordingly: §1 documents router startup (`--models-dir`,
model `id` = file basename) with single-model `-m`/`--alias` as the
supported alternative; §3 states `GET /v1/models` returns one entry per
served model and the runner checks the DB `model_identifier` is *among*
the returned ids.

## H3 — `exec complete --result "text"` / `exec fail --error "text"` silently drop the value

- `acta_cli/src/argparse.c` flag spec: `{ "result", 0 }` / `{ "error", 0 }`
  (`has_value = 0`), and `execution.c` calls
  `cmd_args_flag(ga, "result", 0)` — the space-separated form never
  consumes the value, so `exec complete 42 --result "answer"` stores
  `NULL` and exits 0. Only the `--result=...` form works.
- [`cli_review.md`](cli_review.md) P1 #3 and summary item 1 still list this
  as an **open data-loss bug** ("the documented example is the broken
  form").
- [`cli_active_action.md`](cli_active_action.md) claims "**All actions are
  closed**" and does not list this item under *Live constraints*.
- [`cli_spec.md`](cli_spec.md) documents `--result` / `--error` without any
  warning that the space-separated form is broken.

**Fix:** make `result`/`error` value-carrying flags (or document
`--result=...` as the only working form in `cli_spec.md`), and reconcile
the open/closed status between the two action docs.

## H4 — DEBUG stdin leak still in code, but treated as closed

- `acta_cli/include/commands.h:200` still has
  `fprintf(stderr, "DEBUG read_stdin_all: %zu bytes: '%s'\n", ...)` — the
  full stdin payload (potentially megabytes of context content) is dumped
  to stderr on every `--stdin` use, polluting the stderr error contract.
- [`cli_review.md`](cli_review.md) P1 #4 / summary item 2 listed it as an
  open one-line removal.
- [`cli_active_action.md`](cli_active_action.md) again claims all actions
  are closed and omits it.

**Resolved:** the `DEBUG read_stdin_all` fprintf block is removed from
`resolve_input_source` (`acta_cli/include/commands.h`); `--stdin` no longer
dumps the payload to stderr, and `cli_review.md` summary item 2 is marked
resolved.

## H5 — `failed → pending` reset is unreachable from the CLI

- [`DBDesign.md`](DBDesign.md) state machine:
  `failed ──reset()──▶ pending` ("enforced in the C layer, see
  `acta_db/include/execution.h`" — `acta_db_execution_reset` exists).
- [`runner_analysis.md`](runner_analysis.md) ("failed execution is retried
  manually via the `failed → pending` reset") and
  [`status.md`](status.md) ("rerun of failed executions (`failed → pending`
  via `acta_db_execution_reset`)") both present it as a normal operation.
- [`cli_spec.md`](cli_spec.md) has **no `exec reset` action** (exec has
  create/get/start/cancel/complete/fail/set-raw/list/count), and
  `src/tools.c` (69 entries) matches that. The only caller of
  `acta_db_execution_reset` in the repo is the GUI Retry button
  (`acta_gui/src/widgets/executionPanel.cpp:314`).

CLI/agent users have no documented way to retry a failed execution
(a raw `db exec` SQL would work but is undocumented). Either add
`exec reset` to the CLI (and spec + tools table) or document the GUI path
as the supported one.

---

## M6 — `context create` JSON key: three-way mismatch (`hash` vs `content_hash`)

- Wire key per parser, spec and tools table: `hash`
  (`acta_cli/src/json.c` `json_parse_context`, `cli_spec.md`
  `{type*, content*, hash*, metadata}`, `tools.c`).
- In-code usage snippet disagrees: `acta_cli/src/commands/context.c:31`
  shows `echo '{"type":"s","content":"hi","content_hash":"ab"}' ...`.
- The README example omits the key entirely.

Pick one key, align the usage text, `cli_spec.md`, `tools.c` and README.

**Resolved:** settled on `hash`. The usage snippet in `context.c`
(now `"hash"`), `cli_spec.md` and `tools.c` (`jk_ctx_opt`) all agree;
`cli_spec.md` carries a note that `hash` is optional and defaults to
SHA-256 of `content`.

## M7 — `ui_review.md` priority section is stale about UR #15

[`ui_review.md`](ui_review.md) still lists "#15 JSON validation *(to
analyze: not all three fields are necessarily JSON … decide scope before
implementing)*". The scope is already settled in
[`runner_plan.md`](runner_plan.md) (R5) and
[`ui_active_action.md`](ui_active_action.md) (H2): validate
`output_schema` and model `configuration` as JSON **objects**; context
`content` is **excluded** (may be plain text). Update the `ui_review.md`
note so all three docs agree.

## M8 — `test-e2e` missing from the runner docs

[`building.md`](building.md) documents
`make -C acta_runner test-e2e` (dead-runner end-to-end suite, spawns real
`acta_runner` processes, ~10–15 s), but
[`runner_analysis.md`](runner_analysis.md)'s tests section (stub server,
`test_run.c`, `test_argparse.c`, `llama_smoke.c`) and
[`runner_active_action.md`](runner_active_action.md) never mention the
suite. Add it to the runner test inventory.

## M9 — Replay is promised but never specified

- [`DBDesign.md`](DBDesign.md) "Replay ?": "It is possible to replay a job,
  it creates a new job with the same parameters by defaults … A link is
  done to the parent".
- [`PointOfView.md`](PointOfView.md) and
  [`README.md`](../README.md) ("Replayable — a replay reproduces the
  request inputs exactly …") promise it.

No replay action exists anywhere; the only mechanism is
`exec create --parent_execution_id` (documented as a flag in
[`cli_spec.md`](cli_spec.md), nothing else). Add a one-paragraph "how to
replay" note (in `cli_spec.md` or `status.md`) so the promise is
actionable.

## M10 — `cli_spec.md` transition status vocabulary is incomplete for `set-raw`

The common-shapes table says `{"id":N,"status":"<s>"}` with
`s ∈ {running, cancelled, completed, failed}`, but the `set-raw` row says
it "echoes the unchanged current status" — which can be `pending`
(a `pending` row is settable via `exec set-raw` per
[`DBDesign.md`](DBDesign.md)'s state machine, where `set-raw` is a
running/pending record). Tiny, but this is the doc declared to be "the
single source of truth".

---

## L11 — `cli_active_action.md` "all closed" framing vs `cli_review.md` open items

Even after fixing H3/H4, [`cli_active_action.md`](cli_active_action.md)
should enumerate which [`cli_review.md`](cli_review.md) summary items
remain open (P1 #1 `--result`/`--error`, P1 #2 DEBUG leak) instead of a
blanket "All actions are closed" that the other doc contradicts.

## L12 — `llamacpp_server_contract.md` §5 exit codes are unnamed numbers

§5 says "any non-2xx → `execution_fail` … `EXIT_HTTP` for transport
failures" but gives no numbers. [`runner_analysis.md`](runner_analysis.md)
documents 12 (HTTP/preflight) and 13 (timeout); the canonical values are
in `acta_runner/include/runner.h` (`EXIT_INVALID 4`, `EXIT_HTTP 12`,
`EXIT_TIMEOUT 13`). Add the numbers (or a pointer to `runner.h`) so the
contract doc is self-contained.

---

*Audit basis: `README.md`, `docs/DBDesign.md`, `docs/PointOfView.md`,
`docs/building.md`, `docs/cli_spec.md`, `docs/cli_review.md`,
`docs/cli_active_action.md`, `docs/llamacpp_server_contract.md`,
`docs/runner_analysis.md`, `docs/runner_plan.md`,
`docs/runner_active_action.md`, `docs/status.md`, `docs/t2_analysis.md`,
`docs/t3_analysis.md`, `docs/t4_analysis.md`, `docs/ui_review.md`,
`docs/ui_active_action.md`, cross-checked against
`acta_cli/{src,include}`, `acta_db/include/execution.h`,
`acta_runner/{src,include}`, `acta_gui/src/widgets/executionPanel.cpp`.*
