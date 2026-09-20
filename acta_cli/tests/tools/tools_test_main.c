/* tools_test_main.c — contract test for the `--tools` schema.
 *
 * Verifies the emitted tool schema against its contract
 * (docs/cli_spec.md):
 *   1. both the compact (default) and --pretty outputs are well-formed
 *      JSON — checked with the project's own json layer (json_validate)
 *      and walked with cJSON (the layer json.c is built on);
 *   2. the top-level global section is intact: name/version/usage,
 *      18 global_flags, entity_aliases, 3 input_sources, 7 exit codes,
 *      error contract with the `code == -exit` invariant;
 *   3. the tools array has exactly 74 entries covering all 10 entities
 *      and the full action set from cli_spec.md (incl. the 10 help
 *      actions); exactly the 8 JSON-capable commands carry json_keys;
 *   4. cross-check: for every entry, a command generated from the
 *      entry's own required positionals + required flags (dummy values)
 *      is fed through the raw-argv path (stest_run_argv:
 *      parse_globals → apply_flag_aliases → cmd_args_validate →
 *      handler) and must not be rejected (rc != EXIT_CLI).  This pins
 *      the T3 "flags ⊆ entity_flag_specs" invariant for all 74 entries
 *      and exercises the S2 global-parse seam.  The 8 flags|json
 *      entries are additionally run with a --json blob built from their
 *      json_keys.required, and --stdin/--from_file smoke runs pin the
 *      remaining global input sources.  Per-entry aliases are pinned too
 *      (M6): exec entries carry ["execution"], log entries carry
 *      ["execution_log"], every other entry carries [].
 *
 * The suite consumes the parsed output as data — the 74-entry table is
 * NOT duplicated here; only the small expected sets from cli_spec.md
 * (spec = source of truth for the action inventory) are hardcoded.
 */
#include "test_helpers.h"
#include "json.h"            /* json_validate — the project's own layer */
#include <cjson/cJSON.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REF_DB  "acta_test_ref.db"
#define TOOLS_MAX_LEN (1 << 18)

/* entity → handler, mirrors commands_dispatch's entity_table
 * (dispatch wiring, not schema data) */
static const struct {
    const char *name;
    stest_cmd_fn_t fn;
} entity_fns[] = {
    { "db",               cmd_db },
    { "context",          cmd_context },
    { "model",            cmd_model },
    { "model_folder",     cmd_model_folder },
    { "model_revision",   cmd_model_revision },
    { "skill",            cmd_skill },
    { "skill_folder",     cmd_skill_folder },
    { "skill_revision",   cmd_skill_rev },
    { "exec",             cmd_exec },
    { "log",              cmd_execution_log },
};

/* expected per-entity action sets — cli_spec.md is the source of truth */
static const char *const EXP_DB[]           =
    { "exec", "version", "help" };
static const char *const EXP_CONTEXT[]      =
    { "create", "get", "delete", "restore", "list", "count", "help" };
static const char *const EXP_MODEL[]        =
    { "create", "get", "update", "delete", "restore", "move",
      "list", "count", "help" };
static const char *const EXP_MODEL_FOLDER[] =
    { "create", "get", "list", "count", "rename", "delete",
      "restore", "move", "help" };
static const char *const EXP_MODEL_REV[]    =
    { "get", "get-latest", "list", "count", "help" };
static const char *const EXP_SKILL[]        =
    { "create", "get", "update", "delete", "restore", "move",
      "list", "count", "help" };
static const char *const EXP_SKILL_FOLDER[] =
    { "create", "get", "list", "count", "rename", "delete",
      "restore", "move", "help" };
static const char *const EXP_SKILL_REV[]    =
    { "get", "get-latest", "list", "count", "help" };
static const char *const EXP_EXEC[]         =
    { "create", "get", "delete", "restore", "start", "cancel",
      "complete", "fail", "reset", "set-raw", "list", "count", "help" };
static const char *const EXP_LOG[]          =
    { "create", "get", "list", "count", "help" };

