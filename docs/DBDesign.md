# ACTA Gamma — SQLite Schema

This is a deliberately small first-pass schema. The goal is to model the core execution primitive without prematurely introducing datasets, workflows, providers, or other higher-level concepts.

## The core model

The important relationship is deliberately small:

```text
┌───────────┐
│  Context  │
└─────┬─────┘
      │
      │
      ▼
┌───────────────┐
│   Execution   │◄──── Skill Revision
└───────┬───────┘             ▲
        │                     │
        │                     │
        ▼                     │
      Model ──────────────────┘
```

More precisely:

```text
Context
   +
Skill Revision
   +
Model
   │
   ▼
Execution
   │
   ├── prompt
   ├── raw response
   ├── validated result
   ├── status
   ├── error
   └── logs
```

What do we have is simple:
- Models (folder, model, model revisions)
- Skills (folder, skill, skill revisions)
- Context (context data full self content => for the poc no attachment)
- Execution (auditable execution: given a model, skill, context, generate the output)



## Models

The `models` table represents the configuration needed to invoke an LLM.

For the initial llama.cpp setup:

```text
backend  = openai-compatible
base_url = http://localhost:8080/v1
model    = qwen3-...
```

`configuration` can contain backend-specific JSON without forcing those details into the core schema.

For example:

```json
{
  "temperature": 0,
  "max_tokens": 4096
}
```

The engine should treat the backend as an interchangeable implementation.

```sql
-- ============================================================
-- Model Folders
-- ============================================================

CREATE TABLE model_folders (
    id              TEXT PRIMARY KEY,
    name            TEXT NOT NULL,
    parent_id       TEXT,
    created_at      TEXT NOT NULL,

    UNIQUE (parent_id, name),

    FOREIGN KEY (parent_id)
        REFERENCES model_folders(id)
);

CREATE INDEX idx_model_folders_parent
    ON model_folders(parent_id);

-- ============================================================
-- Models / LLM endpoints
-- ============================================================

CREATE TABLE models (
    id              TEXT PRIMARY KEY,
    folder_id       TEXT,
    name            TEXT NOT NULL,
    backend         TEXT NOT NULL,
    base_url        TEXT NOT NULL,
    model           TEXT NOT NULL,
    configuration   TEXT,
    created_at      TEXT NOT NULL,

    FOREIGN KEY (folder_id)
        REFERENCES model_folders(id)
);

CREATE INDEX idx_models_folder
    ON models(folder_id);

-- ============================================================
-- Model History
-- ============================================================

CREATE TABLE model_revisions (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    model_id        TEXT NOT NULL,
    revision        INTEGER NOT NULL,

    backend         TEXT NOT NULL,
    base_url        TEXT NOT NULL,
    model           TEXT NOT NULL,
    configuration   TEXT,

    created_at      TEXT NOT NULL,

    UNIQUE (model_id, revision),

    FOREIGN KEY (model_id)
        REFERENCES models(id)
);

CREATE INDEX idx_model_revisions_model
    ON model_revisions(model_id);
```


## Skills
Ok basically you have a skill folder, a skill, skill revisions


```sql
-- ============================================================
-- Skill Folders
-- ============================================================

CREATE TABLE skill_folders (
    id              TEXT PRIMARY KEY,
    name            TEXT NOT NULL,
    parent_id       TEXT,
    created_at      TEXT NOT NULL,

    UNIQUE (parent_id, name),

    FOREIGN KEY (parent_id)
        REFERENCES skill_folders(id)
);

CREATE INDEX idx_skill_folders_parent
    ON skill_folders(parent_id);

-- ============================================================
-- Skills
-- ============================================================

CREATE TABLE skills (
    id              TEXT PRIMARY KEY,
    folder_id       TEXT,
    name            TEXT NOT NULL UNIQUE,
    description     TEXT,
    created_at      TEXT NOT NULL,

    FOREIGN KEY (folder_id)
        REFERENCES skill_folders(id)
);

CREATE INDEX idx_skills_folder
    ON skills(folder_id);

CREATE TABLE skill_revisions (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    skill_id        TEXT NOT NULL,
    revision        INTEGER NOT NULL,
    prompt_template TEXT NOT NULL,
    output_schema   TEXT,
    created_at      TEXT NOT NULL,

    UNIQUE (skill_id, revision),

    FOREIGN KEY (skill_id)
        REFERENCES skills(id)
);
```

