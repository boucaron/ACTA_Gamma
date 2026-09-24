# Worked example: five personas reviewing one document

A concrete walkthrough of ACTA Gamma's core idea: **one immutable context, several
versioned skills, one model** — each run is an independent, fully auditable
execution, and the results are directly comparable because *only the prompt
varies*.

The scenario: send the project's own `README.md` to five "persona" skills
(`boris_summarize`, `clara_summarize`, `maya_summarize`, `victor_summarize`,
`sam_summarize`) — each a different reviewing persona — against one GGUF model,
and export each observation to its own `.md` file.

The steps below walk through the flow command by command (with real
outputs); the complete annotated script that does everything in one go is
embedded at the end of this page. If you just want to poke around the
database first (no backend needed), start with
[playground.md](playground.md).

## What this example demonstrates

| Claim (see [README](../README.md)) | How this example shows it |
|---|---|
| Comparability across skills/prompts | Five executions share the same context and model revision; only the skill revision differs |
| Explicit revision binding (no "active" flag) | Each `exec create` names a concrete `skill_revision_id` and `model_revision_id` |
| Immutable context | The README snapshot is created once and bound by all five executions |
| Audit trail | Every run leaves an `execution_log` timeline, including the exact prompt sent |
| Replay | Re-running = new executions on the same three ids — inputs identical, outputs not guaranteed identical |

## Prerequisites

- Built binaries (`make all` from the repo root): `acta_cli`, `acta_runner`.
- The working database `acta.db` bundled in this folder (a copy of
  `acta_gui/src/release/acta.db`) — it already contains the five
  `*_summarize` skills and the model
  `JBDRAFTBig_Qwen3.8-27B-GSQ-RCO-IQ3_XXS`. (A fresh `acta.db` has none of
  these; the ids below are therefore specific to *this* database.)
