# Execution Creation — UI analysis

Analysis of what it takes to let the ACTA Gamma Qt UI **create execution
entries** (the first step of the future "run" flow; no backend/runner
involved yet — the created row simply lands in `pending`).

## 1. Current state (verified in code)

- **No creation path exists anywhere in the app.** Executions are only ever
  *read*: `ExecutionPanel` lists them (`acta_db_execution_query`),
  `ExecutionDialog` opens them read-only (`editExecution`), and the
  context menu / Enter / Show all funnel into that read-only dialog.
- The gap is documented in the code itself: in `executionPanel.cpp`,
  "The one-shot execution flow (create execution row, call the model
  backend, write logs, update status) has no backend client yet, so the
  'Run One-Shot' button was removed instead of left dead."
- `ui_review.md` #18 flags this as the app's core missing feature.

## 2. DB layer — already complete, nothing to build

`acta_db/include/execution.h` provides everything:

- `acta_db_execution_create(db, e, &out_id)` with required fields:
  `context_id > 0`, `skill_revision_id > 0`, `model_revision_id > 0`,
  `prompt != NULL`. `e->status` is **ignored** — new rows are always
  `pending`; all further movement is via the transition functions
  (`start` / `complete` / `fail` / `cancel`), which the runner will use
  later.
- Error codes to surface: `ACTA_DB_ERR_INVALID` (missing field / NULL),
  `ACTA_DB_ERR_FK` (dangling reference), `ACTA_DB_ERR_SQL`.
- `execution_t` carries `parent_execution_id` (0 = root) — usable for
  chained executions (e.g. load→run, multi-step) without any schema work.

The creation flow is therefore **pure UI** on top of an existing C API.

## 3. Listers available for the form (verified signatures)

| Form field | Source call | Notes |
|---|---|---|
| Context | `acta_db_context_query(db, NULL, 0, 0, &n, &err)` | NULL filter = all contexts; display = `type` + creation date, content (truncated) in the tooltip (contexts have no name column, as in the context panel) |
| Skill folder | `acta_db_skill_folder_list_all(db, 0, 0, &n, &err)` | nested via `parent_id`, same skeleton as `FolderTreePanel` |
| Skill (entity) | `acta_db_skill_list_all(db, 0, 0, &n, &err)` | id + name, grouped by `folder_id` |
| Skill revision | `acta_db_skill_revision_list_by_skill(db, skill_id, 0, 0, &n, &err)` | ascending; **select the latest** by default |
| Model folder | `acta_db_model_folder_list_all(db, 0, 0, &n, &err)` | nested via `parent_id` |
| Model (entity) | `acta_db_model_list_all(db, 0, 0, &n, &err)` | id + name, grouped by `folder_id` |
| Model revision | `acta_db_model_revision_list_by_model(db, model_id, 0, 0, &n, &err)` | ascending; latest by default |
| Parent execution (optional) | `acta_db_execution_query(db, NULL, 0, 0, &n, &err)` | "— none —" entry maps to `parent_execution_id = 0` |

## 4. Design decisions

### 4.1 New lightweight dialog, not an extension of `ExecutionDialog`

`ExecutionDialog` is a tabbed read-only viewer (Input / Prompt / Output /
Execution / Metadata / Logs) built on `ui_executionDialog.ui`. Two options:

- **A. Extend it** with a "New" mode in the spirit of `EntityDialog`
  (Mode New/Edit/ReadOnly).
- **B. New `ExecutionCreateDialog`** — a flat form dialog (combos +
  prompt editor + button box), following the same conventions
  (`QDialogButtonBox`, `saved()` flag, `qWarning` + `QMessageBox` on
  error).

**Recommendation: B.** Executions have no revision tree (the
`EntityDialog` base exists to manage revisions, which executions don't
have), so the shared base brings little, and mixing a create form into
the 6-tab viewer bloats the .ui. The create dialog is small and
standalone; `ExecutionDialog` stays exactly as is.

### 4.2 Skill/model selection as folder trees (implemented)

The original recommendation was one combo per *entity* + one combo per
*revision* ("name" → "rev n", latest preselected). On review that was
rejected: **each of skill and model is a `QTreeWidget` picker** with
the same layout as `FolderTreePanel`:

```
<folder>                  (nested via parent_id, folder icon)
  <entity>
    rev 1
    rev 2   (listers ascending; last = latest)
<root-level entity>       (folder_id = 0)
```

