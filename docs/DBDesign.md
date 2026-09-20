# ACTA Gamma — SQLite Schema

This is a deliberately small first-pass schema. The goal is to model the core execution primitive without prematurely introducing datasets, workflows, providers, or other higher-level concepts.

The canonical, executable copy of this schema is `acta_gui/db/schema.sql` (the same DDL is embedded in `acta_cli/acta_test_ref.sql` and seeded by the runner tests); the SQL blocks below mirror it.

## Scope and assumptions (PoC)

This is a PoC in progress, not a product. Three deliberate non-goals are baked into `acta_db`:

- **No schema versioning or migration framework.** The schema is a first pass applied once at creation (`acta_gui/db/schema.sql`). Existing DB files are opened as-is (`ACTA_DB_OPEN_EXISTING`); if the schema changes, there is no built-in migration path. Manual `ALTER TABLE` recipes (e.g. the soft-delete columns above) are the supported way to move a file forward.
- **No purge / hard delete, by design.** Rows are soft-deleted (`deleted_at`) and never physically removed. If a clean state is genuinely needed, create a new database file rather than purging the existing one.
- **Not thread-safe by design.** One `db_t` is one SQLite connection, owned by a single thread. Concurrency is not shared through the library: the runner uses its own connection, the GUI uses its own connection (the in-app runner thread opens its own handle), and the CLI opens its own. WAL makes cross-process read/write work, but simultaneous writers on the same file are out of scope; there is no `busy_timeout` or retry logic in `acta_db`.
- **One raw-SQL escape hatch.** `acta_db_exec` (`acta_db/include/db.h`) is the only public, non-parameterized path into the connection; every other API is prepared and bound. Its `sql` argument must be static or developer-supplied (DDL, migrations, schema scripts) — never composed from user-supplied input. It is intentionally kept because DDL cannot be parameterized; current callers are the GUI's first-launch schema application and the CLI `db exec` command, both operator-supplied SQL.

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
   ├── prompt (legacy — never written by current code; may hold a
   │   value in rows created before the removal)
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
backend          = openai-compatible
base_url         = http://127.0.0.1:8080
model_identifier = qwen3-...
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
-- (parent_id, name) uniqueness is enforced per level via the partial
-- unique indexes below, not a plain UNIQUE constraint.
CREATE TABLE model_folders (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL,
    parent_id       INTEGER,
    created_at      TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at      TEXT,
    deleted_at      TEXT,
    FOREIGN KEY(parent_id) REFERENCES model_folders(id) ON DELETE RESTRICT
);
CREATE INDEX idx_model_folders_parent ON model_folders(parent_id);
CREATE UNIQUE INDEX uq_model_folders_root ON model_folders(name) WHERE parent_id IS NULL;
CREATE UNIQUE INDEX uq_model_folders_child ON model_folders(parent_id, name) WHERE parent_id IS NOT NULL;

-- Models
CREATE TABLE models (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    folder_id        INTEGER,
    name             TEXT NOT NULL,
    description      TEXT,
    backend          TEXT NOT NULL,
    base_url         TEXT,
    model_identifier TEXT NOT NULL,
    configuration    TEXT,
    created_at       TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at       TEXT,
    deleted_at       TEXT,
    FOREIGN KEY(folder_id) REFERENCES model_folders(id) ON DELETE RESTRICT
);
CREATE INDEX idx_models_folder ON models(folder_id);
CREATE UNIQUE INDEX uq_models_root ON models(name) WHERE folder_id IS NULL;
CREATE UNIQUE INDEX uq_models_child ON models(folder_id, name) WHERE folder_id IS NOT NULL;

-- Model History - snapshot of immutable state
CREATE TABLE model_revisions (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    model_id         INTEGER NOT NULL,
    revision         INTEGER NOT NULL,
    folder_id        INTEGER,
    name             TEXT NOT NULL,
    description      TEXT,
    backend          TEXT NOT NULL,
    base_url         TEXT,
    model_identifier TEXT NOT NULL,
    configuration    TEXT,
    created_at       TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at       TEXT,
    deleted_at       TEXT,
    UNIQUE(model_id, revision),
    FOREIGN KEY(model_id) REFERENCES models(id) ON DELETE RESTRICT
);
-- Live-revision index: revisions that are not soft-deleted.
CREATE INDEX idx_model_revisions_live ON model_revisions(model_id, revision) WHERE deleted_at IS NULL;

DROP TRIGGER IF EXISTS models_create_initial_revision;
DROP TRIGGER IF EXISTS models_update_revision;
DROP TRIGGER IF EXISTS models_soft_delete_revision;

