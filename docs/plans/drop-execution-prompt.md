# Plan — remove `execution.prompt` from the contract

Status: **done** (owner-approved design change). Items 1 (`acta_db`, `00eb877`), 2 (`acta_cli`, `4a5e9ac`), 3 (`acta_runner`, `1ac8624`), 4 (`acta_gui`, `47b97b2`), 5 (schema comment, `47b97b2`), 6 (docs, `c849509`) and 7 (verification) are complete: `make all` and `make test` green (runner suite incl. legacy-ignore pin and empty-context scenario, CLI unknown-key negative case), dead-runner e2e 14/14, GUI manual smoke (create dialog has no prompt field; `prompt_resolved` `user` = context content; legacy `prompt` still shown in the dialog Prompt tab), and the wire check `exec create --json` with a `prompt` key exits 4 (unknown key).

**Follow-up:** the legacy `executions.prompt` column itself was
subsequently fully dropped from the schema, `execution_t`, all read
paths and the GUI — see
[`drop-execution-prompt-column.md`](drop-execution-prompt-column.md).

## Rationale

Review feedback: `execution.prompt` is mostly unused and awkward to use, and it
is the one input of the execution that is not versioned, not reusable, and not
part of any revision snapshot. It weakens the core abstraction the project is
built on — the clean `Context × Skill Revision × Model Revision` matrix — by
adding a hidden fourth, unversioned input axis to replay and comparison. It is
also a prompt-injection vector for the chaining pattern described in
`PointOfView.md` (a higher-level program passing one execution's result on as
the next execution's prompt; with the clean design, results flow only into
*contexts*, and instruction text always comes from a *skill revision*).

The replacement rule: **the skill's `prompt_template` is the only instruction
source, and `context.content` is the only user-message content.** A variant
instruction is a new skill revision.

## Target contract

- Runner: `system = skill.prompt_template`, `user = context.content` (always).
  Empty context content → execution fails (`EXIT_INVALID`, error
  `empty context content`).
- `exec create` (CLI + GUI): no `--prompt` flag / no `prompt` JSON key.
- `executions.prompt` column: **kept in the schema** as a legacy nullable
  field, **never written** by current code. Rows created before the removal
  keep their value and it is still readable. No migration is needed (no
  migration framework by design; new and old DB files are both open as-is).
- Read paths unchanged: `exec get` still emits the `prompt` key (NULL for new
  rows); `--fields prompt`, `--raw_out prompt`, `--table`, and the light
  projection (which already omits it) are untouched.
- `prompt_resolved` log event: unchanged shape (`system`, `user`,
  `system_bytes`, `user_bytes`) — it remains the audit artifact of the
  actually sent prompt.

## Work items

### 1. `acta_db/` — done (`00eb877`)

- `include/execution.h` — `execution_t.prompt` stays (read path); update the
  `acta_db_execution_create` doc: `prompt` is ignored / always stored NULL
  (legacy column).
- `src/execution.c` — `acta_db_execution_create`: stop binding `e->prompt`,
  bind SQL NULL unconditionally. Getters/listers unchanged.

### 2. `acta_cli/` — done (`4a5e9ac`)

- `src/commands/execution.c`
  - Remove `--prompt` from `exec create` help text and flag parsing.
  - Remove `prompt` from the accepted JSON keys for `exec create` (a body
    carrying `prompt` is then rejected as an unknown key → exit 4, per the
    unknown-key contract).
  - Keep read paths: `exec get` JSON, `--fields`, `--raw_out prompt`,
    `--table`, vlog lines.
- `src/tools.c` — tools table: drop `{ "prompt", 1, 0 }` from the
  `exec create` flags and from `jk_exec_opt` (leaving
  `parent_execution_id`); bump whatever schema version tracking applies.
- `tests/exec/execution_test_create.c` — stop passing `--prompt` in the
  create tests; `test_create_without_prompt` becomes the canonical positive
  case; add one negative case: JSON body with `prompt` → exit 4.
- `tests/exec/execution_test_get.c` — unchanged (ref DB exec id 4 still
  carries the legacy value `'Summarize the Q3 revenue report'`; read path
  pinned).
- `acta_test_ref.sql` / `acta_test_ref.db` — unchanged (seed data is
  historical; keeps the legacy read path exercised).

### 3. `acta_runner/` — done (`1ac8624`)

