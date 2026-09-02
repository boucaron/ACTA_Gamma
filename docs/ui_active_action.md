# Active Actions — from `ui_review.md`

Action plan derived from [`ui_review.md`](ui_review.md). Item numbers
(`UR #N`) reference that document. Completed items are removed from
this list once they are shipped and documented; the full original list
lives in `ui_review.md`.

## Queued (from `ui_review.md`)

### High

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| H2 | **JSON validation (to analyze)** for `output_schema`, `configuration`, context `content` (`QJsonDocument::fromJson`, clear "invalid JSON" message; at minimum a "Validate JSON" button) | UR #15 | analyze first: not all three fields are necessarily JSON — `configuration` is backend-specific JSON (`DBDesign.md`), `output_schema` is a JSON schema, but context `content` may be plain text; decide scope before implementing (dialogs + context panel editor) |

### Polish

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| P1 | `.ui` file cleanup — placeholder text instead of "default name", real revision-tree header, drop hardcoded `readOnly`, sane min sizes, consistent window titles | UR #12, #27 | |
| P2 | Theming + chrome — light QSS stylesheet, panel group headers, button icons/tooltips, window icon, minimal menu bar | UR #21, #22, #28, #30 | |
| P3 | Splitter constraint fix (`setMaximumWidth(320)` vs splitter sizes) and window-state persistence via `QSettings` | UR #29, #40 | |
| P5 | Redundant "Show" affordances, empty-state placeholders, data lifecycle (delete/prune executions + contexts) | UR #31, #41, #42 | |
| P6 | Friendlier error mapping for all `acta_db_strerror` surfaces (unique-name constraint → "already exists in this folder") | UR #45 | |

## Summary

- **Now:** H2 (to analyze).
- **Polish last:** P1–P3, P5–P6.

(Full original list lives in `ui_review.md`.)
