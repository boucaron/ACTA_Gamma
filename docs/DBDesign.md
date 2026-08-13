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
      Model Revision ─────────┘
```

More precisely:

```text
Context
   +
Skill Revision
   +
Model Revision
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
-- Model Folders
CREATE TABLE model_folders (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL,
    parent_id       INTEGER,
    created_at      TEXT NOT NULL,
    updated_at      TEXT,
    deleted_at      TEXT,
    UNIQUE(parent_id, name),
    FOREIGN KEY(parent_id) REFERENCES model_folders(id) ON DELETE CASCADE
);
CREATE INDEX idx_model_folders_parent ON model_folders(parent_id);

-- Models
CREATE TABLE models (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    folder_id       INTEGER,
    name            TEXT NOT NULL,
    backend         TEXT NOT NULL,
    base_url        TEXT NOT NULL,
    model           TEXT NOT NULL,
    configuration   TEXT,
    current_revision INTEGER NOT NULL DEFAULT 0,
    created_at      TEXT NOT NULL,
    updated_at       TEXT,
    deleted_at      TEXT,
    FOREIGN KEY(folder_id) REFERENCES model_folders(id) ON DELETE SET NULL
);
CREATE INDEX idx_models_folder ON models(folder_id);
CREATE UNIQUE INDEX uq_models_folder_name ON models(folder_id, name);
CREATE INDEX IF NOT EXISTS idx_models_name ON models(name);
CREATE INDEX IF NOT EXISTS idx_models_deleted ON models(deleted_at);

-- Model History - snapshot of immutable state
CREATE TABLE model_revisions (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    model_id        INTEGER NOT NULL,
    revision        INTEGER NOT NULL,
    folder_id       INTEGER,
    name            TEXT NOT NULL,
    backend         TEXT NOT NULL,
    base_url        TEXT NOT NULL,
    model           TEXT NOT NULL,
    configuration   TEXT,
    created_at      TEXT NOT NULL,
    updated_at      TEXT,
    deleted_at      TEXT,
    UNIQUE(model_id, revision),
    FOREIGN KEY(model_id) REFERENCES models(id) ON DELETE CASCADE
);
CREATE INDEX idx_model_revisions_model ON model_revisions(model_id);
CREATE INDEX IF NOT EXISTS idx_model_revisions_created ON model_revisions(created_at);

DROP TRIGGER IF EXISTS models_create_initial_revision;
DROP TRIGGER IF EXISTS models_update_revision;
DROP TRIGGER IF EXISTS models_set_current;

CREATE TRIGGER models_create_initial_revision
AFTER INSERT ON models
BEGIN
  INSERT INTO model_revisions(
    model_id, revision, folder_id, name, backend, base_url, model, configuration, created_at, updated_at
  ) VALUES (
    NEW.id,
    1,
    NEW.folder_id,
    NEW.name,
    NEW.backend,
    NEW.base_url,
    NEW.model,
    NEW.configuration,
    datetime('now'),
    datetime('now')
  );
END;

CREATE TRIGGER models_update_revision
AFTER UPDATE OF folder_id, name, backend, base_url, model, configuration ON models
WHEN NEW.folder_id IS NOT OLD.folder_id
   OR NEW.name IS NOT OLD.name
   OR NEW.backend IS NOT OLD.backend
   OR NEW.base_url IS NOT OLD.base_url
   OR NEW.model IS NOT OLD.model
   OR IFNULL(NEW.configuration,'') IS NOT IFNULL(OLD.configuration,'')
BEGIN
  INSERT INTO model_revisions(
    model_id, revision, folder_id, name, backend, base_url, model, configuration, created_at, updated_at
  ) VALUES (
    NEW.id,
    COALESCE((SELECT MAX(revision) FROM model_revisions WHERE model_id = NEW.id),0) + 1,
    NEW.folder_id,
    NEW.name,
    NEW.backend,
    NEW.base_url,
    NEW.model,
    NEW.configuration,
    datetime('now'),
    datetime('now')
  );
END;

CREATE TRIGGER models_set_current
AFTER INSERT ON model_revisions
BEGIN
  UPDATE models SET current_revision = NEW.revision, updated_at = datetime('now') WHERE id = NEW.model_id;
END;


```


## Skills
Ok basically you have a skill folder, a skill, skill revisions


```sql
-- ============================================================
-- Skill Folders
-- ============================================================
CREATE TABLE skill_folders (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL,
    parent_id       INTEGER,
    created_at      TEXT NOT NULL,
    updated_at      TEXT,
    deleted_at      TEXT,
    UNIQUE(parent_id, name),
    FOREIGN KEY(parent_id) REFERENCES skill_folders(id) ON DELETE CASCADE
);

CREATE INDEX idx_skill_folders_parent ON skill_folders(parent_id);

-- ============================================================
-- Skills
-- ============================================================
-- Skills now holds the current versioned data
CREATE TABLE skills (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    folder_id       INTEGER,
    name            TEXT NOT NULL,
    description     TEXT,
    prompt_template TEXT NOT NULL,
    output_schema   TEXT,
    current_revision INTEGER NOT NULL DEFAULT 0,
    created_at      TEXT NOT NULL,
    updated_at      TEXT,
    deleted_at      TEXT,
    FOREIGN KEY(folder_id) REFERENCES skill_folders(id) ON DELETE SET NULL
);

