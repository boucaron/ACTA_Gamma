# UR #8 — Full-tree reload on every change (analysis)

Analysis of the remaining parts of review item UR #8
(`docs/ui_review.md`, Code #8):

1. **preserve the selected item (re-select by id after rebuild)** — *partly done*
2. **consider a `list_all`-style single query for skills/models instead of N
   per-folder queries** — *open*
3. **skip the rebuild when a dialog was cancelled with no changes** — *open*

Analysis date: 2026-07-12. No code changes, no build.

---

## 1. Current state: where `reload()` is called

### `FolderTreePanel` (shared by `SkillPanel` and `ModelPanel`)

| Call site | Reload needed? | Current behavior |
|---|---|---|
| constructor / `setDao()` | yes | reloads (correct) |
| `showDeletedCheck` toggled | yes | reloads (correct) |
| `filterEdit` text changed | no | `applyTreeFilter` only, incremental — already correct |
| `onShowBtnClicked` | no | no reload — correct (read-only dialog) |
| `onEntityDeleteClicked` / `onEntityRestoreClicked` | only if DB call succeeded | reloads only on `ACTA_DB_OK` — correct |
| `onNewFolderBtnClicked` | only on success | early-returns on cancel/empty name; reload + select-new-row on success — correct |
| `onRenameFolderBtnClicked` / `onDeleteFolderBtnClicked` / `onRestoreFolderBtnClicked` | only on success | reloads only on `ACTA_DB_OK` — correct |
| **`onNewBtnClicked`** | **only if dialog saved** | `m_dao.openNew(this, selectedFolderId()); reload();` — **unconditional** |
| **`onEditBtnClicked`** | **only if dialog saved** | `m_dao.openEdit(this, entityId); reload();` — **unconditional** |

### `ContextPanel`

| Call site | Reload needed? | Current behavior |
|---|---|---|
| constructor / `setDb()` | yes | reloads (correct) |
| `filterEdit` text changed | no | `applyTreeFilter` only — correct |
| **`onNewBtnClicked`** | **only if dialog saved** | `dlg.exec(); reload();` — **unconditional** (comment says "refresh the list either way" — deliberate today, but the exact pattern UR #8 targets) |

### `ExecutionPanel`

| Call site | Reload needed? | Current behavior |
|---|---|---|
| constructor / `setDb()` | yes | reloads (correct) |
| `filterEdit` / `statusFilter` changed | no | `applyFilters` only — correct |
| `onExecutionDoubleClicked` | no | no reload — correct (executions are immutable; the dialog is read-only) |

**Conclusion:** the only unconditional reloads after a dialog are the two
`FolderTreePanel` entity paths (New, Edit) and `ContextPanel` New. These are
exactly the "dialog cancelled with no changes" cases from UR #8.

---

## 2. Sub-item 8a — preserve the selected item

### Done

`FolderTreePanel::reload()` already captures `keepFolder` / `keepEntity`
from `tree->currentItem()` before `tree->clear()` and re-selects after the
rebuild via `findItemByRole` (folder preferred, since a soft-deleted folder
can vanish from a live-only rebuild). Skill and model panels both benefit.

### Gaps

- **`ContextPanel::reload()`** does `list->clear()` and rebuilds without
  re-selection. Side effect: `currentItemChanged(nullptr)` fires during the
  clear, so `showContext(nullptr)` also **clears the inline content
  editor**. A user who selects a context, clicks New, and closes the dialog
  (with or without saving) loses both selection and editor text.
- **`ExecutionPanel::reload()`** has the same gap: selection lost, and
  `showExecutionLogs(nullptr)` clears the log list back to the empty-state
  placeholder.
- **Scroll position** is not preserved anywhere (the review also mentions it).
  Best-effort restore is possible: capture
  `tree->viewport()->verticalScrollBar()->value()` before the clear and
  restore it after the rebuild. It is inherently approximate (rows may have
  shifted), so this is optional polish, not correctness.

Both context and execution rows already store their id in a role
(`RoleContextId`, `RoleExecutionId`), so re-selection by id is a few lines
each — the same pattern as `FolderTreePanel`.

**Recommendation:** extend the `FolderTreePanel` keep/re-select pattern to
`ContextPanel` and `ExecutionPanel`. Treat scroll restore as an optional
addition, only for `FolderTreePanel` (the other two lists are usually short).

---

## 3. Sub-item 8b — single `list_all` query for skills/models

### Current query count

`FolderTreePanel::reload()` runs:

- 1 query for the folder skeleton (`listFolders` →
  `acta_db_*_folder_list_all[_with_deleted]`) — already a single call.
- 1 `listEntities(0)` for root-level entities, **then one `listEntities(folderId)`
  per folder** → **N + 1 queries per reload** (N = number of folders).

### The API already supports the single-query version

No `acta_db` changes are needed. Both listers exist:

- `acta_db_skill_list_all(db, 0, 0, &n, &err)` / `..._with_deleted`
- `acta_db_model_list_all(db, 0, 0, &n, &err)` / `..._with_deleted`

and both row structs carry the folder link: `skill_t::folder_id` and
`model_t::folder_id` (`0` = root level). So the panel can fetch **all**
entities in one call and group them by `folder_id` on the C++ side.

### Proposed design

1. **`FolderRow`** — add `int folderId = 0;` (the entity's folder; 0 = root).
   `parent_id` stays folder-only; do not overload it.
2. **`FolderTreeDao`** — replace
   `std::function<QList<FolderRow>(int folderId, bool withDeleted)> listEntities`
   with
   `std::function<QList<FolderRow>(bool withDeleted)> listAllEntities`
   ("return every entity, optionally including soft-deleted ones").
3. **`FolderTreePanel::reload()`** — one `m_dao.listAllEntities(showDeleted)`
   call; group into `QHash<int, QList<FolderRow>>` by `folderId`; then
   `addEntities(rootList)` for the top level and one
   `addEntities(folderItem, list)` per folder item. Change
   `addEntities(QTreeWidgetItem *parent, int folderId)` to
   `addEntities(QTreeWidgetItem *parent, const QList<FolderRow> &rows)`.
4. **`SkillPanel::makeSkillDao` / `ModelPanel::makeModelDao`** — swap the
   `listEntities` lambdas to call
   `acta_db_skill_list_all[_with_deleted]` /
   `acta_db_model_list_all[_with_deleted]` and fill `FolderRow::folderId`.

### Properties

- **Query count:** N + 1 → **2 per reload** (folders + all entities).
- **Ordering unchanged:** both `list_in_folder` and `list_all` are "ordered
  by id"; grouping a global id-ordered stream by folder keeps each folder's
  rows in ascending id order — byte-identical tree content.
- **`withDeleted` semantics unchanged:** the `_with_deleted` variants exist
  for both skills and models, matching the checkbox toggle.
- **Pagination caveat:** `ACTA_DB_MAX_PAGE` = 10000. The old approach capped
  at 10000 rows *per folder* (so it could return >10000 total); the new
  approach caps at 10000 *total*. For the app's data sizes this is
  irrelevant; if full correctness above 10k rows ever matters, page
  `list_all` with successive offsets until `out_count < limit`. Noted here
  so it is a conscious decision, not an oversight.
- **Both panels must change in the same commit** — the DAO contract change
  is a shared-panel breaking change; `skillPanel.cpp` and
  `modelPanel.cpp` are the only two implementers.

**Recommendation:** implement as described. This is the cleanest of the three
sub-items: no DB changes, deterministic output, and it removes the
N+1-query cost from every tree mutation.

---

## 4. Sub-item 8c — skip the rebuild when the dialog was cancelled

### Why `exec()`'s result cannot be used

`QDialog::exec()` returns `Accepted`/`Rejected`, but in this codebase a save
**does not close the dialog**:

- `EntityDialog::onSaveClicked` (New mode): creates the row, switches to
  Edit mode, **stays open** ("Keep the dialog open on the created row").
- `EntityDialog::onSaveClicked` (Edit mode): updates, switches to
  Read-Only, **stays open**.
- `Close` is wired to `QDialog::reject` only; there is no accept path at all.

So a dialog that *saved* still ends with `Rejected`, and `exec()` tells the
panel nothing. The panel needs an explicit "did this dialog persist a
change?" channel.

### Proposed design

1. **`EntityDialog`** (base class): add
   `bool m_saved = false;` + `bool saved() const;` + `void setSaved(bool);`.
   - Reset `setSaved(false)` in every setup method: `SkillDialog::newSkill`,
     `SkillDialog::editSkill`, `ModelDialog::newModel`,
     `ModelDialog::editModel`, `ContextDialog::newContext`,
     `ContextDialog::editContext`.
   - Set `setSaved(true)` at the two successful-persistence points:
     `EntityDialog::onSaveClicked` after `createEntity`/`updateEntity`
     returns `ACTA_DB_OK`, and `ContextDialog::onSaveClicked` after
     `acta_db_context_create` returns `ACTA_DB_OK`.
2. **`FolderTreeDao`**: change
   `std::function<void(QWidget *, int folderId)> openNew` and
   `std::function<void(QWidget *, int id)> openEdit` to return `bool`
   ("the dialog persisted a change"). DAO lambdas become
   `dlg.exec(); return dlg.saved();` (in `skillPanel.cpp` and
   `modelPanel.cpp`). `openShow` stays `void` (no reload either way).
3. **`FolderTreePanel`**:
   - `onNewBtnClicked`: `if (m_dao.openNew(this, selectedFolderId())) reload();`
   - `onEditBtnClicked`: `if (m_dao.openEdit(this, entityId)) reload();`
4. **`ContextPanel::onNewBtnClicked`**: `dlg.exec(); if (dlg.saved()) reload();`
   (replace the "refresh the list either way" comment).

### Edge cases

| Scenario | Flag | Reload? | Correct? |
|---|---|---|---|
| New dialog, Cancel/Close without save | false | no | yes — nothing persisted |
| New dialog, Save (dialog stays open in Edit mode), then Close | true | yes | yes |
| New dialog, Save, then further edits + Save, then Close | true | yes (once) | yes |
| Edit dialog, Close without save | false | no | yes — nothing persisted |
| Edit dialog, Save (dialog flips Read-Only), then Close | true | yes | yes |
| Save attempt fails validation (e.g. empty prompt) | false | no | yes |
| Save attempt fails in DB (e.g. duplicate name) | false | no | yes — no row changed |
| Save succeeded, then user reverts fields in Edit mode and Saves with a value identical to the old one | true | yes | harmless — reload still correct, just one extra rebuild |

The flag is a one-way latch set only on a successful `ACTA_DB_OK` mutation;
every "no change" scenario maps to "no reload" and every "change" scenario
maps to "reload". There is no false-negative case: if the flag is true, a
DB mutation happened, so a reload is required.

### Alternatives considered and rejected

- **Snapshot compare** (hash row names/counts before and after the dialog):
  requires the full tree walk anyway (no savings), cannot detect
  content-only edits, and is fragile.
- **Rely on `exec()` == Accepted**: impossible — the dialogs never accept
  (see above).
- **Keep unconditional reload, optimize `reload()` instead**: the single
  `list_all` query (8b) already makes a reload cheap; skipping the no-change
  reload is still worth doing because it also avoids the visible
  select/re-select flicker. The two sub-items are complementary, not
  alternatives.

---

## 5. Files to touch (if implemented)

| File | Change |
|---|---|
| `src/widgets/folderTreePanel.h` | `FolderRow::folderId`; DAO: `listEntities` → `listAllEntities`; `openNew`/`openEdit` → `bool` return |
| `src/widgets/folderTreePanel.cpp` | group entities by `folderId` in `reload()`; `addEntities` takes a row list; conditional reload in `onNewBtnClicked`/`onEditBtnClicked`; (optional) scroll restore |
| `src/widgets/skillPanel.cpp` | `listAllEntities` lambda via `acta_db_skill_list_all[_with_deleted]`; `openNew`/`openEdit` return `dlg.saved()` |
| `src/widgets/modelPanel.cpp` | same, via `acta_db_model_list_all[_with_deleted]` |
| `src/widgets/entityDialog.h` / `.cpp` | `m_saved` flag + accessor; set on successful save |
| `src/widgets/skillDialog.cpp`, `src/widgets/modelDialog.cpp` | `setSaved(false)` in `newSkill`/`editSkill`/`newModel`/`editModel` |
| `src/widgets/contextDialog.h` / `.cpp` | same flag; reset in `newContext`/`editContext`; set on successful create |
| `src/widgets/contextPanel.cpp` | conditional reload in `onNewBtnClicked`; (8a) keep/re-select by `RoleContextId` in `reload()` |
| `src/widgets/executionPanel.cpp` | (8a) keep/re-select by `RoleExecutionId` in `reload()` |

No `acta_db`, `.ui`, or `mainWindow.cpp` changes.

---

## 6. Related observations (not part of UR #8, noted for the record)

- **`ExecutionPanel::reload()` runs 3 extra queries per row**
  (`skill_revision_get`, `model_revision_get`, `context_get`) to fill the
  Skill/Model/Context columns. That is O(rows) queries and will become
  significant once the run flow (UR #18 / #44) exists — worth revisiting
  then, possibly with a batch lookup or a JOIN-level lister.
- **`FolderTreePanel::reload()` also re-runs `applyTreeFilter`** after every
  rebuild (needed, since rows are replaced). That is O(rows) pure C++ with no
  DB cost — fine.
- The review's "consider `list_all`-style single queries" applies to
  **skills/models only**; contexts and executions already use single
  `acta_db_context_query` / `acta_db_execution_query` calls.

---

## 7. Effort and risk

- **8b** — small; mechanical; the biggest risk is forgetting one of the two
  DAO implementers (skill + model) in the same commit. Low.
- **8c** — small; the latch flag is trivial; risk is a missed `setSaved(false)`
  in a setup path (would cause one spurious reload, not a data bug). Low.
- **8a (context/execution)** — small; mirrors an existing pattern. Low.
- **Scroll restore** — optional; cosmetic; approximate. Trivial.

Overall: a low-risk, self-contained change inside `acta_gamma/src/widgets`,
no backend changes, no schema changes.

## 8. Suggested action items (for `ui_active_action.md`)

- **P8a** — single `list_all` query for skills/models (8b above): change DAO
  contract + group by `folder_id` in `FolderTreePanel::reload()`.
- **P8b** — skip reload when a dialog was cancelled with no changes (8c
  above): `saved()` latch in the dialogs, `bool`-returning `openNew`/
  `openEdit`, conditional reload in the panels.
- **P8c** — preserve selection in `ContextPanel` and `ExecutionPanel`
  reloads (8a above); optional scroll restore in `FolderTreePanel`.
