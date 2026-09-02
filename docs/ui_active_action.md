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
| P5 | Data lifecycle (delete/prune executions + contexts) | UR #42 | **Closed by owner decision (2026-07-10): no delete for contexts/executions planned — soft-delete only.** UR #31 (empty-state placeholders) and UR #41 (redundant "Show" affordances) shipped; the delete/prune remainder is deliberately not implemented: `acta_db` gets no hard-delete API, and no `deleted_at` soft delete for contexts/executions is added. If ever wanted later, it must be a `deleted_at` soft delete mirroring the skill/model/folder pattern |


## Summary

- **Now:** H2 (to analyze).
- **Polish last:** P1 (`.ui` cleanup). P5 closed by decision (no delete for
  contexts/executions; soft-delete only if ever wanted).

(Full original list lives in `ui_review.md`.)
