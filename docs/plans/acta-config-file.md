# Plan — optional config file: API key + database path

Status: **open — not scheduled, no code work started.** This is a recorded
future constraint, not a current requirement. It becomes relevant only if
ACTA Gamma outgrows single-user, single-machine use.

## Context

Two settings today have no per-machine file source:

- **The API key.** `$OPENAI_API_KEY` is the **only** key source (owner
  decision, `docs/runner_contract.md` decision 4; history in the git log —
  `api_key` was removed from the model `configuration` blob and the
  `--api_key` CLI flag). For a single user on one machine this is the right
  answer: no key in the DB, no key in argv, one conventional secret
  channel. Set it in `.bashrc` and move on.
- **The database path.** Resolution is `--db` → `$ACTA_DB` → the shared
  app-data default (`%APPDATA%\ACTA Gamma\acta.db` /
  `~/.local/share/ACTA Gamma/acta.db`, the same file for GUI, CLI, and
  runner) → `./acta.db` last resort (single source of truth:
  `docs/cli_spec.md`). The GUI additionally has a *Choose database file*
  dialog (remembered in QSettings) for non-default setups.

The constraint: both settings live in the launching shell/process context
(`.bashrc` exports, dialog choice). If ACTA ever runs beyond that context —
a shared server, a service unit without a shell environment, several users
on one machine — then "export it in `.bashrc`" stops being the sane
operational story. A config file with explicit file permissions, read by
all three binaries, would be the more operationally sound source.

## Trigger (when this becomes worth doing)

Any of:

- ACTA Gamma is run as a service (systemd / launchd / scheduled task) where
  per-user shell environments do not exist;
- several users share one machine (and therefore one default DB path) but
  need different backend keys;
- the backend is remote (non-localhost) and the key is a real secret that
  should live in a `0600` file the operator controls, not in a login
  shell's exported environment.

Until then: do nothing. `$OPENAI_API_KEY` and the current DB-path
resolution stay as-is.

## Target contract (proposed, to be refined when scheduled)

- A single per-machine file, e.g. `ACTA Gamma.conf` in the same app-data
  directory as the default DB file, holding **at most**:
  - the API key;
  - the database file path.
- **Permissions are part of the contract:** `0600` (owner read/write only)
  on POSIX, and the equivalent single-user ACL on Windows. The tools refuse
  to read a file with group/other read bits set (fail closed, same spirit
  as the current "unset → hard error" key policy).
- **Key precedence:** `$OPENAI_API_KEY` (if set) wins over the file; the
  file is a fallback, not a second channel. Unset env + missing/unreadable
  file = the existing `ACTA_RUNNER_ERROR` "OPENAI_API_KEY is not set" hard
  error.
- **DB-path precedence:** `--db` → `$ACTA_DB` → **config file** → shared
  app-data default → `./acta.db` last resort. The file is consulted only
  after explicit flags/vars, so a stray file can never silently retarget a
  run that named its DB. The GUI's *Choose database file* dialog (QSettings)
  stays above the file: an explicit operator choice in the GUI wins.
- **One resolution path:** the file is read by all three binaries
  (`acta_cli`, `acta_runner`, `acta_gui`) through the same helper, so the
  three-way "same default file out of the box" guarantee
  (`docs/cli_spec.md`) extends to "same file *and* same key out of the
  box".
- **Scope of the file:** the key and the DB path only. Backend URL, model
  id, and configuration stay in the DB model record — the file exists to
  hold per-machine operator settings, not to duplicate the DB.
- The key policy is applied at the same entry points as today (`cmd_run`
  in `acta_runner`, `runnerWorker` in the GUI), **not** inside
  `run_execution`, so the shared pipeline and its direct-call test
  scenarios stay unchanged.
- Windows note: `_putenv` cannot set/remove env vars reliably (see the
  `test_api_key` suite notes), which is exactly why a file-based source is
  a cleaner fit there than env tricks.

## Work items (TBD — none started)

1. File format + parser (key + optional `db` path; reject unknown/extra
   content with the same fail-closed error style as the model
   `configuration` blob).
2. Key precedence policy helper shared by `acta_runner` and `acta_gui`
   (mirror of `runner_api_key_status`).
3. DB-path resolution: insert the file into `acta_dbpath.c` (both copies)
   and `MainWindow::defaultDbPath` as the step between `$ACTA_DB` and the
   app-data default; keep the `acta_cli/tests/dbpath` suite as the pinning
   mechanism (it already asserts the exact per-platform default string and
   resolution order).
4. Permission checks per platform (POSIX `stat` mode bits; Windows ACL /
   `icacls`-equivalent check) — refuse group/other-readable files.
5. GUI surface: same resolution as the runner (no separate GUI config;
   dialog choice stays on top).
6. Tests: env-set-wins, file-fallback, bad-permissions refusal, missing
   file + unset env → existing hard error; DB-path order with/without the
   file.
7. Docs: README "Environment variables" section, `cli_spec.md` (the DB-path
   and key contracts), and `runner_contract.md` decision 4 amended.

## Deliberately out of scope

- No multi-user accounts, per-user key isolation, or authorization layer —
  that belongs to a higher-level application built on top (see
  `docs/PointOfView.md`, "What ACTA Gamma is not").
- No key rotation, no key server, no secrets manager integration.
- No per-entity settings in the file (model URLs, timeouts, …) — those
  belong to the DB.
- Nothing changes until the trigger above is real; the POC contract
  (`$OPENAI_API_KEY` only, current DB-path resolution) stays as-is.
