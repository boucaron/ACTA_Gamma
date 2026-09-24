# Playground: poke the database with `acta_cli`

A short hands-on tour of the bundled database. **No backend, no model, no
API key needed** — every command below only reads (or soft-deletes/restores)
rows in the local SQLite file. This is a playground: the DB is a copy, you
are invited to break it, and breaking it teaches you the lifecycle.

Related: [multi-persona-review.md](multi-persona-review.md) — running real
executions against a backend.

## Setup

```sh
mkdir acta_runner/MYTEST
sqlite3 docs/examples/acta.db ".backup 'acta_runner/MYTEST/acta.db'"
export ACTA_DB=$PWD/acta_runner/MYTEST/acta.db
```

Everything below is `acta_cli …` run from the repo root.

## Skills

```sh
$ acta_cli skill list --fields id,name
[{"id":2,"name":"tata"}, … {"id":9,"name":"boris_summarize"},
 {"id":11,"name":"clara_summarize"}, {"id":12,"name":"maya_summarize"},
 {"id":13,"name":"victor_summarize"}, {"id":14,"name":"sam_summarize"}, …]

$ acta_cli skill get 9
{"id":9, "folder_id":12, "name":"boris_summarize",
 "description":"Boris — The Maintainer: Optimizes for boring, stable
 software that is easy to maintain for years.",
 "prompt_template":"You are Boris, a conservative senior software
 engineer who values boring, proven technology over clever solutions. …",
 "output_schema":null, "created_at":"2026-09-05 09:44:51",
 "updated_at":"2026-09-06 18:26:44", "deleted_at":null}
```

`skill get` shows the **parent** row — the live prompt. Its history is in
the revisions:

```sh
$ acta_cli skill_revision list 9
[{"id":12, "skill_id":9, "revision":1, …},
 {"id":18, "skill_id":9, "revision":2, …}]

$ acta_cli skill_revision get-latest 9 --id_only
18
```

Revision rows are immutable snapshots: one per create, per update, per
soft-delete. The parent always shows the current state; the revisions show
what each state was.

## Models

```sh
$ acta_cli model list --fields id,name
[{"id":10,"name":"JBTEST1"}, {"id":11,"name":"JBMT2P_Qwen3.8-27B-…"},
 {"id":12,"name":"JBDRAFTBig_Qwen3.8-27B-GSQ-RCO-IQ3_XXS"}, …]

$ acta_cli model get 12
{"id":12, "name":"JBDRAFTBig_Qwen3.8-27B-GSQ-RCO-IQ3_XXS",
 "backend":"llamacpp", "base_url":"http://localhost:8080",
 "model_identifier":"JBDRAFTBig_Qwen3.8-27B-GSQ-RCO-IQ3_XXS",
 "configuration":null, …}
```

A model record is what the runner needs to make its call: `backend`,
`base_url`, `model_identifier`, plus an optional JSON `configuration`
(`temperature`, `max_tokens`, `top_k`, `supports_response_format`).
`model_revision list 12` / `get-latest 12` work the same way as
`skill_revision`. (The stored `backend` label — here `"llamacpp"` — is
informational; the runner only uses `base_url` to make its HTTP call.)

## Contexts

```sh
$ acta_cli context list --fields id,type
[{"id":1,"type":"text"}, …, {"id":9,"type":"test_summarization"}, …]

$ acta_cli context get 9
{"id":9, "type":"test_summarization", "hash":"…",
 "content":"(first lines of the document)…"}

$ acta_cli context count
31
```

Contexts are immutable: a trigger rejects updates. The only way to change
the input is to create a **new** context — that is what makes replay inputs
exact.

## Executions and the audit trail

The DB already contains 57 finished executions. Pick one:

```sh
$ acta_cli exec list --fields id,skill_revision_id,model_revision_id,status
[{"id":12,"skill_revision_id":12,"model_revision_id":16,"status":"completed"}, …]

$ acta_cli exec get 12 --fields id,context_id,skill_revision_id,model_revision_id,status
{"id":12, "context_id":13, "skill_revision_id":12,
 "model_revision_id":16, "status":"completed"}

$ acta_cli log list 12 --fields event,created_at
[{"event":"execution_started", …}, {"event":"context_loaded", …},
 {"event":"prompt_resolved", …}, {"event":"preflight_passed", …},
 {"event":"llm_request", …}, {"event":"llm_response", …},
 {"event":"execution_completed", …}]
```

`exec get` gives you the binding (which context, skill revision, model
revision) and the recorded result; `log list` gives you the phase timeline,
including the exact prompt that was sent.

## The trash: soft-delete and restore

Rows are never hard-deleted. Try it on a scratch row (a `tata` skill, id 2):

```sh
$ acta_cli skill delete 2
{"deleted":true}

$ acta_cli skill list --fields id,name
[{"id":3,"name":"tata"}, …]              # id 2 no longer shown

$ acta_cli skill list --include_deleted --fields id,name,deleted_at
[{"id":2,"name":"tata","deleted_at":"2026-…"}, …]

$ acta_cli skill restore 2
{"id":2,"restored":true}                 # back

$ acta_cli skill count
15
```

Note what the soft-delete *also* did: a trigger snapshotted a **final
revision** of the skill carrying `deleted_at` — check
`acta_cli skill_revision list 2`. That is why a deleted row is never truly
lost: its last state is a revision row, and `restore` brings the parent
back.

The same `delete` / `restore` actions exist for `model`, `context`, and
`exec`; `list`/`count` all default to live rows and take
`--include_deleted` to opt back in.

## Before you go wild

Take a backup of your working copy before experimenting:

```sh
$ acta_cli db backup --to acta_runner/MYTEST/acta.db.bak
```

The DB file is the data; a backup is the clean-state path (there is no
purge — restoring from a backup file is how you start over).

## Where to go next

- [create-and-revise.md](create-and-revise.md) — create and update a
  skill, and watch the revision snapshot appear.
- [replay.md](replay.md) — reuse a binding to replay a run.
- `docs/cli_spec.md` — every action and flag, with exact output contracts.
- `docs/DBDesign.md` — the schema, triggers, and state machine behind
  what you just poked.
- [multi-persona-review.md](multi-persona-review.md) — the same database,
  now driving real executions.
