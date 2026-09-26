# Plan: runner operational hardening (four small fixes + one low-priority probe)

Status: item 1 done (commit `4400496`, see `docs/status.md`); item 2 done (see `docs/status.md`); items 3–5 not started. Source: the MYTEST persona reviews
(`acta_runner/MYTEST/*_summarize.md`); items 1, 2, 3, 10 of the
consolidated review were accepted. Items raised but rejected are listed
under "Considered and rejected".

## Context

The reviews converged on four cheap, high-value operational gaps, all in
`acta_runner` / `acta_db` territory:

1. A set `OPENAI_API_KEY` (even empty) silently shadows the config
   file's `"api_key"` (runner_contract decision 4). The README documents
   the shadowing; the binaries do not warn. An `export OPENAI_API_KEY=`
   in a shell profile plus a key in `ACTA_Gamma.conf` produces no
   `Authorization` header and an opaque 401.
2. Single-writer SQLite with no `busy_timeout`: a transient contention
   (WAL checkpoint, `db backup`) during a mid-pipeline write returns a
   raw `SQLITE_BUSY`, the completed LLM result is lost, and "rerun the
   command" is lossy because the call already happened.
3. A crash, OOM-kill, or Ctrl+C between the atomic claim and the result
   write leaves the execution stuck in `running` forever; `sweep` is
   manual with a global `--stale-seconds` the operator must guess.
4. `acta_runner check` is manual-only: with a dead backend, `run --pending`
   claims executions, calls the LLM, and writes `failed` rows before the
   operator ever triages.

## Goal

Four fail-fast / fail-clean behaviors, each a small change to the existing
pipeline — no new architecture, no new dependencies:

1. One stderr warning when the env var shadows a non-empty config key.
2. `sqlite3_busy_timeout` in `acta_db` + a wrapped `SQLITE_BUSY` message
   naming the single-writer design.
3. A death marker: on clean process exit (SIGINT/SIGTERM/atexit) while an
   execution is claimed in `running`, write a `failed` row + a
   `runner died` log row instead of leaving `running` orphaned.
4. Auto preflight: `run --pending` (and `run <id>`) calls the two
   token-free `check` GETs before claiming; dead backend → exit with the
   `check` verdict code, no DB writes.

## Design

### 1. Shadow warning

- At key resolution (the shared policy `acta_conf_api_key_status`
  path in `acta_runner`; the GUI reader `acta_gui/src/confreader.h`
  already exposes `apiKeyPresent`), when `OPENAI_API_KEY` is **set**
  (even to `""`) and the config file has a **non-empty** `"api_key"`,
  print one stderr line per run:
  `warning: OPENAI_API_KEY is set (empty or not) and shadows the
  config file "api_key"; the effective key is the environment value`.
- No warning when the env var is unset, or the config key is absent/empty
  (nothing is being shadowed).
- Docs: README env bullet ("The binaries do not warn…" → updated),
  `docs/runner_contract.md` decision 4.

### 2. `busy_timeout` + wrapped `SQLITE_BUSY`

- In `acta_db_open`, after connect: `sqlite3_busy_timeout(db, 5000)`.
  Transient contention (checkpoint, backup) now waits up to 5 s instead
  of failing mid-pipeline.
- When a write still fails with `SQLITE_BUSY` after the timeout,
  `acta_db` wraps the error:
  `SQLITE_BUSY: concurrent write detected; this database is single-writer
  by design — wait for the other process to finish and rerun`.
- The 5000 ms value is a named constant in `db.c` (not magic); document it
  in `docs/DBDesign.md` (concurrency paragraph: "no `busy_timeout`" →
  "5 s `busy_timeout`, single-writer still the expected pattern").

### 3. Death marker for orphaned `running` rows

- The runner keeps the claimed execution id + `acta_db` handle reachable
  from a small exit path. Install `SIGINT`/`SIGTERM` handlers and an
  `atexit` hook (the handler functions are plain, testable C — the signal
  wiring is thin).
- On clean exit while the claimed execution is still `running`:
  mark it `failed` with the diagnostic `runner process exited during
  execution`, and append the matching `execution_log` phase row
  (existing phase vocabulary, new `error` text). Process-exit-only path —
  no effect on normal completion.
- Hard kill / power loss still orphans the row; `sweep` remains the
  recovery path. Document the `--stale-seconds` choice rule in README:
  "larger than the longest legitimate run (`timeout` + slack)", and note
  that the `failed`-by-sweep diagnostic names the staleness basis.
- Scope: the claim is per-execution and sequential in `run --pending`, so
  at most one row is in `running` at a time — one marker path, no bookkeeping.

