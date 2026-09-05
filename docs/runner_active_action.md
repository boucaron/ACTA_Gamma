# Active Actions — runner (from `runner_analysis.md`)

Action plan for the runner workstream. Specs and decisions per
[`runner_analysis.md`](runner_analysis.md); the UI side (UR #15) per
[`ui_review.md`](ui_review.md) / [`ui_active_action.md`](ui_active_action.md).

## Queued actions

### Low

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| R5 | **JSON validation (to analyze)** for `output_schema` and model `configuration` (context `content` likely excluded — it may be plain text) | H2 in `ui_active_action.md` / UR #15 | analyze scope first; then `QJsonDocument::fromJson` with a clear "invalid JSON" message in the dialogs/panel editor, at minimum a "Validate JSON" button |
| R6 | UR #26 remaining: JSON highlighting / line numbers in the editor | `ui_review.md` Polish | independent polish |
| R7 | Housekeeping: decide fate of untracked `docs/llamacpp_server_README.md` (referenced by `runner_analysis.md` — commit or delete); consider `.gitignore` for build outputs | working tree | — |
| R8 | **Preflight catalog logging** — during preflight, fetch the server model catalog (`GET /`, the `models.json` format) and record the matched entry's server-instance config (launch `args`, `meta`: `n_ctx`, `n_params`, `size`, `ftype`) in a `preflight_passed` log event. Best-effort: a missing catalog logs `catalog: null` and never fails the execution (non-llama OpenAI-compatible backends don't serve it) | `models.json` / auditability | same model id can be served under different server flags (`--ctx-size`, `--temperature`, `--top-k`, ...); config drift must be visible in the execution timeline; touches `run.c`, `stub_server.{h,c}`, `test_run.c`, three docs; no schema change |

## Summary

- **Now:** R5 (JSON validation, analyze first), then R8, then R6–R7
  (polish / housekeeping).
- After shipping each item: drop it from the open lists and the Summary
  (the commit is the record).

## R8 plan — preflight catalog logging

**Goal.** An execution currently proves only "some server had this model id".
Two servers serving the same id with different `--ctx-size` / `--temperature`
produce different outputs; the server-instance configuration must be recorded
per execution (`PointOfView.md` audit requirement — the client-side half is
already covered by the immutable `model_revisions.configuration` snapshot).

1. **`acta_runner/src/run.c`** — after the `/v1/models` id match succeeds:
   - `GET {base_url}/` and parse the catalog `data[]`; find the entry with
     `id == model_identifier`.
   - Log a new event `preflight_passed` (level `info`) with metadata:
     `model_id`, `max_context` (captured from the `/v1/models` entry when
     present — today only `id` is read), and `catalog` = the matched
     entry's `status.args` array + `meta` object, or `null` when the
     fetch/parse failed or no entry matched.
   - **Best-effort contract:** a catalog failure is never an execution
     failure (the engine must not depend on llama.cpp itself; other
     OpenAI-compatible backends have no catalog) — it only records
     `catalog: null`.
2. **`acta_runner/tests/stub_server.{h,c}`** — serve `GET /` with a canned
   catalog entry matching `cfg->model_id` (fixed `args` + `meta`);
   add `catalog_status` to `stub_config_t` (default 200; 404 simulates a
   non-llama backend).
3. **`acta_runner/tests/run/test_run.c`** — add `preflight_passed` to
   `EVT_FULL_SUCCESS`; assert its metadata carries the canned `n_ctx` and
   the `args` array; add one scenario with `catalog_status = 404` where the
   run still completes and the `preflight_passed` metadata holds
   `"catalog":null`.
4. **Docs:** `runner_analysis.md` (pipeline step 3, decision 7 event list,
   implementation notes); `DBDesign.md` (add `preflight_passed` to the event
   vocabulary); `llamacpp_server_contract.md` (document the `GET /` catalog —
   llama.cpp-specific, best-effort audit source).
5. **Verify:** `make test` in `acta_runner/`.

No schema change, no UI work.