- Entities come from `*_list_all` grouped by `folder_id` in C++
  (UR #8 / P8a, same as the panels); revisions are child rows of their
  entity. The trees are fully expanded — they are pickers, not
  browsers.
- Selection semantics: a **revision row** selects its id (the DB only
  needs `skill_revision_id` / `model_revision_id`); an **entity row**
  auto-selects its latest revision (last child, via
  `currentItemChanged` re-entry); a **folder row** deselects (Save
  stays disabled). Default after load: the first entity's latest
  revision, mirroring the old "latest preselected" behavior.

Why a tree instead of combos: the folder hierarchy is part of the
identity of a skill/model (the same hierarchy the Skill/Model panels
show), a single widget replaces the two-stage cascade, and it scales
to any number of entities and revisions without two parallel lists.

The context combo likewise shows more than `type`: `type (yyyy-MM-dd)`
(the same two columns as the context panel list), with the content
(truncated to 400 chars) in the tooltip — the content is the main
identifying data of a context.

### 4.3 Status is not user-selectable

`create` always writes `pending` (the C layer ignores `e->status`).
No status widget in the form — the state machine owns it.

### 4.4 Validation

- Client-side: context selected, skill + its revision selected, model +
  its revision selected, prompt non-empty (trimmed) → Save disabled
  otherwise.
- Server-side errors mapped to a warning box:
  `INVALID` / `FK` → "reference does not exist" (defensive; dropdowns
  come from live queries, so FK failures are unlikely),
  `SQL` → raw `acta_db_strerror`.

### 4.5 Entry point and post-save behaviour

- **ExecutionPanel**: a "New" button beside the existing "Show" button
  (matches the Context panel's New/Show pair and moves #22's toolbar
  standardization one step forward). Opens `ExecutionCreateDialog`.
- On save: panel reloads **only if `saved()` is true** (UR #8 pattern,
  same as `SkillPanel`'s dao lambdas), then selects the new row (linear
  scan by id, as `reload()` already does for re-selection) so the
  inline log list shows its empty-state placeholder.
- **Phase 2 (not in scope here)**: a "Run" action in `SkillPanel`
  opening the same dialog pre-filled with the selected skill + latest
  revision. That pre-fill needs panel signals (#19) or a direct
  call-site — deferred until the runner exists.

### 4.6 Context combo affordances (implemented)

The context entry in the form is a combo; it needed the same
affordances the other pickers have:

- **"Show" button** beside the combo: opens the selected context in
  the read-only `ContextDialog` (all data: type, content, hash,
  metadata, dates) — the same convention as the Show buttons of the
  `ExecutionDialog` Input tab. Enabled only while a context is
  selected.
- **Right-click context menu** on the combo: **"New…"** (creates a
  context via `ContextDialog` in New mode; on save the combo is
  rebuilt and the new row is selected — needs the small
  `ContextDialog::createdId()` accessor, same pattern as this
  dialog's) and **"Show"** (the read-only dialog for the selected
  entry).
- **No "Edit" entry**: context rows are immutable — `acta_db` has no
  update / soft-delete API for contexts, and the schema installs a
  BEFORE UPDATE trigger that aborts out-of-band updates. So there is
  nothing to edit; the context menu deliberately omits it.

## 5. Files touched

- `acta_gamma/ui/executionCreateDialog.ui` (new): context combo +
  "Show" button, skill and model folder trees (`QTreeWidget`, hidden
  header), optional parent combo, monospace `QTextEdit` for the prompt
  (per #26 style rule), button box.
- `acta_gamma/src/widgets/executionCreateDialog.{h,cpp}` (new): combo
  context menu (New… / Show; no Edit — contexts are immutable).
- `acta_gamma/src/widgets/contextDialog.{h,cpp}`: small addition —
  `createdId()` accessor (id of the created row, valid once
  `saved()`), so the combo can select the new row after a create.
- `acta_gamma/src/widgets/executionPanel.{h,cpp}`: add `newExecutionBtn`
  + slot → dialog → conditional `reload()`.
- `acta_gamma/src/src.pro`: add the new sources + ui file.

## 6. Explicit non-goals (follow-up work)

- Running the execution (runner backend: model load → LLM call →
  `start`/`complete`/`fail` transitions, `execution_log` phase rows).
- Cancel button (state machine supports it; UI affordance comes with the
  runner — a running row to cancel needs a live process).
- Live refresh of running rows (#44).
- Metrics display (log-row-based; see the runner design).

## 7. Effort

Small: one .ui, one dialog class (~150 lines), one panel button, no DB
changes, no new dependencies. Testable end-to-end immediately (create a
PENDING row, see it in the panel, open it read-only) — with the
`release/acta.db` or a test DB.
