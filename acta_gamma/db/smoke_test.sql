-- Model: create
INSERT INTO models (name, backend, model_identifier) VALUES ('test-model', 'llamacpp', 'llama-3-8b');
SELECT 'model current_revision:', current_revision FROM models WHERE name='test-model';
SELECT 'revision count:', COUNT(*) FROM model_revisions WHERE model_id=1;

-- Model: update
UPDATE models SET model_identifier='llama-3-8b-v2' WHERE name='test-model';
SELECT 'after update current_revision:', current_revision FROM models WHERE name='test-model';
SELECT 'revision count:', COUNT(*) FROM model_revisions WHERE model_id=1;

-- Model: soft-delete
UPDATE models SET deleted_at=datetime('now') WHERE name='test-model';
SELECT 'after delete current_revision:', current_revision FROM models WHERE name='test-model';
SELECT 'deleted revision:', revision FROM model_revisions WHERE model_id=1 AND deleted_at IS NOT NULL;

-- Skill: create
INSERT INTO skills (name, prompt_template) VALUES ('summarize', 'Summarize: {{context}}');
SELECT 'skill current_revision:', current_revision FROM skills WHERE name='summarize';

-- Context: create
INSERT INTO contexts (type, content, content_hash) VALUES ('text', 'hello world', 'abc123');

-- Execution: create
INSERT INTO executions (context_id, skill_revision_id, model_revision_id, status) VALUES (1, 1, 2, 'pending');
SELECT 'execution:', id, status FROM executions;
