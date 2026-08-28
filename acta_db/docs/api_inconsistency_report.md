# acta_db API Inconsistency Report

Scope: public headers in `acta_db/include/` cross-checked against `acta_db/src/`
implementations and the reference schema (`acta_gamma/db/schema.sql`).

Severity key:
- **[BUG]** wrong behavior / silently wrong error codes
- **[DOC]** header documentation contradicts the implementation
- **[ASMT]** asymmetric treatment of analogous cases between sibling modules
- **[STYLE]** naming / convention drift (low impact)

---

## 1. [BUG] `acta_db_model_folder_soft_delete` does not protect contained models

`src/skill_folder.c:294` (`acta_db_skill_folder_soft_delete`) rejects the
delete if the folder has **live child sub-folders** *or* **live skills**.

`src/model_folder.c:138` (`acta_db_model_folder_soft_delete`) checks only
**child sub-folders**. A folder whose `models.folder_id` it is soft-deleted,
leaving live models pointing at a deleted folder.

The headers claim the two share the same invariant
(`model_folder.h`: "Invariant shared with acta_db_skill_folder_soft_delete")
— for skill folders that invariant is "no live children *of any kind*",
for model folders it is "no live child folders". Downstream,
`acta_db_model_move_to_folder` will then refuse to move those orphaned models
into any soft-deleted folder, and `list_in_folder` will still show them.

**Fix:** add the same
`SELECT COUNT(*) FROM models WHERE folder_id = ? AND deleted_at IS NULL`
guard to `acta_db_model_folder_soft_delete` (and decide whether the
soft-delete should reject or auto-reparent).

---

## Recommended action order

1. **Fix #1** (model_folder_soft_delete missing model guard) - data
   integrity.

---

## Fixed

- **Style drift** — redundant `folder_id` bind removed in
  `acta_db_model_update`; `ACTA_EXEC_QUERY_ANY` macro replaced by
  `acta_db_execution_query_any()` inline function (all call sites
  updated); `db_t` typedef moved above `acta_db_strerror` (f24cd61).
- **`acta_db_strerror` returned "unknown error" for defined codes** —
  added `ACTA_DB_ERR_DUPLICATE` ("duplicate name") and
  `ACTA_DB_ERR_FK` ("foreign key violation") cases + test (e4e0bc3).
- **Stale `acta_db_open` comment / unnamed open modes** — comment
  rewritten in `db.c`; `ACTA_DB_OPEN_EXISTING` / `ACTA_DB_OPEN_CREATE`
  constants added in `db.h`; all in-repo call sites now use the named
  constants (e4e0bc3).
- **Empty-string validation drift between model and skill folders** —
  model folder create/rename now reject empty names; docs aligned
  (2a9bf6d).
- **`acta_db_model_update` missing `id <= 0` guard** — added guard,
  doc, and test, matching `acta_db_skill_update` (eeb98bc).
- **`acta_db_execution_create` state-machine bypass / constraint
  misreporting** — ids validated up front, `e->status` ignored (always
  `pending`), FK failures return `ACTA_DB_ERR_FK`. Note: the handle runs
  with extended result codes (see db.c), so the code to match is
  `SQLITE_CONSTRAINT_FOREIGNKEY`, never the generic
  `SQLITE_CONSTRAINT` — same convention as `skill.c` (0bfd47c, 327e76b).
