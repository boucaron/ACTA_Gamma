# ACTA Gamma — Qt UI Review

Open items from the original review of the ACTA Gamma desktop app
(`acta_gamma/`), grouped by **Code**, **UI**, and **UX**, with a suggested
priority order at the end. **Completed items have been removed** (they are
documented in the git history; the active work list lives in
[`ui_active_action.md`](ui_active_action.md)). Item numbers are kept from
the original list so cross-references stay valid — gaps are intentional.

Scope reviewed: `src/main.cpp`, `src/mainWindow.*`, `src/dbhandle.*`,
`src/widgets/{skill,model,context,execution}Panel.*`,
`src/widgets/{skill,model,context,execution}Dialog.*`,
`ui/*.ui`, `src/src.pro`, `ACTA_Gamma.pro`, `db/schema.sql`.

---

## 1. Code

### Bugs / dead code

15. **No input validation beyond emptiness.** `output_schema`,
    `configuration`, and context `content` are JSON-ish payloads
    ("Immutable input JSON…", "constraint the Output Schema… JSON").
    Validate with `QJsonDocument::fromJson` and show a clear "invalid
    JSON" message before saving; at minimum add a "Validate JSON" button.

### Architecture

18. **Run flow missing.** Executions exist in the DB and are browsable,
    and the creation path is now in the UI: "New" in the Execution
    panel opens `ExecutionCreateDialog` (context / skill / model pickers,
    prompt editor, optional parent execution) and the row lands
    `pending`. The runner backend now exists: standalone
    `acta_runner` (phase 2, d142a8e — claim → resolve → preflight →
    `POST /v1/chat/completions` → `set_raw_response` → optional
    post-hoc validation → `complete`/`fail`, with `execution_log` phase
    rows; see [`runner_analysis.md`](runner_analysis.md)). What remains
    is the in-app "→ Run" button: spawn `acta_runner run <id>` via
    `QProcess` from the Execution panel (Plan D) and poll the DB for
    status/log rows.
    *(The first step — UI creation of `pending` executions — shipped as
    H3 (8b4c16d, design details in the git history; the analysis doc
    was removed once everything was implemented). The runner backend
    shipped as `acta_runner` phase 2 (d142a8e); the GUI Run button is the
    remaining piece.)*

---

## 2. UI (widgets & layout)

26. **JSON/prompt editors are plain `QTextEdit`.** `prompt`,
   `output_schema`, `configuration`, and context `content` should use a
   monospace font; optionally syntax-highlight JSON or at least show line
   numbers — prompts and schemas are the core content of this app.
   *(Partly done: all `QTextEdit` editors are monospace via the
   `assets/style.qss` rule; syntax highlighting and line numbers are
   deliberately left out.)*

---

## 3. UX

42. **No data lifecycle.** Contexts and executions are append-only with no
    delete/cleanup in the UI — the DB will grow unbounded. Add "Delete"
    (with confirmation) for executions/logs and contexts, or at least an
    "older than N days" prune action.
    *(Scope decision, 2026-07-10: **no hard-delete API in `acta_db`** —
    lifecycle operations are soft-delete only. Deletion for contexts/
    executions, if implemented, must be a `deleted_at` soft delete
    mirroring the skill/model/folder pattern.)*
44. **Running-execution UX (when Run is implemented).** Status `running`
    should update live (polling or an in-app worker via `QThread` /
    `QtConcurrent`), with a progress indicator in the panel, and the log
    list should auto-scroll to the newest line.

---

## Priority order

1. **Must-fix:** *(none remaining)*
2. **High:**
   - #18 remaining: the in-app "Run" button — spawn
     `acta_runner run <id>` via `QProcess` (Plan D); the runner backend
     shipped as `acta_runner` phase 2 (d142a8e), the UI creation flow as
     H3 (8b4c16d)
   - #15 JSON validation
     *(to analyze: not all three fields are necessarily JSON — context
     `content` may be plain text; decide scope before implementing)*
3. **Polish:**
   - #26 remaining: JSON highlighting / line numbers
   - #44 live execution UX

(#42 data lifecycle is closed by owner decision, not an open action.)
