# Session notes — backend dialects, implementation progress

Handoff document. Date: 2026-10-12 (continuation of
[`docs/session_strata_backend.md`](session_strata_backend.md)).
Branch state: **nothing committed** — the whole working tree is uncommitted
(10 portability/memory fixes from the previous session + Phases 1–3 of
[`docs/plans/backend_dialects.md`](plans/backend_dialects.md)).

Read first: [`docs/plans/backend_dialects.md`](plans/backend_dialects.md)
(§2 dialect matrix, §3.2 where the dialect comes from, **§9 = the four
decisions, now answered**), then this file, then
[`docs/session_strata_backend.md`](session_strata_backend.md) §4 "Known
gotchas".

---

## 1. Decisions (answered 2026-10-12, round 2 — written into plan §9)

| # | Topic | Decision |
|---|---|---|
| 1 | migration vs new names | **No `0.2.sql`.** Vocabulary is `llamacpp` / `openai`, adapted **in situ** (no clients, no deployed DBs). `llama` is NOT an alias. Schema stays at `0.1`. |
| 2 | validate `backend` in `acta_cli` | **Yes** — `model create/update` rejects anything outside `llamacpp`/`openai` (`EXIT_INVALID`); the runner re-validates at run time. |
| 3 | `top_k` in openai mode | **Omit** (`sends_top_k = 0`). |
| 4 | catalog audit in openai mode | **Skip** (`fetch_catalog = 0`): no `GET /`, `"catalog": null`. No `/props` probe. |
| + | envelope `json_schema.name` | fixed constant **`acta_output`** (replay-deterministic) |
| + | `max_context_of()` signature | extra `const char **source` out-parameter (so the Phase-1 "which field produced the ceiling" label survives) |

---

## 2. DONE and verified on BOTH platforms

### Phase 1 — `max_context` fallback (land alone)

* `acta_runner/src/backend.c`: the `/health` body is parsed before it is
  released; the ceiling is resolved with fallbacks and the source is reported.
* `acta_runner/src/backend.h`: `backend_preflight_t` gains
  `char max_context_source[24]` (`"entry.max_context" | "entry.meta.n_ctx" |
  "health.max_context" | "none"`).
* `acta_runner/src/run.c`: `preflight_passed` metadata gains
  `"max_context_source"`; the pre-claim VLOG prints
  `max_context=… from …`.
* `acta_runner/src/check.c`: one extra `-v` line
  `cmd_check: max_context=N (source: …)`. **stdout verdict JSON unchanged.**
* `acta_runner/tests/stub_server.{h,c}`: two new opt-in fields
  `models_meta_n_ctx`, `health_max_context` (0 = field omitted → every
  pre-existing scenario is byte-identical).
* Tests: `tests/check/test_check.c` scenario 10 (4-case chain + source labels
  + `meta.n_ctx`-only value reaching the verdict line);
  `tests/run/test_run.c` scenario 18 + one metadata check.
* `acta_runner/Makefile`: `test_check.o` rule lists `src/backend.h` as a
  prerequisite (test_check.c now includes `backend.h`).

Live proof (Strata, was `max_context:0`):

```
$ acta_runner check --base-url http://127.0.0.1:8080 --model-identifier qwen3.8-flash-next-iq3_xxs -v
[v1] cmd_check: ok (base_url=http://127.0.0.1:8080, model=qwen3.8-flash-next-iq3_xxs)
[v1] cmd_check: max_context=204800 (source: entry.meta.n_ctx)
{"ok":true,"model":"qwen3.8-flash-next-iq3_xxs","max_context":204800,"base_url":"http://127.0.0.1:8080"}
```

### Phase 2 — dialect module (no behavior change)

New: `acta_runner/src/dialect.h`, `acta_runner/src/dialect.c`,
`acta_runner/tests/dialect/test_dialect.c` (83 checks), Makefile wiring
(`DIA_SRCS/OBJS/TARGET/LDLIBS`, added to `test:` and `clean:`).

| | llamacpp | openai |
|---|---|---|
| `fetch_catalog` | 1 | 0 |
| `sends_top_k` | 1 | 0 |
| `response_format` | `{"type":"json_schema","schema":{…}}` | `{"type":"json_schema","json_schema":{"name":"acta_output","strict":true,"schema":{…}}}` |
| `max_context_of` order | `entry.max_context` → `entry.meta.n_ctx` → `health.max_context` | `entry.meta.n_ctx` → `entry.max_context` → `health.max_context` |

`dialect_from_string()` is exact-match only; `dialect_get(DIALECT_UNKNOWN)==NULL`;
`dialect_name(DIALECT_UNKNOWN)=="(unknown)"`; `dialect_expected_names()==
"llamacpp or openai"`.

### Test counts after Phase 1 + Phase 2 (all green)

| | before | after |
|---|---|---|
| Windows (MinGW, 12 suites) | 415 checks / 0 fail | **487 / 0** |
| WSL `insightcae` (12 suites) | 446 / 0 | **509 / 0** |

### ASAN finding (WSL) — fixed

Phase 1 introduced `const char *max_ctx_src = pf.max_context_source` while
`pf` is block-scoped → `stack-use-after-scope` in `cJSON_AddStringToObject`
(`run.c`). Fixed: `char max_ctx_src[24] = "none";` declared outside the block,
filled with `snprintf`. ASAN now clean for `test_run` / `test_check` /
`test_dialect`.

Remaining ASAN findings are **library-internal, not ACTA code**: libcurl global
init state (13688 B / 32 allocs — `backend.c` calls `curl_global_init` once and
never `curl_global_cleanup`) and the known 16-byte sqlite leak on `db init`.

---

## 3. Phase 3 — **written, built and green on both platforms** (no new
> run-pipeline asserts yet — that is the only Phase-3 work left)

Everything below is in the working tree, compiles with no new warnings, and all
12 suites pass on Windows and in WSL (counts in the table below).

### `acta_runner/src/backend.h`
* `#include "dialect.h"`.
* New prototype (6 args):
  `int backend_preflight(const char *base_url, const char *model_id, const dialect_t *dialect, const char *api_key, int timeout_sec, backend_preflight_t *out);`
* Docs: the two GETs are dialect-independent; the catalog `GET /` is NOT part
  of the preflight (it belongs to the run pipeline, gated on `fetch_catalog`);
  callers must resolve the dialect first.

### `acta_runner/src/backend.c`
* Removed the Phase-1 helpers `json_positive_int()` / `resolve_max_context()`
  (the extraction now lives in `dialect.c`).
* `cJSON *jh` (parsed `/health` body) is kept alive until the ceiling is
  resolved and `cJSON_Delete(jh)` is called on **every** early return
  (`/v1/models` transport failure, non-200, unparseable, model-not-served,
  success). ⚠ memory-sensitive: re-run ASAN.
* `const char *source = NULL; long ceiling = dialect->max_context_of(match, jh, &source);`
* Defensive: `if (!dialect) dialect = dialect_get(DIALECT_LLAMACPP);`

### `acta_runner/include/runner_util.h`
* `#include "dialect.h"`.
* `run_execution(...)` unchanged + new
  `int run_execution_dialect(db_t*, int exec_id, int timeout_sec, long max_chars, const char *api_key, dialect_id_t dialect_override);`
  (`DIALECT_UNKNOWN` = no override; the GUI keeps calling `run_execution`, so
  no GUI drift and no Qt rebuild needed for this signature).

### `acta_runner/src/run.c`
* `run_execution()` is now a wrapper → `run_execution_dialect(..., DIALECT_UNKNOWN)`.
* After the model-revision validation block: dialect resolution
  (`row_backend`, `row_did`, `use_did`, `dialect`, `dialect_source` =
  `"model_revision"` or `"cli-override"`), with
  `FAIL(EXIT_INVALID, "model revision %d has unsupported backend '%s' (expected %s)", …)`
  **before any HTTP call**; on override≠row: a `warn`-level
  `execution_log` event `dialect_override` (metadata `dialect`,
  `model_backend`, `reason`) **and** a `[warn] --dialect X overrides model
  revision N backend 'Y'` line on stderr.
* `backend_preflight(...)` now passes `dialect` (both call sites: pipeline and
  `run_preflight_before_claim`, which resolves the dialect itself and exits
  `EXIT_INVALID` with the same message, no claim, no DB writes).
* Catalog block wrapped in `if (dialect->fetch_catalog) { … }` (the `CANCEL()`
  check moved inside); when skipped, metadata records `"catalog": null` as
  before. `preflight_passed` metadata also gains `"dialect"`.
* `if (top_k > 0 && dialect->sends_top_k)`.
* `response_format` built by `dialect->build_response_format(schema_j)`.
* `llm_request` metadata gains `"dialect"`, `"dialect_source"`,
  `"model_backend"`.

### `acta_runner/src/check.c`
* Copies the row's `backend` (`strdup`, freed on every return; `strdup` only
  when non-NULL).
* Standalone mode → `dialect_get(DIALECT_LLAMACPP)` (today's behavior;
  `--dialect` arrives in Phase 4). DB mode → `dialect_get(dialect_from_string(backend))`,
  and on unknown: `emit_runner_error(EXIT_INVALID, "model %s has unsupported backend '%s' (expected llamacpp or openai)")`
  using the positional id string.
* `backend_preflight(base, mid, dialect, NULL, timeout_sec, &pf)`.

### `acta_runner/Makefile`
* **`CFLAGS += -Iinclude -I../acta_db/include -Isrc`** (global `-Isrc` added so
  `runner_util.h` can include `src/dialect.h`; `src/` holds only `backend.h`
  and `dialect.h`, no name collisions with `include/`). The per-rule `-Isrc`
  hacks were removed; `tests/dialect/test_dialect.o` keeps an explicit rule
  with a `src/dialect.h` prerequisite.
* `src/dialect.c` added to every test source list (`TEST_SRCS`, `PEND_SRCS`,
  `AUTO_SRCS`, `DEL_SRCS`, `KEY_SRCS`, `CONF_SRCS`, `SHAD_SRCS`, `CHK_SRCS`)
  and to `SMOKE_SRCS`.

### Tests adapted in situ
* 11 runner suites: `m.backend = "llama"` → `m.backend = "llamacpp"`
  (`test_run`, `test_pending`, `test_autopreflight`, `test_sweep`,
  `test_deleted`, `test_api_key`, `test_conf`, `test_shadow`,
  `test_deathmark`, `test_deadrunner`, `tests/check/test_check`).
* `tests/check/test_check.c`: `seed_model()` → thin wrapper over new
  `seed_model_backend(db, base_url, model_id, backend)`; the Phase-1 direct
  `backend_preflight()` call now passes `dialect_get(DIALECT_LLAMACPP)`;
  **new scenario 11**: model with `backend "vllm"` → `EXIT_INVALID`,
  `stub_server_health_requests()==0`, `stub_server_models_requests()==0`,
  zero chat requests, no execution rows.

### Test counts after Phase 3 (all green, both platforms)

| Suite | Windows | WSL |
|---|---|---|
| argparse | 47 | 47 |
| run | 116 | 116 |
| pending | 29 | 29 |
| autopreflight | 37 | 37 |
| sweep | 43 | 43 |
| deleted | 12 | 12 |
| api_key | 16 | 19 |
| conf | 36 | 50 |
| shadow | 35 | 49 |
| deathmark | 20 | 20 |
| check | **58** | **58** |
| dialect | 83 | 83 |
| **total** | **492 / 0 fail** | **514 / 0 fail** |

(Phase 1+2 baseline was 487 / 509; the +5 in `check` is scenario 11.)

ASAN (WSL, `make clean` first): `test_run`, `test_check`, `test_autopreflight`,
`test_pending`, `test_dialect` — **no use-after-free / use-after-scope / leak in
ACTA code**; only the known libcurl global-init leak (13688 B / 32 allocs,
unchanged from before Phase 3). The `jh` (parsed `/health` body) lifetime change
is clean.

### Still to do for Phase 3

> **SUPERSEDED — all four items are DONE, see §8.1.** Kept for the record.
1. Add run-pipeline asserts in `tests/run/test_run.c` (or a small new block):
   `llm_request` metadata contains `"dialect":"llamacpp"`,
   `"dialect_source":"model_revision"`, `"model_backend":"llamacpp"`; and
   `preflight_passed` contains `"dialect":"llamacpp"`.
2. A run-pipeline scenario with a model whose `backend` is unsupported (e.g.
   `"vllm"`): expect `EXIT_INVALID`, row `failed`, error message
   `model revision N has unsupported backend 'vllm' (expected llamacpp or openai)`,
   and **zero** stub requests (`stub_server_health_requests()==0`) — the
   pre-claim auto preflight must abort before the claim. Needs a
   `seed(..., backend)` variant in `tests/run/test_run.c` (its `seed()`
   hard-codes `m.backend = "llamacpp"`).
3. Update `acta_runner/tests/README.md` (new `tests/dialect/` suite, the new
   `stub_config_t` fields, scenarios 10/11 of `test_check`).
4. Show the Phase 3 diff (`git diff HEAD --ignore-cr-at-eol`) and get approval.
   **No commit without approval.**

---

## 4. REMAINING PHASES

> **SUPERSEDED for Phases 3 and 4 (done — §8.1/§8.2); Phase 5 is half done
> and acta_cli currently does not build — read §8.4 before touching
> anything.**

