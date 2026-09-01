# Active Actions — from `ui_review.md`

Action plan derived from [`ui_review.md`](ui_review.md). Item numbers
(`UR #N`) reference that document. The active round is the **folder
round**: give the Skill panel real folder management first, then mirror
it in the Model panel. Everything else from the review stays queued
below.

## Folder round (active)

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| F1 | **Folders in the Skill panel** — create / rename / delete / restore folders in `SkillPanel` | UR #17 | the whole `acta_db_skill_folder_*` surface already exists (`create`, `rename`, `move_to`, `soft_delete`, `restore`, `get`, `list_all`, `count_all`); only the UI is missing. F2 depends on the patterns established here |
| F2 | **Folders in the Model panel** — mirror F1 in `ModelPanel` | UR #17 | same button set and dialog flows as F1; `acta_db_model_folder_soft_delete` additionally rejects live *models*, so the friendly error text differs |

### F1 — Folders in the Skill panel

- **Buttons.** One standard folder row: `New Folder`, `Rename`,
  `Delete Folder`, `Restore Folder`, with the same selection-based
  enable/disable wiring the skill buttons already use
  (`currentItemChanged`, folder role ≠ 0). Enable rules:
  - `New Folder`: always enabled (parent = selected folder, else root);
  - `Rename` / `Delete Folder`: selected item is a live folder;
  - `Restore Folder`: selected item is a soft-deleted folder.
- **Create.** `QInputDialog::getText` for the name (empty → `Cancel`,
  matching `ACTA_DB_ERR_INVALID` on NULL/empty name). Parent is the
  selected folder, or `0` (root) when a skill or nothing is selected.
  After `acta_db_skill_folder_create`, `reload()` and select the new
  folder by its id.
- **Rename.** `QInputDialog::getLineEdit` pre-filled with the current
  name; `acta_db_skill_folder_rename`.
- **Delete.** Confirm with `QMessageBox::question` (UR #33) before
  `acta_db_skill_folder_soft_delete`. Map `ACTA_DB_ERR_INVALID`
  (folder still has live children) to the friendly "delete or move its
  children first" text (UR #45) instead of the raw strerror.
- **Show deleted.** The existing "Show deleted items" checkbox must also
  surface soft-deleted *folders* (icon + grey-out, UR #23) so
  `Restore Folder` is reachable.
- **Reload hygiene.** Preserve the current selection across `reload()`
  by re-selecting the same folder/skill id (UR #8).
- **Folder icon.** `SP_DirIcon` on folder rows (UR #23) — part of this
  change because selection states must visually distinguish folders from
  skills.

### F2 — Folders in the Model panel

- Mirror F1 one-to-one in `ModelPanel` (`New Folder` / `Rename` /
  `Delete Folder` / `Restore Folder`), same dialog flows and enable
  rules.
- Error mapping difference: `acta_db_model_folder_soft_delete` rejects
  when the folder has live child folders **or live models** — the
  friendly message must say "delete or move its folders and models
  first".
- Reuse whatever helper F1 introduced (folder button row, input-dialog
  flow, error mapping). Until the UR #7 panel/dedup base class lands,
  keep the two implementations in sync explicitly.

## Queued (from `ui_review.md`)

### Must-fix

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| N1 | **Resolve the dead "Run One-Shot" button** — implement the one-shot execution flow (create execution row → call backend → write logs → update status) or remove/hide the button | UR #1, #18 | the app's core feature; once implemented, add live status + auto-scrolling logs (UR #44) |
| N2 | **Remove dead code** — the `QDialog::Accepted` `// TODO` branches in `ContextPanel::editContext` and `ExecutionPanel::onExecutionDoubleClicked`; the mislabeled "Edit" context-menu entry for immutable contexts; duplicate includes in `mainWindow.cpp`; `modelpanel.cpp` case mismatch | UR #2, #3, #4, #5 | zero behavior risk, do first alongside F1 |
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

### Polish

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| P1 | `.ui` file cleanup — placeholder text instead of "default name", real revision-tree header, drop hardcoded `readOnly`, sane min sizes, consistent window titles | UR #12, #27 | |
| P2 | Theming + chrome — light QSS stylesheet, panel group headers, button icons/tooltips, window icon, minimal menu bar | UR #21, #22, #28, #30 | |
| P3 | Splitter constraint fix (`setMaximumWidth(320)` vs splitter sizes) and window-state persistence via `QSettings` | UR #29, #40 | |
| P4 | Keyboard/accelerator support — `F2` rename (natural fit once F1 lands), `Delete` key, accelerators | UR #39 | F1 supplies the first rename target |
| P5 | Redundant "Show" affordances, empty-state placeholders, data lifecycle (delete/prune executions + contexts) | UR #31, #41, #42 | |
| P6 | Friendlier error mapping for all `acta_db_strerror` surfaces (unique-name constraint → "already exists in this folder") | UR #45 | F1 establishes the pattern |

## Summary

- **Now:** F1 (folders in the Skill panel), then F2 (folders in the
  Model panel); do N2's zero-risk cleanups in the same pass.
- **Next:** N1 (the Run flow), then N3–N5, then H1–H5.
- **Polish last:** P1–P6.

(Full original list lives in `ui_review.md`.)
