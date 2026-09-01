# ACTA Gamma — Qt UI Review

Comprehensive list of changes and improvements for the ACTA Gamma desktop app
(`acta_gamma/`), grouped by **Code**, **UI**, and **UX**, with a suggested
priority order at the end.

Scope reviewed: `src/main.cpp`, `src/mainWindow.*`, `src/dbhandle.*`,
`src/widgets/{skill,model,context,execution}Panel.*`,
`src/widgets/{skill,model,context,execution,executionLog}Dialog.*`,
`ui/*.ui`, `src/src.pro`, `ACTA_Gamma.pro`, `db/schema.sql`.

---

## 1. Code

### Bugs / dead code

1. **`runBtn` ("Run One-Shot") is never connected.**
   `executionPanel.cpp` creates the button but only `showBtn` has a `connect`.
   The button does nothing. Either implement the one-shot execution flow
   (create execution row → call backend → write logs → update status) or
   hide/remove the button until it works.
2. **Dead `QDialog::Accepted` branches:**
   - `ContextPanel::editContext`: `if (dlg.exec() == QDialog::Accepted) { // TODO }`
     — the read-only dialog only has a Close button (which rejects), so this
     can never fire.
   - `ExecutionPanel::onExecutionDoubleClicked`: same pattern with `// TODO`.
   Clean these up; they signal unfinished logic.
3. **Misleading action name.** `ContextPanel`'s right-click menu says
   **"Edit"** for a context that is explicitly immutable (the DB has a
   `contexts_immutable` trigger). It opens a read-only dialog. Rename to
   "Show" and make it consistent with the "Show" button (or drop the
   context menu entirely since single-click already shows the content in
   the editor).
4. **Case-mismatched filename:** `widgets/modelpanel.cpp` vs `modelPanel.h`
   (and the `.pro` entry says `modelpanel.cpp`). Works on Windows
   (case-insensitive) but **breaks Linux/macOS builds**. Rename to
   `modelPanel.cpp`.
5. **Duplicate includes** in `mainWindow.cpp`: the four
   `#include "widgets/...Panel.h"` lines appear twice.
6. **`dupString`, `utf8`, `toDateTime` copy-pasted in 5 dialogs.**
   Factor into a small shared header (e.g. `widgets/util.h`) — single
   implementation, one test target. Also simplify `toDateTime`: the
   `Qt::ISODate` attempt can never match `"yyyy-MM-dd HH:mm:ss"` anyway;
   just try `Qt::ISODateWithMs`, then `"yyyy-MM-dd HH:mm:ss"`.
7. **Massive duplication between `SkillPanel`/`ModelPanel` and
   `SkillDialog`/`ModelDialog`:** identical tree building, folder nesting,
   soft-delete/restore wiring, mode handling, save flow. Extract:
   - a generic `FolderTreePanel` (or template) parameterized by DAO +
     dialog, and
   - a common `EntityDialog` base (mode handling, `setMode`, save
     boilerplate, `dupString`/`toDateTime`).
   This removes ~400 lines of copy-paste and halves the surface for
   divergence bugs.
8. **Full-tree reload on every change.** `reload()` clears and rebuilds the
   whole tree, re-runs one query per folder, and loses the current
   selection/scroll position. For moderate data this is fine, but:
   - preserve the selected item (re-select by id after rebuild),
   - consider `list_all`-style single queries for skills/models instead of
     N per-folder queries,
   - skip the rebuild when a dialog was cancelled with no changes.
9. **DB path.** `QCoreApplication::applicationDirPath() + "/acta.db"` writes
   the DB next to the exe — can fail on read-only install dirs. Use
   `QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)` and
   `mkdir` it. Also with `ACTA_DB_OPEN_EXISTING`, first launch with no file
   fails with a cryptic status-bar error; add a **bootstrap/migration path**
   (create + run `schema.sql` on first launch, or at least a
   "Create database…" action).
10. **`DbHandle` / offline state.** On open failure the panels silently show
    empty trees. Keep `DbHandle` as-is, but have `MainWindow` expose a
    `dbAvailable()` flag so panels can show an explicit disabled/offline
    state instead of a silent empty tree.
11. **Application identity.** Set `QCoreApplication::setApplicationName` /
    organization (needed if `QSettings` is added later), and give the window
    an icon (`setWindowIcon`) from the logo asset.
12. **Stale `.ui` window titles.** The `.ui` files carry stale titles
    ("Skill: default name", "Model: default name", "Context", "Execution")
    while code overrides them via `setWindowTitle`. Clean them up so the
    `.ui` and code don't disagree.
13. **"New" target-folder logic.** In `SkillPanel::onNewBtnClicked` (and
    Model), if the user has a *skill/model* selected (not a folder),
    `selectedFolderId()` returns 0 and the new item lands at root, not next
    to its sibling. Better: fall back to the selected item's own folder.
14. **`ExecutionDialog::editExecution` error handling.** Each open builds a
    new `QStandardItemModel` — fine — but the `context_get`,
    `skill_revision_get`, `model_revision_get` queries have no `err` checks
    (only the log list does); failures silently leave blank fields. Add
    `err` handling / `qWarning` consistent with the rest of the codebase.
