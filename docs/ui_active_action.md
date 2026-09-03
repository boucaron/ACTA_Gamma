# Active Actions — from `ui_review.md`

Action plan derived from [`ui_review.md`](ui_review.md). Item numbers
(`UR #N`) reference that document, which now lists only the open items
(completed items were removed from it; item numbers keep the original
numbering).

## Queued (from `ui_review.md`)

### High

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| H2 | **JSON validation (to analyze)** for `output_schema`, `configuration`, context `content` (`QJsonDocument::fromJson`, clear "invalid JSON" message; at minimum a "Validate JSON" button) | UR #15 | analyze first: not all three fields are necessarily JSON — `configuration` is backend-specific JSON (`DBDesign.md`), `output_schema` is a JSON schema, but context `content` may be plain text; decide scope before implementing (dialogs + context panel editor) |

### Polish

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| P5 | Data lifecycle (delete/prune executions + contexts) | UR #42 | **Closed by owner decision (2026-07-10): no delete for contexts/executions planned — soft-delete only.** UR #31 (empty-state placeholders) shipped. UR #41 (redundant "Show" affordances) was shipped as a removal, then reverted on owner decision: the ContextPanel got its Show button back (read-only ContextDialog with all fields) plus a right-click context menu on the list ("New…" / "Show"; no "Edit" — contexts are immutable). The delete/prune remainder is deliberately not implemented: `acta_db` gets no hard-delete API, and no `deleted_at` soft delete for contexts/executions is added. If ever wanted later, it must be a `deleted_at` soft delete mirroring the skill/model/folder pattern |


## Summary

- **Now:** H2 (JSON validation, to analyze) — the only queued action.
- Shipped: H3 (execution creation flow, 8b4c16d, incl. picker trees,
  Show buttons and inline previews; the analysis doc was removed once
  everything was implemented), P1 (c1ec1f2), P8 (05c68bc),
  P2 (105eed5 + 4d0a315, one icon-only toolbar row per panel),
  P4 (a400386, editExecution err handling; unit tests transversal),
  P6 (9968571, panel itemChanged(int) signals), UR #23 (context tree
  newest-first sort, execution tree Skill/Model/Context columns, folder/
  trash icons, panel log table as `QTableView`; 5a6fbc3) and UR #37 (status-meaning
  tooltips via `statusMeaning()`, "Show trash" label, one-shot note in the
  About dialog) and UR #39 (F2 on entities opens Edit, Enter opens the
  Context Show dialog; both in 25906b2).
- P5 closed by decision (no delete for contexts/executions; soft-delete only
  if ever wanted).
