# Replay an execution

A replay in ACTA Gamma is not a special command — it is a **second
`exec create` with the same three ids**: `context_id`, `skill_revision_id`,
and `model_revision_id`. That is what makes replays exact on the input side:
the context is immutable, and both revisions are immutable snapshots, so the
request sent on replay is byte-for-byte the same chat call.

**Input determinism, not output determinism.** Resending identical inputs
does not guarantee identical outputs: model weights, server-instance
configuration, and LLM sampling are not pinned (see
[PointOfView.md](../PointOfView.md), "Replay caveat").

## What you need

- A working DB copy with a **completed** execution to replay
  (`docs/examples/acta.db` has dozens),
- a running `llama-server` router and `OPENAI_API_KEY` — replaying is a real
  run, see [multi-persona-review.md](multi-persona-review.md),
- `ACTA_DB` pointing at your copy.

**Which model your router must serve.** The runner's preflight checks that
the router's `GET /v1/models` lists the `model_identifier` of the model
record the replay is bound to. Every completed execution in the bundled
database is bound to model revision 18 — model record 12,
`model_identifier` `JBDRAFTBig_Qwen3.8-27B-GSQ-RCO-IQ3_XXS`,
`base_url` `http://localhost:8080` — the author's local GGUF. Unless you
happen to have that exact file, you will bring your own model and replay a
**new** first execution run against it, instead of execution 58.

On a fresh working copy, the two cases produce these ids:

| | Bring your own model | Serve the author's model |
|---|---|---|
| Model record / model revision | 13 / 19 | 12 / 18 |
| Original run | 59 | 58 |
| Replay | 60 | 59 |

## 1. The original run

**Bring your own model** — register it, then run a first execution with the
same context and skill revision as execution 58:

```sh
# model_identifier must match the GGUF file name in your --models-dir
$ acta_cli model create --json '{"name":"my-model","backend":"openai","base_url":"http://127.0.0.1:8080","model_identifier":"my-model.gguf"}'
{"id":13}

$ acta_cli model_revision get-latest 13 --id_only
19

$ acta_cli exec create --context_id 31 --skill_revision_id 22 --model_revision_id 19 --id_only
59

$ acta_runner run 59
$ acta_cli exec get 59 --raw_out status
completed
$ acta_cli exec get 59 --raw_out raw_response > original.md
```

**Serve the author's model** — your original run is execution 58:

```sh
$ acta_cli exec get 58 --fields id,context_id,skill_revision_id,model_revision_id,status
{"id":58, "context_id":31, "skill_revision_id":22,
 "model_revision_id":18, "status":"completed"}

$ acta_cli exec get 58 --raw_out raw_response > original.md
```

## 2. Create the replay

Same three ids as the original run, plus `--parent_execution_id` linking the
new row to the one being replayed (optional but recommended — it is how you
read the pair later):

```sh
# Own model (original run 59):
$ acta_cli exec create --context_id 31 --skill_revision_id 22 --model_revision_id 19 --parent_execution_id 59 --id_only
60
```

```sh
# Author's model (original run 58):
$ acta_cli exec create --context_id 31 --skill_revision_id 22 --model_revision_id 18 --parent_execution_id 58 --id_only
59
```

## 3. Run it

```sh
$ acta_runner run 60
$ acta_cli exec get 60 --raw_out status
completed
$ acta_cli exec get 60 --raw_out raw_response > replay.md
```

(If you are serving the author's model, use 59 instead of 60 in these three
commands.)

## 4. Compare the pair

```sh
$ diff original.md replay.md
```

Expect differences: same prompt, same input, but sampling and server state
produce different text. What you *can* assert, from the records alone:

- **Same inputs** — both executions bind context 31, skill revision 22, and
  the same model revision (19 with your own model, 18 with the author's).
  Immutable on all three sides.
- **Which one replays which** — `acta_cli exec get 60 --raw_out
  parent_execution_id` prints `59` (and `exec get 59` prints `58` in the
  author's-model case).
- **Same request, logged twice** — each run's `prompt_resolved` log row
  carries the exact prompt sent; `acta_cli log list 59` and
  `acta_cli log list 60` show two timelines of the same request.

```sh
$ acta_cli log list 60 --fields event,created_at
[{"event":"execution_started", …}, {"event":"context_loaded", …},
 {"event":"prompt_resolved", …}, {"event":"preflight_passed", …},
 {"event":"llm_request", …}, {"event":"llm_response", …},
 {"event":"execution_completed", …}]
```

## Not a replay: retrying a failed run

A replay creates a **new** execution. Re-running a *failed* one is different:
`acta_cli exec reset <id>` moves it `failed → pending` and re-runs **the
same row** (manual retry by design — there is no automatic retry). Use
`exec reset` when you want the original execution to succeed, and `exec
create` when you want a second, linked observation.

## Where to go next

- [multi-persona-review.md](multi-persona-review.md) — the full run flow,
  including "Bring your own model".
- `docs/runner_contract.md` — the pipeline and its guarantees.
- `docs/PointOfView.md` — the full "Replay caveat" treatment.
