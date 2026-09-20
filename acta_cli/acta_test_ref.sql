PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE model_folders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    parent_id INTEGER,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT,
    deleted_at TEXT,    
    FOREIGN KEY(parent_id) REFERENCES model_folders(id) ON DELETE RESTRICT
);
INSERT INTO model_folders VALUES(1,'default',NULL,'2026-08-25 14:01:10',NULL,NULL);
INSERT INTO model_folders VALUES(2,'test',NULL,'2026-08-25 20:41:31',NULL,NULL);
INSERT INTO model_folders VALUES(3,'test2',NULL,'2026-08-25 20:41:55',NULL,NULL);
INSERT INTO model_folders VALUES(4,'childTest',3,'2026-08-25 20:42:33',NULL,NULL);
INSERT INTO model_folders VALUES(5,'childTest',NULL,'2026-08-25 20:44:12',NULL,NULL);
INSERT INTO model_folders VALUES(6,'childTest',1,'2026-08-25 20:45:59',NULL,NULL);
INSERT INTO model_folders VALUES(7,'childTest',2,'2026-08-25 20:46:06',NULL,NULL);
INSERT INTO model_folders VALUES(8,'childTest',6,'2026-08-25 20:46:12',NULL,NULL);
CREATE TABLE models (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    folder_id INTEGER,
    name TEXT NOT NULL,
    description TEXT,
    backend TEXT NOT NULL,
    base_url TEXT,
    model_identifier TEXT NOT NULL,
    configuration TEXT,    
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT,
    deleted_at TEXT,
    FOREIGN KEY(folder_id) REFERENCES model_folders(id) ON DELETE RESTRICT
);
INSERT INTO models VALUES(1,NULL,'test-model',NULL,'llamacpp',NULL,'llama-3-8b-v2',NULL,'2026-08-19 08:57:02',NULL,NULL);
INSERT INTO models VALUES(2,NULL,'llama-70b',NULL,'vllm','http://localhost:8000/v1','meta/llama-70b',NULL,'2026-08-25 13:54:54',NULL,NULL);
INSERT INTO models VALUES(3,NULL,'llama-20b',NULL,'vllm','http://localhost:8000/v1','meta/llama-20b',NULL,'2026-08-25 13:55:10',NULL,NULL);
INSERT INTO models VALUES(4,1,'llama-70b',NULL,'vllm','http://localhost:8000/v1','meta/llama-70b',NULL,'2026-08-25 14:01:27',NULL,NULL);
INSERT INTO models VALUES(5,NULL,'test',NULL,'test',NULL,'unique-test-xyz',NULL,'2026-08-25 14:10:00',NULL,NULL);
INSERT INTO models VALUES(6,NULL,'t1',NULL,'t1',NULL,'t1',NULL,'2026-08-25 14:12:05',NULL,NULL);
INSERT INTO models VALUES(7,NULL,'jtest',NULL,'jtest',NULL,'jtest-1',NULL,'2026-08-25 14:14:24',NULL,NULL);
INSERT INTO models VALUES(8,NULL,'jtest2',NULL,'jtest',NULL,'jtest-2',NULL,'2026-08-25 14:15:28',NULL,NULL);
CREATE TABLE model_revisions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    model_id INTEGER NOT NULL,
    revision INTEGER NOT NULL,
    folder_id INTEGER,
    name TEXT NOT NULL,
    description TEXT,
    backend TEXT NOT NULL,
    base_url TEXT,
    model_identifier TEXT NOT NULL,
    configuration TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT,
    deleted_at TEXT,
    UNIQUE(model_id, revision),
    FOREIGN KEY(model_id) REFERENCES models(id) ON DELETE RESTRICT
);
INSERT INTO model_revisions VALUES(1,1,1,NULL,'test-model',NULL,'llamacpp',NULL,'llama-3-8b',NULL,'2026-08-19 08:57:02','2026-08-19 08:57:02',NULL);
INSERT INTO model_revisions VALUES(2,1,2,NULL,'test-model',NULL,'llamacpp',NULL,'llama-3-8b-v2',NULL,'2026-08-19 08:57:02','2026-08-19 08:57:02',NULL);
INSERT INTO model_revisions VALUES(3,1,3,NULL,'test-model',NULL,'llamacpp',NULL,'llama-3-8b-v2',NULL,'2026-08-19 08:57:02','2026-08-19 08:57:02','2026-08-19 08:57:02');
INSERT INTO model_revisions VALUES(4,2,1,NULL,'llama-70b',NULL,'vllm','http://localhost:8000/v1','meta/llama-70b',NULL,'2026-08-25 13:54:54','2026-08-25 13:54:54',NULL);
INSERT INTO model_revisions VALUES(5,3,1,NULL,'llama-20b',NULL,'vllm','http://localhost:8000/v1','meta/llama-20b',NULL,'2026-08-25 13:55:10','2026-08-25 13:55:10',NULL);
INSERT INTO model_revisions VALUES(6,4,1,1,'llama-70b',NULL,'vllm','http://localhost:8000/v1','meta/llama-70b',NULL,'2026-08-25 14:01:27','2026-08-25 14:01:27',NULL);
INSERT INTO model_revisions VALUES(7,5,1,NULL,'test',NULL,'test',NULL,'unique-test-xyz',NULL,'2026-08-25 14:10:00','2026-08-25 14:10:00',NULL);
INSERT INTO model_revisions VALUES(8,6,1,NULL,'t1',NULL,'t1',NULL,'t1',NULL,'2026-08-25 14:12:05','2026-08-25 14:12:05',NULL);
INSERT INTO model_revisions VALUES(9,7,1,NULL,'jtest',NULL,'jtest',NULL,'jtest-1',NULL,'2026-08-25 14:14:24','2026-08-25 14:14:24',NULL);
INSERT INTO model_revisions VALUES(10,8,1,NULL,'jtest2',NULL,'jtest',NULL,'jtest-2',NULL,'2026-08-25 14:15:28','2026-08-25 14:15:28',NULL);
CREATE TABLE skill_folders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    parent_id INTEGER,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT,
    deleted_at TEXT,  
    FOREIGN KEY(parent_id) REFERENCES skill_folders(id) ON DELETE RESTRICT
);
INSERT INTO skill_folders VALUES(1,'authSkill',NULL,'2026-08-25 20:53:59',NULL,NULL);
INSERT INTO skill_folders VALUES(2,'oauthFlow',1,'2026-08-25 20:54:14',NULL,NULL);
CREATE TABLE skills (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    folder_id INTEGER,
    name TEXT NOT NULL,
    description TEXT,
    prompt_template TEXT NOT NULL,
    output_schema TEXT,    
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT,
    deleted_at TEXT,
    FOREIGN KEY(folder_id) REFERENCES skill_folders(id) ON DELETE RESTRICT
);
INSERT INTO skills VALUES(1,2,'summarize_v2','Updated summarizer','Summarize concisely: {{input}}',NULL,'2026-08-19 08:57:02','2026-08-25 21:31:45',NULL);
INSERT INTO skills VALUES(2,NULL,'tata',NULL,'prompt that works',NULL,'2026-08-25 21:11:31',NULL,NULL);
INSERT INTO skills VALUES(3,1,'tata',NULL,'prompt that works',NULL,'2026-08-25 21:19:04',NULL,NULL);
INSERT INTO skills VALUES(4,2,'tata',NULL,'prompt that works',NULL,'2026-08-25 21:19:08',NULL,NULL);
INSERT INTO skills VALUES(5,NULL,'summarize2',NULL,'Summarize: {{input}}',NULL,'2026-08-25 21:20:00',NULL,NULL);
INSERT INTO skills VALUES(6,2,'summarize2',NULL,'Summarize: {{input}}',NULL,'2026-08-25 21:20:29',NULL,NULL);
INSERT INTO skills VALUES(7,2,'summarize4','blabla','Summarize: {{input}}',NULL,'2026-08-25 21:23:16',NULL,NULL);
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
INSERT INTO skill_revisions VALUES(1,1,NULL,'summarize',NULL,1,'Summarize: {{context}}',NULL,'2026-08-19 08:57:02','2026-08-19 08:57:02',NULL);
INSERT INTO skill_revisions VALUES(2,2,NULL,'tata',NULL,1,'prompt that works',NULL,'2026-08-25 21:11:31','2026-08-25 21:11:31',NULL);
INSERT INTO skill_revisions VALUES(3,3,1,'tata',NULL,1,'prompt that works',NULL,'2026-08-25 21:19:04','2026-08-25 21:19:04',NULL);
INSERT INTO skill_revisions VALUES(4,4,2,'tata',NULL,1,'prompt that works',NULL,'2026-08-25 21:19:08','2026-08-25 21:19:08',NULL);
INSERT INTO skill_revisions VALUES(5,5,NULL,'summarize2',NULL,1,'Summarize: {{input}}',NULL,'2026-08-25 21:20:00','2026-08-25 21:20:00',NULL);
INSERT INTO skill_revisions VALUES(6,6,2,'summarize2',NULL,1,'Summarize: {{input}}',NULL,'2026-08-25 21:20:29','2026-08-25 21:20:29',NULL);
INSERT INTO skill_revisions VALUES(7,7,2,'summarize4','blabla',1,'Summarize: {{input}}',NULL,'2026-08-25 21:23:16','2026-08-25 21:23:16',NULL);
INSERT INTO skill_revisions VALUES(8,1,NULL,'summarize','update description',2,'Summarize: {{context}}','whatever','2026-08-25 21:30:55','2026-08-25 21:30:55',NULL);
INSERT INTO skill_revisions VALUES(9,1,2,'summarize_v2','Updated summarizer',3,'Summarize concisely: {{input}}',NULL,'2026-08-25 21:31:45','2026-08-25 21:31:45',NULL);
CREATE TABLE contexts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    type TEXT NOT NULL,
    content TEXT NOT NULL,
    content_hash TEXT NOT NULL,
    metadata TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    deleted_at TEXT
);
INSERT INTO contexts VALUES(1,'text','hello world','abc123',NULL,'2026-08-19 08:57:02',NULL);
INSERT INTO contexts VALUES(2,'text','hello world','',NULL,'2026-08-25 09:51:50',NULL);
INSERT INTO contexts VALUES(3,'text','hello world2','',NULL,'2026-08-25 10:12:21',NULL);
INSERT INTO contexts VALUES(4,'test','some content','',NULL,'2026-08-25 18:30:14',NULL);
INSERT INTO contexts VALUES(5,'test','some content 2','',NULL,'2026-08-25 18:30:19',NULL);
INSERT INTO contexts VALUES(6,'system','You are a helpful assistant.','abc123',NULL,'2026-08-25 18:36:29',NULL);
INSERT INTO contexts VALUES(7,'user','lo','x1',NULL,'2026-08-25 18:37:48',NULL);
CREATE TABLE executions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    context_id INTEGER NOT NULL,
    skill_revision_id INTEGER NOT NULL,
    model_revision_id INTEGER NOT NULL,
    raw_response TEXT,
    result TEXT,
    status TEXT NOT NULL DEFAULT 'pending' CHECK(status IN ('pending','running','completed','failed','cancelled')),
    error TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    started_at TEXT,
    completed_at TEXT,
    deleted_at TEXT,
    parent_execution_id INTEGER,
    FOREIGN KEY(context_id) REFERENCES contexts(id) ON DELETE RESTRICT,
    FOREIGN KEY(skill_revision_id) REFERENCES skill_revisions(id) ON DELETE RESTRICT,
    FOREIGN KEY(model_revision_id) REFERENCES model_revisions(id) ON DELETE RESTRICT,
    FOREIGN KEY(parent_execution_id) REFERENCES executions(id) ON DELETE SET NULL
);
INSERT INTO executions VALUES(1,1,1,2,NULL,NULL,'pending',NULL,'2026-08-19 08:57:02',NULL,NULL,NULL,NULL);
INSERT INTO executions VALUES(2,1,1,1,NULL,NULL,'pending',NULL,'2026-08-25 20:15:45',NULL,NULL,NULL,NULL);
INSERT INTO executions VALUES(3,1,1,2,NULL,NULL,'pending',NULL,'2026-08-25 20:15:48',NULL,NULL,NULL,NULL);
INSERT INTO executions VALUES(4,1,1,2,NULL,NULL,'pending',NULL,'2026-08-25 20:18:06',NULL,NULL,NULL,NULL);
INSERT INTO executions VALUES(5,1,1,2,NULL,NULL,'pending',NULL,'2026-08-25 20:18:44',NULL,NULL,NULL,4);
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
INSERT INTO execution_logs VALUES(1,1,'error','seed',NULL,NULL,'2026-08-25 19:37:26');
INSERT INTO execution_logs VALUES(2,1,'debug','hi',NULL,NULL,'2026-08-25 19:56:02');
PRAGMA writable_schema=ON;
CREATE TABLE IF NOT EXISTS sqlite_sequence(name,seq);
DELETE FROM sqlite_sequence;
INSERT INTO sqlite_sequence VALUES('model_revisions',10);
INSERT INTO sqlite_sequence VALUES('models',8);
INSERT INTO sqlite_sequence VALUES('skill_revisions',9);
INSERT INTO sqlite_sequence VALUES('skills',7);
INSERT INTO sqlite_sequence VALUES('contexts',7);
INSERT INTO sqlite_sequence VALUES('executions',5);
INSERT INTO sqlite_sequence VALUES('model_folders',8);
INSERT INTO sqlite_sequence VALUES('execution_logs',2);
INSERT INTO sqlite_sequence VALUES('skill_folders',2);
CREATE TRIGGER models_create_initial_revision AFTER INSERT ON models BEGIN
  INSERT INTO model_revisions(model_id,revision,folder_id,name,description,backend,base_url,model_identifier,configuration,created_at,updated_at)
  VALUES (NEW.id,1,NEW.folder_id,NEW.name,NEW.description,NEW.backend,NEW.base_url,NEW.model_identifier,NEW.configuration,datetime('now'),datetime('now'));
