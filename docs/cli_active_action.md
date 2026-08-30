# Active Actions — from `cli_review.md`

Action plan derived from [`cli_review.md`](cli_review.md), ordered by
priority (highest first). The earlier `db`-surface round closed the
in-scope items; what remains there (F2, F3) is folded into the plan below
(F3 → S2, F2 → S3). F1 landed as P4 (`ed142cc`).

## Top fixes (by severity)

| # | Action | Source | Severity | Notes / dependencies |
|---|--------|--------|----------|----------------------|

## Cheap wins

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|

## Structural (highest long-term payoff)

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| S1 | **Shared CRUD drivers** (atom-first: V1 + V4 now, V2 next round; the def-driven form V3 is excluded — see variants section below). Shared `create`/`get`/`list`/`count`/`update`/`delete`/`restore`/`move` drivers (and a shared `get-latest`-class driver), preceded by the common-atom extraction; per-entity field descriptors only as far as V2 needs them. The copy-paste family (~5.4k lines of mostly-CRUD across 8 files) is where most bugs live; this makes P3–P5 and the P1-class divergences impossible to re-introduce rather than fixing 8 files by hand. Also settle the `get-latest` null-semantics ambiguity: a NULL fetch conflates *parent row missing* with *parent exists but no revisions* — the shared driver (with library support, e.g. `get_latest` reporting parent-missing separately) should distinguish the two so the error message is precise | P4 #10 | Do after P3 or the plan is moot — structural fix supersedes the per-file edits for the patterns it covers |
| S2 | **Test the global parse layer** — raw `argv` through `parse_globals` + `commands_dispatch` (in-process or spawning the binary). Every bug in the P2 class (`--json` blob ignored, `--stdin` eaten, bare `--json` unparseable, dead local `--count`) lives in this untested seam (former F3) | P5 #3 | Orphaned `tests_parse_globals.c` is the seed; the `stest_run_argv` helper (`tests/helpers/test_helpers`) is already the in-process raw-argv path used by the P2 input-source regression cases — a dedicated suite is the missing piece |
| S3 | **Per-action stdout-schema table in the spec + one shared emit helper** — success shapes keep multiplying (`{"id":N}`, `{"id":N,"folder_id":M}`, `{"deleted":true}`, bare `N`, …); `db`'s shapes are already documented in `db_usage()` (former F2). Also settle the root-folder wire representation (`null` in `model_to_json` vs `0` in move success lines) | P3 #3 / P4 #8 | Cross-entity; db row is the template |
| S4 | Resolve the remaining parse-layer inconsistencies: `cmd_args_flag` doc/implementation mismatch (present-vs-absent indistinguishable); `parse_globals` conflates OOM / missing flag value / too-few positionals into one `EXIT_CLI` with the wrong main.c message; bare `--json` (no value) still unparseable after the input-source fix — parser still requires a value (help text no longer documents it) | P1 #1–2 | Small, but the flag API is what W1 and the dead flags keep tripping on |

## S1 variants — atom-first decomposition (decision aid)

S1 as worded above is the strongest (V3) of four variants considered. The
review concern: a def-driven commitment is more definitive than needed —
it fixes every entity file's shape up front, and an unanticipated need
becomes a framework fight. The alternative is to enter S1 from the
bottom: pull up the tiny functions that are re-typed everywhere first,
and only go further if the bottom layer proves out. All variants share
the same atom inventory; they differ in how far past the atoms they go,
and the path V1 → V2 is monotone — nothing done earlier is wasted. V4 is
orthogonal and composes with the rest.

**Decision: V3 is excluded** — S1 will not be executed in the def-driven
form; it is documented below for the comparison only. S1 proceeds as
V1 + V4 now, V2 next round.

### Common atoms (every variant)

Measured repeats across `acta_db_cli/src/commands/`:

