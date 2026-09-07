
rm engine.db
sqlite3 engine.db < schema.sql
sqlite3 -header -column engine.db < smoke_test.sql

# Expected failures — run separately, you'll see the error message:
sqlite3 engine.db "PRAGMA foreign_keys=ON; UPDATE contexts SET content='changed' WHERE id=1;"
sqlite3 engine.db "PRAGMA foreign_keys=ON; DELETE FROM models WHERE name='test-model';"
