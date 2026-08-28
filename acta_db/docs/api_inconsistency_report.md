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

## 2. [DOC] `acta_db_strerror` returns "unknown error" for two defined codes

`db.h` promises:
> "Returns 'unknown error' for values outside the defined range."

But `src/db.c:16` has no cases for `ACTA_DB_ERR_DUPLICATE` (-6) or
`ACTA_DB_ERR_FK` (-7) — both *defined* codes fall through to
"unknown error". Add the two cases (e.g. "duplicate name" /
"foreign key violation").

---

## 3. [DOC] Stale/incorrect comments in `db.c` around `acta_db_open`

- `src/db.c:70` comment: "Returns `ACTA_DB_ERR_NOTFOUND` when the file
  did not exist…" — no such constant exists; the code sets
  `ACTA_DB_ERR_INVALID_DB`, and it is not about a missing file, it is
  about an empty/non-SQLite file.
- `db.h` documents `creationMode` only as "If creationMode is set to 0,
  we check if this is a SQLite DB". Any non-zero value is "create mode",
  but no `ACTA_DB_OPEN_EXISTING` / `ACTA_DB_OPEN_CREATE`-style constants
  are defined, so callers pass raw `0`/`1` literals. Define two named
  constants.

---

## 4. [STYLE] Minor naming / style drift

1. `model.c:178` (`acta_db_model_update`) does a redundant
   `sqlite3_bind_int(stmt, 1, m->folder_id);` immediately overwritten by
   `sqlite3_bind_null` / `sqlite3_bind_int` in the if/else — harmless but
   confusing; the skill version doesn't have the redundant line.
2. `execution_query_t` zero-value = "any" works, but the struct mixes
   `0`-sentinel ints with `NULL`-sentinel strings while the doc block
   repeats both rules twice (once in prose, once in comments) — fine, but
   `ACTA_EXEC_QUERY_ANY` is a `#define` of a compound literal while every
   other multi-line convenience is a function; consider a
   `acta_db_execution_query_any()` returning the struct, and note the
   macro produces a statement-level-only expression in some C contexts.
3. `db.h` typedefs `db_t` *after* declaring `acta_db_strerror`; `model.h`
   et al. include only `db.h`, so users who include a single entity
   header get the whole convention block duplicated per header — keep the
   convention block in exactly one place.

---

## Recommended action order

1. **Fix #1** (model_folder_soft_delete missing model guard) - data
   integrity.
2. Code cleanup: #2 (strerror DUPLICATE/FK cases), #3 (stale db.c
   comment + named open-mode constants).
3. Style drift (#4).

---

## Fixed

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
