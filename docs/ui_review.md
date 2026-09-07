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

45. **UI spawns the runner as a child process (`QProcess`).**
    `ExecutionPanel` locates and spawns `acta_runner run <id> --db <path>`
    and uses the DB as a message bus (1500 ms polling) while the process
    lives. The app already links `libacta_db.a` in-process; the runner's
    pipeline is in-process-callable: `run_execution(db, exec_id,
    timeout, api_key)` in `acta_runner/src/run.c` (the runner's own tests
    link `run.c` + `backend.c` without `main.o`). Refactor: compile
    `run.c` + `backend.c` into the app (`LIBS += -lcurl -lcjson`), call
    `run_execution()` from a worker thread with a DB handle opened inside
    the thread (SQLite connections are not shareable across threads; the
    GUI thread keeps its own handle and the existing polling), and drop
    QProcess / `findRunnerExe` / stderr-error parsing from the panel. The
    standalone `acta_runner` CLI stays as-is. First analysis done in the
    review conversation (2026-07-10); see the active-action entry for the
    plan.

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
---

## Priority order

1. **Must-fix:** *(none remaining)*
2. **High:**
   - #15 JSON validation
     *(to analyze: not all three fields are necessarily JSON — context
     `content` may be plain text; decide scope before implementing)*
3. **Medium:**
   - #45 in-process runner (no QProcess)
4. **Polish:**
   - #26 remaining: JSON highlighting / line numbers

(#42 data lifecycle is closed by owner decision, not an open action.)