- A running `llama-server` router (e.g. `http://127.0.0.1:8080`) serving that
  model — see the README [Quick start](../README.md#quick-start) for how to
  obtain a GGUF and launch the router.
- The router's API key — the value you passed with `--api-key` when you
  launched `llama-server` — exported as `OPENAI_API_KEY`.

### Bring your own model

`JBDRAFTBig_Qwen3.8-27B-GSQ-RCO-IQ3_XXS` is the author's local favourite —
the one thing in this example you cannot copy. The joke is simple: register
your own model in the database, and adapt **one line** of the script.

```sh
$ acta_cli model create --json '{"name":"my-model","backend":"openai","base_url":"http://127.0.0.1:8080","model_identifier":"my-model.gguf"}'
```

Then find its id with `acta_cli model list --fields id,name` and point
`MODEL_ID` at it in the script. (`model_identifier` must match what the
router actually serves — the GGUF file name in your `--models-dir` — and
`GET /v1/models` on the router shows you what that is.) Everything else —
the five skills, the context, the runs, the audit trail — works unchanged.

## 1. Prepare a working copy of the database

The `acta.db` bundled with this page is a snapshot — never run against it
directly. Copy it (with a proper WAL checkpoint, not a raw file copy) into
your working folder, and point both tools at the copy:

```sh
mkdir acta_runner/MYTEST
sqlite3 docs/examples/acta.db ".backup 'acta_runner/MYTEST/acta.db'"

export ACTA_DB=$PWD/acta_runner/MYTEST/acta.db
export OPENAI_API_KEY=<key of your llama-server router>
```

## 2. Discover the ids you need

Executions bind to explicit ids. Find them:

```sh
$ acta_cli skill list --fields id,name
[{"id":9, "name":"boris_summarize"}, {"id":11, "name":"clara_summarize"},
 {"id":12, "name":"maya_summarize"}, {"id":13, "name":"victor_summarize"},
 {"id":14, "name":"sam_summarize"}, …]

$ acta_cli skill_revision get-latest 9 --id_only
18
$ acta_cli skill_revision get-latest 11 --id_only
19
$ acta_cli skill_revision get-latest 12 --id_only
20
$ acta_cli skill_revision get-latest 13 --id_only
21
$ acta_cli skill_revision get-latest 14 --id_only
22

$ acta_cli model list --fields id,name
[…, {"id":12, "name":"JBDRAFTBig_Qwen3.8-27B-GSQ-RCO-IQ3_XXS"}]

$ acta_cli model_revision get-latest 12 --id_only
18
```

(There is no `active`/`current` flag in the schema — "latest" is simply the
highest revision, and you always point an execution at the revision id you
want.)

Note: the ids below are from *this* database and this run. On your machine
they will differ — use whatever the commands above print for you, and
substitute it in the following steps.

## 3. Create the context and the five executions

```sh
# One immutable snapshot of the README (its SHA-256 hash is derived
# automatically).
$ acta_cli context create --type text --content_file README.md --id_only
33

# All five executions bind the SAME context and the SAME model revision —
# only the skill revision differs, so the five outputs differ by prompt
# alone. That is what makes them comparable.
$ acta_cli exec create --context_id 33 --skill_revision_id 18 --model_revision_id 18 --id_only
64
$ acta_cli exec create --context_id 33 --skill_revision_id 19 --model_revision_id 18 --id_only
65
$ acta_cli exec create --context_id 33 --skill_revision_id 20 --model_revision_id 18 --id_only
66
$ acta_cli exec create --context_id 33 --skill_revision_id 21 --model_revision_id 18 --id_only
67
$ acta_cli exec create --context_id 33 --skill_revision_id 22 --model_revision_id 18 --id_only
68
```

## 4. Run them sequentially and export each observation

Sequential use is the normal pattern — run each one, check the status, and
only move on when it is `completed`:

```sh
$ acta_runner run 64
$ acta_cli exec get 64 --raw_out status
completed
$ acta_cli exec get 64 --raw_out raw_response > boris_summarize.md
```

One run takes a couple of minutes with a ~30 KB context on a local 27B
model, so the five runs total roughly ten minutes — the runner sitting
silent while generating is normal, not a hang. If the model is still
loading, the preflight gets a 503 and the execution is `failed`: wait for
the model to finish loading, then reset and re-run.

If a run does not complete, the recorded error is:

```sh
$ acta_cli exec get 64 --raw_out error
```

Per the runner's design there is no automatic retry: a failed execution is
reset manually with `acta_cli exec reset <id>` and re-run.

## 5. Read the audit trail

Each execution recorded a phase timeline:

```sh
$ acta_cli log list 68 --fields event,created_at
[{"event":"execution_started","created_at":"2026-09-23 20:41:46"},
 {"event":"context_loaded","created_at":"2026-09-23 20:41:46"},
 {"event":"prompt_resolved","created_at":"2026-09-23 20:41:46"},
 {"event":"preflight_passed","created_at":"2026-09-23 20:41:46"},
 {"event":"llm_request","created_at":"2026-09-23 20:41:46"},
 {"event":"llm_response","created_at":"2026-09-23 20:44:04"},
 {"event":"execution_completed","created_at":"2026-09-23 20:44:04"}]
```

The full log (`acta_cli log list 68`) carries the details: the
`prompt_resolved` row contains the exact prompt sent (system = the bound
skill's prompt template, user = the context content verbatim), and the
`llm_response` row the HTTP status, latency (`138150 ms`), and token counts
(`8117` prompt / `5576` completion). That is the "prove exactly what was
sent" property: six months later you can still answer which skill revision,
which context, and which model revision produced each file.

## 6. What you get

Five files in `acta_runner/MYTEST/`, each the verbatim observation of one
persona (abridged here):

| File | Size | Persona angle |
|---|---|---|
| `boris_summarize.md` | 12.7 KB | "boring, proven technology" |
| `clara_summarize.md` | 14.2 KB | — |
| `maya_summarize.md` | 9.2 KB | architecture boundaries |
| `victor_summarize.md` | 20.4 KB | — |
| `sam_summarize.md` | 13.3 KB | "do we actually need this?" |

From `boris_summarize.md`:

> This is what I want to see. A C program that makes one HTTP call, writes a
> row in SQLite, and tells you what happened. No framework, no "intelligent
> orchestration", … You've made the right call by not building an agent
> framework. Agent frameworks are where projects go to die.

Diffing these files *is* the experiment: same input, same model, five prompts
— and every file is traceable to the exact revisions that produced it.

## The complete script

The entire flow, annotated, in one script. Save it as
`acta_runner/MYTEST/run_tests.sh` **inside the folder that holds your
`acta.db` copy** (the script `cd`s into its own folder and uses
`../..` as the repo root, so the layout is:

```text
<repo>
├── acta_cli/acta_cli(.exe)        <- built by make all
└── acta_runner/
    ├── acta_runner(.exe)          <- built by make all
    └── MYTEST/
        ├── acta.db                <- the working copy from step 1
        └── run_tests.sh
```

then run `bash acta_runner/MYTEST/run_tests.sh` with `OPENAI_API_KEY`
exported. The embedded paths assume Windows (`.exe`); on Linux/macOS use
`acta_cli` and `acta_runner` without the suffix:


```bash
#!/usr/bin/env bash
# MYTEST: run the 5 *_summarize skills (latest revisions, resolved at
# run time) against JBDRAFTBig_Qwen3.8-27B-GSQ-RCO-IQ3_XXS on a
# context containing README.md.
set -u

cd "$(dirname "$0")"            # -> acta_runner/MYTEST/
REPO_ROOT="$(cd ../.. && pwd)"   # -> repo root

export ACTA_DB="$PWD/acta.db"

# The API key must be provided by the environment — never hardcoded here.
# (An empty value is allowed only for a keyless localhost server.)
if [[ -z "${OPENAI_API_KEY:-}" && -z "${OPENAI_API_KEY+x}" ]]
then
  echo "ERROR: OPENAI_API_KEY is not set. Export it before running, e.g.:" >&2
  echo "  export OPENAI_API_KEY=<key of your llama-server router>" >&2
  exit 1
fi

CLI="$REPO_ROOT/acta_cli/acta_cli.exe"
RUNNER="$REPO_ROOT/acta_runner/acta_runner.exe"
OUT_DIR="$PWD"

echo "== 1. Create context from README.md =="
CONTEXT_ID=$("$CLI" context create --type text --content_file "$REPO_ROOT/README.md" --id_only)
echo "context_id=$CONTEXT_ID"

# NOTE on revision ids:
# Executions must bind to EXPLICIT revision ids, and there is no
# "latest" flag in the schema — so the ids below are resolved from the
# database instead of being hardcoded (18..22 / 18 in THIS database).
# Run these yourself to see where the ids come from:
#
#   acta_cli skill_revision get-latest 9    # boris_summarize  -> 18
#   acta_cli skill_revision get-latest 11   # clara_summarize  -> 19
#   acta_cli skill_revision get-latest 12   # maya_summarize   -> 20
#   acta_cli skill_revision get-latest 13   # victor_summarize -> 21
#   acta_cli skill_revision get-latest 14   # sam_summarize    -> 22
#   acta_cli model_revision get-latest 12   # JBDRAFTBig ...   -> 18

# The skill parent ids above were found with:
#   acta_cli skill list --fields id,name
SKILL_IDS=(boris_summarize:9 clara_summarize:11 maya_summarize:12 victor_summarize:13 sam_summarize:14)
# The model id above was found with:
#   acta_cli model list --fields id,name
MODEL_ID=12   # JBDRAFTBig_Qwen3.8-27B-GSQ-RCO-IQ3_XXS

declare -A SKILL_REV
for pair in "${SKILL_IDS[@]}"; do
  skill="${pair%%:*}"
  SKILL_REV[$skill]=$("$CLI" skill_revision get-latest "${pair##*:}" --id_only)
  echo "skill '$skill' -> skill_revision_id=${SKILL_REV[$skill]}"
done
MODEL_REVISION_ID=$("$CLI" model_revision get-latest "$MODEL_ID" --id_only)
echo "model id $MODEL_ID -> model_revision_id=$MODEL_REVISION_ID"

echo "== 2. Create executions =="
# All five executions bind the SAME context and the SAME model
# revision — only the skill revision differs, so the five outputs
# differ by prompt alone. That is what makes them comparable.
declare -A EXEC_ID
for skill in boris_summarize clara_summarize maya_summarize victor_summarize sam_summarize; do
  id=$("$CLI" exec create --context_id "$CONTEXT_ID" \
        --skill_revision_id "${SKILL_REV[$skill]}" \
        --model_revision_id "$MODEL_REVISION_ID" --id_only)
  EXEC_ID[$skill]=$id
  echo "exec $id  <- $skill (skill_revision ${SKILL_REV[$skill]}, model_revision $MODEL_REVISION_ID)"
done

echo "== 3. Run each execution sequentially =="
for skill in boris_summarize clara_summarize maya_summarize victor_summarize sam_summarize; do
  id=${EXEC_ID[$skill]}
  echo
  echo "== Run $skill (execution $id) =="
  "$RUNNER" run "$id"
  status=$("$CLI" exec get "$id" --raw_out status)
  echo "status=$status"
  if [[ "$status" != "completed" ]]; then
    "$CLI" exec get "$id" --raw_out error
    echo "ERROR: execution $id ($skill) did not complete; stopping." >&2
    exit 1
  fi
  "$CLI" exec get "$id" --raw_out raw_response > "$OUT_DIR/$skill.md"
  echo "-> saved $OUT_DIR/$skill.md ($(wc -c < "$OUT_DIR/$skill.md") bytes)"
done

echo
echo
echo "== 4. Audit trail for the last run (execution ${EXEC_ID[sam_summarize]}) =="
# One row per pipeline phase. The full log (with the exact prompt sent,
# latency, token counts) is: acta_cli log list ${EXEC_ID[sam_summarize]}
"$CLI" log list "${EXEC_ID[sam_summarize]}" --fields event,created_at

echo
echo "== Done. Outputs in $OUT_DIR =="
ls -la "$OUT_DIR"/*.md
```

## If you don't have these five skills

A skill is just a versioned prompt template. These five are one paragraph
each, describing a reviewing persona — the `acta.db` bundled with this page
contains them all. If you work from a fresh database, create them the same
way any skill is created:

```sh
$ acta_cli skill create --json '{"name":"boris_summarize","prompt_template":"You are Boris, a conservative senior software engineer who values boring, proven technology over clever solutions. When reviewing a software project, README, documentation, or architecture, optimize for simplicity, low maintenance, operational stability, and long-term ownership. Challenge unnecessary complexity, abstractions, dependencies, and trends. Prefer explicit, conventional solutions that future engineers can easily understand and maintain. Think in years, not weeks."}'
```

The other four follow the same one-paragraph pattern:

- **clara_summarize** — developer experience and clarity: "Could a competent engineer understand and use this without asking the author?"
- **maya_summarize** — architecture boundaries: "Will this architecture remain coherent as the system grows?"
- **victor_summarize** — failure modes: "What happens when things go wrong?"
- **sam_summarize** — necessity: "Do we actually need this?"

Each `skill create` gives you a skill id and revision 1; point the script's
`SKILL_IDS` and the `get-latest` calls at your new ids.

## Replay note

Re-running this later is just a new batch of `exec create` with the same
`context_id`, `skill_revision_id`, and `model_revision_id` (optionally with
`--parent_execution_id` linking each new row to the run it replays). The
request inputs will be identical; the outputs are **not** guaranteed to be —
server instance configuration and sampling are not pinned (see
[PointOfView.md](../PointOfView.md), "Replay caveat").
