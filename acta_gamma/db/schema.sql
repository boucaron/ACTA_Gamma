PRAGMA foreign_keys = ON;
PRAGMA journal_mode=WAL;

CREATE TABLE model_folders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    parent_id INTEGER,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT,
    deleted_at TEXT,    
    FOREIGN KEY(parent_id) REFERENCES model_folders(id) ON DELETE RESTRICT
);


CREATE TABLE models (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    folder_id INTEGER,
    name TEXT NOT NULL,
    description TEXT,
    backend TEXT NOT NULL,
    base_url TEXT NOT NULL,
    model_identifier TEXT NOT NULL,
    configuration TEXT,
    current_revision INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT,
    deleted_at TEXT,
    FOREIGN KEY(folder_id) REFERENCES model_folders(id) ON DELETE RESTRICT
);




CREATE TABLE model_revisions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    model_id INTEGER NOT NULL,
    revision INTEGER NOT NULL,
    folder_id INTEGER,
    name TEXT NOT NULL,
    description TEXT,
    backend TEXT NOT NULL,
    base_url TEXT NOT NULL,
    model_identifier TEXT NOT NULL,
    configuration TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT,
    deleted_at TEXT,
    UNIQUE(model_id, revision),
    FOREIGN KEY(model_id) REFERENCES models(id) ON DELETE RESTRICT
);

CREATE INDEX idx_model_folders_parent ON model_folders(parent_id);
CREATE INDEX idx_models_folder ON models(folder_id);
CREATE UNIQUE INDEX uq_models_root ON models(name) WHERE folder_id IS NULL;
CREATE UNIQUE INDEX uq_models_child ON models(folder_id, name) WHERE folder_id IS NOT NULL;
CREATE UNIQUE INDEX uq_model_folders_root ON model_folders(name) WHERE parent_id IS NULL;
CREATE UNIQUE INDEX uq_model_folders_child ON model_folders(parent_id, name) WHERE parent_id IS NOT NULL;

DROP TRIGGER IF EXISTS models_create_initial_revision;
CREATE TRIGGER models_create_initial_revision AFTER INSERT ON models BEGIN
  INSERT INTO model_revisions(model_id,revision,folder_id,name,description,backend,base_url,model_identifier,configuration,created_at,updated_at)
  VALUES (NEW.id,1,NEW.folder_id,NEW.name,NEW.description,NEW.backend,NEW.base_url,NEW.model_identifier,NEW.configuration,datetime('now'),datetime('now'));
END;

DROP TRIGGER IF EXISTS models_update_revision;
CREATE TRIGGER models_update_revision AFTER UPDATE OF folder_id,name,description,backend,base_url,model_identifier,configuration ON models
WHEN OLD.deleted_at IS NULL AND (
    NEW.folder_id IS NOT OLD.folder_id OR
    NEW.name IS NOT OLD.name OR
    NEW.description IS NOT OLD.description OR
    NEW.backend IS NOT OLD.backend OR
    NEW.base_url IS NOT OLD.base_url OR
    NEW.model_identifier IS NOT OLD.model_identifier OR
    NEW.configuration IS NOT OLD.configuration
)
BEGIN
  INSERT INTO model_revisions(model_id,revision,folder_id,name,description,backend,base_url,model_identifier,configuration,created_at,updated_at)
  VALUES (NEW.id,COALESCE((SELECT MAX(revision) FROM model_revisions WHERE model_id = NEW.id),0)+1,NEW.folder_id,NEW.name,NEW.description,NEW.backend,NEW.base_url,NEW.model_identifier,NEW.configuration,datetime('now'),datetime('now'));
END;


DROP TRIGGER IF EXISTS models_soft_delete_revision;
CREATE TRIGGER models_soft_delete_revision AFTER UPDATE OF deleted_at ON models
WHEN OLD.deleted_at IS NULL AND NEW.deleted_at IS NOT NULL
BEGIN
  INSERT INTO model_revisions(model_id,revision,folder_id,name,description,backend,base_url,model_identifier,configuration,created_at,updated_at,deleted_at)
  VALUES (NEW.id,COALESCE((SELECT MAX(revision) FROM model_revisions WHERE model_id = NEW.id),0)+1,NEW.folder_id,NEW.name,NEW.description,NEW.backend,NEW.base_url,NEW.model_identifier,NEW.configuration,datetime('now'),datetime('now'),NEW.deleted_at);
END;

CREATE TRIGGER models_set_current AFTER INSERT ON model_revisions 
WHEN NEW.deleted_at IS NULL
BEGIN
  UPDATE models SET current_revision = NEW.revision, updated_at = datetime('now') WHERE id = NEW.model_id;
END;

CREATE TABLE skill_folders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    parent_id INTEGER,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT,
    deleted_at TEXT,  
    FOREIGN KEY(parent_id) REFERENCES skill_folders(id) ON DELETE RESTRICT
);


CREATE TABLE skills (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    folder_id INTEGER,
    name TEXT NOT NULL,
    description TEXT,
    prompt_template TEXT NOT NULL,
    output_schema TEXT,
    current_revision INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT,
    deleted_at TEXT,
    FOREIGN KEY(folder_id) REFERENCES skill_folders(id) ON DELETE RESTRICT
);

CREATE TABLE skill_revisions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    skill_id INTEGER NOT NULL,
    folder_id INTEGER,
    name TEXT NOT NULL,
    description TEXT,
    revision INTEGER NOT NULL,
    prompt_template TEXT NOT NULL,
    output_schema TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT,
    deleted_at TEXT,
    UNIQUE(skill_id, revision),
    FOREIGN KEY(skill_id) REFERENCES skills(id) ON DELETE RESTRICT
);