END;
CREATE TRIGGER models_update_revision AFTER UPDATE OF folder_id,name,description,backend,base_url,model_identifier,configuration ON models
WHEN OLD.deleted_at IS NULL AND (
  NEW.deleted_at IS NULL AND (
    NEW.folder_id IS NOT OLD.folder_id OR
    NEW.name IS NOT OLD.name OR
    NEW.description IS NOT OLD.description OR
    NEW.backend IS NOT OLD.backend OR
    NEW.base_url IS NOT OLD.base_url OR
    NEW.model_identifier IS NOT OLD.model_identifier OR
    NEW.configuration IS NOT OLD.configuration
))
BEGIN
  INSERT INTO model_revisions(model_id,revision,folder_id,name,description,backend,base_url,model_identifier,configuration,created_at,updated_at)
  VALUES (NEW.id,COALESCE((SELECT MAX(revision) FROM model_revisions WHERE model_id = NEW.id),0)+1,NEW.folder_id,NEW.name,NEW.description,NEW.backend,NEW.base_url,NEW.model_identifier,NEW.configuration,datetime('now'),datetime('now'));
END;
CREATE TRIGGER models_soft_delete_revision AFTER UPDATE OF deleted_at ON models
WHEN OLD.deleted_at IS NULL AND NEW.deleted_at IS NOT NULL
BEGIN
  INSERT INTO model_revisions(model_id,revision,folder_id,name,description,backend,base_url,model_identifier,configuration,created_at,updated_at,deleted_at)
  VALUES (NEW.id,COALESCE((SELECT MAX(revision) FROM model_revisions WHERE model_id = NEW.id),0)+1,NEW.folder_id,NEW.name,NEW.description,NEW.backend,NEW.base_url,NEW.model_identifier,NEW.configuration,datetime('now'),datetime('now'),NEW.deleted_at);