CREATE TRIGGER models_create_initial_revision
AFTER INSERT ON models
BEGIN
  INSERT INTO model_revisions(
    model_id, revision, folder_id, name, description, backend, base_url, model_identifier, configuration, created_at, updated_at
  ) VALUES (
    NEW.id,
    1,
    NEW.folder_id,
    NEW.name,
    NEW.description,
    NEW.backend,
    NEW.base_url,
    NEW.model_identifier,
    NEW.configuration,
    datetime('now'),
    datetime('now')
  );
END;

CREATE TRIGGER models_update_revision
AFTER UPDATE OF folder_id, name, description, backend, base_url, model_identifier, configuration ON models
WHEN OLD.deleted_at IS NULL AND (
    NEW.deleted_at IS NULL AND (
      NEW.folder_id IS NOT OLD.folder_id
      OR NEW.name IS NOT OLD.name
      OR NEW.description IS NOT OLD.description
      OR NEW.backend IS NOT OLD.backend
      OR NEW.base_url IS NOT OLD.base_url
      OR NEW.model_identifier IS NOT OLD.model_identifier
      OR NEW.configuration IS NOT OLD.configuration
    ))
BEGIN
  INSERT INTO model_revisions(
    model_id, revision, folder_id, name, description, backend, base_url, model_identifier, configuration, created_at, updated_at
  ) VALUES (
    NEW.id,
    COALESCE((SELECT MAX(revision) FROM model_revisions WHERE model_id = NEW.id),0) + 1,
    NEW.folder_id,
    NEW.name,
    NEW.description,
    NEW.backend,
    NEW.base_url,
    NEW.model_identifier,
    NEW.configuration,
    datetime('now'),
    datetime('now')
  );
END;

-- A soft delete of a model snapshots a final, deleted_at-carrying revision
-- (hard delete is out of scope: soft delete only).
CREATE TRIGGER models_soft_delete_revision
AFTER UPDATE OF deleted_at ON models
WHEN OLD.deleted_at IS NULL AND NEW.deleted_at IS NOT NULL
BEGIN
  INSERT INTO model_revisions(
    model_id, revision, folder_id, name, description, backend, base_url, model_identifier, configuration,
    created_at, updated_at, deleted_at
  ) VALUES (
    NEW.id,
    COALESCE((SELECT MAX(revision) FROM model_revisions WHERE model_id = NEW.id),0) + 1,
    NEW.folder_id,
    NEW.name,
    NEW.description,
    NEW.backend,
    NEW.base_url,
    NEW.model_identifier,
    NEW.configuration,
    datetime('now'),
    datetime('now'),
    NEW.deleted_at
  );
END;


```


## Skills
Ok basically you have a skill folder, a skill, skill revisions


```sql
-- ============================================================
-- Skill Folders
-- (parent_id, name) uniqueness is enforced per level via the partial
-- unique indexes below, not a plain UNIQUE constraint.
-- ============================================================
CREATE TABLE skill_folders (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL,
    parent_id       INTEGER,
    created_at      TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at      TEXT,
    deleted_at      TEXT,
    FOREIGN KEY(parent_id) REFERENCES skill_folders(id) ON DELETE RESTRICT
);
CREATE INDEX idx_skill_folders_parent ON skill_folders(parent_id);
CREATE UNIQUE INDEX uq_skill_folders_root ON skill_folders(name) WHERE parent_id IS NULL;
CREATE UNIQUE INDEX uq_skill_folders_child ON skill_folders(parent_id, name) WHERE parent_id IS NOT NULL;

-- ============================================================
-- Skills
-- ============================================================
CREATE TABLE skills (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    folder_id       INTEGER,
    name            TEXT NOT NULL,
    description     TEXT,
    prompt_template TEXT NOT NULL,
    output_schema   TEXT,
    created_at      TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at      TEXT,
    deleted_at      TEXT,
    FOREIGN KEY(folder_id) REFERENCES skill_folders(id) ON DELETE RESTRICT
);
CREATE INDEX idx_skills_folder ON skills(folder_id);
CREATE UNIQUE INDEX uq_skills_root ON skills(name) WHERE folder_id IS NULL;
CREATE UNIQUE INDEX uq_skills_child ON skills(folder_id, name) WHERE folder_id IS NOT NULL;

-- ============================================================
-- Skill Revisions
-- ============================================================
CREATE TABLE skill_revisions (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    skill_id        INTEGER NOT NULL,
    folder_id       INTEGER,
    name            TEXT NOT NULL,
    description     TEXT,
    revision        INTEGER NOT NULL,
    prompt_template TEXT NOT NULL,
    output_schema   TEXT,
    created_at      TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at      TEXT,
    deleted_at      TEXT,
    UNIQUE(skill_id, revision),
    FOREIGN KEY(skill_id) REFERENCES skills(id) ON DELETE RESTRICT
);

