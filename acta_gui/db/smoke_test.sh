
rm engine.db
sqlite3 engine.db < schema.sql
sqlite3 -header -column engine.db < smoke_test.sql

# Soft delete on contexts and executions (new): setting/clearing deleted_at must
# succeed on live rows (it is an UPDATE, so it never touches FKs).
sqlite3 engine.db "PRAGMA foreign_keys=ON; UPDATE contexts SET deleted_at=datetime('now') WHERE id=1; SELECT 'context deleted_at='||deleted_at FROM contexts WHERE id=1;"
sqlite3 engine.db "PRAGMA foreign_keys=ON; UPDATE contexts SET deleted_at=NULL WHERE id=1; SELECT 'context restored deleted_at='||COALESCE(deleted_at,'NULL') FROM contexts WHERE id=1;"
sqlite3 engine.db "PRAGMA foreign_keys=ON; UPDATE executions SET deleted_at=datetime('now') WHERE id=1; SELECT 'execution deleted_at='||deleted_at FROM executions WHERE id=1;"
sqlite3 engine.db "PRAGMA foreign_keys=ON; UPDATE executions SET deleted_at=NULL WHERE id=1; SELECT 'execution restored deleted_at='||COALESCE(deleted_at,'NULL') FROM executions WHERE id=1;"

# Expected failures — run separately, you'll see the error message:
# 1. Content immutability: the contexts_soft_delete_only trigger aborts any UPDATE
#    of type/content/content_hash/metadata (deleted_at is the only mutable column).
#    Expect:  Error: contexts are immutable: only deleted_at may change
sqlite3 engine.db "PRAGMA foreign_keys=ON; UPDATE contexts SET content='changed' WHERE id=1;"
# 2. Physical delete of a model is refused: model_revisions (and model_folders)
#    reference models with ON DELETE RESTRICT. The lifecycle path is soft delete
#    (deleted_at), not DELETE.
#    Expect:  Error: FOREIGN KEY constraint failed
sqlite3 engine.db "PRAGMA foreign_keys=ON; DELETE FROM models WHERE name='test-model';"