END;
CREATE TRIGGER skills_create_initial_revision AFTER INSERT ON skills BEGIN
  INSERT INTO skill_revisions(skill_id,revision,folder_id,name,description,prompt_template,output_schema,created_at,updated_at)
  VALUES (NEW.id,1,NEW.folder_id,NEW.name,NEW.description,NEW.prompt_template,NEW.output_schema,datetime('now'),datetime('now'));
END;
CREATE TRIGGER skills_update_revision AFTER UPDATE OF folder_id,name,description,prompt_template,output_schema ON skills
WHEN OLD.deleted_at IS NULL AND (
  NEW.deleted_at IS NULL AND (
    NEW.folder_id IS NOT OLD.folder_id OR
    NEW.name IS NOT OLD.name OR
    NEW.description IS NOT OLD.description OR
    NEW.prompt_template IS NOT OLD.prompt_template OR
    NEW.output_schema IS NOT OLD.output_schema
))
BEGIN
  INSERT INTO skill_revisions(skill_id,revision,folder_id,name,description,prompt_template,output_schema,created_at,updated_at)
  VALUES (NEW.id,COALESCE((SELECT MAX(revision) FROM skill_revisions WHERE skill_id = NEW.id),0)+1,NEW.folder_id,NEW.name,NEW.description,NEW.prompt_template,NEW.output_schema,datetime('now'),datetime('now'));
