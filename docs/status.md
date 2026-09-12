# Current status

Early prototype / POC.

**Done:**

- Entity model and persistence: C library + CLI + GUI over SQLite.
- Skill/model versioning and folder organization.
- Execution lifecycle and execution log (`execution_log` phase rows).
- Replayable immutable contexts. `context create` derives the content hash when omitted: SHA-256 of the content, lowercase hex — the same rule the GUI applies (SHA-256 implementation from Brad Conte's crypto-algorithms, public domain; see `acta_cli/src/sha256.c` and the README "Third-party code" section). An explicitly supplied `--hash` / JSON `hash` is stored as-is.
- Standalone runner (`acta_runner`): claim → resolve → preflight → OpenAI-compatible chat call → raw response capture → optional output-schema validation → complete/fail (see `docs/runner_analysis.md`).
- In-app "Run" button: the GUI runs the runner's pipeline in-process on a worker thread, directly using the same runner source files (`acta_runner/src/run.c`, `acta_runner/src/backend.c`) — one shared pipeline codebase, no duplicated pipeline logic — on its own DB connection, with live status polling of the shared database. While a run is in flight the button toggles into Cancel, which cooperatively cancels the run and transitions the row to `cancelled`.
- `sweep` (`acta_runner sweep --stale-seconds N`) cleans up executions left in `running` after a dead runner process: an execution is stale when its last runner activity — the latest of its newest `execution_log.created_at` and `started_at` (falling back to `created_at`) — is older than `now − N` seconds. A stale row transitions `running → failed` with the error `stale running: no runner activity for N s` and an `execution_failed` log row; any row that leaves `running` between the query and the fail is skipped rather than overwriting a live outcome. `--stale-seconds` is required (there is no default) and must be a positive integer (0 is rejected).
- Normal timeout handling is done by the runner itself (hard per-call HTTP timeout, `--timeout`, default 300 s), and rerun of failed executions (`failed → pending` via `acta_db_execution_reset`, exposed by the GUI Retry button; the CLI has no `exec reset` action).

**Not yet implemented:** streaming responses and automatic retries (a failed execution can be retried manually via the `failed → pending` reset, through the GUI Retry button). Automatic retries are deliberately deferred: transient backend failures are rare in the current single-node deployment, and a manual reset is simpler to reason about and avoids retry storms.
