# Active Actions — from `ui_review.md`

Action plan derived from [`ui_review.md`](ui_review.md). Item numbers
(`UR #N`) reference that document. Completed items are removed from
this list once they are shipped and documented; the full original list
lives in `ui_review.md`.

## Queued (from `ui_review.md`)

### High

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
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
| P5 | Redundant "Show" affordances, empty-state placeholders, data lifecycle (delete/prune executions + contexts) | UR #31, #41, #42 | |
| P6 | Friendlier error mapping for all `acta_db_strerror` surfaces (unique-name constraint → "already exists in this folder") | UR #45 | F1 establishes the pattern |
| P7 | **Icons + tooltips on the `SkillPanel` buttons** — give each action button an icon and a tooltip (including the folder row): `New`, `Show`, `Edit Skill`, `Delete`, `Restore`, and `New Folder` / `Rename Folder` / `Delete Folder` / `Restore Folder` (standard style icons, e.g. `SP_DialogYesButton`/`SP_DialogNoButton`/`SP_DialogResetButton`/`SP_TrashIcon`/`SP_DialogRestoreButton`/`SP_DirIcon`) | UR #28 | tooltips clarify the text-only buttons and match the menu entries from H6 |
| P8 | **Icons + tooltips on the `ModelPanel` buttons** — same as P7 mirrored in `ModelPanel` (`New`, `Show`, `Edit Model`, `Delete`, `Restore`, and the folder row) | UR #28 | keep the two panels in sync explicitly until the UR #7 base class lands |

## Summary

- **Now:** H2–H5.
- **Polish last:** P1–P3, P5–P8.

(Full original list lives in `ui_review.md`.)
