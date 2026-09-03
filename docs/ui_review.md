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

13. **"New" target-folder logic.** In `SkillPanel::onNewBtnClicked` (and
    Model), if the user has a *skill/model* selected (not a folder),
    `selectedFolderId()` returns 0 and the new item lands at root, not next
    to its sibling. Better: fall back to the selected item's own folder.
14. **`ExecutionDialog::editExecution` error handling.** Each open builds a
    new `QStandardItemModel` — fine — but the `context_get`,
    `skill_revision_get`, `model_revision_get` queries have no `err` checks
    (only the log list does); failures silently leave blank fields. Add
    `err` handling / `qWarning` consistent with the rest of the codebase.
15. **No input validation beyond emptiness.** `output_schema`,
    `configuration`, and context `content` are JSON-ish payloads
    ("Immutable input JSON…", "constraint the Output Schema… JSON").
    Validate with `QJsonDocument::fromJson` and show a clear "invalid
    JSON" message before saving; at minimum add a "Validate JSON" button.
16. **`main.cpp`.** Consider a global crash guard or at least
    `qInstallMessageHandler` writing to a log file in the app dir, since DB
    errors currently only go to `qWarning`/console.

### Architecture

18. **Run flow missing.** Executions exist in the DB and are browsable,
    and the creation path is now in the UI: "New" in the Execution
    panel opens `ExecutionCreateDialog` (context / skill / model pickers,
    prompt editor, optional parent execution) and the row lands
    `pending`. What is missing is the "→ Run" part: no runner backend
    yet (model load → LLM call → status transitions, `execution_log`
    phase rows). That is the app's core value ("LLMs as actions").
    *(The first step — UI creation of `pending` executions — shipped as
    H3 (8b4c16d); the analysis is in
    [`execution_creation_analysis.md`](execution_creation_analysis.md).
    Full Run flow follows once the runner backend exists.)*
19. **Panels emit no signals.** No signal after mutation, so `MainWindow`
    can't react (e.g., selecting a skill could prefill an "execute" form).
    At minimum, emit `itemChanged(int id)` so future features can hook in.

---

## 2. UI (widgets & layout)