END;
CREATE TRIGGER skills_soft_delete_revision AFTER UPDATE OF deleted_at ON skills
WHEN OLD.deleted_at IS NULL AND NEW.deleted_at IS NOT NULL
BEGIN
  INSERT INTO skill_revisions(skill_id,revision,folder_id,name,description,prompt_template,output_schema,created_at,updated_at,deleted_at)
  VALUES (NEW.id,COALESCE((SELECT MAX(revision) FROM skill_revisions WHERE skill_id = NEW.id),0)+1,NEW.folder_id,NEW.name,NEW.description,NEW.prompt_template,NEW.output_schema,datetime('now'),datetime('now'),NEW.deleted_at);
END;
DROP TRIGGER IF EXISTS contexts_immutable;
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
CREATE INDEX idx_model_folders_parent ON model_folders(parent_id);
CREATE INDEX idx_models_folder ON models(folder_id);
CREATE UNIQUE INDEX uq_models_root ON models(name) WHERE folder_id IS NULL;
CREATE UNIQUE INDEX uq_models_child ON models(folder_id, name) WHERE folder_id IS NOT NULL;
CREATE UNIQUE INDEX uq_model_folders_root ON model_folders(name) WHERE parent_id IS NULL;
CREATE UNIQUE INDEX uq_model_folders_child ON model_folders(parent_id, name) WHERE parent_id IS NOT NULL;
CREATE INDEX idx_model_revisions_live ON model_revisions(model_id, revision) WHERE deleted_at IS NULL;
CREATE INDEX idx_skills_folder ON skills(folder_id);
CREATE INDEX idx_skill_folders_parent ON skill_folders(parent_id);
CREATE UNIQUE INDEX uq_skill_folders_root ON skill_folders(name) WHERE parent_id IS NULL;
CREATE UNIQUE INDEX uq_skill_folders_child ON skill_folders(parent_id, name) WHERE parent_id IS NOT NULL;
CREATE UNIQUE INDEX uq_skills_root ON skills(name) WHERE folder_id IS NULL;
CREATE UNIQUE INDEX uq_skills_child ON skills(folder_id, name) WHERE folder_id IS NOT NULL;
CREATE INDEX idx_skill_revisions_live ON skill_revisions(skill_id, revision) WHERE deleted_at IS NULL;
CREATE INDEX idx_executions_parent ON executions(parent_execution_id);
CREATE INDEX idx_executions_context ON executions(context_id);
CREATE INDEX idx_executions_skill_rev ON executions(skill_revision_id);
CREATE INDEX idx_executions_model_rev ON executions(model_revision_id);
CREATE INDEX idx_executions_status_created ON executions(status, created_at);
CREATE INDEX idx_executions_completed ON executions(completed_at);
CREATE INDEX idx_executions_model_skill_context ON executions(model_revision_id, skill_revision_id, context_id);
CREATE INDEX idx_execution_logs_execution ON execution_logs(execution_id);
CREATE INDEX idx_execution_logs_created ON execution_logs(created_at);
CREATE INDEX idx_execution_logs_event ON execution_logs(event, execution_id);
PRAGMA writable_schema=OFF;
COMMIT;
