#!/usr/bin/env sh
#
# One-off migration: remove the legacy `executions.prompt` column from an
# existing ACTA database.
#
# Background: `executions.prompt` was a nullable column that current code
# never writes (see docs/plans/drop-execution-prompt.md). This script drops
# the column so pre-removal DB files match the new schema
# (acta_gui/db/schema.sql, no `prompt` column).
#
# There is no migration framework by design: run this manually, once, on
# each DB file that predates the removal. The script:
#   * backs up the DB first (<db-file>.bak-<timestamp>);
#   * is idempotent (if the column is already gone, it exits 0);
#   * verifies the drop afterwards.
#
# Usage:
#   ./drop_execution_prompt.sh <db-file>
#
# Fast path needs SQLite >= 3.35.0 (ALTER TABLE ... DROP COLUMN); older
# versions fall back to a table rebuild that preserves all rows and the
# indexes. Needs the `sqlite3` CLI on PATH.

set -eu

if [ $# -ne 1 ]; then
    echo "usage: $0 <db-file>" >&2
    exit 2
fi

db="$1"
[ -f "$db" ] || { echo "no such file: $db" >&2; exit 2; }
command -v sqlite3 >/dev/null || { echo "sqlite3 not found on PATH" >&2; exit 2; }

# Idempotence: nothing to do if the column is already gone.
cols="$(sqlite3 "$db" 'PRAGMA table_info(executions);')"
if ! echo "$cols" | grep -Eq '^[0-9]+\|prompt\|'; then
    echo "executions.prompt already absent in $db — nothing to do"
    exit 0
fi

bak="$db.bak-$(date +%Y%m%d-%H%M%S)"
cp "$db" "$bak"
echo "backup written: $bak"

# Fast path (SQLite >= 3.35.0): plain DROP COLUMN.
if sqlite3 "$db" 'ALTER TABLE executions DROP COLUMN prompt;'; then
    echo "dropped executions.prompt (fast path)"
else
    echo "ALTER TABLE DROP COLUMN failed (SQLite < 3.35.0?) — table rebuild"
    sqlite3 "$db" <<'SQL'
PRAGMA foreign_keys=OFF;
BEGIN;
CREATE TABLE executions_mig (
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
INSERT INTO executions_mig (id, context_id, skill_revision_id, model_revision_id,
                            raw_response, result, status, error, created_at,
                            started_at, completed_at, deleted_at, parent_execution_id)
    SELECT id, context_id, skill_revision_id, model_revision_id,
           raw_response, result, status, error, created_at,
           started_at, completed_at, deleted_at, parent_execution_id
    FROM executions;
DROP TABLE executions;
ALTER TABLE executions_mig RENAME TO executions;
COMMIT;
CREATE INDEX IF NOT EXISTS idx_executions_parent ON executions(parent_execution_id);
CREATE INDEX IF NOT EXISTS idx_executions_context ON executions(context_id);
CREATE INDEX IF NOT EXISTS idx_executions_skill_rev ON executions(skill_revision_id);
CREATE INDEX IF NOT EXISTS idx_executions_model_rev ON executions(model_revision_id);
CREATE INDEX IF NOT EXISTS idx_executions_status_created ON executions(status, created_at);
PRAGMA foreign_keys=ON;
SQL
    echo "dropped executions.prompt (table rebuild)"
fi

# Verify.
cols="$(sqlite3 "$db" 'PRAGMA table_info(executions);')"
if echo "$cols" | grep -Eq '^[0-9]+\|prompt\|'; then
    echo "ERROR: executions.prompt still present — restore from $bak" >&2
    exit 1
fi
n="$(sqlite3 "$db" 'SELECT COUNT(*) FROM executions;')"
echo "OK: executions.prompt removed from $db ($n rows preserved); backup at $bak"
