# Create and revise a skill

Editing in ACTA Gamma **creates, not modifies**: every `create`, `update`,
or `soft-delete` of a skill (or model) parent snaps a new immutable revision
row via a database trigger. There is no "snapshot" command and no
`active`/`current` flag — the latest revision is simply the latest, and
existing executions keep pointing at the revision they were bound to.

Everything below is DB-only — **no backend, no API key** — except the
optional "run both" step at the end.

## Setup

```sh
mkdir acta_runner/MYTEST
sqlite3 docs/examples/acta.db ".backup 'acta_runner/MYTEST/acta.db'"
export ACTA_DB=$PWD/acta_runner/MYTEST/acta.db
```

## Create a skill

```sh
$ acta_cli skill create --json '{"name":"demo","prompt_template":"Classify the sentiment of the input. Reply with one word."}'
{"id":17}
```

The trigger already snapshotted revision 1 — no separate step:

```sh
$ acta_cli skill get 17 --fields id,name,prompt_template
{"id":17, "name":"demo",
 "prompt_template":"Classify the sentiment of the input. Reply with one word."}

$ acta_cli skill_revision list 17 --fields id,revision
[{"id":25, "revision":1}]
```

## Revise it

`skill update` takes a **partial** payload (at least one field) and inserts
revision 2:

```sh
$ acta_cli skill update 17 --json '{"prompt_template":"Classify the sentiment of the input. Reply with JSON: {\"label\": \"positive\"|\"negative\"}"}'
{"id":17}
```

The **parent** now shows the new prompt:

```sh
$ acta_cli skill get 17 --fields id,prompt_template
{"id":17,
 "prompt_template":"Classify the sentiment of the input. Reply with JSON: {\"label\": \"positive\"|\"negative\"}"}
```

…while **revision 1 still contains the old one**:

```sh
$ acta_cli skill_revision list 17 --fields id,revision
[{"id":25, "revision":1}, {"id":26, "revision":2}]

$ acta_cli skill_revision get 25 --fields prompt_template
{"prompt_template":"Classify the sentiment of the input. Reply with one word."}
```

That is the whole versioning story: the parent row is the current state,
the revision rows are the history, and revisions cannot be edited or
deleted — only read.

## Why bindings matter

Point two executions at the two revisions of the same skill, same context,
same model:

```sh
$ acta_cli exec create --context_id 13 --skill_revision_id 25 --model_revision_id 18 --id_only
59
$ acta_cli exec create --context_id 13 --skill_revision_id 26 --model_revision_id 18 --id_only
60
```

Now execution 59 answers in one word and 60 answers in JSON — the only
difference is the prompt revision. If you run both (backend required, see
[multi-persona-review.md](multi-persona-review.md)), the outputs differ by
prompt revision alone, and each execution's `execution_log` records exactly
which revision was sent.

(Any earlier executions that pointed at revision 1 are untouched — they
still point at 25, whatever the parent now says.)

## Soft-delete also snapshots

```sh
$ acta_cli skill delete 17
{"deleted":true}

$ acta_cli skill_revision list 17 --fields id,revision,deleted_at
[{"id":25,"revision":1,"deleted_at":null},
 {"id":27,"revision":2,"deleted_at":"2026-…"}]
```

A trigger snapshotted a **final revision** carrying `deleted_at` — the last
state of the skill survives its soft-delete. `acta_cli skill restore 17`
brings the parent back; the revision history stays.

## Same for models

`model create` / `model update` / `model delete` work identically, with
`model_revision list <id>` / `get-latest <id>` for the history. Updating a
model's `configuration` blob (e.g. `max_tokens`) inserts a new model
revision the same way.

## Where to go next

- [playground.md](playground.md) — tour the whole database.
- [replay.md](replay.md) — reuse the same binding to replay a run.
- `docs/DBDesign.md` — the triggers and state machine behind all of this.
