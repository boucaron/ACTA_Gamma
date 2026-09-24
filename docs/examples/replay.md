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

## 1. Look at the original run

```sh
$ acta_cli exec get 58 --fields id,context_id,skill_revision_id,model_revision_id,status
{"id":58, "context_id":31, "skill_revision_id":22,
 "model_revision_id":18, "status":"completed"}

$ acta_cli exec get 58 --raw_out raw_response > original.md
```

## 2. Create the replay

Same three ids; `--parent_execution_id` links the new row to the one being
replayed (optional but recommended — it is how you read the pair later):

```sh
$ acta_cli exec create --context_id 31 --skill_revision_id 22 --model_revision_id 18 --parent_execution_id 58 --id_only
60
```

## 3. Run it

```sh
$ acta_runner run 60
$ acta_cli exec get 60 --raw_out status
completed
$ acta_cli exec get 60 --raw_out raw_response > replay.md
```

## 4. Compare the pair

```sh
$ diff original.md replay.md
```

Expect differences: same prompt, same input, but sampling and server state
produce different text. What you *can* assert, from the records alone:

- **Same inputs** — both executions bind context 31, skill revision 22,
  model revision 18. Immutable on all three sides.
- **Which one replays which** — `acta_cli exec get 60 --raw_out parent_execution_id`
  prints `58`.
- **Same request, logged twice** — each run's `prompt_resolved` log row
  carries the exact prompt sent; `acta_cli log list 58` and
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

- [multi-persona-review.md](multi-persona-review.md) — the full run flow.
- `docs/runner_contract.md` — the pipeline and its guarantees.
- `docs/PointOfView.md` — the full "Replay caveat" treatment.
