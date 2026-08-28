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

## 2. [ASMT] Empty-string validation drift between model and skill folders — **FIXED** (2a9bf6d)

Resolved by aligning the model side to the skill side:
`acta_db_model_folder_create` and `acta_db_model_folder_rename` now reject
empty names (`!*name` / `!*new_name` → `ACTA_DB_ERR_INVALID`), and
`model_folder.h` documents "NULL/empty" like `skill_folder.h` does.
Covers `create_empty_name` / `rename_empty_name` tests in
`tests/test_model_folder.c`.

| Check | model | skill |
|---|---|---|
| create: empty `name` rejected? | **No** (`src/model_folder.c:94` only checks NULL) | Yes (`!*name`, src/skill_folder.c:160) |
| rename: empty `new_name` rejected? | **No** (`src/model_folder.c:118` only checks NULL) | Yes (`src/skill_folder.c:185`) |

The headers agree the implementations should differ
(`model_folder.h`: "if db or name is NULL" vs `skill_folder.h`:
"NULL/empty"), so the code matches the docs — but the two halves of the
API behave differently for identical inputs, and neither header documents
that the model side is *weaker*. Either align the implementations and
docs, or explicitly document the difference.

---

## 3. [ASMT] `acta_db_model_update` lacks the `id <= 0` guard that `acta_db_skill_update` has — **FIXED** (eeb98bc)

Resolved by adding `m->id <= 0` to the `acta_db_model_update` guard
(→ `ACTA_DB_ERR_INVALID`), documenting it in `model.h` like
`skill.h` does, and adding the `test_model_update_invalid_id` test.

- `src/skill.c:130`: `if (!db || !s || s->id <= 0 || ...) → INVALID`
- `src/model.c:163`:  `if (!db || !m || !m->name || ...)` — **no id check**

Consequences:
- `acta_db_model_update(db, &(model_t){.id = -5, ...})` → `NOT_FOUND`
  instead of `INVALID`.
- The doc of `acta_db_skill_update` explicitly lists `s->id <= 0` under
  `ACTA_DB_ERR_INVALID`; `model_update`'s doc does not.

**Fix:** add `m->id <= 0` to the model guard and doc.

---

## 4. [DOC/BUG] `acta_db_execution_create` bypasses the state machine and misreports constraints — **FIXED** (0bfd47c, 327e76b)

Resolved:
- `context_id` / `skill_revision_id` / `model_revision_id` must be > 0
  (else `ACTA_DB_ERR_INVALID`), `prompt` still required.
- `e->status` is ignored at create time — rows are always inserted
  `pending`; other states are only reachable via the transition
  functions.
- `SQLITE_CONSTRAINT` → `ACTA_DB_ERR_FK` (was folded into INVALID).
  Because `acta_db_open` enables extended result codes (see db.c), the
  code to match is the specific extended code
  `SQLITE_CONSTRAINT_FOREIGNKEY`, never the generic
  `SQLITE_CONSTRAINT` — same convention as `skill.c`.
- `execution.h` now documents all required fields and the new error
  mapping. Tests: `test_exec_create_zero_ids`,
  `test_exec_create_ignores_status`, and the FK cases now expect
  `ACTA_DB_ERR_FK`.

Header contract (`execution.h`):
> "Insert a new execution row (status defaults to 'pending').
>  Returns ACTA_DB_ERR_INVALID if db or e is NULL or required fields
>  (context_id, prompt) are missing."

Implementation (`src/execution.c:287`):
1. **`context_id` is not validated** — only `!e->prompt` is. A
   `context_id` of 0 passes the C check and then dies at SQLite as an
   FK violation, which is folded into `ACTA_DB_ERR_INVALID`
   (`if (rc == SQLITE_CONSTRAINT) return ACTA_DB_ERR_INVALID;`) —
   conflating "bad argument" with "referenced row does not exist".
   (The sibling module `model_create` uses `ACTA_DB_ERR_FK` for this case.)
2. **`e->status` is honored verbatim.** The header says status *defaults*
   to pending and that the state machine is enforced in the C layer;
   in practice the caller can insert a row straight into `completed` /
   `failed` / `cancelled`, or any string (the schema CHECK then aborts
   with `SQLITE_CONSTRAINT` → `ACTA_DB_ERR_INVALID`, an undocumented path).
3. **`skill_revision_id` / `model_revision_id` are NOT NULL in the schema**
   but the header does not mention them as required; a zero-initialized
   struct silently produces `ACTA_DB_ERR_INVALID` (FK) with no hint in the
   docs.

**Fix:** validate `e->context_id > 0` (and the two revision ids) up front
→ `ACTA_DB_ERR_INVALID`; ignore or whitelist `e->status` at create time
(force `pending`, or only accept `pending`); return `ACTA_DB_ERR_FK` on
`SQLITE_CONSTRAINT` instead of `INVALID`. Update the header to list all
required fields.

---

## 5. [DOC] `acta_db_strerror` returns "unknown error" for two defined codes

`db.h` promises:
> "Returns 'unknown error' for values outside the defined range."

But `src/db.c:16` has no cases for `ACTA_DB_ERR_DUPLICATE` (-6) or
`ACTA_DB_ERR_FK` (-7) — both *defined* codes fall through to
"unknown error". Add the two cases (e.g. "duplicate name" /
"foreign key violation").

---

## 6. [DOC] Stale/incorrect comments in `db.c` around `acta_db_open`

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

## 7. [STYLE] Minor naming / style drift

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
2. **Fix #4** (execution_create validation / status bypass / FK code) — ✅ done (0bfd47c, 327e76b).
3. Code cleanup: #5 (strerror DUPLICATE/FK cases), #6 (stale db.c
   comment + named open-mode constants).
4. Decide and align the model/skill asymmetries (#2 ✅, #3 ✅).
5. Style drift (#7).