* **Phase 4 — `--dialect` CLI surface.** `argparse.c`: add `{ "dialect", 1 }`
  to `runner_flag_specs[]`. `cmd_run`: parse + validate the value
  (`EXIT_INVALID`, message via `dialect_expected_names()`), pass it as
  `run_execution_dialect(..., did)`; **reject `run --pending --dialect`**
  (`EXIT_INVALID`, no claim, no DB writes — check it before
  `run_preflight_before_claim`). `cmd_check`: `--dialect` accepted **only** in
  standalone mode (with `--base-url` + `--model-identifier`), default
  `llamacpp`; `check <model-record-id> --dialect X` → `EXIT_INVALID`. Update
  `run_usage()`, `check_usage()`, `main.c` `help_print()` (run flags + check
  flags), `tests/argparse/test_argparse.c` (unknown-flag strictness now accepts
  `--dialect`), and `docs/cli_spec.md` later.
* **Phase 5 — openai envelope end-to-end + CLI validation.** The envelope is
  already implemented in `dialect.c`; what remains: `acta_cli`
  `model create/update` validation of `--backend` against
  `llamacpp`/`openai` (`acta_cli/src/commands/model.c`, message listing the
  vocabulary; update `acta_cli/src/tools.c` `--tools` schema/`jk_model_req`
  wording), and adapt `acta_cli/tests/model/model_test_create.c`
  (`"vllm"` ×12), `model_test_update.c` (`"openai"`, `"together"`) and
  `model_test_misc.c` (`"vllm"`) to the canonical vocabulary. Then a real
  skill-with-`output_schema` run against live Strata (today: HTTP 400
  `response_format.json_schema needs a schema object`).
* **Phase 6 — tests.** `tests/stub_server.c`: add a dialect parameter to
  `stub_config_t` (reject the wrong envelope on `POST /v1/chat/completions`
  with HTTP 400 `response_format.json_schema needs a schema object`, serve
  `meta.n_ctx`), add `tests/run/test_openai_dialect.c` (model seeded
  `backend:"openai"` + `output_schema` skill → completes; llamacpp envelope
  rejected; `top_k` omitted; no `GET /` call — assert a new
  `stub_server_catalog_requests()` counter), keep every existing check green,
  and fix the pre-existing warning `tests/stub_server.c:139 implicit
  declaration of strncasecmp` (add `#include <strings.h>` in the POSIX branch).
  Verify live with `scripts/probe_strata.sh` (optionally `--dialect openai`).
* **Phase 7 — docs.** WSL section in `docs/building.md`; new
  `docs/openai_dialect_contract.md` (or `docs/strata_server_contract.md`) +
  `docs/llamacpp_server_contract.md` cross-links; `docs/runner_contract.md`
  dialect decision record + `dialect`/`dialect_source` metadata; `README.md`
  backend wording + a Strata example (`"backend": "openai"`) and the
  `llamacpp` example; `docs/cli_spec.md` vocabulary + the
  `acta_runner check <model-record-id>` clarification; the
  "`output_schema` must be a JSON **string**" clarification (issue C,
  `acta_cli/src/json.c` `JF_STR`); `docs/DBDesign.md` backend vocabulary.
* **GUI (plan Phase 7).** `acta_gui` compiles `run.c`/`backend.c` directly and
  calls `run_execution(...)` — the signature is unchanged, but it now also
  needs `src/dialect.c` in its source list, and the model editor should show
  the new vocabulary. Qt 6 is not installed in `insightcae`, so this cannot be
  compiled here; flag it.
* **Commit approvals pending.** Suggested split (plan §10): (a) the 10
  portability/memory fixes, (b) Phase 1, (c) Phase 2, (d) Phase 3, … Also
  decide whether `scripts/` and `docs/plans/` are committed, and what to do
  with the untracked `acta_runner/MYTEST/` notes directory.

---

## 5. Verification commands (helper scripts used this session)

`/c/msys64/tmp/winrunner.sh` and `/c/msys64/tmp/wslrunner.sh` + WSL-side
`~/acta_suites.sh` are ephemeral — recreate them if missing.

Windows (note: make targets have **no `.exe` suffix** because `EXEEXT` is empty
in this mingw32-make; the produced binaries do get `.exe`; run the suites from
`acta_runner/` because they load `../acta_db/schema.sql` relative to cwd):

```bash
export TMP='C:/msys64/tmp' TEMP='C:/msys64/tmp' PATH=/c/msys64/mingw64/bin:$PATH
cd /c/msys64/home/admin/github/boucaron/ACTA_Gamma
mingw32-make all && mingw32-make -C acta_runner \
  tests/argparse/test_argparse tests/run/test_run tests/run/test_pending \
  tests/run/test_autopreflight tests/run/test_sweep tests/run/test_deleted \
  tests/run/test_api_key tests/run/test_conf tests/run/test_shadow \
  tests/run/test_deathmark tests/check/test_check tests/dialect/test_dialect
cd acta_runner && for s in tests/argparse/test_argparse tests/run/test_run \
  tests/run/test_pending tests/run/test_autopreflight tests/run/test_sweep \
  tests/run/test_deleted tests/run/test_api_key tests/run/test_conf \
  tests/run/test_shadow tests/run/test_deathmark tests/check/test_check \
  tests/dialect/test_dialect; do ./$s | tail -2; done
```

WSL (sync from MSYS2 bash; run suites one by one — a single long run gets
killed by the tool timeout):

```bash
MSYS2_ARG_CONV_EXCL='*' wsl.exe -d insightcae -e bash -lc \
 'rsync -a --include="acta_test_ref.db" --exclude=".tmp/" --exclude=".tmp_build/" \
    --exclude="build/" --exclude="*.o" --exclude="*.exe" --exclude="*.db" \
    --exclude="*.log" --exclude="out" --exclude="acta_db_out" --exclude="*.a" \
    --exclude="*.so" --exclude="test/" --exclude="tmp/" --exclude="MYTEST/" \
    /mnt/c/msys64/home/admin/github/boucaron/ACTA_Gamma/ ~/ACTA_Gamma/ && \
  cd ~/ACTA_Gamma && make all && make -C acta_runner <test targets…}'
MSYS2_ARG_CONV_EXCL='*' wsl.exe -d insightcae -e bash -lc '~/acta_suites.sh'
```

ASAN (WSL) — **always `make clean` first** (stale non-PIC/non-ASAN objects get
relinked and the link then fails with `collect2: error: ld returned 1 exit status`):

```bash
wsl.exe -d insightcae -e bash -lc 'cd ~/ACTA_Gamma && make -C acta_db clean && \
  make -C acta_runner clean && make -C acta_db CC="cc -fsanitize=address -O0 -g" libacta_db.a && \
  make -C acta_runner CC="cc -fsanitize=address -O0 -g" tests/run/test_run tests/check/test_check tests/dialect/test_dialect && \
  cd acta_runner && for s in tests/run/test_run tests/check/test_check tests/dialect/test_dialect; do ./$s | grep -E "AddressSanitizer|checks,"; done'
```

Live Strata: `http://127.0.0.1:8080` (Windows) / `http://172.28.160.1:8080`
(WSL, NAT), model id `qwen3.8-flash-next-iq3_xxs`; `scripts/probe_strata.sh`
runs from either side. Strata was live this session (`/health` →
`max_context 204800`, `service strata`).

---

## 6. New gotchas found this session (add to the gotchas table)

| Gotcha | Workaround |
|---|---|
| `git diff` (index↔worktree) shows **whole-file** diffs for e.g. `tests/run/test_conf.c` (1284 lines) when the real change is 1 line — the index holds CRLF while the blobs are LF (`core.autocrlf=true`) | review with `git diff HEAD --ignore-cr-at-eol` (or `git add --renormalize .` before committing); `git diff --ignore-cr-at-eol --numstat` gives the true counts |
| `sed -i` on MSYS2 rewrites whole files and can flip line endings | prefer the `edit` tool; check with `awk '/\r$/{n++} END{print n}' file` |
| `make -C acta_runner tests/…/test_run.exe` → "No rule to make target" | `EXEEXT` is empty here: pass the target **without** `.exe` |
| Runner suites must run with cwd = `acta_runner/` | they load `../acta_db/schema.sql` relative to cwd; from `tests/run/` they abort with `cannot load schema: PRAGMA journal_mode=WAL did not take effect…` |
| `test_check.c` includes `backend.h` (in `src/`) | Makefile now has a global `-Isrc` |
| ASAN + changed CFLAGS | `make clean` in `acta_db` **and** `acta_runner` first, else link fails (`collect2: error: ld returned 1 exit status`, suites then "MISSING"); rebuild plain afterwards so the tree is not left with ASAN objects |
| Editing a test file with `acta_db_close(db);` as the last line of `oldText` **deletes that call** (happened twice in `tests/check/test_check.c`; the second time ASAN reported it as a 243 KB / 788-allocation sqlite leak because the `db_t` was never closed) | never anchor on `acta_db_close(db);` — re-emit it in `newText`, and re-run LeakSanitizer after touching a suite's tail |
| libcurl global-init "leak" under LeakSanitizer | pre-existing, inside libcurl (`curl_global_init` without `curl_global_cleanup`); not ACTA code — ignore or `ASAN_OPTIONS=detect_leaks=0` |
| `tests/stub_server.c:139` implicit `strncasecmp` on glibc | pre-existing warning; fix with `#include <strings.h>` in the POSIX branch (Phase 6) |

---

## 7. Resume prompt for the next session

> Read `docs/plans/backend_dialects.md` (§2 matrix, §3.2, **§9 decisions**),
> `docs/session_strata_backend.md` (issues A–J + "Known gotchas") and
> `docs/session_dialect_progress.md` (this file) in ACTA Gamma
> (`C:/msys64/home/admin/github/boucaron/ACTA_Gamma`).
>
> State: **Phases 1, 2 and 3 of the dialect plan are implemented and green on
> both platforms** (Windows 492 checks / 0 fail, WSL 514 / 0 fail; ASAN clean —
> only the known libcurl global-init leak), Phase 1 verified live against Strata
> (`max_context=204800` from `entry.meta.n_ctx`). The only Phase-3 leftovers are
> the new run-pipeline asserts (`llm_request` metadata `dialect` /
> `dialect_source` / `model_backend`, and an unsupported-`backend` run scenario
> that must exit 4 with zero stub requests), plus `acta_runner/tests/README.md`.
> Start there, then continue with Phase 4.
>
> Then Phase 4 (`--dialect`: argparse + `run <id>` override + reject
> `run --pending --dialect` + `check` standalone only + usage/help text),
> Phase 5 (`acta_cli` `--backend` validation per decision 2, `--tools` schema,
> adapt `acta_cli/tests/model/model_test_{create,update,misc}.c` which use
> `vllm`/`openai`/`together`, then a real `output_schema` run against live
> Strata), Phase 6 (`stub_server` dialect parameter rejecting the wrong
> envelope + `meta.n_ctx`, new `tests/run/test_openai_dialect.c`, fix the
> `strncasecmp` warning, keep every existing check green, verify with
> `scripts/probe_strata.sh`), Phase 7 (docs: WSL section in `docs/building.md`,
> the OpenAI/Strata dialect contract doc, README backend wording + Strata
> example, `acta_runner check <model-record-id>` clarification,
> `output_schema`-must-be-a-JSON-string clarification, `docs/runner_contract.md`
> dialect decisions).
>
> Decisions are settled (plan §9): no migration, vocabulary `llamacpp`/`openai`
> adapted in situ, `llama` is not an alias, validate `backend` in `acta_cli`,
> omit `top_k` and skip the catalog call in openai mode, envelope schema name
> `acta_output`. Do not reopen them.
>
> Show the diff per phase (`git diff HEAD --ignore-cr-at-eol`) and **commit only
> after I approve** — nothing is committed yet, including the 10 portability/
> memory fixes from the earlier session.

---

## 8. SESSION 3 (2026-10-12, later) — Phase-3 leftovers DONE, Phase 4 DONE,
## Phase 5 IN FLIGHT

**Nothing is committed.** §3 and §4 above are superseded by this section for
Phases 3 and 4.

### 8.1 Phase 3 leftovers — DONE

* `acta_runner/tests/run/test_run.c`: `seed()` → thin wrapper over new
  `seed_backend(db, ctx, skr, mkr, output_schema, model_config, ctx_content,
  backend)` (backend stored verbatim, so a test can seed `"vllm"`).
  Scenario 1 gained 4 asserts: `llm_request` metadata `"dialect":"llamacpp"`,
  `"dialect_source":"model_revision"`, `"model_backend":"llamacpp"` and
  `preflight_passed` `"dialect":"llamacpp"`.
  **New scenario 19**: `seed_backend(..., "vllm")` + stub server UP →
  `run_execution` returns `EXIT_INVALID`, row `failed`, exact message
  `model revision N has unsupported backend 'vllm' (expected llamacpp or
  openai)`, `stub_server_health_requests()==0`,
  `stub_server_models_requests()==0`, `stub_server_chat_requests()==0`,
  no `preflight_passed`, no `llm_request`, one `execution_failed`.
* `acta_runner/tests/run/test_autopreflight.c`: **new scenario 7**
  (`run --pending` with a `vllm` model revision) → exit 4, message on
  **stderr** (`emit_runner_error` writes to stderr, stdout stays empty),
  rows stay `pending`, `no_log_rows`, zero stub requests **including
  /health**. Harness additions: `cap_t` gained `int target`, new
  `cap_begin_fd()/cap_begin_err()`, `cap_end()` restores `c->target`, new
  `cmd_run_capture_both()` (stdout+stderr), new `log_has_metadata()`.
