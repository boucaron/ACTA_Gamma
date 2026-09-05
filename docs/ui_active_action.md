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

## Queued (local observations, not from `ui_review.md`)

### High

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| L1 | **Tree panel (model/skill): right-click on empty area shows no context menu.** Fix `FolderTreePanel::onListContextMenu` so the menu is shown when `tree->itemAt(pos)` is null | local observation | analysis below |

## L1 analysis — context menu swallowed on empty-area right-click

- **Symptom:** in the model/skill tabs, right-clicking the tree panel
  *outside* any item (empty viewport area, below the last row) shows
  no menu at all.
- **Root cause:** `FolderTreePanel::onListContextMenu`
  (`acta_gamma/src/widgets/folderTreePanel.cpp:467`) starts with:
  `const auto *item = tree->itemAt(pos); if (!item) return;` — the
  whole handler (including `menu.exec(...)`) is skipped for empty-area
  clicks, so `Qt::CustomContextMenu` swallows the right-click silently.
- **Sibling case (same pattern):** `ExecutionPanel::onListContextMenu`
  (`executionPanel.cpp:673`) has the identical `if (!item) return;` —
  empty-area right-click in the execution list is also swallowed. Fix
  both in the same change for consistent behavior.
- **Contrast:** `ContextPanel::onListContextMenu` (`contextPanel.cpp:279`)
  does **not** early-return: it always shows the menu and lets the
  row-scoped actions ("Show") fall back to the currently selected row
  (`aShow->setEnabled(showBtn->isEnabled())`). That is the behavior to
  mirror.
- **Proposed fix:** in `onListContextMenu`, when `!item`, skip the
  `setCurrentItem` call and build the menu with the row-dependent
  actions disabled (`Show` / `Edit` / `Delete` / `Restore` and the
  `*Folder` variants) while the always-available actions stay enabled
  (`New`, `New Folder`), then `menu.exec(tree->viewport()->mapToGlobal(pos))`.
  No DAO changes needed; the handlers already guard on selection.

## Summary

- **Now:** H2 (JSON validation, to analyze) and L1 (tree context menu on
  empty area — fix `FolderTreePanel`, sibling `ExecutionPanel`).
- After shipping each item: drop it from the open lists and the Summary
  (the commit is the record).
