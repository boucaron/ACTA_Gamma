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
| H3 | **Execution creation flow.** "New" button in ExecutionPanel + new `ExecutionCreateDialog` (context / skill + revision / model + revision combos, prompt editor, optional parent execution) → `acta_db_execution_create`; row lands `pending`, panel reloads only if saved, new row auto-selected | UR #18 | analysis in [`execution_creation_analysis.md`](execution_creation_analysis.md); pure UI on the existing DB API — no runner involved; phase 2: pre-filled "Run" from SkillPanel once the runner exists |

### Polish

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| P5 | Data lifecycle (delete/prune executions + contexts) | UR #42 | **Closed by owner decision (2026-07-10): no delete for contexts/executions planned — soft-delete only.** UR #31 (empty-state placeholders) shipped. UR #41 (redundant "Show" affordances) was shipped as a removal, then reverted on owner decision: the ContextPanel got its Show button back (read-only ContextDialog with all fields) plus a right-click context menu on the list ("New…" / "Show"; no "Edit" — contexts are immutable). The delete/prune remainder is deliberately not implemented: `acta_db` gets no hard-delete API, and no `deleted_at` soft delete for contexts/executions is added. If ever wanted later, it must be a `deleted_at` soft delete mirroring the skill/model/folder pattern |


## Summary

- **Now:** H3 (execution creation flow, analyzed in
  [`execution_creation_analysis.md`](execution_creation_analysis.md))
  and H2 (to analyze) — the queued actions; P1 shipped (c1ec1f2), P8
  shipped (05c68bc).
- P5 closed by decision (no delete for contexts/executions; soft-delete only
  if ever wanted).