## Contexts

Just a bunch of data to feed the LLM, for the initial part it is content, a type 
No attachment, no image for the start, pure text.

For the POC, I'd keep the actual content in SQLite:

```text
contexts
├── id
├── type
├── content
├── content_hash
├── metadata
└── created_at
```

`type` remains generic.

Examples:

```text
text
document
code
json
review
incident
custom
```

The engine does not need to interpret these.

`content_hash` gives you stable identity and allows deduplication.

Later, if large contexts become inconvenient to store directly, the storage implementation can evolve toward content-addressed blobs or external references without changing the conceptual model.


```sql

-- ============================================================
-- Contexts
-- ============================================================

CREATE TABLE contexts (
    id              TEXT PRIMARY KEY,
    type            TEXT NOT NULL,
    content         TEXT NOT NULL,
    content_hash    TEXT NOT NULL UNIQUE,
    metadata        TEXT,
    created_at      TEXT NOT NULL
);
```

## Executions

This is where we put the things together, an execution is a given:
- model
- skill
- context

The execution has a status (pending, running, completed, failed).
There is an additional table to store the execution_logs ==> not the app logs.

### Status

Keep the initial status vocabulary small:

```text
pending
running
completed
failed
```

A failed execution remains in the database.

That is important because failures are part of the experiment history.


### Logs

`execution_logs` is intentionally different from application logging.

It records meaningful execution events:

```text
execution_started
context_loaded
prompt_resolved
llm_request
llm_response
validation_started
validation_failed
execution_completed
execution_failed
```

This makes the UI able to display a timeline without parsing application log files.


### Why keep `raw_response` and `result` separate?

This is important for auditability.

A model can return something that cannot be parsed or validated.

```text
LLM
 │
 ▼
raw_response
 │
 ▼
parse / validate
 │
 ├── success ──► result
 │
 └── failure ──► error
```

The original response should survive the validation failure.

That means an unsuccessful execution can still be inspected.

### Why store the prompt?

The skill revision contains the template, but the actual execution has a **resolved prompt**.

For example:

```text
Skill revision:

"Analyze the following context for security issues:

{{ context }}"
```

The execution stores:

```text
"Analyze the following context for security issues:

<actual context>"
```

This makes the execution self-describing and protects the audit trail if prompt-resolution behavior changes later.


```sql
-- ============================================================
-- Executions
-- ============================================================

CREATE TABLE executions (
    id                  TEXT PRIMARY KEY,

    context_id          TEXT NOT NULL,
    skill_revision_id   INTEGER NOT NULL,
    model_id            TEXT NOT NULL,

    -- Exact prompt sent to the model
    prompt              TEXT,

    -- What the backend actually returned
    raw_response        TEXT,

    -- Parsed / validated response
    result              TEXT,

    status              TEXT NOT NULL,

    -- Structured error information
    error               TEXT,

    started_at          TEXT,
    completed_at        TEXT,

    FOREIGN KEY (context_id)
        REFERENCES contexts(id),

    FOREIGN KEY (skill_revision_id)
        REFERENCES skill_revisions(id),

    FOREIGN KEY (model_id)
        REFERENCES models(id)
);

-- ============================================================
-- Execution events
-- ============================================================

CREATE TABLE execution_logs (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    execution_id    TEXT NOT NULL,

    level           TEXT NOT NULL,
    event           TEXT NOT NULL,
    message         TEXT,
    metadata        TEXT,

    created_at      TEXT NOT NULL,

    FOREIGN KEY (execution_id)
        REFERENCES executions(id)
);

```


## Things deliberately missing

I would **not** add these yet:

* datasets
* benchmark tables
* workflow definitions
* skill dependencies
* skill-to-skill relationships
* users
* permissions
* organizations
* provider-specific tables
* vector databases
* conversation/session tables
* queues

Those can all be built later if the POC demonstrates that they are actually needed.

The useful question for the first implementation is simply:

> **Can I create a context, define a skill revision, select a model, execute it once, inspect exactly what happened, and replay it?**

If the answer is yes, the schema is doing its job.