* `acta_runner/tests/README.md` rewritten (all 12 suites, the
  `stub_config_t` field table incl. `max_context` / `models_meta_n_ctx` /
  `health_max_context`, the request counters, "run from `acta_runner/`,
  make targets take no `.exe`").

### 8.2 Phase 4 — `--dialect` CLI surface — DONE

* `src/argparse.c`: `{ "dialect", 1 }` added to `runner_flag_specs[]`
  (value-taking, so `run 1 --dialect openai` never eats the id; the table
  only drives unknown-flag rejection + positional skipping — value
  validation lives in `cmd_run`/`cmd_check`).
* `src/run.c` `cmd_run`: after `--max`/`--timeout`, before the config read,
  the claim and any HTTP call — missing value → `--dialect requires a
  value`; value outside the vocabulary → `invalid --dialect 'x' (expected
  llamacpp or openai)`; combined with `--pending` → `--dialect cannot be
  combined with --pending: … (use run <id> --dialect)`; all `EXIT_INVALID`
  + `run_usage(stderr)`. The single-id path now calls
  `run_execution_dialect(..., dialect_override)`; the `--pending` branch
  keeps `run_execution` (override always `DIALECT_UNKNOWN`). `run_usage()`
  and the file header (steps 3/4) document the dialect gating.
* `src/check.c` `cmd_check`: `--dialect` validated **after** the
  standalone/positional conflict checks and **before** the model-row
  lookup: bad value → `invalid --dialect 'x' (expected …)`; present without
  `--base-url`+`--model-identifier` → `--dialect is only valid with
  --base-url and --model-identifier: check <model-record-id> takes the
  dialect from the model row`. Standalone dialect =
  `cli_dialect != DIALECT_UNKNOWN ? cli_dialect : DIALECT_LLAMACPP`
  (VLOG level 2 says `--dialect` or `default`). `check_usage()` + file
  header updated.
* `src/main.c` `help_print`: `--dialect` documented under **run flags** and
  **check flags**.
* Tests added: `tests/argparse/test_argparse.c` +9 (known flag, inline
  value, `run <id> --dialect X` and `run --dialect X <id>` positional
  handling, `--dialect` does not eat a following flag);
  `tests/run/test_run.c` +18 — **20** override ≠ row via
  `run_execution_dialect(..., DIALECT_OPENAI)` (completed, `llm_request`
  `dialect=openai`/`dialect_source=cli-override`/`model_backend=llamacpp`,
  `dialect_override` warn event, `preflight_passed dialect=openai`),
  **21** override == row (no warn event, source still `cli-override`),
  **22** row registered `openai` with no flag (`model_revision`, ceiling
  read from `meta.n_ctx`); `tests/run/test_autopreflight.c` +21 — **8**
  `run --pending --dialect openai` → 4 / rows pending / no log rows / zero
  requests, **9** `run <id> --dialect vllm` → 4 / row pending / zero
  requests, **10** `run <id> --dialect openai` → 0 / completed /
  cli-override metadata + warn event; `tests/check/test_check.c` +19 —
  **12** standalone `--dialect openai` (exit 0, `max_context:204800` from
  `meta.n_ctx` although the entry `max_context` is 162000 → the dialect
  really changes the ceiling order; exactly 1 `/health` + 1 `/v1/models`),
  **13** standalone `--dialect vllm` → 4 + vocabulary on stderr + zero
  requests + empty stdout, **14** `check <id> --dialect openai` → 4 +
  "only valid with --base-url" + zero requests, **15** DB mode on an
  `openai` model → 0, ceiling from `meta.n_ctx`. Harness: `cap_t` gained
  `target`, `cap_begin_fd()/cap_begin_err()`,
  `run_check_capture_both()` (stderr capture), `run_check_capture()` is now
  a wrapper.

### 8.3 Suite counts after Phase 4 (all green, both platforms)

| Suite | Windows | WSL |
|---|---|---|
| argparse | 56 | 56 |
| run | 148 | 148 |
| pending | 29 | 29 |
| autopreflight | 68 | 68 |
| sweep | 43 | 43 |
| deleted | 12 | 12 |
| api_key | 16 | 19 |
| conf | 36 | 50 |
| shadow | 35 | 49 |
| deathmark | 20 | 20 |
| check | 77 | 77 |
| dialect | 83 | 83 |
| **total** | **623 / 0 fail** | **654 / 0 fail** |

(Phase-3 leftovers end: Windows 556 / WSL 587. The "492 / 514" totals in
§3 were an arithmetic slip — that table sums to 532 / 563.)

ASAN (WSL, `make clean` in `acta_db` **and** `acta_runner` first, then
rebuild plain): `test_run`, `test_check`, `test_autopreflight`,
`test_dialect` — no use-after-free / use-after-scope, every leak stack is
inside `libcurl.so` (`Direct/Indirect leak … calloc … libcurl.so`),
13688 B / 32 allocs (13735 / 34 for `test_autopreflight`). **New gotcha:**
under LeakSanitizer the final `N checks, 0 failure(s)` line is NOT flushed
— run with `ASAN_OPTIONS=detect_leaks=0` to see the counts.

Per-phase diffs: snapshot of the post-Phase-3 tree in
`/c/msys64/tmp/actsnap/phase3/` (rsync, no build artifacts) — compare with
`diff -u /c/msys64/tmp/actsnap/phase3/<file> <file>`. Phase 4 touched
exactly: `src/argparse.c`, `src/run.c`, `src/check.c`, `src/main.c`,
`tests/argparse/test_argparse.c`, `tests/run/test_run.c`,
`tests/run/test_autopreflight.c`, `tests/check/test_check.c`,
`tests/README.md`. Helper scripts: `/c/msys64/tmp/winsuites.sh` (build via
`winsuites.sh build <targets>`, run via `winsuites.sh`), WSL
`~/acta_suites.sh [suites…]`.

### 8.4 Phase 5 — IN FLIGHT, acta_cli DOES NOT BUILD RIGHT NOW

Done in `acta_cli/src/commands/model.c`:
* `#include "dialect.h"` + new static helper `backend_rejected(backend,
  buf, bufsz)` built on `dialect_from_string()` /
  `dialect_expected_names()` → message
  `invalid backend '<value>' (expected llamacpp or openai)`.
* `model create`: checked right after the `missing required field:
  backend` check → `emit_error(emsg)` + `usage_create(stderr)` +
  `EXIT_INVALID` (`goto cleanup_create`).
* `model update`: checked after the "at least one field" check, **before**
  the row fetch → `EXIT_INVALID`.
* Usage text updated in all four blocks (`model_usage` create/update +
  `usage_create`/`usage_update`): `--backend <llamacpp|openai>  Backend
  dialect (closed vocabulary)` + a short llamacpp/openai explanation, and
  the create example is now a local llama-server
  (`--backend llamacpp --model_identifier llama-3-8b --base_url
  http://127.0.0.1:8080`) instead of `gpt-4o`/`api.openai.com`.

**BLOCKER to fix first:** `acta_cli/Makefile` has no `-I../acta_runner/src`
and does not compile `../acta_runner/src/dialect.c`, so
`mingw32-make -C acta_cli` fails with
`src/commands/model.c:5:10: fatal error: dialect.h: No such file or
directory`. Planned wiring (decision 2 says the vocabulary is
single-sourced from `dialect_expected_names()`, hence compile-not-duplicate;
flag this to the owner — the project's existing pattern for shared code is
the byte-identical `acta_dbpath.c` duplicated in both src trees):

```make
CFLAGS    += -I../acta_runner/src          # for dialect.h only
DIALECT_OBJ := src/dialect.o               # acta_cli-local object
APP_OBJS_NO_MAIN := $(filter-out src/main.o,$(OBJS)) $(DIALECT_OBJ)
src/dialect.o: ../acta_runner/src/dialect.c ../acta_runner/src/dialect.h
	$(CC) $(CFLAGS) -c ../acta_runner/src/dialect.c -o src/dialect.o
```
plus `$(DIALECT_OBJ)` in `clean`. (`-I../acta_runner/src` is safe: that dir
holds only `backend.h` and `dialect.h`, and `-Iinclude` comes first.)

Still to do in Phase 5:
1. `acta_cli/src/tools.c`: state the vocabulary in the `model.create` /
   `model.update` entry descriptions (the `tool_entry_t.description`
   strings; `jk_model_req` stays `{name, backend, model_identifier}`).
   `tests/tools/tools_test_main.c` builds each command with dummy value
   `"x"` for every required flag and asserts `rc != EXIT_CLI` — a handler
   `EXIT_INVALID` is fine, but re-run the suite to confirm.
2. Adapt the acta_cli model tests to the canonical vocabulary: `model_test_create.c`
   (`"vllm"` ×12 flags + the `"backend":"vllm"` JSON blobs),
   `model_test_update.c` (`"openai"` is legal, `"together"` must become a
   negative case or `openai`), `model_test_misc.c` (`"vllm"` ×2),
   `model_test_move.c` (seed `"vllm"`), `model_test_helpers.c`
   (`mtest_seed_model` inserts `backend` via raw SQL — DB layer does not
   validate, so seeded values may stay; prefer canonical anyway),
   `tests/model_revision/*` seeds `"vllm"` via SQL too. Add negative cases:
   `model create --backend vllm` → `EXIT_INVALID` with the vocabulary in
   the message, same for `update`, and via `--json`.
3. Run the **acta_cli** suites (15 suites, cwd **must be `acta_cli/`**, needs
   the fixture `acta_cli/acta_test_ref.db`) on both platforms.
   Pre-existing warning there: `cc1.exe: warning: tests/acta_test_ref.db:
   not a directory` — `TEST_DIRS := $(wildcard tests/*/)` matches the ref
   DB file and emits `-Itests/acta_test_ref.db`.
4. A real skill-with-`output_schema` run against **live Strata** (issue A):
   `scripts/probe_strata.sh`, model id `qwen3.8-flash-next-iq3_xxs`,
   `http://127.0.0.1:8080` (Windows) / `http://172.28.160.1:8080` (WSL),
   model registered `"backend": "openai"`, skill `output_schema` passed as
   a JSON **string** (issue C).

### 8.5 Open items / flags for the owner

* **Pre-existing bug found, NOT fixed (needs approval):**
  `acta_runner/include/runner_util.h` `emit_runner_error()` prints
  `{""error\":\"ACTA_RUNNER_ERROR\"…` → the line is
  `{error":"ACTA_RUNNER_ERROR",…}`, missing the opening quote, while
  `docs/runner_contract.md:120` documents `{"error":…}`. One-character fix
  (`"{"` → `"error…`), no test asserts the malformed form. Out of the
  dialect plan's scope.
* GUI (plan Phase 7): `acta_gui` compiles `run.c`/`backend.c`/`argparse.c`/
  `deathmark.c` directly → it now also needs `src/dialect.c`; Qt 6 is not
  installed in `insightcae`, so it cannot be compiled here.
* Phase 6 (stub_server dialect parameter + envelope rejection +
  `stub_server_catalog_requests()` counter + `tests/run/test_openai_dialect.c`
  + fix `tests/stub_server.c:139` implicit `strncasecmp` — add
  `#include <strings.h>` in the POSIX branch) and Phase 7 (docs) are
  untouched.
* Commit split + what to do with `scripts/`, `docs/plans/`,
  `acta_runner/MYTEST/` still undecided.

### 8.6 Resume prompt for the next session

> Read `docs/session_dialect_progress.md` (§8 first — it supersedes §3/§4),
> `docs/plans/backend_dialects.md` (§2 matrix, §3.2, **§9 decisions**) and
> `docs/session_strata_backend.md` §4 in ACTA Gamma
> (`C:/msys64/home/admin/github/boucaron/ACTA_Gamma`).
>
> State: **Phases 1–4 are implemented and green on both platforms** (Windows
> 623 checks / 0 fail, WSL 654 / 0 fail; ASAN clean — only the known libcurl
> global-init leak). Nothing is committed. **Phase 5 is half done and
> `acta_cli` currently does not build**: `acta_cli/src/commands/model.c`
> already validates `--backend` against the dialect vocabulary
> (`backend_rejected()` + create/update checks + the four usage blocks
> reworded), but `acta_cli/Makefile` still lacks `-I../acta_runner/src` and
> the rule that compiles `../acta_runner/src/dialect.c` into `src/dialect.o`
> (see §8.4 for the exact wiring and the duplicate-vs-compile trade-off to
> confirm with me first).
>
> Finish Phase 5 (Makefile wiring → build + acta_cli suites on both
> platforms, cwd `acta_cli/`, fixture `acta_test_ref.db`; `tools.c`
> descriptions; adapt `acta_cli/tests/model/model_test_{create,update,misc,
> move,helpers}.c` off `vllm`/`together`; add the negative
> `--backend vllm` cases; then a real `output_schema` run against live
> Strata). Then Phase 6 (`stub_server` dialect parameter rejecting the wrong
> envelope + `meta.n_ctx` + a `stub_server_catalog_requests()` counter, new
> `tests/run/test_openai_dialect.c`, fix the `strncasecmp` warning, keep
> every existing check green, verify with `scripts/probe_strata.sh`),
> Phase 7 (docs: WSL section in `docs/building.md`, the OpenAI/Strata dialect
> contract doc, README backend wording + Strata example, `acta_runner check
> <model-record-id>` clarification, `output_schema`-must-be-a-JSON-string
> clarification, `docs/runner_contract.md` dialect decisions +
> `dialect`/`dialect_source` metadata, `docs/cli_spec.md` vocabulary +
> `--dialect`, `docs/DBDesign.md`).
>
> Decisions are settled (plan §9): no migration, vocabulary
> `llamacpp`/`openai` adapted in situ, `llama` is not an alias, validate
> `backend` in `acta_cli`, omit `top_k` and skip the catalog call in openai
> mode, envelope schema name `acta_output`. Do not reopen them.
>
> Verify after each phase on both platforms (Windows: `mingw32-make`,
> `TMP='C:/msys64/tmp' TEMP='C:/msys64/tmp'`, `PATH=/c/msys64/mingw64/bin:$PATH`,
> targets **without** `.exe`, suites run from `acta_runner/`; WSL: the rsync
> command in §5, `make all`, build the targets, then `~/acta_suites.sh` one
> batch at a time; ASAN only after `make clean` in `acta_db` **and**
> `acta_runner`, and rebuild plain afterwards). Show the diff per phase with
> `diff -u /c/msys64/tmp/actsnap/phase3/<file> <file>` for Phase 4/5 work
> (re-snapshot after each approved phase) or `git diff HEAD
> --ignore-cr-at-eol` overall, and **commit only after I approve**.

---

## 9. SESSION 4 (2026-10-12, later) — Phase 5 DONE + verified, Phase 6 IN FLIGHT

**Still nothing committed.** Option A (compile-not-duplicate) was chosen by the
owner for the `acta_cli` ↔ `dialect.c` wiring. Phase 5 is finished and green on
both platforms; Phase 6 is half written and **not yet compiled or run**.

### 9.1 Phase 5 — DONE, verified on both platforms

Files changed (diff baseline: `/c/msys64/tmp/actsnap/phase4/`):
`acta_cli/Makefile`, `acta_cli/acta_test_ref.sql` (+ regenerated
`acta_cli/acta_test_ref.db`), `acta_cli/src/tools.c`,
`acta_cli/tests/model/model_test_{create,update,misc,move,helpers.h}.c/.h`,
`acta_cli/tests/model_revision/model_revision_test_get.c`.

1. **`acta_cli/Makefile` (option A)**: `CFLAGS += -I../acta_runner/src`,
   `DIALECT_OBJ := src/dialect.o`, `APP_OBJS_NO_MAIN := … $(DIALECT_OBJ)`,
   explicit rule `$(DIALECT_OBJ): ../acta_runner/src/dialect.c
   ../acta_runner/src/dialect.h`, **plus the two corrections §8.4 was missing**:
   `$(TARGET): $(OBJS) $(DIALECT_OBJ)` (otherwise `acta_cli` itself does not
   link) and `../acta_runner/src/dialect.h` added to `HDRS`; `$(DIALECT_OBJ)`
   in `clean`.
2. **Extra fix (owner to veto or keep)**: `TEST_DIRS := $(filter-out
   tests/helpers/, $(filter %/, $(sort $(wildcard tests/*/))))`. Before this,
   `$(wildcard tests/*/)` also matched the `tests/acta_test_ref.db` **file**,
   which produced the known `-Itests/acta_test_ref.db` warning *and* a bogus
   `test_acta_test_ref.db` target that cannot link (`undefined reference to
   WinMain`) → **`make -C acta_cli test` was broken (rc≠0) at HEAD**. Now 17
   suites build and run with `make test` rc=0 on both platforms and the warning
   is gone.
3. **`tools.c`**: `model.create` / `model.update` descriptions state the closed
   vocabulary + exit 4. `jk_model_req` unchanged.
4. **Vocabulary adapted**: `model_test_create.c` 14 × `vllm`→`llamacpp`,
   `model_test_misc.c` 2, `model_test_move.c` 1,
   `model_revision_test_get.c` 2 (it pins fixture revision 4),
   `model_test_update.c` `together`→`openai` (+ `base_url`/`model_identifier`
   reworded), `model_test_helpers.h` doc note.
5. **New cases** — create ×6: `--backend vllm` → 4 + stderr
   `invalid backend 'vllm' (expected llamacpp or openai)` + empty stdout;
   `llama` rejected (not an alias); `OpenAI` rejected (case-sensitive);
   `openai` accepted; `--json …"backend":"together"` rejected;
   `--stdin …"backend":"ollama"` rejected. update ×3: `vllm` and `llama`
   rejected **with `count_revisions` unchanged** (validation runs before the row
   fetch), `openai` accepted and creates exactly one new revision (model 3,
   seeded `llamacpp`).
6. **Fixture normalized** (decision 1): `acta_test_ref.sql` models 2–6 →
   `llamacpp`, 7–8 → `openai`; revisions 4–10 to match.
   **NEW GOTCHA:** `UPDATE`ing `acta_test_ref.db` fires `models_update_revision`
   → 7 spurious revision rows + `sqlite_sequence` bump → 5 assertions failed
   (`count_revisions(ctx,5)==2` got 3, `model_revision_test_count` …). Correct
   procedure: edit the `.sql`, then **regenerate** the `.db`
   (`rm acta_test_ref.db` + `python3 -c "sqlite3.connect(...).executescript(open('acta_test_ref.sql').read())"`).
   Verified: the regenerated `.db` equals HEAD's except the 14 backend values.
   ⚠ `acta_test_ref.db` is **git-ignored** (`*.db`) while `acta_test_ref.sql` is
   tracked → anyone with a pre-change local `.db` will see failures (KI-6 set the
   same precedent). Owner decision pending: keep / un-ignore the fixture /
   generate it from the `.sql` in the Makefile.
7. **Live Strata `output_schema` run — BOTH sides** (acceptance criterion 2):
   model registered `"backend":"openai"`, skill `output_schema` as a JSON
   **string**, `scripts/wsl_strata_demo.sh` (WSL → `http://172.28.160.1:8080`)
   and the same flow inline on Windows (`http://127.0.0.1:8080`, needs
   `OPENAI_API_KEY=''` exported — unset → the api_key gate exits 4).
   Execution **completed** with `{"label":"positive","confidence":0.95}`;
   `preflight_passed` = `{max_context:204800, max_context_source:
   "entry.meta.n_ctx", dialect:"openai", catalog:null}`;
   `llm_request` = `{response_format:true, dialect:"openai",
   dialect_source:"model_revision", model_backend:"openai"}`.

**Phase-5 verification (all green):**

| | Windows (MinGW) | WSL `insightcae` |
|---|---|---|
| `acta_cli` suites (`make test`, cwd `acta_cli/`) | **17/17 PASS**, rc 0 | **17/17 PASS**, rc 0 |
| `acta_runner` 12 suites | **623 checks / 0 fail** | **654 / 0** |

ASAN (WSL, after `make clean` in `acta_db`+`acta_cli`+`acta_runner`, then plain
rebuild — the tree is left plain and re-verified 654/0):
`test_model test_json test_tools test_exec test_db test_gparse` +
`test_dialect test_check` — **0 AddressSanitizer errors**.
**New gotcha:** with LeakSanitizer *enabled by default* `test_model` **hangs**
(`timeout` 90 → rc 124, the final `PASS` line never flushed). Workaround:
`ASAN_OPTIONS=detect_leaks=0` for error detection, or
`ASAN_OPTIONS=detect_leaks=1,log_path=/tmp/leakN` (that completes; report goes
to the file). Leak report of `test_model`: **68886 B / 517 allocations, 509
leaks, all in the test harness** (`targs_new` tests/helpers/test_helpers.c:48,
`gopts_default` :133 — 66 + 62 + 3 + 2 + 1 sites); **0 leaks mention any new
Phase-5 test** → pre-existing harness leak, not product code, not fixed.
**New gotcha:** `make -C acta_db clean` + `make -C acta_db libacta_db.a` does
**not** regenerate `include/schema_sql.h` / `migrations_sql.h` (only `all` does)
→ the next `acta_cli` compile dies with `fatal error: migrations_sql.h: No such
file or directory`. Workaround: `make -C acta_db include/schema_sql.h
include/migrations_sql.h`.

### 9.2 Phase 6 — WRITTEN, NOT BUILT, NOT RUN

Done in `acta_runner/tests/stub_server.h` + `.c`:
* `stub_config_t` gains `const char *dialect;` (`"llamacpp"|"openai"`, NULL =
  inspect nothing → every pre-existing scenario byte-identical).
* `#include <strings.h>` in the POSIX branch → the pre-existing
  `tests/stub_server.c:139 implicit declaration of strncasecmp` warning is gone
  (verify on WSL).
* POST bodies are now captured in `g_body` (16 KiB **tail-preserving** sliding
  window — `response_format` is the LAST key of the runner's body, so the tail
  must survive); only POST bodies land there, so it holds the last chat request
  across the pipeline's GETs.
* `envelope_ok(dialect, body)` + `envelope_error(dialect)`; the chat handler
  answers **400** before generation when the envelope is the other dialect's:
  openai → `response_format.json_schema needs a schema object` (Strata's real
  message), llamacpp → `response_format.schema is required for type
  json_schema` (stub-invented mirror). Detection is textual: only the OpenAI
  envelope has a `json_schema` **key** (`"json_schema":{`); `cJSON_PrintUnformatted`
  guarantees no spaces.
* `stub_server_catalog_requests()` (GET / counter, reset in `stub_server_start`)
  and `stub_server_last_chat_body()` (NULL before any POST).

Written but **unverified**: `acta_runner/tests/run/test_openai_dialect.c`
(6 scenarios: 1 openai e2e + envelope/`acta_output` body asserts + 0 catalog
calls + `meta.n_ctx` ceiling + dialect metadata; 2 OpenAI envelope refused by a
llama.cpp stub → `EXIT_HTTP` 12; 3 llama.cpp envelope refused by an openai stub
→ issue A reproduced; 4 `top_k` not sent on openai; 5 `top_k` sent + 1 catalog
call on llamacpp; 6 `supports_response_format:false` → no envelope, post-hoc
validation). Port **8923** (8917/8918/8919/8921 are taken).

**⚠ Three known-wrong things in that new file — fix before building:**
1. `main()` uses `acta_db_open_memory()`, which does not exist. Copy test_run.c:
   `db_t *db = acta_db_open(":memory:", &err, ACTA_DB_OPEN_CREATE);` +
   `acta_db_strerror(err)`.
2. `main()` must set the runner globals like test_run.c does, or `run_execution`
   VLOGs dereference a NULL `runner_gopts`:
   `static const global_opts_t gopts = { NULL, 1, 0, 0, 0, NULL }; runner_gopts = &gopts;`
   (needs `#include "argparse.h"`).
3. `log_has_event()` in that file is a hack (`contains(event,"") ||
   !contains(event,"\x01")`) — replace with the plain implementation from
   `tests/run/test_run.c` (list rows, `strcmp(event)`, free list).

**Still to do for Phase 6:**
1. `acta_runner/Makefile`: `OAI_SRCS := tests/run/test_openai_dialect.c
tests/stub_server.c src/run.c src/backend.c src/dialect.c src/argparse.c
src/deathmark.c`, `OAI_OBJS`, `OAI_TARGET :=
tests/run/test_openai_dialect$(EXEEXT)`, the link rule
`$(OAI_TARGET): $(OAI_OBJS) $(ACTA_DB_LIB)` + `$(TEST_LDLIBS)`, the compile rule
`tests/run/test_openai_dialect.o: tests/run/test_openai_dialect.c
tests/stub_server.h` with `-Itests`, and add `$(OAI_TARGET)` to `test:` +
`$(OAI_OBJS) $(OAI_TARGET)` to `clean:`.
2. Build + run the **13** runner suites on both platforms; every pre-existing
   check must stay green (Windows 623 + new, WSL 654 + new).
3. `acta_runner/tests/README.md`: the new suite, `stub_config_t.dialect`,
   `stub_server_catalog_requests()`, `stub_server_last_chat_body()`, the two
   envelope-refusal messages.
4. Live verify with `scripts/probe_strata.sh` (optionally `--dialect openai`).
5. ASAN (WSL): `make clean` in `acta_db` **and** `acta_runner`, rebuild with
   `CC="cc -fsanitize=address -O0 -g"`, run `test_openai_dialect` +
   `test_run` + `test_check` with `ASAN_OPTIONS=detect_leaks=0` (hang workaround
   above), then **rebuild plain** and re-verify.
6. Show the Phase-6 diff: `diff -u /c/msys64/tmp/actsnap/phase5/<file> <file>`
   (snapshot exists: `acta_cli`, `acta_db`, `acta_runner`, `docs`, `scripts`,
   `README.md`, no build artifacts).

### 9.3 Phase 7 — NOT STARTED

Docs only: WSL section in `docs/building.md`; new `docs/openai_dialect_contract.md`
(or `docs/strata_server_contract.md`) + cross-links in
`docs/llamacpp_server_contract.md`; `docs/runner_contract.md` dialect decision
record + `dialect`/`dialect_source`/`model_backend` metadata + the
`--dialect` override semantics; `README.md` backend wording + one `llamacpp` and
one `openai`/Strata example (the README currently registers a local llama-server
as `"backend": "openai"` — must become `llamacpp`); `docs/cli_spec.md` vocabulary
+ `--dialect` + the `check <model-record-id>` clarification; the
`output_schema` must-be-a-JSON-**string** clarification (issue C,
`acta_cli/src/json.c` `JF_STR`); `docs/DBDesign.md` backend vocabulary; check
`docs/examples/acta.db` for non-canonical backends.

### 9.4 Open items / flags for the owner (unchanged + new)

* **New:** the `TEST_DIRS` fix and the fixture normalization are outside the
  written plan — keep or veto (see 9.1 items 2 and 6).
* **New:** pre-existing `acta_cli` test-harness leak (68886 B / 517 allocs, all
  `targs_new`/`gopts_*` in `tests/helpers/test_helpers.c`) — not fixed.
* **New:** `make -C acta_db libacta_db.a` does not regenerate the embedded
  schema/migration headers after `clean` — not fixed.
* Pre-existing, not fixed: `emit_runner_error()` prints `{error":"…` (missing
  opening quote) in `acta_runner/include/runner_util.h`, while
  `docs/runner_contract.md:120` documents `{"error":…`.
* GUI (plan Phase 7): `acta_gui/src/src.pro` must add `../../acta_runner/src/dialect.c`
  to `SOURCES` (+ `-I../../acta_runner/src` for `dialect.h`); Qt 6 is not
  installed in `insightcae`, so it cannot be compiled here.
* Commit split + what to do with `scripts/`, `docs/plans/`, `acta_runner/MYTEST/`
  still undecided. Nothing committed.

### 9.5 Resume prompt for the next session

> Read `docs/session_dialect_progress.md` (**§9 first**, then §8; §3/§4 are
> superseded), `docs/plans/backend_dialects.md` (§2 matrix, §3.2, **§9
> decisions**) and `docs/session_strata_backend.md` §4 in ACTA Gamma
> (`C:/msys64/home/admin/github/boucaron/ACTA_Gamma`).
>
> State: **Phases 1–5 are implemented and green on both platforms** (Windows
> acta_runner 623/0 + acta_cli 17/17 PASS; WSL 654/0 + 17/17 PASS; ASAN clean,
> only the known libcurl global-init leak and a pre-existing acta_cli
> test-harness leak). Nothing is committed. Phase 5 used **option A**
> (`acta_cli` compiles `../acta_runner/src/dialect.c` into `src/dialect.o`).
> **Phase 6 is half done and unbuilt**: `acta_runner/tests/stub_server.{h,c}`
> already have `stub_config_t.dialect`, the 400 envelope rejection, the
> tail-preserving body capture, `stub_server_catalog_requests()`,
> `stub_server_last_chat_body()` and the `strings.h` fix; the new
> `acta_runner/tests/run/test_openai_dialect.c` exists but has **three known
> defects** (nonexistent `acta_db_open_memory()`, missing `runner_gopts`
> initialization, a hacky `log_has_event`) listed in §9.2 — fix those, add the
> `OAI_*` Makefile wiring, then build and run the 13 suites on both platforms
> and keep every pre-existing check green.
>
> Then finish Phase 6 (tests/README.md, `scripts/probe_strata.sh`, ASAN with
> `ASAN_OPTIONS=detect_leaks=0` after `make clean` in `acta_db` **and**
> `acta_runner`, rebuild plain afterwards, diff against
> `/c/msys64/tmp/actsnap/phase5/`), then Phase 7 (docs, see §9.3).
>
> Decisions are settled (plan §9): no migration, vocabulary
> `llamacpp`/`openai` adapted in situ, `llama` is not an alias, validate
> `backend` in `acta_cli`, omit `top_k` and skip the catalog call in openai
> mode, envelope schema name `acta_output`. Do not reopen them.
>
> Verify per phase (Windows: `mingw32-make`, `TMP='C:/msys64/tmp'
> TEMP='C:/msys64/tmp'`, `PATH=/c/msys64/mingw64/bin:$PATH`, targets **without**
> `.exe`, runner suites run from `acta_runner/`, acta_cli suites run from
> `acta_cli/` with the `acta_test_ref.db` fixture; WSL: the rsync command in §5,
> `make all`, build the targets, then `~/acta_suites.sh` in batches).
> Show diffs with `diff -u /c/msys64/tmp/actsnap/phase5/<file> <file>`
> (re-snapshot per approved phase) or `git diff HEAD --ignore-cr-at-eol`, and
> **commit only after I approve**.


---

## 10. SESSION 5 (2026-10-12, later) — Phase 6 DONE + verified, Phase 7 DONE (docs + GUI wiring, GUI not compiled)

**Still nothing committed.** The two Phase-5 out-of-plan changes (§9.1 items 2
and 6) are still awaiting the owner's verdict, and Phase 7 added a **third**
out-of-plan area (the GUI: `src.pro`, `modelDialog.cpp`, `modelDialog.ui`) that
could not be compiled here (no Qt 6 in `insightcae`).

### 10.1 Phase 6 — DONE, verified on both platforms

Files changed (diff baseline `/c/msys64/tmp/actsnap/phase5/`):
`acta_runner/Makefile`, `acta_runner/tests/README.md`,
`acta_runner/tests/stub_server.{h,c}`, **new** `acta_runner/tests/run/test_openai_dialect.c`,
`scripts/probe_strata.sh`.

1. **The three §9.2 defects in `test_openai_dialect.c` are fixed:**
   `acta_db_open(":memory:", &err, ACTA_DB_OPEN_CREATE)` + `acta_db_strerror(err)`;
   `#include "argparse.h"` + `const global_opts_t *runner_gopts;` +
   `static const global_opts_t gopts = { NULL, 1, 0, 0, 0, NULL }; runner_gopts = &gopts;`
   in `main()`; `log_has_event()` replaced with the plain
   `tests/run/test_run.c` implementation (list rows, `strcmp(event)`, free list)
   and `log_lacks_event()` is now `!log_has_event(...)`.
2. **A fourth defect found while building:** scenario 4 asserted
   `"temperature"` in the body while the model `configuration` set only
   `top_k` — `run.c` sends `temperature` **only when configured**
   (`double temperature = -1.0; … if (temperature >= 0.0)`). Fixed by configuring
   `{"top_k":40,"temperature":0.7}` in scenarios 4 **and** 5 (4: `top_k` absent,
   `temperature` present → the omission is dialect-specific, not a general
   parameter drop; 5: both sent). Scenario 5 gained the
   "temperature sent on llamacpp too" check. **Suite total: 58 checks.**
3. **Makefile `OAI_*` wiring** exactly as §9.2 item 1 listed (sources, objs,
   `OAI_TARGET`, link rule + `$(TEST_LDLIBS)`, compile rule with `-Itests`,
   added to `test:` + `clean:`), plus the header-comment suite list updated.
4. **Suite counts (all green):**

   | | Windows (MinGW) | WSL `insightcae` |
   |---|---|---|
   | 13 runner suites | **681 checks / 0 fail** (623 pre-existing + 58 new) | **712 / 0** (654 + 58) |
   | `acta_cli` (`make test`, cwd `acta_cli/`) | **17/17 PASS**, rc 0 | **17/17 PASS**, rc 0 |

   New per-suite counts unchanged; `test_openai_dialect` = 58 on both.
   `tests/stub_server.c` recompiled on glibc with **no** `strncasecmp` warning →
   the `<strings.h>` fix is confirmed (§8.5's open item closed).
5. **ASAN (WSL)** after `make -C acta_db clean` + `make -C acta_runner clean`
   (+ `make -C acta_db include/schema_sql.h include/migrations_sql.h`, §9.1
   gotcha): ASAN build of `libacta_db.a` + `test_openai_dialect`,
   `test_run`, `test_check`, `test_dialect`, `test_autopreflight`; run with
   `ASAN_OPTIONS=detect_leaks=0` → 58 / 148 / 77 / 83 / 68 checks, **0 failures,
   zero AddressSanitizer errors**. `test_openai_dialect` with
   `detect_leaks=1,log_path=…` wrote **no leak log at all** (clean). `test_run`
   reports the known pre-existing libcurl global-init leak
   (**13688 B / 32 allocations, every frame in `libcurl.so.4`**). Tree then
   rebuilt **plain** and re-verified: 712/0 + 17/17.
6. **`acta_runner/tests/README.md`**: `stub_config_t.dialect` row in the field
   table, `stub_server_catalog_requests()` + `stub_server_last_chat_body()` in
   the counters paragraph, the two-envelope refusal table (openai stub →
   `response_format.json_schema needs a schema object` = Strata's real message;
   llamacpp stub → `response_format.schema is required for type json_schema` =
   the stub's invention), the `cJSON_PrintUnformatted` textual-detection note,
   and the new suite entry.
7. **`scripts/probe_strata.sh`**: `--dialect <llamacpp|openai>` (also
   `--dialect=…`, `ACTA_STRATA_DIALECT`, `-h/--help`; invalid →
   `invalid --dialect 'vllm' (expected llamacpp or openai)` rc 1; missing value
   → rc 1) and **new step 4b**, which POSTs the envelope `acta_runner` really
   builds for that dialect (schema = the sentiment object; `-w '\n%{http_code}'`
   so an empty content is diagnosable) and classifies the answer: 200+content =
   ok; **400 on `--dialect llamacpp` = expected** on an OpenAI-dialect server
   (informational, exit stays 0); anything else = failure. Verdict line now
   prints `backend=$DIALECT`.
   **Live results (Strata live, `qwen3.8-flash-next-iq3_xxs`):**
   * Windows `--dialect openai` → `4b json_schema openai HTTP 200
     {"label":"positive","confidence":1.0}`, rc 0
   * Windows `--dialect llamacpp` → `HTTP 400 response_format.json_schema needs
     a schema object <- expected …`, rc 0 — **the stub's wording is exactly
     Strata's**
   * WSL (host IP `172.28.160.1`) → same, rc 0
   * `scripts/wsl_strata_demo.sh` after the plain rebuild: both executions
     `completed`; execution 2 (`output_schema`) log =
     `preflight_passed {"max_context":204800,"max_context_source":"entry.meta.n_ctx","dialect":"openai","catalog":null}`
     and `llm_request {"response_format":true,"dialect":"openai","dialect_source":"model_revision","model_backend":"openai"}`
     → acceptance criteria 2, 3, 5, 6 verified live.
8. Helper scripts `/c/msys64/tmp/winsuites.sh` and `~/acta_suites.sh`
   (source copy `/c/msys64/tmp/acta_suites.sh`) now include
   `tests/run/test_openai_dialect` (13 suites).

### 10.2 Phase 7 — DONE (docs + GUI wiring). GUI NOT COMPILED here.

* **`docs/runner_contract.md`**: status line gained "backend dialects shipped";
  decision 3 reworded (the envelope is the dialect's); decision 7 now records
  `max_context_source` + `dialect` in `preflight_passed`; **new decision 9
  "Backend dialect (`llamacpp` / `openai`)"** (vocabulary + single source, no
  auto-detection, source = model revision, `--dialect` override semantics incl.
  the `--pending` rejection and the `check` standalone default, unknown-value
  failure message, what differs / what stays shared, the audit keys, links to
  both contract docs); pipeline steps 2/3/4 updated (`backend` resolved, catalog
  call gated on `fetch_catalog`, `top_k` gated, `llm_request` metadata keys);
  the `check` section gained `--dialect` in the usage block, the
  **"`<model-record-id>` is a `models` row id, not an execution id"**
  clarification (issue F), the dialect-aware ceiling and "no `GET /` in either
  dialect".
* **New `docs/openai_dialect_contract.md`** (10 sections: registration,
  `/health`, `/v1/models` + the ceiling-source table, no catalog call, the exact
  request body ACTA sends, the envelope difference + Strata's verbatim 400 body,
  response reading, errors, non-goals, how it is verified). `openai` is
  documented as "strict OpenAI-compatible HTTP dialect", not the vendor.
* **`docs/llamacpp_server_contract.md`**: dialect-framed header + cross-links,
  §3 ceiling order, §3a catalog is a `llamacpp`-dialect property, §4 `top_k`
  dialect note and the envelope cross-reference.
* **`docs/building.md`**: dependency line now names both dialects; **new WSL2
  section** (apt deps, the rsync command with `--include="acta_test_ref.db"`
  before `--exclude="*.db"`, the `*.o/*.a/*.so/*.exe/build/` excludes, the
  **host-IP rule** `http://<windows-host-ip>:8080` never `127.0.0.1` under NAT,
  the cold-path first-connect retry, missing `curl`/`ip`/`pkg-config`,
  `scripts/wsl_strata_demo.sh`); the GUI-coupling section now lists
  `dialect.c` and records that `acta_cli` compiles
  `../acta_runner/src/dialect.c` into its own `src/dialect.o`.
* **`docs/cli_spec.md`**: `--backend` closed-vocabulary note (exit 4, exact
  message, case-sensitive, `llama` is not an alias, a `backend` change creates a
  new revision, the runner re-validates) after the `model` table; **issue C**
  note after the `skill` table: `output_schema` must be a **string containing
  JSON** — a JSON object is silently dropped and stores `null` (verified live:
  `skill get 1` → `"output_schema":null` vs `skill get 2` → the string), and
  invalid JSON fails the execution with `skill output_schema is not valid JSON`.
* **`docs/DBDesign.md`**: the Models section rewritten — `backend = llamacpp`
  example, closed vocabulary, enforced in the **application** layer (no schema
  `CHECK`, schema stays frozen at `0.1`), `configuration` as the capability layer
  with the strict known-key set, "interchangeable **behind one of the two
  dialects**", no plugin mechanism.
* **`README.md`**: Core concepts → Model now names the two dialects; §2 "Start
  llama.cpp" wording; the local `llama-server` example is
  **`"backend": "llamacpp"`** (was `openai`); the skill example uses flags with
  `--output_schema '<JSON string>'` + the JSON-string warning; `acta_runner check 1`
  annotated "the MODEL record id"; the command table row is
  `acta_runner check <model-record-id>`; **new "# OpenAI-compatible backends"**
  section (Strata example + the 4-row dialect table + `check`/`--dialect`
  wording); architecture note names both dialects; the "intentionally narrow"
  bullet and the docs table gained the new contract row.
* **`docs/status.md`**: GUI source list + `dialect.c`; appended a
  **"Done — backend dialects (`llamacpp` / `openai`)"** paragraph with the
  verification numbers.
* **`docs/examples/multi-persona-review.md` + `docs/examples/replay.md`**:
  `model create … "backend":"openai"` for a GGUF-served llama.cpp router →
  **`"backend":"llamacpp"`** (`playground.md` was already canonical).
* **GUI (plan Phase 7, unverifiable here — no Qt 6 in `insightcae`):**
  `acta_gui/src/src.pro` adds `../../acta_runner/src/dialect.c` to `SOURCES`
  (+ comment; no `INCLUDEPATH` change needed — `run.c`'s `#include "dialect.h"`
  resolves through `run.c`'s own directory, same as `backend.c`/`backend.h`);
  `acta_gui/src/widgets/modelDialog.cpp` `validateFields()` rejects a backend
  outside `llamacpp`/`openai` (`QLatin1String` compare + `QMessageBox::warning`,
  no new includes); `acta_gui/ui/modelDialog.ui` backend label `toolTip` +
  `backendTextEdit` `placeholderText` "llamacpp or openai".

### 10.3 REMAINING

1. **GUI build verification** (`make gui` on Windows — Qt 6 IS installed in
   MSYS2 per `docs/building.md`; `acta_gui/src/Makefile.Debug/Release` are stale
   generated copies that do not list `dialect.c`, so regenerate them first:
   delete `acta_gui/src/Makefile*` or run
   `qmake6 "CONFIG+=debug" acta_gui.pro`). Confirm `dialect.c` links and the new
   `modelDialog` validation compiles.
2. **`docs/examples/acta.db` vocabulary — DONE at 22:46, i.e. AFTER this
   section was written (22:41); see §11.3.** The text below is what was true
   when §10 was written and is kept only as the rationale:
   `models.backend` = `jtest`×2, `llamacpp-cli`, `t1`, `test`, `vllm`×3;
   `model_revisions.backend` additionally has
   `JBDRAFTBig_Qwen3.8-27B-GSQ-RCO-IQ3_XXS` (a model name in the backend
   column). Copying the snapshot and running those rows now fails with
   "unsupported backend". A plain SQL `UPDATE` fires `models_update_revision`
   → spurious revision rows + `sqlite_sequence` bump (§9.1 item 6 gotcha) and
   would break the revision ids the example tutorials pin — which is why the
   change was made with the trigger-drop/recreate script of §11.3, not with a
   bare `UPDATE`.
3. **Owner verdicts still open** (nothing may be committed before them):
   §9.1 item 2 (`acta_cli/Makefile` `TEST_DIRS := $(filter-out tests/helpers/,
   $(filter %/, $(sort $(wildcard tests/*/))))` — HEAD's `make -C acta_cli test`
   was genuinely broken by the `tests/acta_test_ref.db` fixture becoming a bogus
   suite), §9.1 item 6 (`acta_test_ref.sql` normalization + the git-ignored `.db`
   fixture question), and now the GUI trio in §10.2.
4. **Commit split** + the fate of the untracked `scripts/`, `docs/plans/`,
   `docs/session_*.md`, `acta_runner/MYTEST/` — undecided, nothing committed.
5. Not fixed (pre-existing, out of scope): `emit_runner_error()` prints
   `{error":"…` (missing opening quote) while `docs/runner_contract.md` documents
   `{"error":…}`; the `acta_cli` test-harness leak (§9.1);
   `make -C acta_db libacta_db.a` not regenerating the embedded schema/migration
   headers after `clean`.

### 10.4 Resume prompt for the next session

> Read `docs/session_dialect_progress.md` (**§10 first**, then §9.1/§9.4; §3/§4/§8
> are superseded), `docs/plans/backend_dialects.md` (§2 matrix, §3.2, §9
> decisions) and `docs/session_strata_backend.md` §4 in ACTA Gamma
> (`C:/msys64/home/admin/github/boucaron/ACTA_Gamma`).
>
> State: **Phases 1–7 are implemented. Nothing is committed.** Phase 6 is done
> and green on both platforms (Windows 13 runner suites 681/0 — 623 pre-existing
> + 58 new — `acta_cli` 17/17 PASS; WSL 712/0 — 654 + 58 — and 17/17 PASS; ASAN
> clean apart from the known libcurl global-init leak; live Strata verified from
> both sides with an `output_schema` skill; `probe_strata.sh --dialect`
> reproduces both envelope refusals live). Phase 7 (docs) is written:
> `docs/runner_contract.md` decision 9, new `docs/openai_dialect_contract.md`,
> `docs/llamacpp_server_contract.md`, `docs/building.md` (WSL2 section),
> `docs/cli_spec.md`, `docs/DBDesign.md`, `README.md`, `docs/status.md`,
> `docs/examples/{replay,multi-persona-review}.md`, plus the GUI trio
> (`src.pro` + `modelDialog.cpp` + `modelDialog.ui`) which **has never been
> compiled** here.
>
> Do next: (1) build the GUI on Windows (`make gui`, regenerate the stale
> `acta_gui/src/Makefile.*` first) and fix whatever the `dialect.c` /
> `modelDialog` changes expose; (2) get my verdict on `docs/examples/acta.db`'s
> non-canonical backends (§10.3 item 2 — do NOT just `UPDATE` it, the revision
> triggers bite); (3) ask me again about the §9.1 items 2 and 6 Phase-5 changes
> and the GUI trio before any commit; (4) propose the commit split and what to do
> with `scripts/`, `docs/plans/`, `docs/session_*.md`, `acta_runner/MYTEST/`.
>
> Decisions are settled (plan §9): no migration, vocabulary
> `llamacpp`/`openai` adapted in situ, `llama` is not an alias, validate
> `backend` in `acta_cli`, omit `top_k` and skip the catalog call in openai
> mode, envelope schema name `acta_output`. Do not reopen them.
>
> Verify on both platforms (Windows: `mingw32-make`, `TMP='C:/msys64/tmp'
> TEMP='C:/msys64/tmp'`, `PATH=/c/msys64/mingw64/bin:$PATH`, targets **without**
> `.exe`, runner suites from `acta_runner/` via `/c/msys64/tmp/winsuites.sh`
> (13 suites incl. `tests/run/test_openai_dialect`), acta_cli suites from
> `acta_cli/` with the `acta_test_ref.db` fixture; WSL: the rsync command in §5,
> `make all`, build the targets, then `~/acta_suites.sh` in batches; ASAN only
> after `make clean` in `acta_db` **and** `acta_runner`, with
> `ASAN_OPTIONS=detect_leaks=0` (LeakSanitizer on by default makes `test_model`
> hang; use `log_path=` if you need the report), then rebuild plain and
> re-verify). Show diffs with `diff -u /c/msys64/tmp/actsnap/phase5/<file>
> <file>` (no phase6/phase7 snapshot exists yet — take one when a phase is
> approved) or `git diff HEAD --ignore-cr-at-eol`, and **commit only after I
> approve**.

---

## 11. SESSION 6 (2026-10-12, later) — re-verification + doc consistency, no code changes

### 11.1 Both platforms re-verified against the CURRENT working tree

| | Windows (MinGW) | WSL `insightcae` |
|---|---|---|
| 13 `acta_runner` suites | **681 checks / 0 fail** | **712 / 0** |
| 17 `acta_cli` suites | **17/17 PASS** | **17/17 PASS** |

Per-suite (identical on both except `api_key` 16/19, `conf` 36/50,
`shadow` 35/49): argparse 56, run 148, pending 29, autopreflight 68,
sweep 43, deleted 12, api_key 16|19, conf 36|50, shadow 35|49, deathmark 20,
check 77, dialect 83, openai_dialect 58. **§10.1's numbers still hold.**
Windows ran the binaries left at 22:13/22:14 (no rebuild needed — nothing in
`acta_*` changed after 22:15); WSL ran the binaries built at 22:24/22:25.

**Clean-rebuild gate (Windows, session 6):** `make clean` in `acta_db` +
`acta_cli` + `acta_runner`, then `mingw32-make all` (rc 0, **zero compiler
warnings**), then the 13 runner targets (rc 0, 681/0) and `mingw32-make -C
acta_cli test` (rc 0, 17 PASS, no compiler warnings — only the expected
`config file … group- or other-readable` test output). Note the binaries embed
`-DACTA_RUNNER_GIT_HASH="d7f7395a-dirty"` / `ACTA_DB_GIT_HASH` — the `-dirty`
suffix is a visible reminder that nothing is committed; it disappears after the
first commit of this batch.

### 11.1a Consistency fixes made in session 6 (docs only, no code touched)

* `docs/examples/multi-persona-review.md`: two **broken relative links**
  (pre-existing at HEAD, found by a whole-tree link audit — every other
  relative link in `README.md`, `docs/`, `docs/examples/`, `docs/plans/`,
  `acta_runner/tests/README.md` resolves): `[README](../README.md)` →
  `../../README.md` and `[Quick start](../README.md#quick-start)` →
  `../../README.md#quick-start`.
* `docs/plans/backend_dialects.md` status line: "decided and in progress" →
  **implemented and verified, nothing committed**, phases 0–6 + 8 done,
  phase 7 (GUI) written but not compiled, acceptance criteria 1–8 verified.
* §10.3 item 2 rewritten (it claimed the example DB was untouched — see §11.3).

### 11.2 Git state (verified, unchanged)

`HEAD = d7f7395` (2026-09-27 "docs(README): show the app logo") — **no commit
after it**, no stash, nothing staged (`git diff --cached` empty, `git reflog`
ends at the same commit). 49 modified tracked files + untracked
`acta_runner/src/dialect.{c,h}`, `acta_runner/tests/dialect/`,
`acta_runner/tests/run/test_openai_dialect.c`, `docs/openai_dialect_contract.md`,
`docs/plans/`, `docs/session_*.md`, `scripts/`, `acta_runner/MYTEST/`.
True change sizes: `git diff HEAD --ignore-cr-at-eol --numstat`.

### 11.3 What session 5 actually did LAST (22:45–22:46) — the example-DB normalization

Not recorded in §10 (written at 22:41). Evidence, all still present:

* `/c/msys64/tmp/normalize_backend_vocab.py` — dry-run first
  (`/c/msys64/tmp/dryrun/{acta.db,ex.db}`), then applied. Method: read the
  `models_update_revision` SQL **from the database itself**, `BEGIN IMMEDIATE`,
  `DROP TRIGGER`, `UPDATE models`/`model_revisions` setting every non-canonical
  value to `llamacpp`, re-execute the stored SQL to recreate the trigger
  byte-for-byte, `COMMIT`; then assert table counts, `sqlite_sequence`, the
  trigger set and **every cell of every table** (only `backend` may differ).
* Pre-change backups: `/c/msys64/tmp/actsnap/pre-vocabulary/{acta.db,
  docs_examples_acta.db, acta_gui_src_release_acta.db}`.
* Applied to **`docs/examples/acta.db`** (tracked — `!docs/examples/acta.db`
  overrides `*.db` in `.gitignore`) and to **`acta_gui/src/release/acta.db`**
  (git-ignored). Changed: `models` 2–9 (`vllm`×3, `test`, `t1`, `jtest`×2,
  `llamacpp-cli`) and `model_revisions` 4–10, 12–14, 17 (+ the model-name-in-
  `backend` row 17) → **`llamacpp`**.
* Verified independently in session 6: 12 models / 18 revisions /
  `sqlite_sequence` identical to HEAD, `.schema` identical (sorted), **only
  `backend` cells differ**, trigger set identical — only
  `models_update_revision` moved in `sqlite_master`, which is why the `.dump`
  (and therefore the binary diff) shows the trigger block reordered. No
  spurious revision rows, no id drift → the ids the tutorials pin are intact.

**Session 6 finished the job (owner: "update the DBs in situ"):**
`acta_runner/MYTEST/acta.db` — the untracked 8.9 MB scratch copy from
2026-09-26 that holds the `*_summarize` review outputs (158 executions, 1037
log rows, 51 contexts, 16 skills) — was normalized with the **same script**,
dry-run first on a copy (`/c/msys64/tmp/dryrun/mytest.db`), then in place:
`models` 2–9 and `model_revisions` 4–10, 12–14, 17 → `llamacpp`, **19 cells,
all in a `backend` column**; table counts, `sqlite_sequence` (all 9 sequences)
and the 7 triggers verified identical afterwards; pinned rows intact
(e.g. `model_revisions` 17/18 = model 12 rev 1/2, `JBDRAFTBig_…`).
Backups: `/c/msys64/tmp/actsnap/pre-vocabulary/acta_runner_MYTEST_acta.db`
(pre-change original) and `…/_c_msys64_tmp_dryrun_mytest.db` (the dry-run
copy). The script is kept at `/c/msys64/tmp/normalize_backend_vocab.py`.

**Tree-wide audit after the change** (every `.db` in the repo that has a
`models` table; all canonical, `llamacpp`/`openai` only):

| file | models | non-canonical before → after |
|---|---|---|
| `docs/examples/acta.db` (tracked) | 12 | 8 models + 11 revisions → **0** |
| `acta_runner/MYTEST/acta.db` (untracked) | 12 | 8 models + 11 revisions → **0** |
| `acta_gui/src/release/acta.db` (ignored) | 12 | 8 + 11 → **0** (session 5) |
| `acta_cli/acta_test_ref.db` (ignored, regenerated from the `.sql`) | 8 | already 0 (§9.1 item 6) |
| `acta_gui/db/engine.db` | 1 | already 0 |

Other `.db` files in the tree (`acta_cli/acta.db`,
`acta_cli/acta_test_cli_main_init.db`, `acta_cli/tests/acta_test_ref.db`,
`acta_gui/release/acta.db`, `acta_runner/acta.db`) have **no `models` table**
— empty/leftover scratch files, nothing to normalize. `acta_cli/tmp/*.db`
scratch DBs are deleted by the harness at exit (confirmed: `tmp/` is empty
after a clean `make test`); two stale WSL leftovers from a pre-vocabulary run
were removed.
Minor: the WSL copy has no `docs/examples/acta.db` at all (the §5 rsync
excludes `*.db`); the examples are a Windows-side activity.

### 11.4 Remaining, GUI work explicitly deferred by the owner

1. GUI build verification — deferred (see §10.3 item 1; Qt 6 IS installed in
   MSYS2: `/c/msys64/mingw64/bin/qmake6.exe` + `Qt6Core.dll`; the generated
   `acta_gui/src/Makefile{,.Debug,.Release}` dated 19:49 contain **zero**
   mentions of `dialect`, i.e. they predate the 22:34 `src.pro` edit and must
   be regenerated before `acta_gui.exe` reflects the change — the current
   `acta_gui/src/release/acta_gui.exe` is from 2026-09-26).
2. Owner verdicts before any commit: §9.1 item 2 (`TEST_DIRS`), §9.1 item 6
   (`acta_test_ref.sql` + the git-ignored `.db` fixture), the GUI trio (§10.2),
   the example-DB normalization (§11.3, done in situ per the owner's
   instruction — the same question now also covers `acta_runner/MYTEST/acta.db`
   and `acta_gui/src/release/acta.db`), and the fate of `scripts/`,
   `docs/plans/`, `docs/session_*.md`, `acta_runner/MYTEST/`.
3. Commit split — proposal in §11.5; **nothing committed**.
4. Pre-existing, out of scope, still unfixed: `emit_runner_error()`
   `{error":"…`; the `acta_cli` test-harness leak; `make -C acta_db
   libacta_db.a` not regenerating `include/{schema,migrations}_sql.h` after
   `clean`.

### 11.5 Commit-split proposal (for approval)

Plan §10 listed 7 commits; the tree does not split that cleanly because the 10
portability/memory fixes share files with the dialect phases (`backend.c`,
`check.c`, `main.c`, `stub_server.c`, `model_test_helpers.c`). Two options:

* **A (recommended, no hunk surgery):**
  1. `fix(portability): _POSIX_C_SOURCE + -fPIC in acta_db, free(db->path),
     sweep.c last_activity() use-after-free` — the three files that contain
     **only** fixes: `acta_db/Makefile`, `acta_db/src/db.c`,
     `acta_runner/src/sweep.c`.
  2. Phase 1 (`backend.{c,h}` max_context fallback + `stub_server` fields +
     test_check scenario 10 / test_run scenario 18) — carries the
     `backend.c` `CURLE_OPERATION_TIMEDOUT` fix in its message.
  3. Phase 2 (`dialect.{h,c}` + `tests/dialect/` + Makefile wiring).
  4. Phase 3 (`run.c`/`backend.c`/`check.c` dialect wiring + the `check.c`
     use-after-free fix + run-pipeline asserts + the `llama`→`llamacpp`
     reseeding of 11 suites).
  5. Phase 4 (`argparse.c`, `run.c`, `check.c`, `main.c` — the `main.c`
     `const char *note =` hoist rides here) + the four test suites.
  6. Phase 5 (`acta_cli/Makefile`, `commands/model.c`, `tools.c`,
     `cli_main.c` hoist, model tests, `acta_test_ref.{sql,db}`).
  7. Phase 6 (`stub_server.{h,c}` — carries the `MSG_NOSIGNAL` fix —,
     `test_openai_dialect.c`, `acta_runner/Makefile`, `tests/README.md`).
  8. Phase 7 (docs + `docs/examples/{replay,multi-persona-review}.md` +
     `docs/examples/acta.db` + `scripts/`).
  9. GUI trio (`src.pro`, `modelDialog.cpp`, `modelDialog.ui`) — **only after
     the deferred GUI build passes**.
  10. Session notes (`docs/session_*.md`, `docs/plans/backend_dialects.md`).
* **B:** `git add -p` to peel each fix into its own commit — cleaner history,
  but 5 files need hunk-level staging and each intermediate commit must still
  build (it does not today: `acta_cli` only builds once the Phase-5 Makefile
  wiring lands, so the portability fixes cannot precede Phase 5 without
  carrying `dialect.c` too).

Line-ending hygiene before committing: `git add --renormalize .` (the index
holds CRLF while the blobs are LF, `core.autocrlf=true` — §6 gotcha 1).

---

## 12. SESSION 7 (2026-10-12) — the batch is COMMITTED (7 commits); only the GUI trio is left

### 12.1 What is in git now

The owner chose **option B** of §11.5, and B collapsed to **7 commits** (not 10):
whole-file staging means a file must land in the **earliest** phase that touches
it, and `backend.{c,h}`, `run.c`, `check.c`, `Makefile`, `stub_server.{h,c}`,
`test_run.c`, `test_check.c`, `test_autopreflight.c` each carry edits from
several phases. Hunk-splitting them would leave intermediate commits that either
do not compile or fail their own suites (`run.c`'s `cmd_run` `--dialect` handling
without `argparse.c`'s known-flag table → `EXIT_CLI` instead of `EXIT_INVALID` in
`test_autopreflight` scenario 8; a `Makefile` listing the openai suite without
that suite in the tree → `make test` breaks). Hence Phases 1–4 + 6 are one
commit, with the phase boundaries written out inside its message.

| commit | content | files |
|---|---|---|
| `8e06493` | `docs(plans)`: add `docs/plans/backend_dialects.md` (status line updated to "implemented and verified, nothing committed") | 1 (+304) |
| `4f26f1a` | `docs`: add the two session notes (`session_dialect_progress.md`, `session_strata_backend.md`) — resolves the plan's two dangling links | 2 (+1530) |
| `9786d3c` | `fix`: the three portability/memory findings that share no file with the dialect phases — `acta_db/Makefile` (`-D_POSIX_C_SOURCE=200809L`, `-fPIC`), `acta_db/src/db.c` (`sqlite3_free`→`free` on the `db_strdup` copy), `acta_runner/src/sweep.c` (`last_activity()` use-after-free) | 3 |
| `52dd03c` | `runner`: **Phases 1–4 + 6** — `max_context` fallback + `max_context_source`; `src/dialect.{h,c}` + `tests/dialect/`; the revision-sourced dialect in `run.c`/`backend.c`/`check.c` + metadata; `--dialect`; `stub_server` dialect/envelope rejection + `test_openai_dialect.c`; also carries the `check.c` UAF, the `main.c` declaration-in-condition, `MSG_NOSIGNAL`, `<strings.h>` | 27 (+3581/‑875) |
| `cb2be70` | `cli`: **Phase 5** — `--backend` validation via `backend_rejected()`, the four usage blocks, `tools.c` descriptions, option-A Makefile wiring (`-I../acta_runner/src`, `src/dialect.o`, `$(TARGET)` prerequisite), the `TEST_DIRS` fix, the model tests + `acta_test_ref.sql` normalization, the `cli_main.c` hoist | 12 (+305/‑61) |
| `7810e7c` | `docs`: **Phase 7** — `runner_contract.md` decision 9, new `openai_dialect_contract.md`, `llamacpp_server_contract.md`, `cli_spec.md`, `DBDesign.md`, `building.md` WSL2 section, README, `status.md`, the two example tutorials + the 2 broken links, the normalized `docs/examples/acta.db`, new `scripts/` | 13 (+792/‑54) |
| `083dd71` | `tests`: the `acta_cli` harness creates `./tmp` (see §12.3) | 1 |

All blobs landed `i/lf` (no CRLF pollution) even though `core.autocrlf=true`;
no `git add --renormalize` was needed for these files (the already-tracked
`docs/status.md` is still `i/crlf` — untouched by this batch).

### 12.2 Per-commit snapshot verification (the method worth keeping)

`git worktree add --detach /c/msys64/tmp/wt_x HEAD` → build and test **that
commit alone** → `git worktree remove --force` (force: the build artifacts are
git-ignored). Results:

| commit | snapshot check |
|---|---|
| `9786d3c` | `make all` rc 0, no warnings; suites as they exist there: `test_sweep 43/0`, `test_run 102/0`, `test_check 38/0` |
| `52dd03c` | `make all` rc 0, no warnings; **all 13 suites 681/0** |
| `cb2be70` | fixture regenerated from the committed `.sql` → `acta_cli make test` 17/17 PASS, rc 0 |
| `7810e7c` | `make all` rc 0 · 13 suites **681/0** · `acta_cli` **17/17** · HEAD-wide markdown-link audit: **no dangling links** |

### 12.3 New findings from the commits

* **Fresh-clone blocker 1 — FIXED (`083dd71`)**: nothing created `acta_cli/tmp/`
  (git-ignored), so `stest_init()`'s fixture copy failed with `errno=2` and
  `make -C acta_cli test` exited non-zero in a fresh worktree. The message looked
  like a missing fixture because `copy_file()` prints the **source** while the
  failure was on the **destination**. `stest_init()` now calls
  `TEST_MKDIR("tmp")` (`mkdir(p)` on Windows, `mkdir(p,0755)` elsewhere, EEXIST
  ignored) and the FATAL line names both suspects. Verified on both platforms
  with `acta_cli/tmp` deleted: 17/17 PASS, rc 0.
* **Fresh-clone blocker 2 — STILL OPEN**: `acta_test_ref.db` is git-ignored while
  `acta_test_ref.sql` is tracked, so a clone must regenerate the fixture:
  `python3 -c "import sqlite3; sqlite3.connect('acta_cli/acta_test_ref.db')
  .executescript(open('acta_cli/acta_test_ref.sql').read())"`. Owner call: keep
  ignoring it, un-ignore it, or generate it in the Makefile.
* **Gotcha**: relinking in a fresh worktree once produced `collect2.exe: error:
  ld returned 5 exit status` with no source error; the immediate retry linked
  cleanly. Transient (AV/file-lock class), not a code problem — re-run before
  hunting.
* Binaries built from an uncommitted tree embed `ACTA_RUNNER_GIT_HASH="d7f7395a-dirty"`
  / `ACTA_DB_GIT_HASH`; the `-dirty` suffix is a free reminder that nothing was
  committed, and it is gone now.

### 12.4 REMAINING

1. **The GUI trio is the only uncommitted code**: `acta_gui/src/src.pro`,
   `acta_gui/src/widgets/modelDialog.cpp`, `acta_gui/ui/modelDialog.ui` —
   deliberately left out until the Qt build is done (deferred by the owner).
   Regenerate the stale `acta_gui/src/Makefile{,.Debug,.Release}` (19:49, zero
   mentions of `dialect`) first; Qt 6 **is** installed in MSYS2
   (`/c/msys64/mingw64/bin/qmake6.exe`, `Qt6Core.dll`). Commit it as its own
   commit once it compiles.
   *(committed as `bb05950`; **built and verified in session 8 — see §13**,
   which also fixes a real defect the commit contained.)*
2. `acta_runner/MYTEST/` is still untracked (normalized `acta.db`, the five
   `*_summarize` review outputs, `run_tests.sh`) — owner call.
3. The fixture policy of §12.3 blocker 2.
4. Still unfixed, pre-existing, out of scope: `emit_runner_error()` prints
   `{error":"…` while `docs/runner_contract.md` documents `{"error":…`; the
   `acta_cli` test-harness leak (§9.1); `make -C acta_db libacta_db.a` not
   regenerating `include/{schema,migrations}_sql.h` after `clean`.

### 12.5 Resume prompt for the next session

> Read `docs/session_dialect_progress.md` (**§12 first**, then §11; §3/§4/§8/§10
> are superseded), `docs/plans/backend_dialects.md` (§2 matrix, §3.2, §9
> decisions) and `docs/session_strata_backend.md` §4 in ACTA Gamma
> (`C:/msys64/home/admin/github/boucaron/ACTA_Gamma`).
>
> State: **the whole dialect program is committed** — 7 commits from `8e06493` to
> `083dd71` (plan doc, session notes, portability fixes, Phases 1–4+6 as one
> `runner:` commit, Phase 5 `cli:` commit, Phase 7 `docs:` commit, harness
> `tmp/` fix). Each commit was verified in a detached `git worktree`: HEAD builds
> `make all` rc 0 with no warnings, 13 `acta_runner` suites **681/0** on Windows
> (712/0 on WSL) and `acta_cli` **17/17 PASS** on both, and the committed tree has
> no dangling markdown links. All DBs in the tree are on the canonical
> vocabulary.
>
> Do next: (1) the **GUI** — regenerate the stale qmake Makefiles, build
> (`qmake6 "CONFIG+=debug" acta_gui.pro` then `make`), fix whatever `dialect.c` +
> `modelDialog` expose, then commit the three `acta_gui` files (the only
> uncommitted code); (2) decide the fate of untracked `acta_runner/MYTEST/`; (3)
> decide the `acta_test_ref.db` fixture policy (§12.3 blocker 2).
>
> Verify on both platforms (Windows: `mingw32-make`, `TMP='C:/msys64/tmp'
> TEMP='C:/msys64/tmp'`, `PATH=/c/msys64/mingw64/bin:$PATH`, targets **without**
> `.exe`, runner suites from `acta_runner/` via `/c/msys64/tmp/winsuites.sh`,
> acta_cli suites from `acta_cli/`; WSL: the §5 rsync, `make all`, then
> `~/acta_suites.sh` in batches; ASAN only after `make clean` in `acta_db` **and**
> `acta_runner` with `ASAN_OPTIONS=detect_leaks=0`, then rebuild plain). Prefer
> the §12.2 detached-worktree method to verify any new commit on its own.

---

## 13. SESSION 8 (2026-10-12, later) — the GUI is BUILT: one real defect found and fixed

### 13.1 The GUI was compiled for the first time, and it failed

`bb05950` (the GUI trio) had never been compiled. Run in a detached worktree at
HEAD (`git worktree add --detach /c/msys64/tmp/wt_gui HEAD`): `mingw32-make all`
rc 0, no warnings (baseline), then

```bash
export PATH=/c/msys64/mingw64/bin:$PATH TMP='C:/msys64/tmp' TEMP='C:/msys64/tmp'
cd acta_gui
rm -f Makefile src/Makefile src/Makefile.Debug src/Makefile.Release; rm -rf src/build
qmake6 "CONFIG+=debug" acta_gui.pro -o Makefile
mingw32-make
```

**Fails** (rc 2) on the third object:

```
In file included from runnerWorker.cpp:17:
../../acta_runner/include/runner_util.h:7:10: fatal error: dialect.h: No such file or directory
```

**Cause.** `runner_util.h` (in `acta_runner/include/`) does `#include "dialect.h"`,
and `dialect.h` lives in `acta_runner/src/`. A quoted include resolves relative
to the **including** file, not to the translation unit, so `runnerWorker.cpp`
cannot see it. `acta_runner/Makefile` passes `-Isrc` and `acta_cli/Makefile`
passes `-I../acta_runner/src`; `src.pro` had neither, so the GUI was the only
consumer blind to it. `run.c` and `backend.c` compiled fine because their own
directory holds `dialect.h` — which is why §10.2's "no `INCLUDEPATH` change is
needed" was wrong: true for the C files, false for the C++ consumer.

**Fix** (`acta_gui/src/src.pro`, one line + comment, no other change):

```pro
INCLUDEPATH += $$PWD/../../acta_runner/src
```

After it: full clean rebuild **rc 0, zero compiler warnings**; 20 C++ objects +
5 C objects (including `build/obj/dialect.o`) link into
`acta_gui/src/debug/acta_gui.exe` (23.7 MB debug). `docs/building.md` "GUI–runner
source coupling" now records the header rule and the stale-Makefile rule (§13.6).

### 13.2 What the build confirmed (the four items `bb05950` said it must)

| item | result |
|---|---|
| qmake regenerates the Makefiles | yes — regenerated `src/Makefile.Debug` / `src/Makefile.Release` mention `dialect` 11× each (the stale 19:49 ones: 0). `acta_gui/Makefile` is the subdirs wrapper and names no source (expected). |
| `dialect.c` compiles and links | yes — compiled with `$(CC)` (gcc) under qmake's CFLAGS; `nm build/obj/run.o` shows `U dialect_from_string`, `U dialect_get`, `U dialect_expected_names`, all defined by `dialect.o`. |
| the GUI passes no `-D_POSIX_C_SOURCE` and no `-fPIC`, and MinGW needs neither | confirmed — qmake `win32-g++` CFLAGS are `-fno-keep-inline-dllexport -march=nocona -msahf -mtune=generic -Wa,-mbig-obj -g -Wall -Wextra -Wextra $(DEFINES)`; the build is clean without them. `.c` files build with gcc, `.cpp` with g++ `-std=gnu++1z`. |
| `validateFields()` compiles (`QString` vs `QLatin1String`) and still accepts the canonical values | compiles clean under `-Wall -Wextra`; behaviour verified functionally — §13.4. |
| the Run path still links `run_execution()` | yes — `runnerWorker.cpp:157` calls `run_execution(db, id, timeoutSec, maxChars, apiKey)`, the unchanged 5-argument wrapper over `run_execution_dialect(…, DIALECT_UNKNOWN)`; the GUI never passes an override, so it keeps the `model_revision.backend` (llamacpp-default) source. |

Both qmake configurations build rc 0 with zero warnings: `qmake6 "CONFIG+=debug"
acta_gui.pro -o Makefile && mingw32-make` (debug only) and plain
`qmake6 acta_gui.pro -o Makefile && mingw32-make` (debug **and** release —
`src/release/acta_gui.exe`, 873 KB).

### 13.3 Runtime smoke (offscreen)

`QT_QPA_PLATFORM=offscreen` with `APPDATA` pointed at a scratch directory
(`C:/msys64/tmp/guirun/appdata`, so the live `%APPDATA%\ACTA_Gamma` DB is not
touched): the binary starts, `QApplication` + `MainWindow` construct, the event
loop is still alive after 12 s (killed by the harness), no missing-DLL error and
no abort. `src/debug/acta_gui.log` holds only Qt offscreen records (QRhi, the
offscreen plugin creating a default mouse). `qoffscreen.dll` is present in
`/c/msys64/mingw64/share/qt6/plugins/platforms/`. The app stops at its DB
bootstrap modal (no interaction is possible offscreen), so no DB file is created.

### 13.4 `validateFields()` validated through the real save path (scratch probe)

The GUI has no test harness, so a throwaway probe (kept at
`/c/msys64/tmp/guiprobe/probe.cpp`, **not** in the repo) was compiled against the
GUI's own objects — every `acta_gui/src/build/obj/*.o` except `main.o` (the probe
supplies `main()`), plus `libacta_db.a`, `-lsqlite3 -lcjson -lcurl -lws2_32` and
the Qt libs — and drives the real save path headlessly:

* `ModelDialog::newModel(db, 0)` (public) puts the dialog in `Mode::New` with a
  live `db_t *` (a scratch DB built from `acta_db/schema.sql`);
* the `.ui` widgets are reached **by object name** — `findChild<QLineEdit
  *>("nameLineEdit")`, `findChild<QTextEdit*>("modelTextEdit")`,
  `findChild<QTextEdit*>("backendTextEdit")` — no access hacks;
* `QMetaObject::invokeMethod(&dlg, "onSaveClicked")` — a **private slot** is
  invokable by name through the metaobject system, so `validateFields()` →
  `createEntity()` runs exactly as a Save click would;
* a 40 ms timer closes any `QApplication::activeModalWidget()`, so the modal
  `QMessageBox` (warning or "Model saved.") returns instead of blocking, and its
  title + text are printed.

| backend typed | dialog said | saved? | row written |
|---|---|---|---|
| `llamacpp` | "Model saved." | yes | `backend = "llamacpp"` |
| `openai` | "Model saved." | yes | `backend = "openai"` |
| `  llamacpp  ` | "Model saved." | yes | `"llamacpp"` (trimmed) |
| `openai ` | "Model saved." | yes | `"openai"` (trimmed) |
| `llama` | `Backend must be "llamacpp" or "openai" (got "llama").` | no | none |
| `LLAMACPP` | same shape, `got "LLAMACPP"` | no | none |
| `vllm` | same shape, `got "vllm"` | no | none |
| *(empty)* | "Name, backend and model identifier are required." | no | none |

8 cases, 0 mismatches, probe rc 0. The first run showed 3 false mismatches — a
**probe bug**: every case used the same model name and
`uq_models_root ON models(name) WHERE folder_id IS NULL` (`acta_db/schema.sql:53`)
rejected the duplicates; unique names per case made it green. Not a GUI defect.

### 13.5 Re-verification after the fix

| | Windows (MinGW) | WSL `insightcae` |
|---|---|---|
| `make all` | rc 0, no warnings (clean worktree at HEAD) | rc 0, no warnings (after the §5 rsync) |
| GUI | clean rebuild rc 0, **zero warnings**, debug + release | not built (no Qt 6 in `insightcae`; the delta is a `.pro` line only) |
| 13 `acta_runner` suites | **681 checks / 0 fail** (argparse 56, run 148, pending 29, autopreflight 68, sweep 43, deleted 12, api_key 16, conf 36, shadow 35, deathmark 20, check 77, dialect 83, openai_dialect 58) | **712 / 0** (api_key 19, conf 50, shadow 49) |
| 17 `acta_cli` suites | **17/17 PASS**, rc 0 | **17/17 PASS**, rc 0 |

### 13.6 New gotchas (add to the §4 table)

| Gotcha | Workaround |
|---|---|
| Stale qmake Makefiles | `acta_gui/Makefile` and `acta_gui/src/Makefile{,.Debug,.Release}` are git-ignored artifacts that keep the **old** `SOURCES` list. After any `src.pro` change: `rm -f` them (and `src/build/`) and re-run qmake, or a newly added source is silently not compiled. |
| `rm -rf src/build` alone breaks the next build | qmake creates `src/build/{obj,moc,rcc,ui}` only when it generates the Makefiles; with the Makefiles kept, `uic` fails with `Could not create output file` (`Error 1` on the first `.ui`). Delete the Makefiles too, or recreate the directories. |
| `runner_util.h` needs `acta_runner/src/` on the include path | any consumer of it (GUI `src.pro`, `acta_cli/Makefile`) must add it — `dialect.h` is an internal header in `src/`, not in `include/`. |
| Driving the GUI headlessly | `QT_QPA_PLATFORM=offscreen`; point `APPDATA` (Windows) at a scratch dir so the live app-data DB is not opened; private slots are callable via `QMetaObject::invokeMethod`, `.ui` widgets via `findChild(objectName)`; a timer that closes `QApplication::activeModalWidget()` unblocks the `QMessageBox`es. |
| Scratch GUI/DB fixtures | model names are unique per folder (`uq_models_root` / `uq_models_child`): a probe that reuses one name gets `createEntity` failures that look like validation rejections. |

### 13.7 REMAINING (owner calls, unchanged from §12.4)

1. `acta_runner/MYTEST/` is still untracked (normalized `acta.db`, the five
   `*_summarize` review outputs, `run_tests.sh`) — commit / git-ignore / delete.
   **Decided (session 8): git-ignore it** — `.gitignore` gained `acta_runner/MYTEST/`
   (the `acta.db` inside was already covered by `*.db`; the review outputs and
   `run_tests.sh` are local scratch, and the pre-vocabulary DB copy lives at
   `/c/msys64/tmp/actsnap/pre-vocabulary/acta_runner_MYTEST_acta.db`).
2. The `acta_test_ref.db` fixture policy of §12.3 blocker 2.
3. Still unfixed, pre-existing, out of scope: `emit_runner_error()` prints
   `{error":"…` while `docs/runner_contract.md` documents `{"error":…`; the
   `acta_cli` test-harness leak (§9.1); `make -C acta_db libacta_db.a` not
   regenerating `include/{schema,migrations}_sql.h` after `clean`.

### 13.8 Resume prompt for the next session

> Read `docs/session_dialect_progress.md` (**§13 first**, then §12; §3/§4/§8/§10/§11
> are superseded), `docs/plans/backend_dialects.md` (§2 matrix, §3.2, §9 decisions)
> and `docs/session_strata_backend.md` §4 in ACTA Gamma
> (`C:/msys64/home/admin/github/boucaron/ACTA_Gamma`).
>
> State: **the whole dialect program is committed and every target builds** — the
> 9-commit series `8e06493…` plus the GUI commit, whose `dialect.h` include-path
> defect was found by compiling the GUI and fixed (§13.1). Windows: `make all` rc 0
> no warnings, GUI debug + release rc 0 no warnings, 13 runner suites **681/0**,
> `acta_cli` **17/17**. WSL: `make all` rc 0 no warnings, 13 suites **712/0**,
> `acta_cli` **17/17**. Nothing is left unverified except what §13.7 lists.
>
> Do next: the one remaining owner call — the `acta_test_ref.db` fixture policy
> (§12.3 blocker 2; the recommended fix is a `acta_cli/Makefile` rule
> `acta_test_ref.db: acta_test_ref.sql` → `sqlite3 $@ < $<`, with `test` depending
> on it — that exact command is what made a fresh worktree pass 17/17) — plus, if
> asked, the three out-of-scope items in §13.7 item 3. `acta_runner/MYTEST/` is now
> git-ignored (§13.7 item 1). Do not reopen the settled decisions (plan §9).