| Atom | ~Repeats | Replaces |
|---|---|---|
| `parse_id_positional(ga)` | 46 | id positional parse in get/update/delete/restore/move |
| `parse_nonneg_int_flag(ga, name, &out)` | ~86 | `"must be a non-negative integer"` JSON-error blocks |
| `parse_offset_limit(ga, ...)` | ~10 | list pagination 3-check block |
| `emit_not_found` / `emit_error` / `emit_ok_id` / `emit_deleted` | ~40 | stdout-schema strings (this is S3's data too) |
| `require_flag(ga, name)` (present + non-empty) | ~15 | W2 empty-name checks |
| `load_row_or_notfound(…, id)` | ~20 | fetch + NULL check + emit (where the P3 not-found fix had to land 8 times) |

### V1 — Atoms only

Just the six helpers, in `cli_util.*`. Entity files keep their structure
and their `if (strcmp(action, …))` chains; each site becomes a one-liner
calling an atom. No `entity_def`, no dispatch, no field table. A new flag
or verb on entity X is a change to X's file only — isolation is trivial
because there is no shared state to violate. Divergence is *reduced*, not
made impossible: the atom set is opt-in, and the control-flow skeleton
(the per-verb block shape) is still hand-written per file.

### V2 — Atoms + per-verb kernels

Same atoms, plus each generic verb is one helper taking the entity's I/O
as callbacks (`crud_get(ga, "model", model_fetch, model_emit)`,
`crud_list(...)`, `crud_delete_or_404(...)`, …). Entity files still write
each verb out explicitly, but each block is ~3 lines of wiring instead of
~40. A divergent get/list can no longer be hand-rolled, because the
kernel is the only path — yet there is no central registry, and an entity
can opt out of one kernel for one weird case by writing the block by hand
(V4 is what then catches the drift). New flag = new field/filter in that
entity's wiring; new verb = a plain function in that file.

### V3 — Def-driven dispatcher (S1 as worded) — **EXCLUDED**

Not selected; documented for the comparison only.

`entity_def_t` + field table (name, JSON key, flag, type, required,
root-vs-null) + custom-verb table with same-name shadowing over the
generic verbs, dispatched in `crud_run`. Maximum structural guarantee:
divergence is impossible by construction, and the per-entity file shrinks
to data (field table) + genuinely specific verbs. Maximum upfront cost:
it decides every entity file's shape up front, has low reversibility once
8 files depend on it, and forces the open semantic decisions (get-latest
null semantics, stdout shapes) to be settled before it can land.

### V4 — Contract test suite (orthogonal, composes with V1–V3)

One parameterized suite run once *per entity*: a table of cases
(create-missing-required → exit code + error line; get-missing →
`EXIT_NOT_FOUND`; list pagination boundaries; bad id; …) applied to each
entity via the existing `stest_run_argv` in-process raw-argv helper, with
applicability derived from each entity's action table (revision entities
are auto-skipped on create/delete cases). It makes the "no impact on other
cases" constraint a CI assertion: touching `model.c` runs the *same* cases
against `skill`, `execution`, etc., so any shared-helper change that shifts
another entity's observable surface fails. It is the only variant that is
free immediately, works against today's pre-refactor code, and turns every
migration step into "move code, keep the suite green". It produces no
production code and only encodes rules already decided — the open S3
schema decisions must be settled into the cases first.

### Compare

| | V1 atoms | V2 atoms+kernels | V3 def-dispatch | V4 contracts |
|---|---|---|---|---|
| Copy-paste removed | ~60% | ~80% | ~90% | 0 (test layer) |
| New flag in X, Y untouched | trivially yes | trivially yes | yes, via framework data | verified in CI |
| New verb in X, Y untouched | trivially yes | trivially yes | yes (custom table) | verified in CI |
| Divergence made *impossible* | no — reduced | mostly (per verb), manual escape hatch | yes, structurally | no |
| Divergence made *visible* | no | no | no | yes — **all variants need it** |
| Upfront cost / risk | days, per-file incremental | ~a week, per-entity migration | big-bang, framework risk | low |
| Reversibility | fully | fully | low once 8 files depend on it | fully |
| Unanticipated need | write code normally | opt out of one kernel | fight the framework | encode a case |

### Risks

- **V1:** atom set is opt-in — without V4 nothing stops a file from
  reverting to hand-rolled parsing; "under-atomization" (each file adopts
  a slightly different atom subset) is the main failure mode; the
  per-verb control-flow skeleton remains duplicated, so P1-class
  if-chain divergences stay reachable.
- **V2:** the kernel API is itself a design commitment that may get
  reworked mid-migration (migrate one entity, then discover the wrong
  callback shape — the earlier entities must be re-touched); the hand-rolled
  escape hatch weakens the guarantee exactly where it is exercised (the
  weird cases); kernel migration ordering matters (start with the simplest
  verbs, shake out the API before the complex entities).
- **V3:** big-bang commitment — the framework half-built under 8 files is
  the worst outcome; low reversibility; any unanticipated need becomes a
  framework change touching the shared path of every entity; the open
  semantic decisions (get-latest NULL conflation, `null`-vs-`0` root
  folder, stdout shapes) are blocking prerequisites, not side items; the
  def data tables can ossify into a second source of truth that drifts
  from the actual DB schema.
- **V4:** can only encode decided rules — every open decision (S3 wire
  representation, get-latest semantics) blocks the corresponding cases;
  detection, not prevention: it fails after the drift ships to the code,
  not before; the per-entity descriptor (name, valid create-args blob,
  verb list) is itself data that must be kept in sync with the action
  tables.
