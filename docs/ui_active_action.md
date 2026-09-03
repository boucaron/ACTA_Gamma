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
| P2 | **One toolbar per panel (ACTIVE)** — standardize button rows: all panel actions on one toolbar with sensible order, icons + tooltips on every button | UR #22 | Option B chosen: all actions icon-only on one row (tooltips carry the meaning); entity group + separator + folder group on FolderTreePanel (skill/model); context/execution keep their two buttons |

### Polish

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| P5 | Data lifecycle (delete/prune executions + contexts) | UR #42 | **Closed by owner decision (2026-07-10): no delete for contexts/executions planned — soft-delete only.** UR #31 (empty-state placeholders) shipped. UR #41 (redundant "Show" affordances) was shipped as a removal, then reverted on owner decision: the ContextPanel got its Show button back (read-only ContextDialog with all fields) plus a right-click context menu on the list ("New…" / "Show"; no "Edit" — contexts are immutable). The delete/prune remainder is deliberately not implemented: `acta_db` gets no hard-delete API, and no `deleted_at` soft delete for contexts/executions is added. If ever wanted later, it must be a `deleted_at` soft delete mirroring the skill/model/folder pattern |


## Summary

- **Now:** H2 (JSON validation, to analyze) and P2 (one toolbar per
  panel, active — implementation: `makeActionButton` helper in
  `util.h`; FolderTreePanel three rows merged into one icon-only
  toolbar row (entity group + separator + folder group, Alt+letter
  shortcuts, all letters unique); Context/Execution panels switched to
  the same icon-only row).
- H3 (execution creation flow) shipped (8b4c16d, incl. picker trees,
  Show buttons and inline previews; the analysis doc was removed once
  everything was implemented); P1 shipped (c1ec1f2), P8 shipped
  (05c68bc).
- P5 closed by decision (no delete for contexts/executions; soft-delete only
  if ever wanted).