### 4. Auto preflight in `run`

- Before the atomic claim in `run <id>` and `run --pending`, call the
  existing shared preflight path (`backend_preflight()` in
  `acta_runner/src/backend.c` — the same two token-free GETs `check`
  uses). Dead/unreachable backend or model not served → exit with the
  `check` verdict codes (12/13), nothing claimed, no DB writes, same JSON
  verdict line as `check`.
- Preflight runs once per `acta_runner` invocation, not per execution in
  a `--pending` loop (the loop is sequential in one process; re-preflight
  per row would multiply latency for zero benefit).
- The run pipeline's existing preflight (preflight_passed phase) stays —
  this is an additional *server-alive* check before the claim, reusing
  the check code path; no duplication of request/response logic.
- Docs: README quick start / sweep paragraph ("run check before a
  batch" → the runner does it automatically; `check` remains the manual
  triage tool after a `failed` run), `docs/runner_contract.md` (check
  action paragraph).

### 5. Low priority: llama.cpp runtime metadata probe

- Investigate whether llama-server exposes additional calls/endpoints
  (server version header or endpoint, richer `GET /v1/models` fields,
  per-model info) that could pin the *actual* served model/version at
  runtime — the model revision records only the `model_identifier`
  string, and someone can swap the GGUF under the same name, so the
  replay guarantee is weaker than it sounds.
- Deliverable: findings recorded in `docs/llamacpp_server_contract.md`
  (tested builds + what the server reports), and only if a stable
  metadata call exists: record it at model-revision snapshot time and
  surface it in `check`. No code change before the probe says so.
- Priority: low — the operator can swap models; this is a record, not a
  guarantee.

## Files

- `acta_db/src/db.c` (+ `db.h`): `busy_timeout`, wrapped `SQLITE_BUSY`.
- `acta_runner/src/` (key policy seam, `run.c`, `main.c`): shadow
  warning, death-marker exit path, auto preflight before claim.
- `acta_gui/src/confreader.h` / `runnerWorker.cpp`: shadow warning at the
  GUI read site (same message).
- Docs: README (env bullet, sweep paragraph, quick start),
  `docs/runner_contract.md` (decision 4, check action),
  `docs/DBDesign.md` (concurrency), `docs/llamacpp_server_contract.md`
  (probe findings), `docs/status.md` Done note.

## Test plan

- Shadow warning: `OPENAI_API_KEY=` (empty) + non-empty config
  `"api_key"` → run → stderr warning, no auth header; env unset or
  config key absent → no warning. Pin in `acta_runner/tests/run/`
  (env + config stub, captured stderr).
- `busy_timeout`: hold a second write connection briefly (< 5 s) → the
  first writer's write succeeds after waiting (proves the timeout is in
  effect); hold beyond the timeout → the wrapped single-writer message
  appears. Skip-guard if the environment can't hold the lock
  deterministically.
- Death marker: extract the marker function; test it in-process — claim
  an execution to `running`, invoke the exit path, assert `failed`
  status + the `execution_log` row; normal completion path untouched.
- Auto preflight: stub server down → `run --pending` exits with the check
  verdict code and the `executions` table is unchanged; stub healthy →
  run proceeds (existing `test_run.c` scenarios unchanged).
- `make all` / `make test` green; `make gui` green (GUI shadow-warning
  smoke).

## Compatibility / migration

- `busy_timeout` changes behavior: a previously-instant `SQLITE_BUSY`
  failure can now become a 5 s wait-then-succeed. Documented in
  `docs/DBDesign.md`; no data migration.
- Auto preflight adds up to two GETs per `run` invocation (~100 ms);
  `check`'s manual triage path is unchanged.

## Considered and rejected

- **Qt 6 GUI as the heaviest dependency (boris, sam):** rejected — the
  maintainer uses Qt 6 regularly and its setup cost is seconds, not a
  maintenance burden. The GUI stays; the GUI↔runner source coupling is
  accepted (one pipeline, no duplication).
- **Unbounded DB growth / soft-delete without compaction (sam, maya,
  victor):** rejected as a false problem at this scale — "a new DB file
  is the clean-state path" remains the documented policy; no
  `db compact`/`VACUUM` command.
- **Prompt-injection defense (victor, boris):** rejected — sanitizing
  untrusted context is the operator's job and is not something the
  runner can guarantee; the existing README/`runner_contract` wording
  (the runner is not a security boundary) stands.
- **Full JSON Schema enforcement / warning on unsupported keywords
  (victor, clara):** not in this plan; the documented limitation
  ("only `type`, `required`, `properties`, `items` enforced") remains
  the contract.