-- Live-revision index: revisions that are not soft-deleted.
CREATE INDEX idx_skill_revisions_live ON skill_revisions(skill_id, revision) WHERE deleted_at IS NULL;

DROP TRIGGER IF EXISTS skills_create_initial_revision;
DROP TRIGGER IF EXISTS skills_update_revision;
DROP TRIGGER IF EXISTS skills_soft_delete_revision;

CREATE TRIGGER skills_create_initial_revision
AFTER INSERT ON skills
BEGIN
  INSERT INTO skill_revisions(
    skill_id, revision, folder_id, name, description, prompt_template, output_schema, created_at, updated_at
  ) VALUES (
    NEW.id, 1, NEW.folder_id, NEW.name, NEW.description, NEW.prompt_template, NEW.output_schema,
    datetime('now'), datetime('now')
  );
END;

CREATE TRIGGER skills_update_revision
AFTER UPDATE OF folder_id, name, description, prompt_template, output_schema ON skills
WHEN OLD.deleted_at IS NULL AND (
    NEW.deleted_at IS NULL AND (
      NEW.folder_id IS NOT OLD.folder_id
      OR NEW.name IS NOT OLD.name
      OR NEW.description IS NOT OLD.description
      OR NEW.prompt_template IS NOT OLD.prompt_template
      OR NEW.output_schema IS NOT OLD.output_schema
    ))
BEGIN
  INSERT INTO skill_revisions(
    skill_id, revision, folder_id, name, description, prompt_template, output_schema, created_at, updated_at
  ) VALUES (
    NEW.id,
    COALESCE((SELECT MAX(revision) FROM skill_revisions WHERE skill_id = NEW.id),0) + 1,
    NEW.folder_id,
    NEW.name,
    NEW.description,
    NEW.prompt_template,
    NEW.output_schema,
    datetime('now'),
    datetime('now')
  );
END;

-- A soft delete of a skill snapshots a final, deleted_at-carrying revision
-- (hard delete is out of scope: soft delete only).
CREATE TRIGGER skills_soft_delete_revision
AFTER UPDATE OF deleted_at ON skills
WHEN OLD.deleted_at IS NULL AND NEW.deleted_at IS NOT NULL
BEGIN
  INSERT INTO skill_revisions(
    skill_id, revision, folder_id, name, description, prompt_template, output_schema,
    created_at, updated_at, deleted_at
  ) VALUES (
    NEW.id,
    COALESCE((SELECT MAX(revision) FROM skill_revisions WHERE skill_id = NEW.id),0) + 1,
    NEW.folder_id,
    NEW.name,
    NEW.description,
    NEW.prompt_template,
    NEW.output_schema,
    datetime('now'),
    datetime('now'),
    NEW.deleted_at
  );
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

The engine does not need to interpret these. `type` is free form and is deliberately not enforced (owner decision, 2026-07-10).

`content_hash` gives the content a stable identity (a small check today; it is not used for deduplication).

Large contexts are not a problem: a few GB in SQLite is fine, and in the worst case multiple DB files can be used. The storage implementation may still evolve toward content-addressed blobs or external references if that ever becomes useful (owner decision, 2026-07-10).


```sql
-- ============================================================
-- Contexts
-- ============================================================
-- content-immutable; only the deleted_at flag is mutable (soft delete)
-- content_hash is not used for dedup, only a small check
-- type is free form and intentionally not enforced (owner decision, 2026-07-10)
CREATE TABLE contexts (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    type            TEXT NOT NULL,
    content         TEXT NOT NULL,
    content_hash    TEXT NOT NULL,
    metadata        TEXT,
    created_at      TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    deleted_at      TEXT
);

-- Contexts are content-immutable: the trigger allows ONLY the deleted_at
-- flag to change (soft-delete lifecycle); any other column change
-- RAISE(ABORT)s.
DROP TRIGGER IF EXISTS contexts_immutable;
DROP TRIGGER IF EXISTS contexts_soft_delete_only;
CREATE TRIGGER contexts_soft_delete_only
BEFORE UPDATE ON contexts
WHEN (
    NEW.type IS NOT OLD.type
    OR NEW.content IS NOT OLD.content
    OR NEW.content_hash IS NOT OLD.content_hash
    OR NEW.metadata IS NOT OLD.metadata
)
BEGIN
  SELECT RAISE(ABORT, 'contexts are immutable: only deleted_at may change');
END;
```

### Migrating existing databases (soft-delete columns)

Pre-existing DB files created before the soft-delete lifecycle need the
two columns and the trigger replacement (already in the schema above):

```sql
ALTER TABLE contexts   ADD COLUMN deleted_at TEXT;
ALTER TABLE executions ADD COLUMN deleted_at TEXT;
-- plus the contexts_soft_delete_only trigger replacement, see above
```

