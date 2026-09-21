#ifndef ACTA_CONF_H
#define ACTA_CONF_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * acta_conf_t — per-machine settings parsed from the config file.
 *
 * Source of truth: docs/plans/acta-config-file.md (work item 1).  This is
 * the "shared helper" read by all three binaries; acta_cli and acta_runner
 * call this C parser directly, and the GUI uses an equivalent Qt
 * (QJsonDocument) reader (work item 6) rather than this code.
 *
 * The file holds AT MOST these four operator settings.  Backend URL, model
 * id, and configuration stay in the DB model record — this struct holds
 * per-machine operator settings, not entity data.
 *
 * Ownership:
 *   success (return 0)  -> caller owns every non-NULL string field; call
 *                          acta_conf_free() when done.
 *   failure (return -1) -> the struct is left fully zeroed; free nothing.
 *
 * "0" in max_chars / timeout means "absent from the file"; the caller
 * applies the built-in default (see the plan) in that case.
 */
typedef struct {
    char *api_key;   /* heap copy of "api_key", or NULL when absent */
    char *db;        /* heap copy of "db", or NULL when absent */
    long  max_chars; /* "max_chars" positive integer, or 0 when absent */
    long  timeout;   /* "timeout" positive integer (seconds), or 0 when absent */
} acta_conf_t;

/*
 * Parse a config-file JSON blob into `out`.
 *
 * `blob` must be exactly one well-formed, NUL-terminated JSON document whose
 * root is an object containing AT MOST these keys:
 *     "api_key"   string
 *     "db"        string
 *     "max_chars" positive integer
 *     "timeout"   positive integer
 *
 * Fail-closed, mirroring the model `configuration` blob check in
 * acta_runner/src/run.c: any of the following returns -1 and leaves `out`
 * fully zeroed (the caller frees nothing):
 *     - `blob` is NULL or not well-formed JSON (trailing garbage rejected);
 *     - the root is not an object;
 *     - any top-level key is not one of the four known keys (unknown/typo'd);
 *     - "api_key" / "db" present but not a string;
 *     - "max_chars" / "timeout" present but not a positive integer
 *       (wrong type, fractional, zero, negative, or out of range).
 *
 * Returns 0 on success, -1 on failure.  `err_msg`, if non-NULL, receives a
 * malloc'd one-line diagnostic the caller must free (NULL on success).
 */
int acta_conf_parse(const char *blob, acta_conf_t *out, char **err_msg);

/*
 * Free the string fields owned by a parsed acta_conf_t and zero the numeric
 * fields.  NULL-safe.  The struct itself is a plain POD embedded in the
 * caller; this frees only its heap string fields.
 */
void acta_conf_free(acta_conf_t *conf);

/*
 * API key precedence policy — work item 2 of
 * docs/plans/acta-config-file.md.
 *
 * Precedence: $OPENAI_API_KEY (if set) -> the config file's "api_key".
 * The file is a fallback, not a second channel: an environment variable
 * that is set -- even to the empty string -- always wins over the file.
 *
 *   env_key  - value of getenv("OPENAI_API_KEY") (NULL when unset).
 *   file_key - the file's "api_key" (NULL when the file is missing or
 *              unreadable, or when the key is absent from the file).
 *
 * Returns ACTA_KEY_OK, ACTA_KEY_EMPTY_WARN, or ACTA_KEY_UNSET_ERR and
 * sets *msg to the canonical one-line message (NULL when ACTA_KEY_OK).
 * Mirror of runner_api_key_status() in acta_runner/include/runner_util.h
 * so acta_runner (cmd_run) and acta_gui (runnerWorker) apply one shared
 * policy and cannot drift; the GUI passes the key parsed by its own Qt
 * reader (work item 6) as file_key.
 */
enum { ACTA_KEY_OK = 0, ACTA_KEY_EMPTY_WARN = 1, ACTA_KEY_UNSET_ERR = 2 };

int acta_conf_api_key_status(const char *env_key, const char *file_key,
                             const char **msg);