22. **Button rows inconsistent.** Skill/Model have two rows (New/Show/Edit,
    then Delete/Restore) while Context has one (New/Show) and Execution has
    (Show Log Details, Show) stacked vertically. Standardize: one toolbar per panel with
    all actions, sensible order, and **icons + tooltips** on every button.
    *(Partly done: every button in all four panels now has an icon +
    tooltip — execution panel ("Show" / "Show Log Details" with right-click
    context menus) and context panel ("New" / "Show") added with P2; the
    context panel "Show" was later removed as redundant (UR #41), then
    re-added on owner decision together with a right-click context menu
    on the list ("New…" / "Show"; no "Edit" — contexts are immutable); the
    "one toolbar per panel" layout standardization is still open.)*
23. **Trees:**
   - Skill/Model trees: `setHeaderHidden(true)` and no folder icon →
     folders and items are visually indistinguishable. Add a folder icon
     (`SP_DirIcon`) and maybe bold text for folders; keep deleted = trash
     icon and additionally grey-out/italicize deleted rows.
   - Context tree: sorting is enabled but no auto-sort is applied after
     reload; call `sortItems(1, Qt::AscendingOrder)` (or descending) so
     newest-first is stable.
   - Execution tree shows only "Date" and "Status" — useless for
     identification. Add columns for skill name, model name, context type
     (fetch the revision names in the panel, as the dialog already does).
     *(Done: the execution tree now has Skill / Model / Context columns,
     filled in `ExecutionPanel::reload()` from the same revision lookups
     the dialog uses; the skill/model trees use folder icons
     (`SP_DirIcon`) vs trash icons for deleted rows. Still open: the
     context tree has no auto-sort applied after reload (`sortItems`), and
     the `ExecutionPanel` log list is still a `QTreeWidget` (the execution
     *dialog* log table is a `QTableView`).)*
   - The log list is a `QTreeWidget` with 4 columns — a
     `QTableWidget`/`QTableView` is the right control; keep
     `setUniformRowHeights`.
26. **JSON/prompt editors are plain `QTextEdit`.** `prompt`,
   `output_schema`, `configuration`, and context `content` should use a
   monospace font; optionally syntax-highlight JSON or at least show line
   numbers — prompts and schemas are the core content of this app.
   *(Partly done: all `QTextEdit` editors are monospace via the
   `assets/style.qss` rule; syntax highlighting and line numbers are
   deliberately left out.)*

---

## 3. UX

34. **"New" target ambiguity.** Clicking **New** with a skill selected
    silently creates the item at root (see #13). Show the target folder in
    the dialog title or a label ("Creating in folder: X / root").
36. **Modal-on-modal-on-modal.** Panel → dialog → nested dialog (e.g. the
    Execution dialog's "Show context" opens a dialog inside a dialog).
    Consider an inline detail pane or a non-modal browser; at minimum keep
    nesting depth ≤ 2.
37. **Discovery gaps:**
   - No tooltip explaining what "One-Shot" means or what the statuses
     mean.
   - "Show deleted items" — consider "Show trash" and visually distinct
     deleted rows (see #23).
   - No help/about dialog explaining the model: Skill = prompt + schema,
     Model = backend config, Context = immutable input, Execution = one
     shot.
   *(Partly done: every panel button has an icon + tooltip (see #22),
   deleted rows carry a trash icon, and the Help → About dialog explains
   the model. Status-meaning tooltips and a "Show trash" label are still
   open.)*
39. **Keyboard/accelerator support.** Add accelerators (`&New`,
    `&Delete`), `Delete` key to soft-delete the selection, `F2` to rename,
    `Enter` to open the detail dialog. Right now everything is mouse-only.
    *(Partly done: Delete/F2/Enter + button accelerators in the panels.)*
42. **No data lifecycle.** Contexts and executions are append-only with no
    delete/cleanup in the UI — the DB will grow unbounded. Add "Delete"
    (with confirmation) for executions/logs and contexts, or at least an
    "older than N days" prune action.
    *(Scope decision, 2026-07-10: **no hard-delete API in `acta_db`** —
    lifecycle operations are soft-delete only. Deletion for contexts/
    executions, if implemented, must be a `deleted_at` soft delete
    mirroring the skill/model/folder pattern.)*
43. **i18n is half-done.** `MainWindow` uses `tr(...)` in two places,
    everything else is raw English literals, and no `qs_`/translation files
    exist. Either commit to `tr()` everywhere + a `translations/` target, or
    drop it consistently.
44. **Running-execution UX (when Run is implemented).** Status `running`
    should update live (polling or an in-app worker via `QThread` /
    `QtConcurrent`), with a progress indicator in the panel, and the log
    list should auto-scroll to the newest line.

---

## Priority order

1. **Must-fix:** *(none remaining)*
2. **High:**
   - #18 remaining: the Run flow (runner backend) — the UI creation
     flow shipped as H3 (8b4c16d, analyzed in
     [`execution_creation_analysis.md`](execution_creation_analysis.md))
   - #15 JSON validation
     *(to analyze: not all three fields are necessarily JSON — context
     `content` may be plain text; decide scope before implementing)*
   - #37 remaining: status-meaning tooltips, "Show trash" label
3. **Polish:**
   - #13/#34 "New" target-folder fallback + target label
   - #14 `editExecution` `err` handling
   - #16 `main.cpp` log file
   - #19 panel signals
   - #22 one toolbar per panel
   - #23 remaining: context auto-sort, panel log `QTableView`
   - #26 remaining: JSON highlighting / line numbers
   - #39 remaining keyboard support
   - #43 i18n consistency
   - #44 live execution UX

(#42 data lifecycle is closed by owner decision, not an open action.)
