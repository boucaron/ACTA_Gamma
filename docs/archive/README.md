# Archive

Completed work logs, kept as the record — not live state. Nothing in this
directory has open work; the live documents stay in `docs/`.

| File | What it is | State |
|---|---|---|
| `tasks_continuation.md` | Task continuations: `db exec` → `db init`, the `acta_runner check` health action, `db migrate` migrations, and the 2026-09-26 docs consistency audit | Every session **done and verified green** (`make all` / `make test` / `make gui`); the audit's 10 fixes were all applied |
| `backend_dialects.md` | The backend-dialect **plan** (`llamacpp` / `openai`): §2 dialect matrix, §3.2 where the dialect comes from, §9 the four decisions, §10 phase list | **Implemented, verified and committed (2026-10-12)** — phases 0–6 and 8 in git, phase 7 (GUI) compiled and **closed by the owner 2026-10-13**. Still cited as the dialect matrix by `docs/DBDesign.md`, `docs/openai_dialect_contract.md` and `docs/runner_contract.md` |
| `session_dialect_progress.md` | The dialect implementation record, §1–§19 (§1–§17 are the historical work record, several sections read "IN FLIGHT" about phases that later landed) | Done and green. Read **§18** (first live `llama-server` run: `check` sends the resolved api key; the schema prompt names `label`/`confidence`) and **§19** (docs stale-content cleanup + the green WSL re-verification: clean rebuild, 14 runner suites 791 checks / 0 failures) |
| `session_strata_backend.md` | The Strata + WSL-over-NAT investigation that produced **issue A** (the two `response_format` envelopes) and **issue B** — exactly what `backend_dialects.md` resolves | Done. Task list state: §3.1; live re-verification on both platforms: §2.5 |

## Move note (2026-10-13)

`docs/plans/backend_dialects.md` (the `docs/plans/` directory is gone) and the
two `docs/session_*.md` logs moved here, because none of them has open work
any more. **Only the live links were rewritten.** Historical sentences inside
these records that mention `docs/plans/backend_dialects.md` or
`docs/session_*.md` are left exactly as they were written — they describe where
the file was at the time, and they are the record.

Current live status lives in `docs/status.md` and `docs/runner_contract.md`;
these files are the history behind them.