-- Journal mirrors the versioned columns
CREATE TABLE skill_revisions (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    skill_id        INTEGER NOT NULL,
    revision        INTEGER NOT NULL,
    prompt_template TEXT NOT NULL,
    output_schema   TEXT,
    created_at      TEXT NOT NULL,
    updated_at      TEXT,
    deleted_at      TEXT,
    UNIQUE(skill_id, revision),
    FOREIGN KEY(skill_id) REFERENCES skills(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_skill_revisions_skill ON skill_revisions(skill_id);
CREATE INDEX IF NOT EXISTS idx_skill_revisions_skill_rev ON skill_revisions(skill_id, revision);

DROP TRIGGER IF EXISTS skills_insert_revision;
DROP TRIGGER IF EXISTS skills_set_current;

CREATE TRIGGER skills_create_initial_revision
AFTER INSERT ON skills
BEGIN
  INSERT INTO skill_revisions(
    skill_id, revision, prompt_template, output_schema, created_at, updated_at
  ) VALUES (
    NEW.id, 1, NEW.prompt_template, NEW.output_schema, datetime('now'), datetime('now')
  );
END;

CREATE TRIGGER skills_update_revision
AFTER UPDATE OF prompt_template, output_schema ON skills
WHEN NEW.prompt_template IS NOT OLD.prompt_template
   OR IFNULL(NEW.output_schema,'') IS NOT IFNULL(OLD.output_schema,'')
BEGIN
  INSERT INTO skill_revisions(
    skill_id, revision, prompt_template, output_schema, created_at, updated_at
  ) VALUES (
    NEW.id,
    COALESCE((SELECT MAX(revision) FROM skill_revisions WHERE skill_id = NEW.id),0)+1,
    NEW.prompt_template,
    NEW.output_schema,
    datetime('now'),
    datetime('now')
  );
END;

CREATE TRIGGER skills_set_current
AFTER INSERT ON skill_revisions
BEGIN
  UPDATE skills SET current_revision = NEW.revision WHERE id = NEW.skill_id;
END;




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
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    type            TEXT NOT NULL,
    content         TEXT NOT NULL,
    content_hash    TEXT NOT NULL,
    metadata        TEXT,
    created_at      TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_contexts_type ON contexts(type);
CREATE INDEX IF NOT EXISTS idx_contexts_created ON contexts(created_at);
```

## Executions

This is where we put the things together, an execution is a given:
- model revision
- skill revision
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

### Replay ?
It is possible to replay a job, it creates a new job with the same parameters by defaults, or you can use another model
A link is done to the parent from where it comes.


```sql
-- ============================================================
-- Executions
-- ============================================================
CREATE TABLE executions (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    context_id          INTEGER NOT NULL,
    skill_revision_id   INTEGER NOT NULL,
    model_revision_id   INTEGER NOT NULL,
    prompt              TEXT,
    raw_response        TEXT,
    result              TEXT,
    status              TEXT NOT NULL CHECK(status IN ('pending','running','completed','failed')),
    error               TEXT,
    created_at          TEXT NOT NULL,
    started_at          TEXT,
    completed_at        TEXT,
    parent_execution_id INTEGER,
    FOREIGN KEY(context_id) REFERENCES contexts(id) ON DELETE RESTRICT,
    FOREIGN KEY(skill_revision_id) REFERENCES skill_revisions(id) ON DELETE RESTRICT,
    FOREIGN KEY(model_revision_id) REFERENCES model_revisions(id) ON DELETE RESTRICT,
    FOREIGN KEY(parent_execution_id) REFERENCES executions(id) ON DELETE SET NULL;
);

CREATE INDEX idx_executions_parent ON executions(parent_execution_id);
CREATE INDEX IF NOT EXISTS idx_executions_context ON executions(context_id);
CREATE INDEX IF NOT EXISTS idx_executions_skill_rev ON executions(skill_revision_id);
CREATE INDEX IF NOT EXISTS idx_executions_model_rev ON executions(model_revision_id);
CREATE INDEX IF NOT EXISTS idx_executions_status_created ON executions(status, created_at);
CREATE INDEX IF NOT EXISTS idx_executions_completed ON executions(completed_at);

-- ============================================================
-- Execution events
-- ============================================================
CREATE TABLE execution_logs (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    execution_id    INTEGER NOT NULL,
    level TEXT NOT NULL CHECK(level IN ('debug','info','warn','error')),
    event           TEXT NOT NULL,
    message         TEXT,
    metadata        TEXT,
    created_at      TEXT NOT NULL,
    FOREIGN KEY(execution_id) REFERENCES executions(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_execution_logs_execution ON execution_logs(execution_id);
CREATE INDEX IF NOT EXISTS idx_execution_logs_created ON execution_logs(created_at);
CREATE INDEX IF NOT EXISTS idx_execution_logs_event ON execution_logs(event, execution_id);

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
