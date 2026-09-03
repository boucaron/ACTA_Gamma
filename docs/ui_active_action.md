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
| P2 | One toolbar per panel | UR #22 | **Shipped (105eed5), Option B: all actions icon-only on one row per panel (tooltips carry the meaning). `makeActionButton` helper in `util.h` (icon + tooltip + Alt+letter shortcut, NoFocus); FolderTreePanel's three rows merged into one row — entity group (New/Show/Edit/Delete/Restore), separator, folder group; context/execution switched to the same icon-only style. Shortcut letters all unique per panel (also fixed a pre-existing Alt+R conflict between Rename Folder and Restore Folder). Context menu and enable/disable logic unchanged.** |
| P5 | Data lifecycle (delete/prune executions + contexts) | UR #42 | **Closed by owner decision (2026-07-10): no delete for contexts/executions planned — soft-delete only.** UR #31 (empty-state placeholders) shipped. UR #41 (redundant "Show" affordances) was shipped as a removal, then reverted on owner decision: the ContextPanel got its Show button back (read-only ContextDialog with all fields) plus a right-click context menu on the list ("New…" / "Show"; no "Edit" — contexts are immutable). The delete/prune remainder is deliberately not implemented: `acta_db` gets no hard-delete API, and no `deleted_at` soft delete for contexts/executions is added. If ever wanted later, it must be a `deleted_at` soft delete mirroring the skill/model/folder pattern |


## Summary

- **Now:** H2 (JSON validation, to analyze) — the only queued action.
- H3 (execution creation flow) shipped (8b4c16d, incl. picker trees,
  Show buttons and inline previews; the analysis doc was removed once
  everything was implemented); P1 shipped (c1ec1f2), P8 shipped
  (05c68bc), P2 shipped (105eed5).
- P5 closed by decision (no delete for contexts/executions; soft-delete only
  if ever wanted).