- **Shared (all variants):** the semantics-arbitration pass is unavoidable
  and is the real cost — wherever the 8 files disagree, someone must decide
  which behavior is correct; no variant removes that, V3 just moves it
  earlier.

### Modularity · compositionality · reuse · maintenance · extensibility

| Axis | V1 atoms | V2 kernels | V3 def-dispatch | V4 contracts |
|---|---|---|---|---|
| **Modularity** (unit ↔ responsibility) | atoms are exactly the re-typed units; one helper = one responsibility, trivially unit-testable | kernels add one level: verb = atom composition + entity I/O callbacks; boundary is the callback signature | entity file = data + custom verbs; driver = mechanism; clean mechanism/policy split, but the def struct is a wide object (field table + move + latest + hooks + verbs) that accretes | test module ↔ contract cases; per-entity applicability from action tables keeps it decoupled |
| **Compositionality** (do parts combine predictably?) | atoms compose freely inside any file; nothing hidden, nothing ordered | kernel-then-atoms: a kernel is a fixed atom composition, so an entity composes kernels without re-deriving atom order; shadowing-free | def fields compose by precedence rule (custom > generic > error) — predictable, but the precedence is global and must stay global | cases compose by table; adding a case composes with every entity with zero cross-talk |
| **Reuse** (what else benefits) | every future entity reuses the atoms from line one; also the future S3 emit helper lives *in* the atoms | kernels reuse across the 8 entities now, and a new entity reuses ~all verbs for free; atoms still fully reused | the def table is the reuse vehicle — maximal, but the driver is the only reuse boundary: anything not expressible in the def is not reusable at all | contract cases reused by any variant and by every future entity; the existing 66 per-entity test files partially subsume it (specific-verb layer stays) |
| **Maintenance** (cost to keep it true) | low: helpers are small and stable; cost lives in the 8 files as before, minus the atom lines | moderate: kernel API is the maintenance surface — its evolution is the only cross-entity edit; per-entity wiring stays local | highest shared surface: one driver edit changes every entity (needs full-suite review — which is the point, but it means the shared code must be treated as frozen or everything churns) | suite maintenance ≈ case table edits; must track schema decisions (S3) or it decays into a false green |
| **Extensibility** (new flag / new verb / new entity, X-only impact) | flag: one line in X; verb: one function in X; entity: new file reusing atoms — all local, nothing shared to violate | flag: X's wiring; verb: X's function; entity: wiring over existing kernels — local, with the manual opt-out as the one non-local smell | flag: field-table row (framework data, local in effect but framework-shaped); verb: custom table; entity: a def — all isolated, and this is where V3 is strongest: a new entity cannot be written wrong | any extension is immediately covered: new flag → new case row; new entity → whole contract suite applies from day one |

**Net (decision):** V3 excluded. V1 + V4 now, V2 next round. The atom
inventory is identical across variants, so every step is a prefix of the
next; V4 is the only piece that *enforces* the isolation constraint
rather than merely making it easy to keep.

## Summary

- **Next:** S1 (shared CRUD drivers, atom-first: V1 + V4 now, V2 next round; V3 excluded).
- **Finally:** S1–S4 structural work; S1 should land before any of P3/W1
  patterns are re-touched per-file (or those per-file fixes become throwaway).
- Done so far: P3 `get` not-found → `EXIT_NOT_FOUND` + JSON error (`e6c86fe`),
  plus `get-latest` follow-up (`f7d2ef5`), W1 dead local `--count` reads in
  all `list` actions removed + test ga-injections reworked to `g.count = 1`
  (`99d17c1`),
  W6 shared test helpers moved to `tests/helpers/test_helpers.*` (`696d4df`),
  P1 `skill update` data loss (`928c66f`), P2 input-source
  resolution (`e58d956`), P4 entity DB failures emit the stderr JSON error
  line (`ed142cc`), W2 empty-name checks in `model update` /
  `model_folder rename` (`341de4b`), W3 `exec create` rejects dead
  `--status` (`cffe8fe`), W4 removed `json_serialize_*` stubs +
  `json_print_table` (`2e34bbc`), W5 stale `json.h` comment
  (`9f19a6a`, done during the P2 round), W8 `$(EXEEXT)` on all CLI
  executable targets + root `.gitignore` `*.o`/`*.exe`/`*.a`
  (`5959ea1`), W7 minimal scope: `../acta_db/libacta_db.a` rebuild rule as
  a prerequisite of all link targets + `test` runs every suite and reports
  (`787b518`; auto-derived test targets and `LDFLAGS`→`LDLIBS` skipped as
  POC hygiene); round 3's db-surface scope is closed,
  F2/F3 are carried as S3 / S2.
