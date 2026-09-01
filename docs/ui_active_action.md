# Active Actions — from `ui_review.md`

Action plan derived from [`ui_review.md`](ui_review.md). Item numbers
(`UR #N`) reference that document. The active round is the **folder
round**: the Skill panel got real folder management (F1, done) and the
Model panel mirrors it (F2, done). Everything else from the review
stays queued below.

## Folder round (active)

| # | Action | Source | Status / notes |
|---|--------|--------|----------------|
| F1 | **Folders in the Skill panel** — create / rename / delete / restore folders in `SkillPanel` | UR #17 | **Done** (see the F1 notes below). F2 depends on the patterns established here |
| F2 | **Folders in the Model panel** — mirror F1 in `ModelPanel` | UR #17 | **Done** (see the F2 notes below). Same button set and dialog flows as F1; `acta_db_model_folder_soft_delete` additionally rejects live *models*, so the friendly error text differs |

### F1 — Folders in the Skill panel ✅ done

- **Status:** implemented and committed (`7a15399`, UI + DB;
  `f90f306` / `68d08a0`, tests). F1 is the template F2 must mirror.
- **What shipped:**
  - Folder button row: `New Folder` / `Rename Folder` / `Delete Folder`
    / `Restore Folder`, with selection-based enable/disable centralized
    in `updateButtonStates()` (also called after every `reload()`):
    rename/delete on live folders, restore on soft-deleted folders,
    New Folder always enabled (parent = selected folder, else root).
  - Create/rename via `QInputDialog` (empty name → no-op);
    delete via `QMessageBox::question` confirmation before
    `acta_db_skill_folder_soft_delete`, with `ACTA_DB_ERR_INVALID`
    (live child folders or skills) mapped to the friendly "delete or
    move them first" text (UR #45).
  - The "Show deleted items" checkbox now also lists soft-deleted
    *folders* (trash icon + grey foreground) so `Restore Folder` is
    reachable; deleted skills got the grey treatment too. `SP_DirIcon`
    on live folder rows.
  - Selection preserved across `reload()` via `findItemByRole()`
    (UR #8).
  - **DB addition:** `acta_db_skill_folder_list_all_with_deleted`
    (new; the old surface had no deleted-folder lister) — covered by
    tests 6.59–6.64 in `tests/test_skill_folder.c`.

### F2 — Folders in the Model panel ✅ done

- **Status:** implemented and committed (`2821092`, UI + DB; `8a51c49`,
  tests). Mirrors F1 one-to-one in `ModelPanel`.
- **What shipped:**
  - Folder button row: `New Folder` / `Rename Folder` / `Delete Folder`
    / `Restore Folder`, with selection-based enable/disable centralized
    in `updateButtonStates()` (also called after every `reload()`):
    rename/delete on live folders, restore on soft-deleted folders,
    New Folder always enabled (parent = selected folder, else root).
  - Create/rename via `QInputDialog` (empty name → no-op);
    delete via `QMessageBox::question` confirmation before
    `acta_db_model_folder_soft_delete`, with `ACTA_DB_ERR_INVALID`
    (live child folders **or live models**) mapped to the friendly
    "delete or move them first" text — the model-specific difference
    from F1 (UR #45).
  - The "Show deleted items" checkbox now also lists soft-deleted
    *folders* (trash icon + grey foreground) so `Restore Folder` is
    reachable; deleted models got the grey treatment too. `SP_DirIcon`
    on live folder rows.
  - Selection preserved across `reload()` via `findItemByRole()`
    (UR #8).
  - **DB addition:** `acta_db_model_folder_list_all_with_deleted`
    (new; same pattern as F1's skill-folder lister) — covered by the
    `list_allwd_*` tests in `tests/test_model_folder.c`.
- Until the UR #7 panel/dedup base class lands, keep the two
  implementations in sync explicitly.

## Queued (from `ui_review.md`)

### Must-fix

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| N1 | **Resolve the dead "Run One-Shot" button** — implement the one-shot execution flow (create execution row → call backend → write logs → update status) or remove/hide the button | UR #1, #18 | **Done** (`df71922`): button removed (the other sanctioned option in UR #1). The full flow is deferred: no HTTP client exists in the app and no backend wire protocol is defined (models only store `backend`/`base_url`/`model_identifier`); the DB side is ready (`execution_create`, `start/complete/fail`, `set_raw_response`, log appenders). When the protocol is decided: add an async HTTP client (e.g. OpenAI-compatible), wire the Run button back, and add live status + auto-scrolling logs (UR #44) |
| N2 | **Remove dead code** — the `QDialog::Accepted` `// TODO` branches in `ContextPanel::editContext` and `ExecutionPanel::onExecutionDoubleClicked`; the mislabeled "Edit" context-menu entry for immutable contexts; duplicate includes in `mainWindow.cpp`; `modelpanel.cpp` case mismatch | UR #2, #3, #4, #5 | **Done** (`69ef10c`): dead branches replaced by bare `dlg.exec()` + explanatory comments (both dialogs open read-only), context menu entry relabelled "Show", duplicate includes removed, `modelpanel.{h,cpp}` renamed to `modelPanel.{h,cpp}` to match `src.pro` |
| N3 | **Save feedback + confirmations** — "Saved" feedback on dialog save (and honest mode switching), delete confirmations | UR #33 | F1's delete confirmation is the first instance of this |
| N4 | **Revision-selection clobber trap** — in Edit mode, clicking a revision overwrites the form; disable revision switching until Save/Cancel, or move browsing to a read-only dialog | UR #35 | `SkillDialog` and `ModelDialog` |
| N5 | **Database-failure UX** — persistent visible offline state with "Retry", or a startup modal (create db / pick location / exit); DB path via `QStandardPaths::AppDataLocation` + first-run bootstrap | UR #9, #32 | also enables `dbAvailable()`-driven disabled panels (UR #10) |

### High

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| H1 | **Dedupe `SkillPanel`/`ModelPanel` and `SkillDialog`/`ModelDialog`** into a shared `FolderTreePanel` / `EntityDialog` base | UR #7 | F1/F2 make the divergence window smaller; land after both panels have folders |
| H2 | **JSON validation** for `output_schema`, `configuration`, context `content` (`QJsonDocument::fromJson`, clear "invalid JSON" message; at minimum a "Validate JSON" button) | UR #15 | dialogs + context panel editor |
| H3 | **Date formatting + status colors** — locale-formatted dates in columns/dialogs (ISO in tooltips), color-coded execution status and log level | UR #24, #25 | all four panels + dialogs |
| H4 | **Search & filter** — substring filter box per tree; execution status filter | UR #38 | |
| H5 | **Execution tree columns** — skill / model / context names next to Date + Status | UR #23 | revision fetches already done in `ExecutionDialog`; reuse in the panel |
| H6 | **Context menu in the Skill tree** — right-click menu on `SkillPanel` rows offering the same actions as the button rows (`New`, `Show`, `Edit Skill`, `Delete`, `Restore`, and the folder row: `New Folder` / `Rename Folder` / `Delete Folder` / `Restore Folder`), with enable/disable mirroring `updateButtonStates()` (folder actions on folder rows, `New`/`New Folder` target the selected folder or root) | UR #3 (pattern: `ContextPanel`'s right-click menu) | **Done** (`affcfcf`): menu wired via `customContextMenuRequested`; skill delete/restore logic moved into shared `onSkillDeleteClicked` / `onSkillRestoreClicked` slots used by both the buttons and the menu |
| H7 | **Context menu in the Model tree** — same as H6 mirrored in `ModelPanel` (`New`, `Show`, `Edit Model`, `Delete`, `Restore`, folder actions) | UR #3 (pattern: `ContextPanel`'s right-click menu) | **Done** (`101fe38`): menu wired via `customContextMenuRequested`; model delete/restore logic moved into shared `onModelDeleteClicked` / `onModelRestoreClicked` slots used by both the buttons and the menu, mirroring H6 one-to-one (also records the SkillPanel header declarations the H6 `.cpp` depends on). Both panels stay in sync explicitly until the UR #7 base class lands |

### Polish

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| P1 | `.ui` file cleanup — placeholder text instead of "default name", real revision-tree header, drop hardcoded `readOnly`, sane min sizes, consistent window titles | UR #12, #27 | |
| P2 | Theming + chrome — light QSS stylesheet, panel group headers, button icons/tooltips, window icon, minimal menu bar | UR #21, #22, #28, #30 | |
| P3 | Splitter constraint fix (`setMaximumWidth(320)` vs splitter sizes) and window-state persistence via `QSettings` | UR #29, #40 | |
| P4 | Keyboard/accelerator support — `F2` rename (natural fit once F1 lands), `Delete` key, accelerators | UR #39 | F1 supplies the first rename target |
| P5 | Redundant "Show" affordances, empty-state placeholders, data lifecycle (delete/prune executions + contexts) | UR #31, #41, #42 | |
| P6 | Friendlier error mapping for all `acta_db_strerror` surfaces (unique-name constraint → "already exists in this folder") | UR #45 | F1 establishes the pattern |
| P7 | **Icons + tooltips on the `SkillPanel` buttons** — give each action button an icon and a tooltip (including the folder row): `New`, `Show`, `Edit Skill`, `Delete`, `Restore`, and `New Folder` / `Rename Folder` / `Delete Folder` / `Restore Folder` (standard style icons, e.g. `SP_DialogYesButton`/`SP_DialogNoButton`/`SP_DialogResetButton`/`SP_TrashIcon`/`SP_DialogRestoreButton`/`SP_DirIcon`) | UR #28 | tooltips clarify the text-only buttons and match the menu entries from H6 |
| P8 | **Icons + tooltips on the `ModelPanel` buttons** — same as P7 mirrored in `ModelPanel` (`New`, `Show`, `Edit Model`, `Delete`, `Restore`, and the folder row) | UR #28 | keep the two panels in sync explicitly until the UR #7 base class lands |

## Summary

- **Done:** F1 and F2 (folders in both panels, incl. the
  `…_folder_list_all_with_deleted` DB additions + tests for each),
  N2 (the zero-risk dead-code cleanups), N1 (dead "Run One-Shot"
  button removed; the one-shot flow itself is deferred pending a
  backend protocol), and the context menus H6 (Skill tree) and H7
  (Model tree).
- **Now:** N3–N5, then H1–H5.
- **Polish last:** P1–P8.

(Full original list lives in `ui_review.md`.)
