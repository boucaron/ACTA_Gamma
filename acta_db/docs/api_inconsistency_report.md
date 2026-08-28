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

## 2. [DOC] `limit` semantics: headers say "no limit", implementation clamps

`db.h` (correct, matches code via `db_clamp_limit` in `internal.h:116`):
> `limit ≤ 0 or > ACTA_DB_MAX_PAGE → clamped to ACTA_DB_MAX_PAGE (10000)`

Every entity header contradicts this, claiming `<= 0` returns **all** rows:

| Header | Claim |
|---|---|
| `model.h` (list_in_folder, list_all) | "`<= 0` means no limit (return all matching rows)" |
| `skill.h` (list_in_folder, list_all) | same |
| `model_folder.h` (common pagination block) | same |
| `skill_folder.h` (list_children, list_all) | same |
| `model_revision.h` (list_by_model) | same |
| `skill_revision.h` (list_by_skill) | same |
| `context.h` (query) | "`<= 0` = no cap" |
| `execution.h` (query) | "`<= 0` means no limit (return all)" |
| `execution_log.h` (list_by_execution) | "`<= 0` means no limit (return all)" |

In reality every lister caps a page at 10000 rows regardless of what
the caller passes. "Return all" is only true for tables with ≤ 10000 rows.

**Fix:** reword the per-module docs to the `db.h` clamp contract (or
remove the per-module statements and point at the `db.h` convention block).

---

## 3. [DOC] Restore functions: `ACTA_DB_ERR_NOT_FOUND` missing from docs

All four restore implementations return `ACTA_DB_ERR_NOT_FOUND` when the
id does not exist:

- `acta_db_model_restore` — `model.h` documents only OK / SQL
  ("success including when already live – no-op"). **Missing NOT_FOUND.**
- `acta_db_model_folder_restore` — `model_folder.h` documents only
  OK / INVALID / SQL. **Missing NOT_FOUND.** (src returns it at line 198)
- `acta_db_skill_folder_restore` — `skill_folder.h` *does* document NOT_FOUND ✓
- `acta_db_skill_restore` — `skill.h` *does* document NOT_FOUND ✓

**Fix:** add `ACTA_DB_ERR_NOT_FOUND` to the doc of
`acta_db_model_restore` and `acta_db_model_folder_restore`.

---

## 4. [DOC] `acta_db_model_folder_rename` doc omits `ACTA_DB_ERR_NOT_FOUND`

Implementation (`src/model_folder.c:135`) returns `ACTA_DB_ERR_NOT_FOUND`
when no live folder matches `id`; `model_folder.h` documents only
INVALID / SQL. (`skill_folder_rename` documents it correctly.)

---

## 5. [ASMT] Empty-string validation drift between model and skill folders

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

## 6. [ASMT] `acta_db_model_update` lacks the `id <= 0` guard that `acta_db_skill_update` has

- `src/skill.c:130`: `if (!db || !s || s->id <= 0 || ...) → INVALID`
- `src/model.c:163`:  `if (!db || !m || !m->name || ...)` — **no id check**

Consequences:
- `acta_db_model_update(db, &(model_t){.id = -5, ...})` → `NOT_FOUND`
  instead of `INVALID`.
- The doc of `acta_db_skill_update` explicitly lists `s->id <= 0` under
  `ACTA_DB_ERR_INVALID`; `model_update`'s doc does not.

**Fix:** add `m->id <= 0` to the model guard and doc.

---

## 7. [DOC/BUG] `acta_db_execution_create` bypasses the state machine and misreports constraints

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

## 8. [DOC] `acta_db_execution_log_create` validates `level` but the header doesn't say so

`src/execution_log.c:96`: an unrecognized level string (e.g. `"Warning"`)
is rejected with `ACTA_DB_ERR_INVALID`, and `list_by_execution` /
`count` do the same for a non-empty filter. The header only documents
level as a free-form string with four expected values, and never mentions
the validation or the failure code. Either document it (recommended) or
accept any string and let the schema CHECK handle it.

Related [STYLE]: `ACTA_LOG_LEVEL_SET(lvl, name)` in `execution_log.h` is
an odd macro (it token-pastes `ACTA_LOG_LEVEL_##name` and merely assigns a
string constant to `lvl`); the name suggests a setter. Either remove it
or rename to make the intent obvious. Also note its constants are
`ACTA_LOG_*` while everything else in the API is `ACTA_DB_*` /
`ACTA_EXEC_*`.

