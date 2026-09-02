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
| P5 | Data lifecycle (delete/prune executions + contexts) | UR #42 | Partially shipped: UR #31 (empty-state placeholders) and UR #41 (redundant "Show" affordances) are done; what remains is delete/prune, which first needs an `acta_db` delete API — contexts/executions/logs have none today (ON DELETE RESTRICT FKs) |
| P6 | Friendlier error mapping for all `acta_db_strerror` surfaces (unique-name constraint → "already exists in this folder") | UR #45 | |

## Summary

- **Now:** H2 (to analyze).
- **Polish last:** P1, P5 (lifecycle only, `acta_db` work first), P6.

(Full original list lives in `ui_review.md`.)