15. **No input validation beyond emptiness.** `output_schema`,
    `configuration`, and context `content` are JSON-ish payloads
    ("Immutable input JSON…", "constraint the Output Schema… JSON").
    Validate with `QJsonDocument::fromJson` and show a clear "invalid
    JSON" message before saving; at minimum add a "Validate JSON" button.
16. **`main.cpp`.** Consider a global crash guard or at least
    `qInstallMessageHandler` writing to a log file in the app dir, since DB
    errors currently only go to `qWarning`/console.

### Architecture

17. **No folder CRUD anywhere.** The schema has `skill_folders` /
    `model_folders` with soft-delete columns and the trees render folders,
    but there is no way to create, rename, or delete a folder from the UI.
    "New" only targets whatever folder is selected. Add folder buttons
    (New Folder / Delete Folder / Rename) — or remove folders from the
    model.
18. **No execution-creation path.** Executions exist in the DB and are
    browsable, but nothing in the app creates one (see #1). There's no
    "pick context + skill + model → Run" flow. That is the app's core
    value ("LLMs as actions") and it is missing.
19. **Panels emit no signals.** No signal after mutation, so `MainWindow`
    can't react (e.g., selecting a skill could prefill an "execute" form).
    At minimum, emit `itemChanged(int id)` so future features can hook in.
20. **Button-state safety.** After `reload()` the current item is gone and
    buttons are re-enabled through the `currentItemChanged` flow — fine, but
    add an explicit `updateButtonStates()` call after reload for safety.

---

## 2. UI (widgets & layout)

21. **Panel headers look like raw controls.** Bare
    `QLabel("Skill")` / `"Model"` / `"Context"` / `"Execution"`. Use
    `QGroupBox` or styled section headers (bold, small caps) so the four
    quadrants read clearly.
22. **Button rows inconsistent.** Skill/Model have two rows (New/Show/Edit,
    then Delete/Restore) while Context has one (New/Show) and Execution has
    (Run, Show) stacked vertically. Standardize: one toolbar per panel with
    all actions, sensible order, and **icons + tooltips** on every button.
23. **Trees:**
   - Skill/Model trees: `setHeaderHidden(true)` and no folder icon →
     folders and items are visually indistinguishable. Add a folder icon
     (`SP_DirIcon`) and maybe bold text for folders; keep deleted = trash
     icon and additionally grey-out/italicize deleted rows.
   - Context tree: sorting is enabled but no auto-sort is applied after
     reload; call `sortItems(1, Qt::AscendingOrder)` (or descending) so
     newest-first is stable.
   - Execution tree shows only "Date" and "Status" — useless for
     identification. Add columns for skill name, model name, context type
     (fetch the revision names in the panel, as the dialog already does).
   - The log list is a `QTreeWidget` with 4 columns — a
     `QTableWidget`/`QTableView` is the right control; keep
     `setUniformRowHeights`.
24. **Raw date strings everywhere.** Dates are shown as
   `yyyy-MM-dd HH:mm:ss` in list columns and dialogs. Display them
   locale-formatted (`QLocale::system().toString(dt, "yyyy-MM-dd HH:mm")`);
   keep the exact ISO value in tooltips.
25. **No status colors.** Execution `status` (pending/running/completed/
   failed/cancelled) and log `level` (debug/info/warn/error) should be
   color-coded (green/yellow/red text or colored icons). Right now
   everything is grey.
26. **JSON/prompt editors are plain `QTextEdit`.** `prompt`,
   `output_schema`, `configuration`, and context `content` should use a
   monospace font; optionally syntax-highlight JSON or at least show line
   numbers — prompts and schemas are the core content of this app.
27. **Dialog `.ui` cleanup:**
   - `skillDialog.ui`: `nameLineEdit` has literal default text
     `"default name"` — that's placeholder material, not data. Use
     `setPlaceholderText("Name…")` and clear it in `newSkill`.
   - `revisionTreeWidget` column header `"1"` in the `.ui` (overridden in
     code) — set it properly in the `.ui`.
   - `descriptionTextEdit` hard-coded `readOnly=true` in the `.ui` while
     code toggles it — remove the `.ui` default so the `.ui` doesn't lie.
   - Give dialogs sensible `minimumWidth`/`minimumHeight` instead of
     relying on the 609×664 geometry snapshot.
   - The revision tree floats at the top with no caption; give it a label
     ("Revisions of this skill").
28. **MainWindow chrome.** Add a minimal menu bar (File → Exit, Database →
   Reconnect, Help → About) and give the window an icon. Cache the logo
   `QPixmap` and scale for `devicePixelRatio`.
29. **Splitter constraints.** The left widget has `setMaximumWidth(320)`
   *and* the splitter sizes are `{320, 880}` — the user can never widen the
   left pane. Pick one constraint (e.g. max width + `setCollapsible`) and
   let the splitter do the rest.
30. **No theming.** Everything is stock Qt grey. Add a light `QSS`
    stylesheet (accent color, hover states, disabled states, group-box
    headers) to give the product identity; a logo/branding already exists.
31. **Empty states.** Every panel renders a blank tree with zero rows when
    the DB is empty or offline. Add a centered placeholder ("No skills yet —
    click **New**").

---

## 3. UX

32. **Database failure UX.** If the DB can't open, the user gets a 3-second
    status-bar message and a silently useless app. Show a persistent,
    visible state: a banner / disabled panels + a "Retry" action, or a
    modal at startup offering "Create database here" / "Pick another
    location" / "Exit".
33. **No confirmations or feedback for mutations:**
   - Soft-delete and restore fire immediately with no confirmation
     (acceptable since reversible, but add a `QMessageBox::question` for
     delete, and an "Restored" toast).
   - After saving in the dialogs there is **no success feedback** — and
     worse, the dialog silently switches mode (New→Edit, Edit→ReadOnly)
     while staying open. Better: on successful save, show a brief "Saved"
     message and close the dialog (or at least switch to ReadOnly with a
     "Saved as revision N" label so the user understands why editing
     stopped).
34. **"New" target ambiguity.** Clicking **New** with a skill selected
    silently creates the item at root (see #13). Show the target folder in
    the dialog title or a label ("Creating in folder: X / root").
35. **Revision interaction is a trap.** In Edit mode, clicking a row in the
    revision tree overwrites Name/Description/Prompt/etc. with that old
    revision's values — the user can unknowingly clobber in-progress
    edits. Fix: in Edit mode either (a) disable revision switching until
    Save/Cancel, (b) restore live values when leaving the tree, or
    (c) move revision browsing to a separate read-only dialog ("View
    revision history").
36. **Modal-on-modal-on-modal.** Panel → dialog → nested dialog (e.g. the
    Execution dialog's "Show context" opens a dialog inside a dialog).
    Consider an inline detail pane or a non-modal browser; at minimum keep
    nesting depth ≤ 2.
37. **Discovery gaps:**
   - No tooltip explaining what "One-Shot" means or what the statuses
     mean.
   - "Show deleted items" — consider "Show trash" and visually distinct
     deleted rows (see #23).
   - No help/about dialog explaining the model: Skill = prompt + schema,
     Model = backend config, Context = immutable input, Execution = one
     shot.
38. **Search & filter.** No way to find an item once trees grow. Add a
   filter box above each tree (case-insensitive substring), and for
   executions a status filter (All / failed / running…).
39. **Keyboard/accelerator support.** Add accelerators (`&New`,
   `&Delete`), `Delete` key to soft-delete the selection, `F2` to rename,
   `Enter` to open the detail dialog. Right now everything is mouse-only.
40. **Window state persistence.** `resize(1200,700)` on every start; add
   `QMainWindow::saveState`/`restoreState` via `QSettings` (window geometry,
   splitter sizes, "show deleted" flags).
41. **Redundant "Show" affordances.** In `ContextPanel`, single-click
    already fills the editor below; the "Show" button opens a dialog
    showing the same content. In `ExecutionPanel`, the "Show" button on a
    log row opens a dialog duplicating the 4 columns already visible in
    the log list. Either make these the *only* detail view, or make the
    inline view richer and drop the button.
42. **No data lifecycle.** Contexts and executions are append-only with no
    delete/cleanup in the UI — the DB will grow unbounded. Add "Delete"
    (with confirmation) for executions/logs and contexts, or at least an
    "older than N days" prune action.
43. **i18n is half-done.** `MainWindow` uses `tr(...)` in two places,
    everything else is raw English literals, and no `qs_`/translation files
    exist. Either commit to `tr()` everywhere + a `translations/` target, or
    drop it consistently.
44. **Running-execution UX (when Run is implemented).** Status `running`
    should update live (polling or an in-app worker via `QThread` /
    `QtConcurrent`), with a progress indicator in the panel, and the log
    list should auto-scroll to the newest line.
45. **Error messages are opaque.** `acta_db_strerror` messages (e.g.
    constraint violations from the unique-name indexes) go into a generic
    `QMessageBox::warning` ("Could not save skill: …"). Map known errors to
    friendly hints ("A skill with this name already exists in this folder").

---

## Priority order

1. **Must-fix:**
   - #1 dead "Run One-Shot" button (implement or remove)
   - #2, #3 dead `QDialog::Accepted` branches / mislabeled "Edit" menu
   - #4 case-mismatched `modelpanel.cpp` filename
   - #5 duplicate includes
   - #32 database-failure UX
   - #33 save feedback + confirmations
   - #35 revision-selection clobber trap
2. **High:**
   - #17 folder CRUD
   - #18 execution-creation flow (the app's core feature)
   - #7 dedupe panels/dialogs into shared base classes
   - #15 JSON validation
   - #24, #25 date formatting + status colors
   - #37, #38 tooltips + search/filter
3. **Polish:**
   - #27, #29, #30 `.ui` cleanup, splitter constraints, theming
   - #39, #40 keyboard shortcuts + window-state persistence
   - #41, #42 redundant views, data lifecycle
   - #44, #45 live execution UX, friendlier errors