CREATE INDEX idx_skill_folders_parent ON skill_folders(parent_id);
CREATE UNIQUE INDEX uq_skill_folders_root ON skill_folders(name) WHERE parent_id IS NULL;
CREATE UNIQUE INDEX uq_skill_folders_child ON skill_folders(parent_id, name) WHERE parent_id IS NOT NULL;
CREATE UNIQUE INDEX uq_skills_root ON skills(name) WHERE folder_id IS NULL;
CREATE UNIQUE INDEX uq_skills_child ON skills(folder_id, name) WHERE folder_id IS NOT NULL;



DROP TRIGGER IF EXISTS skills_create_initial_revision;
CREATE TRIGGER skills_create_initial_revision AFTER INSERT ON skills BEGIN
  INSERT INTO skill_revisions(skill_id,revision,folder_id,name,description,prompt_template,output_schema,created_at,updated_at)
  VALUES (NEW.id,1,NEW.folder_id,NEW.name,NEW.description,NEW.prompt_template,NEW.output_schema,datetime('now'),datetime('now'));
END;

DROP TRIGGER IF EXISTS skills_update_revision;
CREATE TRIGGER skills_update_revision AFTER UPDATE OF folder_id,name,description,prompt_template,output_schema ON skills
WHEN OLD.deleted_at IS NULL AND (
    NEW.folder_id IS NOT OLD.folder_id OR
    NEW.name IS NOT OLD.name OR
    NEW.description IS NOT OLD.description OR
    NEW.prompt_template IS NOT OLD.prompt_template OR
    NEW.output_schema IS NOT OLD.output_schema
)
BEGIN
  INSERT INTO skill_revisions(skill_id,revision,folder_id,name,description,prompt_template,output_schema,created_at,updated_at)
  VALUES (NEW.id,COALESCE((SELECT MAX(revision) FROM skill_revisions WHERE skill_id = NEW.id),0)+1,NEW.folder_id,NEW.name,NEW.description,NEW.prompt_template,NEW.output_schema,datetime('now'),datetime('now'));
END;


DROP TRIGGER IF EXISTS skills_soft_delete_revision;
CREATE TRIGGER skills_soft_delete_revision AFTER UPDATE OF deleted_at ON skills
WHEN OLD.deleted_at IS NULL AND NEW.deleted_at IS NOT NULL
BEGIN
  INSERT INTO skill_revisions(skill_id,revision,folder_id,name,description,prompt_template,output_schema,created_at,updated_at,deleted_at)
  VALUES (NEW.id,COALESCE((SELECT MAX(revision) FROM skill_revisions WHERE skill_id = NEW.id),0)+1,NEW.folder_id,NEW.name,NEW.description,NEW.prompt_template,NEW.output_schema,datetime('now'),datetime('now'),NEW.deleted_at);
END;

CREATE TRIGGER skills_set_current AFTER INSERT ON skill_revisions 
WHEN NEW.deleted_at IS NULL
BEGIN
  UPDATE skills SET current_revision = NEW.revision, updated_at = datetime('now') WHERE id = NEW.skill_id;
END;

-- immutable
-- content_hash not used for dedup only a small check
-- type not yet enforced, not a design decision for the moment
CREATE TABLE contexts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    type TEXT NOT NULL,
    content TEXT NOT NULL,
    content_hash TEXT,
    metadata TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE executions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    context_id INTEGER NOT NULL,
    skill_revision_id INTEGER NOT NULL,
    model_revision_id INTEGER NOT NULL,
    prompt TEXT,
    raw_response TEXT,
    result TEXT,
    status TEXT NOT NULL DEFAULT 'pending' CHECK(status IN ('pending','running','completed','failed')),
    error TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    started_at TEXT,
    completed_at TEXT,
    parent_execution_id INTEGER,
    FOREIGN KEY(context_id) REFERENCES contexts(id) ON DELETE RESTRICT,
    FOREIGN KEY(skill_revision_id) REFERENCES skill_revisions(id) ON DELETE RESTRICT,
    FOREIGN KEY(model_revision_id) REFERENCES model_revisions(id) ON DELETE RESTRICT,
    FOREIGN KEY(parent_execution_id) REFERENCES executions(id) ON DELETE SET NULL
);
CREATE INDEX IF NOT EXISTS idx_executions_parent ON executions(parent_execution_id);
CREATE INDEX IF NOT EXISTS idx_executions_context ON executions(context_id);
CREATE INDEX IF NOT EXISTS idx_executions_skill_rev ON executions(skill_revision_id);
CREATE INDEX IF NOT EXISTS idx_executions_model_rev ON executions(model_revision_id);
CREATE INDEX IF NOT EXISTS idx_executions_status_created ON executions(status, created_at);
CREATE INDEX IF NOT EXISTS idx_executions_completed ON executions(completed_at);
CREATE INDEX IF NOT EXISTS idx_executions_model_skill_context ON executions(model_revision_id, skill_revision_id, context_id);

CREATE TABLE execution_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    execution_id INTEGER NOT NULL,
    level TEXT NOT NULL CHECK(level IN ('debug','info','warn','error')),
    event TEXT NOT NULL,
    message TEXT,
    metadata TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(execution_id) REFERENCES executions(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_execution_logs_execution ON execution_logs(execution_id);
CREATE INDEX IF NOT EXISTS idx_execution_logs_created ON execution_logs(created_at);
CREATE INDEX IF NOT EXISTS idx_execution_logs_event ON execution_logs(event, execution_id);