/*
 * Default location of the config file: the same app-data directory as
 * the default DB file ("ACTA Gamma.conf" next to "acta.db"):
 *   Windows : %APPDATA%\ACTA Gamma\ACTA Gamma.conf
 *   POSIX   : $XDG_DATA_HOME/ACTA Gamma/ACTA Gamma.conf
 *             (else $HOME/.local/share/ACTA Gamma/ACTA Gamma.conf)
 *   base unresolvable -> "./ACTA Gamma.conf" as a last resort.
 * Same platform-base logic as acta_dbpath.c (both copies) -- keep in
 * lockstep. Returns a pointer valid until the next call (static buffer).
 */
const char *acta_conf_default_path(void);

/*
 * Read and parse the config file at `path`.
 *
 *   success (0): the string fields of `conf` are owned by the caller;
 *                 call acta_conf_free() when done.
 *   file missing or unreadable (0): the struct is left fully zeroed and
 *                 *missing is set to 1; free nothing. A missing or
 *                 unreadable file is NOT an error: the file is simply
 *                 unavailable as a fallback.
 *   wrong permissions (-1, POSIX): the file exists but its mode gives
 *                 read access to group or other; fail-closed hard error
 *                 BEFORE the contents are read (the file may hold the
 *                 "api_key" secret and must be 0600, owner read/write
 *                 only).  On Windows (MSYS2/MinGW) the mode-bit check is
 *                 not run -- st_mode is meaningless there (always 0666
 *                 regardless of the NTFS DACL); the DACL check is a
 *                 separate follow-up work item, so the first cut does
 *                 not enforce the permission guarantee on Windows
 *                 (documented as best-effort, not verified).
 *   readable but malformed (-1): fail-closed hard error under exactly the
 *                 rules of acta_conf_parse; the struct is left fully
 *                 zeroed and *err_msg receives a malloc'd one-line
 *                 diagnostic the caller must free.
 *
 * `missing` and `err_msg` may be NULL.
 */
int acta_conf_read(const char *path, acta_conf_t *conf, int *missing,
                   char **err_msg);

/*
 * Built-in defaults for the two per-machine settings that have no
 * flag/env source of their own (docs/plans/acta-config-file.md, work
 * item 4; docs/plans/max-chars-size-check.md):
 *   ACTA_CONF_DEFAULT_MAX_CHARS — maximum total chars of the prompt sent
 *                                 (skill.prompt_template + context.content);
 *   ACTA_CONF_DEFAULT_TIMEOUT   — default per-call HTTP timeout (seconds).
 * The config file may override either; the file supplies defaults, never
 * per-run overrides.
 */
#define ACTA_CONF_DEFAULT_MAX_CHARS 100000
#define ACTA_CONF_DEFAULT_TIMEOUT 300

/*
 * Resolve the prompt size limit: config file -> built-in default.
 * Precedence (docs/plans/acta-config-file.md): "max_chars" (file) ->
 * ACTA_CONF_DEFAULT_MAX_CHARS. There is no env var or CLI flag for
 * max_chars, so the file is the top rung.
 *
 * `conf` is a parsed acta_conf_t (e.g. from acta_conf_read()); it may be
 * fully zeroed when the file was missing or unreadable, or NULL.
 * acta_conf_parse guarantees the value, when present, is a positive
 * integer, so "absent" is exactly 0.
 */
long acta_conf_resolve_max_chars(const acta_conf_t *conf);

/*
 * Resolve the default per-call HTTP timeout: --timeout flag -> config
 * file -> built-in default.
 * Precedence (docs/plans/acta-config-file.md): --timeout (per-run flag)
 * -> "timeout" (file) -> ACTA_CONF_DEFAULT_TIMEOUT. The file supplies
 * the default, never a per-run override.
 *
 * `flag_timeout` is the parsed --timeout value, or 0 when the flag was
 * not given. `conf` as above. Returns a positive number of seconds.
 */
int acta_conf_resolve_timeout(const acta_conf_t *conf, int flag_timeout);

#ifdef __cplusplus
} 
#endif

#endif /* ACTA_CONF_H */
