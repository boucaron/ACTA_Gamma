# Plan: close the Windows config-file permission gap

Status: dropped (not implemented). The enforcement attempt was abandoned:
MinGW's import libs lack the standard self-relative-DACL conversion
APIs, and the hand-rolled ACE walk kept failing (self-relative
mis-detection → segfault). Windows config-file protection is declared
the owner's responsibility in README / cli_spec / runner_contract;
the pre-change behavior stands (POSIX `0600` hard error; Windows
warn-and-read).

## Context

The per-machine config file `ACTA_Gamma.conf` may hold an `api_key`
at rest, so the README says "treat it as sensitive". Today the gate is
platform-asymmetric:

- **POSIX:** a file that is not user-only readable (mode not `0600`) is
  refused — a fail-closed hard error.
- **Windows:** "a warning is printed and the file is read anyway"
  (README; `docs/cli_spec.md` DB-file paragraph: "the mode bits are
  meaningless and the `0600` check only warns, so the file is read
  anyway — best-effort, not enforced").

On a shared Windows workstation a coworker can `type
C:\Users\Alice\AppData\Roaming\ACTA_Gamma\ACTA_Gamma.conf` and read the
key. The warning is cosmetic. All three binaries read the file through
the shared helper (`acta_conf_*` in `acta_db/src/conf.c`), so the fix
belongs there, once, for `acta_cli`, `acta_runner`, and `acta_gui`.

## Goal

On Windows, make "readable by someone other than the owner" a fail-closed
hard error like on POSIX, with an explicit, logged opt-out for operators
who knowingly run on a shared machine.

## Design

1. **Permission check (Windows).** In `acta_conf_read` (the shared
   helper), replace the "mode bits meaningless → warn" path with a real
   check:

   - take the file's security descriptor (`GetFileInformationByClass` /
     `GetACL` via `advapi32`),
   - compute the **Everyone** SID's (`S-1-1-0`) effective rights with
     `GetEffectiveRights` (Vista+; Windows 7/10/11 — within the
     supported baseline),
   - also compute the **current user's** effective rights as a
     sanity check (the file must at least be readable by its owner).

   Result: `ACTA_CONF_OK` (no one but the owner can read),
   `ACTA_CONF_ERR_SHARED` (world/Authenticated-Users-class readable), or
   `ACTA_CONF_ERR` (unreadable/corrupt, as today).

   Fallback: if `GetEffectiveRights` is unavailable (older build,
   restricted environment), keep the current warn-and-read behavior and
   say so in the warning (no silent behavior change).

2. **Behavior: hard error, explicit opt-out.**

   - Default on Windows: `ACTA_CONF_ERR_SHARED` → same fail-closed path
     as POSIX (non-zero exit for CLI/runner, error before the run starts
     in the GUI), message:
     `ACTA_Gamma.conf is readable by other users; fix it
     (icacls <path> /inheritance:clear /grant:r:(USER):(R)) or set
     ACTA_ALLOW_SHARED_CONF=1 if this is intentional.`
   - **Opt-out:** environment variable `ACTA_ALLOW_SHARED_CONF=1`
     (documented as an operator decision, printed as a warning every run —
     no silent override). Env, not a CLI flag: the config-file step of
     resolution happens before per-command flag handling in the GUI path,
     and a flag would be unusable from the GUI.

3. **Set the right ACL on creation.** When the app *creates*
   `ACTA_Gamma.conf` (first `api_key` write / first save), build a
   restrictive DACL (owner read/write only) via `SetEntriesInAcl` so
   fresh files are safe by default instead of inheriting the profile
   folder's ACL.

4. **Error surfacing.** `ACTA_CONF_ERR_SHARED` gets its own distinct
   stderr line and CLI exit code (reuse exit 4 "invalid usage/config"
   class, per `docs/cli_spec.md` error table) so scripts can distinguish
   "shared file" from "malformed file".

## Files & docs

- `acta_db/src/conf.c` (+ a small `conf_win.c` for the advapi32 path):
  new status value, Everyone-SID check, opt-out handling; header note in
  `conf.h`.
- `acta_cli` / `acta_runner`: map the new status to the existing
  fail-closed error paths (they already refuse malformed files).
- `acta_gui` (`MainWindow` config handling): same mapping; run refuses to
  start with the message shown in the run result.
- `README.md` "Environment variables and the per-machine config file":
  "on Windows a warning is printed and the file is read anyway" →
  "on Windows a file readable by other users is refused the same way;
  set `ACTA_ALLOW_SHARED_CONF=1` to override with a per-run warning".
- `docs/cli_spec.md` DB-file paragraph and `docs/runner_contract.md`
  decision 4: replace "best-effort, not enforced" with the enforced
  contract + opt-out.
- `docs/DBDesign.md` durability/sensitivity section: one sentence on the
  Windows ACL rule.
- `docs/status.md`: Done note when landed.

## Test plan

- `acta_cli/tests/conf` (new scenarios):
  - Windows CI/dev box: create a temp config, clear inheritance and
    grant `Everyone` read via `icacls`/`SetEntriesInAcl` → `db`-path
    resolution / a `model list` run fails with the shared-file message
    and the distinct exit code;
  - `ACTA_ALLOW_SHARED_CONF=1` → warning printed, run proceeds;
  - owner-only ACL → clean pass;
  - `GetEffectiveRights` unavailable → warn-and-read fallback preserved
    (skip-guard when the OS can't provide it, no false red in CI).
- POSIX paths unchanged: existing `0600` tests in
  `acta_cli/tests/conf` still pass.
- GUI smoke: run on a shared-readable file refuses with the message;
  with `ACTA_ALLOW_SHARED_CONF=1` it warns and runs.
- `make test` full suite green; `make gui` build green.

## Compatibility / migration

- **One-time impact on existing Windows users:** a config file that is
  currently readable by other users will suddenly be refused. Document
  the `icacls` one-liner in the error message and in `docs/building.md`
  (or the README paragraph) so the fix is one command.
- `ACTA_ALLOW_SHARED_CONF` is new surface; add it to the README
  environment list.

## Open questions

1. "Readable by other users" = Everyone SID only, or also the local
   `Authenticated Users` / per-user SID of a second account? Default:
   check Everyone + BUILTIN Users; per-user second-account ACLs are
   out of scope (rare on a single-operator tool).
2. Should the opt-out be `ACTA_ALLOW_SHARED_CONF=1` or a `"allow_shared_conf"`
   key inside the config file itself? Default: env var only — putting the
   override inside the sensitive file would be self-referential.

## Rollout (single logical change)

1. `conf.c`/`conf_win.c`: Everyone-SID effective-rights check + fallback.
2. New `ACTA_CONF_ERR_SHARED` status; wire CLI/runner/GUI fail-closed paths
   and the opt-out env var.
3. Restrictive DACL on first creation of the config file.
4. Docs: README, cli_spec, runner_contract, DBDesign, status.
5. Tests: `acta_cli/tests/conf` Windows scenarios + POSIX regression;
   GUI smoke; full suite.