static const struct {
    const char *entity;
    const char *const *actions;
    size_t n_actions;
} expected[] = {
    { "db",            EXP_DB,           3  },
    { "context",       EXP_CONTEXT,      7  },
    { "model",         EXP_MODEL,        9  },
    { "model_folder",  EXP_MODEL_FOLDER, 9  },
    { "model_revision",EXP_MODEL_REV,    5  },
    { "skill",         EXP_SKILL,        9  },
    { "skill_folder",  EXP_SKILL_FOLDER, 9  },
    { "skill_revision",EXP_SKILL_REV,    5  },
    { "exec",          EXP_EXEC,         12 },
    { "log",           EXP_LOG,          5  },
};

/* the 8 JSON-capable commands (T3 audit via resolve_input_source) */
static const char *const EXPECTED_JSON_CAPABLE[] = {
    "context.create",  "model.create",
    "model_folder.create", "skill_folder.create",
    "skill.create",    "skill.update",
    "exec.create",     "log.create",
};

static const char *const EXIT_CODE_KEYS[] =
    { "0", "1", "2", "3", "4", "10", "11" };

/* ── small helpers ───────────────────────────────────────────────── */

static const char *cj_str(cJSON *o, const char *key)
{
    cJSON *v = cJSON_GetObjectItem(o, key);
    return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

static int contains(const char *const *list, size_t n, const char *s)
{
    if (!s) return 0;                     /* NULL = "absent", not a crash */
    for (size_t i = 0; i < n; i++)
        if (strcmp(list[i], s) == 0) return 1;
    return 0;
}

/* per-(entity, action) lookup, NULL-safe on every lookup */
static int entry_exists_safe(cJSON *tools, const char *entity,
                             const char *action)
{
    int n = cJSON_GetArraySize(tools);
    for (int i = 0; i < n; i++) {
        cJSON *e = cJSON_GetArrayItem(tools, i);
        const char *ent = cj_str(e, "entity");
        const char *act = cj_str(e, "action");
        if (ent && act && strcmp(ent, entity) == 0 && strcmp(act, action) == 0)
            return 1;
    }
    return 0;
}

static stest_cmd_fn_t handler_for(const char *entity)
{
    if (!entity) return NULL;
    for (size_t i = 0; i < sizeof(entity_fns) / sizeof(entity_fns[0]); i++)
        if (strcmp(entity_fns[i].name, entity) == 0)
            return entity_fns[i].fn;
    return NULL;
}

/* capture tools_print(stdout, pretty) into buf (NUL-terminated).
 * Returns the byte count, or -1 if it does not fit. */
static int capture_tools(stest_ctx_t *ctx, int pretty,
                         char *buf, size_t cap)
{
    stest_capture_begin(ctx);
    tools_print(stdout, pretty);
    stest_capture_end(ctx);

    const char *s = stest_stdout(ctx);
    size_t n = strlen(s);
    if (n + 1 > cap) return -1;
    memcpy(buf, s, n + 1);
    return (int)n;
}

/* ── shape checks (D7) ───────────────────────────────────────────── */

static void check_shape(stest_ctx_t *ctx, const char *buf, int len, int pretty)
{
    TEST(ctx, len > 0 && buf[len - 1] == '\n');   /* trailing newline */
    if (!pretty) {
        int nl = 0;
        for (int i = 0; i < len; i++)
            if (buf[i] == '\n') nl++;
        TEST_EQ(ctx, nl, 1);                      /* single line */
    } else {
        TEST(ctx, len > 3 && buf[0] == '{' && buf[1] == '\n');
        TEST(ctx, len > 4 && buf[2] == ' ' && buf[3] == ' '); /* 2-space */
    }
}

/* ── global section + inventory (D3, D6) ─────────────────────────── */

static void check_structure(stest_ctx_t *ctx, cJSON *root)
{
    TEST(ctx, cJSON_IsObject(root));
    TEST_STREQ(ctx, cj_str(root, "name"), "acta_cli");
    cJSON *ver = cJSON_GetObjectItem(root, "version");
    TEST(ctx, ver && cJSON_IsNumber(ver));
    TEST_EQ(ctx, ver ? (int)ver->valuedouble : 0, 4);   /* schema v4 */
    TEST_NOT_NULL(ctx, cj_str(root, "usage"));

    cJSON *gf = cJSON_GetObjectItem(root, "global_flags");
    TEST(ctx, cJSON_IsArray(gf));
    TEST_EQ(ctx, cJSON_GetArraySize(gf), 18);

    cJSON *al = cJSON_GetObjectItem(root, "entity_aliases");
    TEST(ctx, cJSON_IsObject(al));
    cJSON *ax = cJSON_GetObjectItem(al, "exec");
    TEST(ctx, ax && cJSON_IsArray(ax) && cJSON_GetArraySize(ax) == 1);
    TEST_STREQ(ctx, (ax && cJSON_GetArraySize(ax) == 1)
        ? cJSON_GetArrayItem(ax, 0)->valuestring : NULL, "execution");
    cJSON *lg = cJSON_GetObjectItem(al, "log");
    TEST(ctx, lg && cJSON_IsArray(lg) && cJSON_GetArraySize(lg) == 1);
    TEST_STREQ(ctx, (lg && cJSON_GetArraySize(lg) == 1)
        ? cJSON_GetArrayItem(lg, 0)->valuestring : NULL, "execution_log");

    cJSON *src = cJSON_GetObjectItem(root, "input_sources");
    TEST(ctx, cJSON_IsArray(src));
    TEST_EQ(ctx, cJSON_GetArraySize(src), 3);
    for (int k = 0; k < 3; k++) {
        cJSON *s = cJSON_GetArrayItem(src, k);
        TEST(ctx, s && cJSON_IsString(s));
        TEST(ctx, contains((const char *const[3]){"json", "stdin", "from_file"},
                           3, s ? s->valuestring : NULL) == 1);
    }

    cJSON *ec = cJSON_GetObjectItem(root, "exit_codes");
    TEST(ctx, cJSON_IsObject(ec));
    int n_ec = 0;                              /* cJSON has no object-size
                                               API — count the child chain */
    if (ec && cJSON_IsObject(ec))
        for (cJSON *it = ec->child; it; it = it->next)
            n_ec++;
    TEST_EQ(ctx, n_ec, 7);
    for (size_t i = 0; i < sizeof(EXIT_CODE_KEYS) / sizeof(EXIT_CODE_KEYS[0]); i++)
        TEST_NOT_NULL(ctx, cJSON_GetObjectItem(ec, EXIT_CODE_KEYS[i]));

    cJSON *err = cJSON_GetObjectItem(root, "error");
    TEST(ctx, cJSON_IsObject(err));
    TEST_STREQ(ctx, cj_str(err, "stream"), "stderr");
    TEST_STREQ(ctx, cj_str(err, "invariant"), "code == -exit");
    TEST_NOT_NULL(ctx, cj_str(err, "line1"));

    /* ── tools array: count + per-entity action coverage ── */
    cJSON *tools = cJSON_GetObjectItem(root, "tools");
    TEST(ctx, cJSON_IsArray(tools));
    TEST_EQ(ctx, cJSON_GetArraySize(tools), 74);

    for (size_t e = 0; e < sizeof(expected) / sizeof(expected[0]); e++)
        for (size_t a = 0; a < expected[e].n_actions; a++)
            TEST(ctx, entry_exists_safe(tools, expected[e].entity,
                                       expected[e].actions[a]));

    /* ── exactly the 8 JSON-capable commands carry json_keys ── */
    int n = cJSON_GetArraySize(tools);
    int n_json = 0;
    for (int i = 0; i < n; i++) {
        cJSON *e = cJSON_GetArrayItem(tools, i);
        const char *cmd = cj_str(e, "command");
        cJSON *jk = cJSON_GetObjectItem(e, "json_keys");
        int has = (jk != NULL && cJSON_IsObject(jk));
        int exp = contains(EXPECTED_JSON_CAPABLE,
                           sizeof(EXPECTED_JSON_CAPABLE) /
                           sizeof(EXPECTED_JSON_CAPABLE[0]), cmd);
        TEST(ctx, has == exp);
        if (has) n_json++;
    }
    TEST_EQ(ctx, n_json, 8);

    /* ── per-entry invariants ── */
    for (int i = 0; i < n; i++) {
        cJSON *e = cJSON_GetArrayItem(tools, i);
        const char *entity = cj_str(e, "entity");
        const char *action = cj_str(e, "action");
        const char *input  = cj_str(e, "input");
        TEST_NOT_NULL(ctx, entity);
        TEST_NOT_NULL(ctx, action);
        if (!entity || !action) continue;   /* schema broken — no crash */

        char want[128];
        snprintf(want, sizeof want, "%s.%s", entity, action);
        TEST_STREQ(ctx, cj_str(e, "command"), want);

        TEST_NOT_NULL(ctx, input);
        TEST(ctx, contains((const char *const[5]){"flags", "flags|json",
            "positional|flags", "positional", "none"}, 5, input) == 1);
        /* input "none" ⇔ the 10 help actions + db.version
         * (a pure-output action). */
        TEST(ctx, (strcmp(action, "help") == 0 ||
                   strcmp(action, "version") == 0) ==
               (input != NULL && strcmp(input, "none") == 0));

        /* P1: structured success (schema v2) — object with a known
         * kind; kind "json" carries a non-empty keys array. */
        cJSON *sc = cJSON_GetObjectItem(e, "success");
        TEST(ctx, cJSON_IsObject(sc));
        const char *kind = sc ? cj_str(sc, "kind") : NULL;
        TEST(ctx, kind != NULL &&
             contains((const char *const[5]){"json", "json_object",
                "json_array", "bare_int", "plain_text"}, 5, kind) == 1);
        if (kind != NULL && strcmp(kind, "json") == 0) {
            cJSON *keys = cJSON_GetObjectItem(sc, "keys");
            TEST(ctx, cJSON_IsArray(keys) && cJSON_GetArraySize(keys) > 0);
            for (int q = 0; q < cJSON_GetArraySize(keys); q++) {
                cJSON *k = cJSON_GetArrayItem(keys, q);
                TEST_NOT_NULL(ctx,
                             (k && cJSON_IsString(k)) ? k->valuestring : NULL);
            }
        }

        cJSON *pos = cJSON_GetObjectItem(e, "positionals");
        TEST(ctx, cJSON_IsArray(pos));
        for (int p = 0; p < cJSON_GetArraySize(pos); p++) {
            cJSON *pe = cJSON_GetArrayItem(pos, p);
            TEST_NOT_NULL(ctx, cj_str(pe, "name"));
            TEST(ctx, cJSON_IsBool(cJSON_GetObjectItem(pe, "required")));
            TEST_NOT_NULL(ctx, cj_str(pe, "type"));
        }

        cJSON *fl = cJSON_GetObjectItem(e, "flags");
        TEST(ctx, cJSON_IsArray(fl));
        for (int f = 0; f < cJSON_GetArraySize(fl); f++) {
            cJSON *fe = cJSON_GetArrayItem(fl, f);
            TEST_NOT_NULL(ctx, cj_str(fe, "name"));
            TEST(ctx, cJSON_IsBool(cJSON_GetObjectItem(fe, "has_value")));
            TEST(ctx, cJSON_IsBool(cJSON_GetObjectItem(fe, "required")));
        }

        /* light-projection lists must advertise --full in the schema so
         * agents know how to fetch the blob columns (context: content;
         * exec: raw_response, result, error). */
        if ((strcmp(entity, "context") == 0 && strcmp(action, "list") == 0) ||
            (strcmp(entity, "exec") == 0 && strcmp(action, "list") == 0)) {
            int has_full = 0;
            for (int f = 0; f < cJSON_GetArraySize(fl); f++) {
                if (strcmp(cj_str(cJSON_GetArrayItem(fl, f), "name"),
                          "full") == 0)
                    has_full = 1;
            }
            TEST(ctx, has_full == 1);
            const char *note = sc ? cj_str(sc, "note") : NULL;
            TEST(ctx, note != NULL && strstr(note, "--full") != NULL);
        }

        /* P5: every list entry advertises --stream (NDJSON output with
         * internal paging). */
        if (strcmp(action, "list") == 0) {
            int has_stream = 0;
            for (int f = 0; f < cJSON_GetArraySize(fl); f++) {
                if (strcmp(cj_str(cJSON_GetArrayItem(fl, f), "name"),
                          "stream") == 0)
                    has_stream = 1;
            }
            TEST(ctx, has_stream == 1);
        }

        /* aliases (M6): exec entries carry ["execution"], log entries
         * carry ["execution_log"], every other entry carries [] */
        cJSON *aliases = cJSON_GetObjectItem(e, "aliases");
        TEST(ctx, cJSON_IsArray(aliases));
        int n_al = aliases ? cJSON_GetArraySize(aliases) : 0;
        if (strcmp(entity, "exec") == 0) {
            TEST_EQ(ctx, n_al, 1);
            TEST_STREQ(ctx, (aliases && n_al == 1)
                ? cJSON_GetArrayItem(aliases, 0)->valuestring : NULL,
                       "execution");
        } else if (strcmp(entity, "log") == 0) {
            TEST_EQ(ctx, n_al, 1);
            TEST_STREQ(ctx, (aliases && n_al == 1)
                ? cJSON_GetArrayItem(aliases, 0)->valuestring : NULL,
                       "execution_log");
        } else {
            TEST_EQ(ctx, n_al, 0);
        }
    }
}

/* ── raw-argv cross-check (D4/D5) ────────────────────────────────── */

static void cross_check(stest_ctx_t *ctx, cJSON *tools)
{
    int n = cJSON_GetArraySize(tools);
    for (int i = 0; i < n; i++) {
        cJSON *e = cJSON_GetArrayItem(tools, i);
        const char *entity = cj_str(e, "entity");
        const char *action = cj_str(e, "action");
        const char *input  = cj_str(e, "input");
        stest_cmd_fn_t fn = handler_for(entity);
        TEST_NOT_NULL(ctx, fn);
        if (!fn) continue;

        /* argv from the entry's own required positionals + required
         * flags (dummy values "1" / "x") */
        char *argv[32];
        char *owned[32];
        int argc = 0, n_owned = 0;
        argv[argc++] = (char *)"acta_cli";
        argv[argc++] = (char *)entity;
        argv[argc++] = (char *)action;

        cJSON *pos = cJSON_GetObjectItem(e, "positionals");
        if (cJSON_IsArray(pos))
            for (int p = 0; p < cJSON_GetArraySize(pos); p++) {
                cJSON *pe = cJSON_GetArrayItem(pos, p);
                cJSON *req = cJSON_GetObjectItem(pe, "required");
                if (req && req->valuedouble != 0.0)
                    argv[argc++] = (char *)"1";
            }

        cJSON *fl = cJSON_GetObjectItem(e, "flags");
        if (cJSON_IsArray(fl))
            for (int f = 0; f < cJSON_GetArraySize(fl); f++) {
                cJSON *fe = cJSON_GetArrayItem(fl, f);
                cJSON *req = cJSON_GetObjectItem(fe, "required");
                cJSON *hv  = cJSON_GetObjectItem(fe, "has_value");
                if (!(req && req->valuedouble != 0.0)) continue;
                const char *fname = cj_str(fe, "name");
                if (!fname) continue;
                char name[64];
                snprintf(name, sizeof name, "--%s", fname);
                char *nm = strdup(name);
                argv[argc++]   = nm;
                owned[n_owned++] = nm;
                if (hv && hv->valuedouble != 0.0)
                    argv[argc++] = (char *)"x";
            }

        /* rc != EXIT_CLI ⇔ accepted by the parse layer; handler-side
         * failures (e.g. atoi("x") → exit 4) are expected and fine. */
        int rc = stest_run_argv(ctx, fn, argc, argv, NULL);
        TEST(ctx, rc != EXIT_CLI);

        if (action && strcmp(action, "help") == 0)
            TEST_EQ(ctx, rc, EXIT_OK);

        /* flags|json entries: a --json blob from json_keys.required */
        if (input && strcmp(input, "flags|json") == 0) {
            cJSON *jk = cJSON_GetObjectItem(e, "json_keys");
            cJSON *reqk = (jk && cJSON_IsObject(jk))
                        ? cJSON_GetObjectItem(jk, "required") : NULL;
            char blob[512];
            int off = (int)snprintf(blob, sizeof blob, "{");
            if (reqk && cJSON_IsArray(reqk))
                for (int k = 0; k < cJSON_GetArraySize(reqk); k++) {
                    cJSON *key = cJSON_GetArrayItem(reqk, k);
                    if (!key || !cJSON_IsString(key)) continue;
                    off += snprintf(blob + off, sizeof blob - off,
                                   "%s\"%s\":\"x\"", k ? "," : "",
                                   key->valuestring);
                }
            snprintf(blob + off, sizeof blob - off, "}");

            char *jargv[5] = { "acta_cli", (char *)entity, (char *)action,
                              "--json", blob };
            TEST(ctx, stest_run_argv(ctx, fn, 5, jargv, NULL) != EXIT_CLI);
        }

        for (int o = 0; o < n_owned; o++)
            free(owned[o]);
    }

    /* input-source smoke runs: --stdin and --from_file end-to-end
     * (deterministic success on context create; its required
     * fields are type + content + hash) */
    const char *blob = "{\"type\":\"t\",\"content\":\"c\",\"hash\":\"h\"}";
    char *argv_stdin[4] = { "acta_cli", "context", "create", "--stdin" };
    TEST_EQ(ctx, stest_run_argv(ctx, cmd_context, 4, argv_stdin, blob),
            EXIT_OK);

    const char *path = stest_write_input(ctx, blob);
    TEST_NOT_NULL(ctx, path);
    if (path) {
        char *argv_file[5] = { "acta_cli", "context", "create",
                              "--from_file", (char *)path };
        TEST_EQ(ctx, stest_run_argv(ctx, cmd_context, 5, argv_file, NULL),
                EXIT_OK);
    }
}

/* ── suite entry point ───────────────────────────────────────────── */

int run_tools_test(void)
{
    stest_ctx_t ctx;
    stest_init(&ctx, REF_DB);

    char compact[TOOLS_MAX_LEN], pretty[TOOLS_MAX_LEN];
    int clen = capture_tools(&ctx, 0, compact, sizeof compact);
    TEST(&ctx, clen >= 0);
    int plen = capture_tools(&ctx, 1, pretty, sizeof pretty);
    TEST(&ctx, plen >= 0);

    if (clen >= 0) {
        /* D3: valid JSON via the project's own json layer */
        TEST_EQ(&ctx, json_validate(compact), 0);
        check_shape(&ctx, compact, clen, 0);

        cJSON *root = cJSON_Parse(compact);
        TEST_NOT_NULL(&ctx, root);
        if (root) {
            check_structure(&ctx, root);
            cross_check(&ctx, cJSON_GetObjectItem(root, "tools"));
            /* raw-argv cross-check (74-entry tools array, not the root) */
            cJSON_Delete(root);
        }
    }

    if (plen >= 0) {
        TEST_EQ(&ctx, json_validate(pretty), 0);
        check_shape(&ctx, pretty, plen, 1);

        cJSON *root = cJSON_Parse(pretty);
        TEST_NOT_NULL(&ctx, root);
        if (root) {
            check_structure(&ctx, root);      /* same structure, indented */
            cJSON_Delete(root);
        }
    }

    stest_teardown(&ctx);
    return ctx.failures;
}

int main(void)
{
    int f = run_tools_test();

    if (f == 0) {
        printf("PASS: all tools (--tools) tests passed\n");
        return 0;
    }
    printf("FAIL: %d assertion(s) failed\n", f);
    return 1;
}