- `src/run.c`
  - File header comment (lines ~15–17): replace the concatenation spec with
    `user = context.content`.
  - User-message block (~line 466): drop the `execution.prompt`
    concatenation branch; `user = ctx->content`; fail with
    `EXIT_INVALID`, `empty context content` when it is NULL/empty.
  - `prompt_resolved` metadata: unchanged shape.
- `tests/run/test_run.c` — drop `e.prompt = "USER-PROMPT"`; update the
  scenario-1 assertion to `"user":"CTX-CONTENT"`; add (or repurpose) an
  empty-context → `EXIT_INVALID` scenario.
- `tests/run/test_pending.c`, `test_deleted.c`, `test_sweep.c`,
  `test_deadrunner.c` — drop the `e.prompt` assignments (or keep them to
  pin that the runner ignores the legacy value — pick one and be
  consistent; recommend: drop them, keep the legacy-value check in
  `test_run.c` only).
- `tests/llama_smoke.c` — unchanged (its `[prompt]` positional is the
  smoke test's own direct chat message, not `execution.prompt`).

### 4. `acta_gui/` — done (`47b97b2`)

- `ui/executionCreateDialog.ui` — remove the `promptTextEdit` widget
  ("The prompt to send…").
- `src/widgets/executionCreateDialog.cpp` — drop the
  `promptTextEdit` textChanged connection, the `clear()` in reset, and the
  `e.prompt = dupString(...)` assignment in the create path; update the
  "context-less, prompt-less execution" comment (now: context content is
  the user message).
- `src/widgets/executionDialog.cpp` — the read-only **Prompt** tab keeps
  showing `e->prompt` (legacy value for old rows; empty for new rows).
  Optional polish: set placeholder text "(legacy field — no longer written)"
  when the value is NULL.
- Regenerate `build/ui/ui_executionCreateDialog.h` (qmake/uic handles it).

### 5. Schema — done (`47b97b2`)

- `acta_gui/db/schema.sql` — `executions.prompt TEXT,` stays, with a
  comment: `-- legacy: never written by current code; may hold a value in
  pre-removal rows`.
- No change to any existing DB file; new DBs are identical.

### 6. Docs

- `README.md` — "Replayable" bullet, "Revisions and lifecycle →
  Execution binding", "How a run is assembled", minimal end-to-end example
  (step 4 drops `prompt`), "Your first session in the GUI" step 4,
  "CLI ergonomics" payload list (drop `execution prompt`).
- `docs/DBDesign.md` — core-model tree (`prompt` marked legacy), `executions`
  DDL comment, "Why store the prompt?" section rewritten.
- `docs/cli_spec.md` — `exec create` row: drop `--prompt` flag and `prompt`
  JSON key; add the legacy-read note and the unknown-key exit-4 behavior.
- `docs/runner_contract.md` — "Implementation notes" user-message bullet,
  Decision 3, Phase-2 pipeline step 4.
- `docs/status.md` — "Planned changes" pointer to this file.

### 7. Verification — done

1. `make all` (CLI + runner + DB libs compile).
2. `make test` — all three C suites green (DB, CLI, runner, incl.
   `test_run.c` scenario 1 user assertion and the new empty-context
   scenario, and the CLI unknown-key negative case).
3. `make -C acta_runner test-e2e` (dead-runner suite).
4. GUI: rebuild, manual smoke — create an execution without a prompt
   (the create dialog has no prompt field), Run it, check the
   `prompt_resolved` log row carries `user` = context content; open an
   old execution (legacy `prompt` value) in the dialog — Prompt tab still
   shows it.
5. Wire check: `acta_cli exec create --json '{"prompt":"x","context_id":1,
   "skill_revision_id":1,"model_revision_id":1}'` → exit 4 (unknown key).

## Compatibility notes

- **Breaking, intentionally**: any CLI caller/script that passed `--prompt`
  or a `prompt` JSON key to `exec create` gets a usage error (exit 4 /
  unknown-key rejection). That is the point of the change.
- Existing DBs need no migration; old executions keep their `prompt` value
  and remain fully auditable.
- Replay of a pre-removal execution with a prompt is now defined by the three
  inputs; the legacy `prompt` value is audit data only, not an input.

## Optional follow-up (not part of this change, owner decision)

- Wrap `context.content` in an explicit "untrusted data" delimiter inside
  the user message as a structural prompt-injection mitigation. This is
  orthogonal to the `execution.prompt` removal and changes the bytes sent
  to the model, so it is a separate decision.
