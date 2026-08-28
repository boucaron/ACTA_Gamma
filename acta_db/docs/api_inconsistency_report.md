# acta_db API Inconsistency Report

Scope: public headers in `acta_db/include/` cross-checked against `acta_db/src/`
implementations and the reference schema (`acta_gamma/db/schema.sql`).

Severity key:
- **[BUG]** wrong behavior / silently wrong error codes
- **[DOC]** header documentation contradicts the implementation
- **[ASMT]** asymmetric treatment of analogous cases between sibling modules
- **[STYLE]** naming / convention drift (low impact)

---

## 1. [STYLE] Minor naming / style drift

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

1. Style drift (#1).

---

## Fixed

- **`acta_db_model_folder_soft_delete` did not protect contained models** —
  added the `SELECT COUNT(*) FROM models WHERE folder_id = ? AND
  deleted_at IS NULL` guard (rejects with `ACTA_DB_ERR_INVALID`, same
  invariant as `acta_db_skill_folder_soft_delete`), header doc updated,
  tests `soft_delete_live_models` / `soft_delete_deleted_models` added.

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
