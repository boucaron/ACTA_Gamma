# P2 — `--json <blob>` ignored (input-source plumbing)

**Status: done** (`e58d956`).

**Per-suite input-source regression cases: done.**
Added to every JSON-input suite — create for context, model,
model_folder, skill, skill_folder, exec, execution_log, plus skill
update (`tests/*/[entity]_test_create.c`, `tests/skill/skill_test_update.c`).
Each suite now runs the raw argv through `parse_globals` + handler via the
shared `stest_run_argv` helper (`tests/skill/skill_test_helpers.{h,c}`),
covering `--json <blob>` (space + `=` form), `--from_file` (present +
missing, fixture via `stest_write_input`), `--stdin`, and all three
conflicting-source pairs; the former `test_create_json_invalid` stubs are
now real (garbage blob via `--json` → `EXIT_INVALID`).

**Remaining:**
- Bare `--json` still unparseable → P3 #4.