Existing rows have `deleted_at = NULL`, i.e. they are live. Apply via
`acta_cli db exec` (or `--file`); idempotent check: `PRAGMA table_info`
before applying.

## Executions

This is where we put the things together, an execution is a given:
- model revision
- skill revision
- context

The execution has a status (pending, running, completed, failed, cancelled).
There is an additional table to store the execution_logs ==> not the app logs.

### Status

Keep the initial status vocabulary small:

```text
pending
running
completed
failed
cancelled
```

A failed execution remains in the database.

That is important because failures are part of the experiment history.

The state machine (enforced in the C layer, see `acta_db/include/execution.h`):

```text
pending ──start()──▶ running ──complete()──▶ completed   (terminal)
                         │
                         └──fail()──────────▶ failed ──reset()──▶ pending

pending, running ──cancel()──▶ cancelled   (terminal)
```

`reset()` makes `failed` re-entrant (retry): it clears the failed attempt's
data (`error`, `raw_response`, `started_at`, `completed_at`) but preserves the
`execution_log` audit trail. `completed` and `cancelled` are terminal.

Executions carry a `deleted_at` soft-delete lifecycle: `delete` is allowed from
`pending`/`completed`/`failed`/`cancelled` but not from `running`;
`restore` clears the flag with the status untouched; a deleted execution is
inert — `reset` refuses it (restore first). Listers and counts are live-only
(`deleted_at IS NULL`) by default.


### Logs

`execution_logs` is intentionally different from application logging.

It records meaningful execution events:

```text
execution_started
context_loaded
prompt_resolved
preflight_passed
llm_request
llm_response
validation_started
validation_failed
execution_completed
execution_failed
execution_cancelled   (UI cancel of a pending/running execution)
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

### Why the resolved prompt lives in the log (and why `executions.prompt` is legacy)

The skill revision contains the template, but the actual execution has a **resolved prompt**.

For example:

```text
Skill revision:

"Analyze the following context for security issues:"
```

The resolved prompt for that execution is therefore:

```text
system: "Analyze the following context for security issues:"
user:   <actual context content>
```

This makes the execution self-describing and protects the audit trail if prompt-resolution behavior changes later.

In the schema this is split in two:

* The fully resolved prompt — the actual `system` and `user` messages sent to the backend — is recorded in the `prompt_resolved` event of `execution_logs` (`metadata` carries `system`, `user`, `system_bytes`, `user_bytes`). It is deliberately not duplicated into the `executions` row: contexts are immutable and already referenced by `context_id`, so the log row provides the audit artifact without storing large content in every execution row.
* `executions.prompt` is a **legacy column**: it used to hold an optional, user-entered instruction given at creation time. That input has been removed from the contract (see `docs/plans/drop-execution-prompt.md`): the column is kept in the schema, is never written by current code, and may hold a value only in rows created before the removal. New executions always have `prompt = NULL`; old values remain readable (`exec get`, `--fields prompt`) as audit data, not as an input.

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
    prompt              TEXT,  -- legacy: never written by current code;
                                -- may hold a value in pre-removal rows
    raw_response        TEXT,
    result              TEXT,
    status              TEXT NOT NULL DEFAULT 'pending' 
                         CHECK(status IN ('pending','running','completed','failed','cancelled')),
    error               TEXT,
    created_at          TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    started_at          TEXT,
    completed_at        TEXT,
    deleted_at          TEXT,
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
CREATE INDEX idx_executions_model_skill_context ON executions(model_revision_id, skill_revision_id, context_id);

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
    created_at      TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(execution_id) REFERENCES executions(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_execution_logs_execution ON execution_logs(execution_id);
CREATE INDEX IF NOT EXISTS idx_execution_logs_created ON execution_logs(created_at);
CREATE INDEX IF NOT EXISTS idx_execution_logs_event ON execution_logs(event, execution_id);

```


## What this schema deliberately does not cover

ACTA Gamma takes a serverless, permission-less approach: one private database
file, no server, no accounts, no authorization layer. Everything that would
normally live around that is out of scope for this project — not because the
idea is bad, but because it is not our business. It would belong to a
higher-level application built on top of this building block:

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
* **hard delete** — lifecycle operations are **soft delete only**
  (`deleted_at`) across all entities: skills, models, folders, and now
  contexts and executions.
  A hard-delete API is out of scope (owner decision, 2026-09-13).
  There is no purge either: deleted rows stay in the file forever.
  If a clean state is genuinely needed, create a new database file.

If the POC shows that any of these are actually needed, they can be built
later, on top of the schema — or, in the case of users, permissions, and
organizations, by the application that embeds ACTA Gamma.

The useful question for the first implementation is simply:

> **Can I create a context, define a skill revision, select a model, execute it once, inspect exactly what happened, and replay it?**

If the answer is yes, the schema is doing its job.