---

## 9. [DOC] `acta_db_strerror` returns "unknown error" for two defined codes

`db.h` promises:
> "Returns 'unknown error' for values outside the defined range."

But `src/db.c:16` has no cases for `ACTA_DB_ERR_DUPLICATE` (-6) or
`ACTA_DB_ERR_FK` (-7) — both *defined* codes fall through to
"unknown error". Add the two cases (e.g. "duplicate name" /
"foreign key violation").

---

## 10. [DOC] Stale/incorrect comments in `db.c` around `acta_db_open`

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

## 11. [DOC] `execution.h` state-machine diagram is garbled and the warning is stale

The ASCII diagram in `execution.h` does not match the text below it
(the `cancel()` / `fail()` arrows land in the wrong boxes; `pending`
reaches both `failed` and `cancelled` in the picture but only
`cancelled` in the rules).

Also the ⚠ warning says:
> "The SELECT-then-UPDATE pattern is not atomic… If the handle is ever
> shared across threads, fold the status guard into the UPDATE itself
> (WHERE id = ? AND status = ?)…"

but the UPDATEs **already** carry `AND status = ?`
(`src/execution.c:373,401,426,453`) and already check
`sqlite3_changes()`. The pre-check exists only for a nicer error code.
The warning describes code that no longer exists and misleads readers
about the actual atomicity story. Redraw the diagram and rewrite the note.

---

## 12. [ASMT] Lister empty-result convention stated differently per header

`db.h` and most headers: an empty result set returns a valid pointer
(possibly NULL array) with `*out_count == 0` and `*err == OK`.

`execution_log.h` is the outlier:
> "no rows (not-found) → returns NULL, *err = ACTA_DB_OK."

The implementation in fact returns a valid (empty) array, so the doc
overstates: an empty page is indistinguishable from a real error only by
`*err`, and the pointer is *not* guaranteed to be NULL on empty. Align the
wording with the other headers.

---

## 13. [ASMT] Module surface asymmetries (likely intentional, but undocumented)

| Capability | model | skill | context | execution | execution_log |
|---|---|---|---|---|---|
| create | ✓ | ✓ | ✓ | ✓ | ✓ |
| get / get_live | ✓/✓ | ✓/✓ | ✓ (no live variant — no deleted_at column, OK) | ✓ | ✓ |
| full-row update | ✓ | ✓ | **✗** (schema: immutable via trigger) | ✗ (transitions only) | ✗ |
| soft_delete / restore | ✓ | ✓ | ✗ | ✗ | ✗ |
| rename | (folder) ✓ | (folder) ✓ | n/a | n/a | n/a |
| revision history | ✓ | ✓ | ✗ | ✗ | ✗ |

Nothing is *wrong* here, but:
- `context` immutability is enforced by a `BEFORE UPDATE` trigger that
  RAISE(ABORT), yet the C API has no update call — fine. However the
  trigger makes `sqlite3_exec`-based updates return
  "contexts are immutable" which surfaces only via
  `acta_db_last_error`. Document the immutability in `context.h`.
- `execution` has no `get_live`/soft-delete, and deleting a context or
  execution is impossible through the API because of the
  `ON DELETE RESTRICT` FKs — so an `execution` row is permanent. Worth a
  one-liner in the headers.

---

## 14. [STYLE] Minor naming / style drift

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
3. Headers use mixed comment conventions (doxygen `/**` vs plain `/* */`,
   section banners only in some files) — cosmetic, but the
   "common pagination contract" block that should be the single source of
   truth exists *only* in `model_folder.h`, not in `db.h` where it
   belongs (and where it currently disagrees with the code, see item 2).
4. `db.h` typedefs `db_t` *after* declaring `acta_db_strerror`; `model.h`
   et al. include only `db.h`, so users who include a single entity
   header get the whole convention block duplicated per header — keep the
   convention block in exactly one place.

---

## Recommended action order

1. **Fix #1** (model_folder_soft_delete missing model guard) — data
   integrity.
2. **Fix #7** (execution_create validation / status bypass / FK code).
3. Rewrite the pagination contract once in `db.h` and delete the
   per-module copies (#2, #12, #14.3).
4. Doc-only fixes: #3, #4, #8, #9, #10, #11.
5. Decide and document the model/skill asymmetries (#5, #6, #13).
