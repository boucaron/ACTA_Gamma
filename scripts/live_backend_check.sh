#!/usr/bin/env bash
# live_backend_check.sh — the make-wired LIVE backend check, in EITHER dialect.
#
# It is the shell counterpart of acta_runner/tests/llama_smoke.c and the entry
# point of the top-level `make test-live*` targets (docs/building.md
# "Live backend check"). Three live scripts exist, with different jobs:
#
#   scripts/live_backend_check.sh  <- THIS ONE: generic (any base_url, either
#                                    dialect), make-wired, probe + full pipeline
#   scripts/probe_strata.sh        <- the Strata investigation record cited by
#                                    docs/archive/session_strata_backend.md (kept as is)
#   scripts/wsl_strata_demo.sh     <- the WSL-over-NAT demo cited by the same doc
#
# Two phases:
#
#   probe    — the HTTP surface acta_runner actually uses (docs/runner_contract.md):
#     1  GET  /health                              reachable? (one retry: the
#                                                   NAT/Hyper-V cold path drops
#                                                   the first packet)
#     2  GET  /v1/models                           is the model id served
#     3  POST /v1/chat/completions  plain          HARD
#     4  POST  response_format json_object         informational
#     5  POST  the CHOSEN dialect's json_schema    HARD  (a refusal is exactly
#                                                   what blocks a skill with
#                                                   output_schema)
#     6  POST  the OTHER dialect's json_schema     informational: a refusal is
#                                                   issue A, the EXPECTED answer
#                                                   from a server that speaks the
#                                                   chosen dialect
#     7  GET  /                                    informational, judged by BODY
#                                                   SHAPE — Strata answers HTTP 200
#                                                   with a non-catalog body, so the
#                                                   status code proves nothing; the
#                                                   runner records "catalog":null and
#                                                   continues either way
#     8  cross-side reachability                   informational (ACTA_LIVE_CROSS=0
#                                                   disables it)
#
#   pipeline — the real pipeline, in a scratch DB (NEVER the repo acta.db):
#     db init -> model create backend=<dialect> -> 2 skills (plain +
#     output_schema AS A JSON STRING, docs/cli_spec.md issue C, with a
#     prompt that NAMES the required fields) -> context create
#     -> 2 exec create -> check <model record id> -> run <e1> -> run <e2> ->
#     both completed, exec 2 raw_response parses and satisfies the schema
#     (label, confidence) — a HARD assertion: a schema-constrained server
#     (Strata, strict:true) guarantees it, but a plain llama-server accepts
#     the envelope without enforcing it, so the prompt must name the
#     fields for the model to comply (measured 3/3 on
#     JBDRAFTBig_Qwen3.8-27B-GSQ-RCO-IQ3_XXS). The execution_log audit keys
#     name the dialect that produced them (preflight_passed.max_context /
#     max_context_source / catalog, llm_request.dialect / dialect_source /
#     model_backend).
#
# Usage:
#   scripts/live_backend_check.sh --dialect llamacpp|openai   (REQUIRED, no default)
#       [--base-url URL] [--model ID] [--key KEY] [--timeout N]
#       [--probe-only | --pipeline-only] [--keep-db] [-h|--help]
#
#   --dialect     llamacpp | openai — REQUIRED. There is deliberately no
#                 default: the two dialects have different expectations and a
#                 silent default is how the old scripts drifted.
#   --base-url    e.g. http://127.0.0.1:8080 (trailing "/" is fine)
#   --model       model id; default: the /health "model", else the first
#                 /v1/models id
#   --key         Bearer key for the probe calls and OPENAI_API_KEY for the
#                 pipeline; "-" or empty = keyless (localhost)
#   --timeout     per-request timeout, seconds (default 60)
#   --probe-only  / --pipeline-only    run one phase only (mutually exclusive)
#   --keep-db     keep the scratch DB instead of deleting it at exit
#
# Env:  ACTA_LIVE_BASE / ACTA_LIVE_MODEL / ACTA_LIVE_KEY / ACTA_LIVE_TIMEOUT
#       ACTA_LIVE_CROSS=0   skip the cross-side reachability step
#       ACTA_LIVE_DB        override the scratch DB path
#       OPENAI_API_KEY      used when --key is absent (see the key note below)
# Default base: http://127.0.0.1:8080 on Windows,
#               http://<windows-host-ip>:8080 inside WSL2 (NAT mode — 127.0.0.1
#               from inside WSL reaches the WSL distro, never the Windows host).
#
# Key policy (docs/runner_contract.md decision 4): the runner reads the key ONLY
# from $OPENAI_API_KEY (or the config file). An UNSET variable is a hard runner
# error; an EMPTY one is the documented keyless-localhost warning. So this
# script always exports OPENAI_API_KEY: "--key VALUE" sets it, keyless sets it to
# "" (never leaves it unset).
#
# Scratch DB: $TMPDIR / $TEMP / $TMP (else /tmp, else the current dir) /
# acta_live_<dialect>.db — removed at exit unless --keep-db. The repo acta.db is
# never touched.
#
# Exit: 0  usable as an acta_runner backend — OR no server answered at all
#         (prints "SKIP: no backend at <url>": a live target must never be red
#         just because a server is switched off)
#       1  a server answered and a hard step failed
#       2  usage or build error (missing --dialect, bad --dialect, no binaries)
set -u

IN_WSL=0
[ -n "${WSL_DISTRO_NAME:-}" ] && IN_WSL=1
grep -qi microsoft /proc/version 2>/dev/null && IN_WSL=1

wsl_host_ip() {   # the Windows-side address of the WSL adapter (NAT mode)
    local gw
    gw=$(awk '$2=="00000000" {print $3}' /proc/net/route | head -1)
    [ -n "$gw" ] && printf '%d.%d.%d.%d' 0x${gw:6:2} 0x${gw:4:2} 0x${gw:2:2} 0x${gw:0:2}
}

DEFAULT_BASE="http://127.0.0.1:8080"
if [ "$IN_WSL" = 1 ]; then
    H=$(wsl_host_ip); [ -n "$H" ] && DEFAULT_BASE="http://$H:8080"
fi

usage() { awk 'NR==1 {next} /^set -u/ {exit} {print}' "$0"; }   # the header comment block

# ---------------------------------------------------------------- arguments --
DIALECT=""
BASE=""
MODEL="${ACTA_LIVE_MODEL:-}"
KEY="${ACTA_LIVE_KEY:-}"
KEY_GIVEN=0
TMO="${ACTA_LIVE_TIMEOUT:-60}"
PROBE=1
PIPELINE=1
PROBE_ONLY=0
PIPELINE_ONLY=0
KEEP_DB=0

# A flag that takes a value must have that value present, or `shift 2` would
# fail out of range and the loop would spin.
need_value() { [ "$2" -ge 2 ] || { echo "$1 needs a value" >&2; exit 2; }; }

if [ $# -eq 0 ]; then usage >&2; exit 2; fi
while [ $# -gt 0 ]; do
    case "$1" in
        --dialect)   need_value "$1" "$#"; DIALECT="$2"; [ -z "$DIALECT" ] && { echo "--dialect needs a value (llamacpp|openai)" >&2; exit 2; }; shift 2 ;;
        --dialect=*) DIALECT="${1#--dialect=}"; shift ;;
        --base-url)  need_value "$1" "$#"; BASE="$2"; [ -z "$BASE" ] && { echo "--base-url needs a value" >&2; exit 2; }; shift 2 ;;
        --base-url=*) BASE="${1#--base-url=}"; shift ;;
        --model)     need_value "$1" "$#"; MODEL="$2"; [ -z "$MODEL" ] && { echo "--model needs a value" >&2; exit 2; }; shift 2 ;;
        --model=*)   MODEL="${1#--model=}"; shift ;;
        --key)       need_value "$1" "$#"; KEY="$2"; KEY_GIVEN=1; shift 2 ;;
        --key=*)     KEY="${1#--key=}"; KEY_GIVEN=1; shift ;;
        --timeout)   need_value "$1" "$#"; TMO="$2"; [ -z "$TMO" ] && { echo "--timeout needs a value" >&2; exit 2; }; shift 2 ;;
        --timeout=*) TMO="${1#--timeout=}"; shift ;;
        --probe-only)    PROBE_ONLY=1; shift ;;
        --pipeline-only) PIPELINE_ONLY=1; shift ;;
        --keep-db)   KEEP_DB=1; shift ;;
        -h|--help)   usage; exit 0 ;;
        *) echo "unknown argument '$1'" >&2; usage >&2; exit 2 ;;
    esac
done

if [ -z "$DIALECT" ]; then
    echo "ERROR: --dialect llamacpp|openai is required (no default: the two dialects expect different servers)" >&2
    usage >&2
    exit 2
fi
if [ "$DIALECT" != llamacpp ] && [ "$DIALECT" != openai ]; then
    echo "invalid --dialect '$DIALECT' (expected llamacpp or openai)" >&2
    exit 2
fi
if [ "$PROBE_ONLY" = 1 ] && [ "$PIPELINE_ONLY" = 1 ]; then
    echo "--probe-only and --pipeline-only cannot be combined" >&2
    exit 2
fi
[ "$PROBE_ONLY" = 1 ] && PIPELINE=0
[ "$PIPELINE_ONLY" = 1 ] && PROBE=0
case "$TMO" in (*[!0-9]*|'') echo "--timeout must be a positive integer" >&2; exit 2 ;; esac
[ "$TMO" -le 0 ] && { echo "--timeout must be a positive integer" >&2; exit 2; }

[ -z "$BASE" ] && BASE="${ACTA_LIVE_BASE:-$DEFAULT_BASE}"
BASE="${BASE%/}"
OTHER=openai; [ "$DIALECT" = openai ] && OTHER=llamacpp
[ "$KEY" = "-" ] && KEY=""

# The scratch DB lives in the platform temp dir, never in the repo. WSL distros
# set none of TMPDIR/TEMP/TMP, so /tmp is the last resort before the current
# directory (a scratch file must not land next to the repo's acta.db).
TMPBASE="${TMPDIR:-${TEMP:-${TMP:-}}}"
if [ -z "$TMPBASE" ]; then
    if [ -d /tmp ] && [ -w /tmp ]; then TMPBASE=/tmp; else TMPBASE=.; fi
fi
DB="${ACTA_LIVE_DB:-$TMPBASE/acta_live_$DIALECT.db}"
LOGF="$DB.audit.json"
cleanup() { [ "$KEEP_DB" = 1 ] || rm -f "$DB" "$DB-journal" "$LOGF"; }
trap cleanup EXIT

CURL=$(command -v curl 2>/dev/null || command -v curl.exe 2>/dev/null || echo curl)
AUTH=(); [ -n "$KEY" ] && AUTH=(-H "Authorization: Bearer $KEY")
fail=0
say() { printf '%-34s %s\n' "$1" "$2"; }

# One HTTP call. Sets R_BODY / R_CODE ("000" = transport failure). Retries once:
# the NAT/Hyper-V firewall can drop the first packet across the WSL boundary.
R_BODY=""; R_CODE="000"
http() {   # http GET|POST <url> [body]
    local method="$1" url="$2" body="${3-}" attempt out
    R_BODY=""; R_CODE="000"
    for attempt in 1 2; do
        if [ "$method" = GET ]; then
            out=$("$CURL" -sS -m "$TMO" "${AUTH[@]}" -w '\n@@HTTPCODE=%{http_code}@@' "$url" 2>&1)
        else
            out=$("$CURL" -sS -m "$TMO" -H "Content-Type: application/json" "${AUTH[@]}" \
                  -w '\n@@HTTPCODE=%{http_code}@@' -d "$body" "$url" 2>&1)
        fi
        case "$out" in
            *'@@HTTPCODE='*)
                R_CODE=${out##*@@HTTPCODE=}; R_CODE=${R_CODE%%@@*}
                R_BODY=${out%%@@HTTPCODE=*}
                ;;
        esac
        case "$R_CODE" in
            [0-9][0-9][0-9]) [ "$R_CODE" = 000 ] || return 0 ;;
        esac
        R_CODE="000"
    done
    return 1                     # no answer at all, after the retry
}

# dotted JSON path reader (jq segfaults on this box)
jget() { python3 -c '
import json,sys
try: d=json.load(sys.stdin)
except Exception: print(""); sys.exit(0)
for p in sys.argv[1:]:
    for part in p.split("."):
        if isinstance(d,list):
            try: d=d[int(part)]
            except Exception: d=None
        elif isinstance(d,dict): d=d.get(part)
        else: d=None
        if d is None: break
    if d is None: break
print("" if d is None else (d if not isinstance(d,(dict,list)) else json.dumps(d)))
' "$@"; }
jids() { python3 -c '
import json,sys
try: d=json.load(sys.stdin)
except Exception: sys.exit(0)
for m in d.get("data",[]): print(m.get("id",""))
'; }
isint() { case "$1" in ('' | *[!0-9]*) return 1 ;; *) return 0 ;; esac; }

SCHEMA='{"type":"object","required":["label","confidence"],"properties":{"label":{"type":"string"},"confidence":{"type":"number"}}}'
SCHEMA_ESC=$(printf '%s' "$SCHEMA" | sed 's/"/\\"/g')
env_rf() {   # env_rf <dialect> -> the response_format envelope THAT dialect builds
    if [ "$1" = openai ]; then
        printf '{"type":"json_schema","json_schema":{"name":"acta_output","strict":true,"schema":%s}}' "$SCHEMA"
    else
        printf '{"type":"json_schema","schema":%s}' "$SCHEMA"
    fi
}
chat_json() { printf '{"model":"%s","messages":[{"role":"system","content":"%s"},{"role":"user","content":"%s"}],"temperature":0,"max_tokens":400,"stream":false,"response_format":%s}' "$1" "$2" "$3" "$4"; }

# =============================================================== probe phase ==
REACHABLE=0
echo "== live backend check: $BASE  (dialect $DIALECT)"
echo "   scratch db: $DB   timeout: ${TMO}s   auth: $([ -n "$KEY" ] && echo "Bearer key" || echo "none (keyless localhost)")"

if [ "$PROBE" = 1 ] || [ "$PIPELINE" = 1 ]; then
    # ---- 1. GET /health : the reachability decision (SKIP vs failure) ----
    if http GET "$BASE/health"; then
        REACHABLE=1
        if [ "$R_CODE" != 200 ]; then
            say "1 /health" "HTTP $R_CODE -> server answers, preflight FAILED"
            printf '   %s\n' "${R_BODY:0:200}"
            exit 1
        fi
        [ -z "$MODEL" ] && MODEL=$(printf '%s' "$R_BODY" | jget model)
        say "1 /health" "HTTP 200 status=$(printf '%s' "$R_BODY" | jget status) model=$MODEL max_context=$(printf '%s' "$R_BODY" | jget max_context)"
        [ "$(printf '%s' "$R_BODY" | jget status)" = "ok" ] || \
            { say "" "preflight failed: status is not ok (503 = model still loading)"; exit 1; }
    else
        echo "SKIP: no backend at $BASE"
        echo "   (no answer on /health after the retry — a live target is never red because a server is off)"
        exit 0
    fi
fi

if [ "$PROBE" = 1 ]; then
    # ---- 2. GET /v1/models : is the id served ----
    if http GET "$BASE/v1/models" && [ "$R_CODE" = 200 ]; then
        IDS=$(printf '%s' "$R_BODY" | jids)
        say "2 /v1/models" "HTTP 200 served: $(printf '%s' "$IDS" | tr '\n' ' ')"
        [ -z "$MODEL" ] && MODEL=$(printf '%s' "$IDS" | head -1)
        if [ -n "$MODEL" ] && printf '%s\n' "$IDS" | grep -qx "$MODEL"; then
            say "   model id present" "$MODEL"
        else
            say "   model id present" "NO -> model_identifier must be one of: $IDS"
            fail=$((fail+1))
        fi
    else
        say "2 /v1/models" "no answer (HTTP $R_CODE)"
        fail=$((fail+1))
    fi
    [ -z "$MODEL" ] && { echo "no model id from /health or /v1/models" >&2; exit 1; }

    # ---- 3. plain chat (HARD) ----
    BODY=$(printf '{"model":"%s","messages":[{"role":"system","content":"Answer with one short sentence."},{"role":"user","content":"Say hello."}],"temperature":0,"max_tokens":64,"stream":false}' "$MODEL")
    if http POST "$BASE/v1/chat/completions" "$BODY" && [ "$R_CODE" = 200 ]; then
        say "3 chat (plain)" "HTTP 200 $(printf '%s' "$R_BODY" | jget choices.0.message.content | tr -d '\n' | cut -c1-60)"
    else
        say "3 chat (plain)" "HTTP $R_CODE $(printf '%s' "$R_BODY" | jget error.message | cut -c1-80)"
        [ -n "$R_BODY" ] || say "   " "no body"
        fail=$((fail+1))
    fi

    # ---- 4. response_format json_object (informational) ----
    # NOTE: the prompt text goes through %s, not through the format string —
    # printf interprets backslash escapes IN THE FORMAT, which would turn the
    # literal \" of the embedded JSON example into a bare " and make the whole
    # request body invalid JSON (Strata then answers 400 "Expecting ','
    # delimiter", which looks like a dialect problem and is not one).
    BODY=$(printf '{"model":"%s","messages":[{"role":"user","content":"%s"}],"temperature":0,"max_tokens":400,"stream":false,"response_format":{"type":"json_object"}}' "$MODEL" 'Classify the sentiment. JSON only: {\"label\":\"positive|negative\",\"confidence\":number}')
    http POST "$BASE/v1/chat/completions" "$BODY" || true
    say "4 json_object" "HTTP $R_CODE $(printf '%s' "$R_BODY" | jget choices.0.message.content | tr -d '\n' | cut -c1-60) [finish=$(printf '%s' "$R_BODY" | jget choices.0.finish_reason)] (informational)"

    # ---- 5. the CHOSEN dialect's envelope (HARD) ----
    # The prompt names the required fields (as the pipeline's schema skill
    # does): acceptance is what this step proves, and on a server that does
    # not enforce the schema the named fields are what make the model
    # comply. The literal backslash-quotes are the body escaping (same
    # class as step 4).
    RF=$(env_rf "$DIALECT")
    say "5 envelope $DIALECT" "sends: $RF"
    BODY=$(chat_json "$MODEL" 'Classify the sentiment of the input. Reply with JSON only: {\"label\": a string, \"confidence\": a number between 0 and 1}.' "The food was excellent." "$RF")
    if http POST "$BASE/v1/chat/completions" "$BODY" && [ "$R_CODE" = 200 ]; then
        say "5 envelope $DIALECT" "HTTP 200 $(printf '%s' "$R_BODY" | jget choices.0.message.content | tr -d '\n' | cut -c1-60)"
    else
        say "5 envelope $DIALECT" "HTTP $R_CODE $(printf '%s' "$R_BODY" | jget error.message | cut -c1-80) -> the $DIALECT envelope was REFUSED: a skill with output_schema would fail here"
        fail=$((fail+1))
    fi

    # ---- 6. the OTHER dialect's envelope (informational: issue A) ----
    RF=$(env_rf "$OTHER")
    BODY=$(chat_json "$MODEL" "Classify the sentiment of the input. Reply with JSON only." "The food was excellent." "$RF")
    http POST "$BASE/v1/chat/completions" "$BODY" || true
    if [ "$R_CODE" = 200 ]; then
        say "6 envelope $OTHER" "HTTP 200 — this server also accepts the $OTHER envelope (informational)"
    else
        say "6 envelope $OTHER" "HTTP $R_CODE $(printf '%s' "$R_BODY" | jget error.message | cut -c1-80) <- EXPECTED refusal (issue A): this server speaks the $DIALECT dialect, register it as backend=$DIALECT"
    fi

    # ---- 7. GET / catalog audit (informational, BODY SHAPE only) ----
    http GET "$BASE/" || true
    CATALOG=$(printf '%s' "$R_BODY" | python3 -c '
import json,sys
try: d=json.load(sys.stdin)
except Exception: print("no"); sys.exit(0)
if isinstance(d,dict) and isinstance(d.get("models"),list): print("yes")
elif isinstance(d,dict) and isinstance(d.get("status"),dict) and "args" in d.get("status",{}): print("yes")
else: print("no")
')
    say "7 GET /" "HTTP $R_CODE catalog-shaped body: $CATALOG (informational — the runner records \"catalog\":null and continues)"
    if [ "$DIALECT" = llamacpp ] && [ "$CATALOG" != yes ]; then
        say "   " "this dialect audits GET /; this server has no catalog body"
    fi
    if [ "$DIALECT" = openai ]; then
        say "   " "this dialect makes NO GET / (plan §9 decision 4): preflight_passed.catalog is null"
    fi

    # ---- 8. cross-side reachability (informational) ----
    if [ "${ACTA_LIVE_CROSS-1}" != 0 ]; then
        if [ "$IN_WSL" = 1 ]; then
            say "8 cross-side" "running inside WSL (${WSL_DISTRO_NAME:-?}); the Windows host is $(wsl_host_ip) — 127.0.0.1 from here would reach the distro, not the host"
        elif command -v wsl.exe >/dev/null 2>&1; then
            D="${ACTA_WSL_DISTRO:-$(wsl.exe -l -q 2>/dev/null | tr -d '\000' | head -1 | tr -d '\r')}"
            PORT=$(printf '%s' "$BASE" | sed -E 's#.*://[^/]*:([0-9]+).*#\1#'); [ "$PORT" = "$BASE" ] && PORT=8080
            if [ -n "$D" ]; then
                say "8 cross-side" "probing port $PORT from WSL ($D)"
                run_timeout() { if command -v timeout >/dev/null 2>&1; then timeout 40 "$@"; else "$@"; fi; }
                run_timeout wsl.exe -d "$D" -e bash -lc '
                    PORT="$1"; P=$(command -v python3 || echo /usr/bin/python3)
                    GW=$(awk "\$2==\"00000000\" {print \$3}" /proc/net/route | head -1)
                    HOST=$(printf "%d.%d.%d.%d" 0x${GW:6:2} 0x${GW:4:2} 0x${GW:2:2} 0x${GW:0:2})
                    echo "   wsl default route -> windows host: $HOST"
                    for t in 127.0.0.1 "$HOST"; do
                        $P -c "import socket,sys
try:
    socket.create_connection((sys.argv[1],int(sys.argv[2])),4); print(\"   REACHABLE  \",sys.argv[1])
except Exception as e: print(\"   unreachable\",sys.argv[1],type(e).__name__)" "$t" "$PORT"
                    done' live "$PORT" 2>&1 | tr -d '\r'
            fi
        else
            say "8 cross-side" "skipped (no wsl.exe on PATH)"
        fi
    fi

    [ "$fail" -eq 0 ] || { echo "== probe FAILED (the server answers, so this is a real failure)"; exit 1; }
    echo "== probe verdict: usable as an acta_runner backend"
    echo "   register it as: backend=$DIALECT base_url=$BASE model_identifier=$MODEL"
fi

# ============================================================ pipeline phase ==
if [ "$PIPELINE" = 1 ]; then
    CLI=acta_cli/acta_cli;   [ -x "$CLI" ]   || CLI=acta_cli/acta_cli.exe
    RUN=acta_runner/acta_runner; [ -x "$RUN" ] || RUN=acta_runner/acta_runner.exe
    if [ ! -x "$CLI" ] || [ ! -x "$RUN" ]; then
        echo "ERROR: acta_cli / acta_runner not built - run 'make all' first" >&2
        exit 2
    fi
    # Built is not the same as runnable: on Windows a missing libcjson/libcurl DLL
    # makes the binary exit 127 with no output at all, which would otherwise
    # surface as a mysterious "db init failed".
    VEROUT=$("$CLI" --version 2>&1); VRC=$?
    if [ "$VRC" != 0 ]; then
        echo "ERROR: '$CLI' is built but not runnable (exit $VRC). On Windows that is usually a DLL missing from the native PATH - export PATH=/c/msys64/mingw64/bin:\$PATH and re-run." >&2
        printf '%s\n' "$VEROUT" | sed 's/^/     /' >&2
        exit 2
    fi
    say "build check" "$(printf '%s' "$VEROUT" | head -1) | $("$RUN" --version 2>&1 | head -1)"
    if [ "$REACHABLE" != 1 ]; then
        echo "SKIP: no backend at $BASE"
        exit 0
    fi
    [ -z "$MODEL" ] && { http GET "$BASE/health" >/dev/null; MODEL=$(printf '%s' "$R_BODY" | jget model); }

    echo "== pipeline phase (scratch db $DB)"
    # The runner reads the key ONLY from $OPENAI_API_KEY: unset is a hard error,
    # empty is the documented keyless-localhost warning. Never leave it unset.
    if [ "$KEY_GIVEN" = 1 ]; then export OPENAI_API_KEY="$KEY"; else export OPENAI_API_KEY="${OPENAI_API_KEY-}"; fi
    export ACTA_DB="$DB"
    rm -f "$DB"
    # A freshly init-ed DB prints "[warn] db opened with degraded pragma
    # state…" on STDERR for EVERY call: informational, never a failure — only
    # STDOUT is parsed, and the displayed output has those lines filtered out.
    DBINIT=$("$CLI" db init 2>&1)
    [ $? -eq 0 ] || { echo "FAIL: '$CLI' db init on $DB did not succeed:" >&2; printf '%s\n' "$DBINIT" | sed 's/^/     /' >&2; exit 1; }

    MID=$(echo "$("$CLI" model create --id_only --json "{\"name\":\"live-check-$DIALECT\",\"backend\":\"$DIALECT\",\"base_url\":\"$BASE\",\"model_identifier\":\"$MODEL\"}" 2>/dev/null)" | tr -d '[:space:]')
    S1=$(echo "$("$CLI" skill create --id_only --json '{"name":"summarize","prompt_template":"Summarize the input in one short sentence."}' 2>/dev/null)" | tr -d '[:space:]')
    # The schema skill's prompt NAMES the required fields: the pipeline's
    # hard schema assertion only holds when the model complies, and on a
    # server that does not enforce the envelope that is prompt-driven
    # (session notes §17.8, gap B: unnamed fields -> {"sentiment":...}).
    S2=$(echo "$("$CLI" skill create --id_only --json "{\"name\":\"sentiment\",\"prompt_template\":\"Classify the sentiment of the input. Reply with JSON only: {\\\"label\\\": a string, \\\"confidence\\\": a number between 0 and 1}.\",\"output_schema\":\"$SCHEMA_ESC\"}" 2>/dev/null)" | tr -d '[:space:]')
    CID=$(echo "$("$CLI" context create --id_only --json '{"type":"text","content":"The build system shipped on time and the release went smoothly."}' 2>/dev/null)" | tr -d '[:space:]')
    say "model/skill/context ids" "model=$MID skills=$S1,$S2 context=$CID"
    isint "$MID" && isint "$S1" && isint "$S2" && isint "$CID" || { echo "FAIL: create did not return ids (stdout only, stderr carries the pragma warn)" >&2; exit 1; }

    MREV=$(printf '%s' "$("$CLI" model_revision get-latest "$MID" 2>/dev/null)" | jget id)
    R1=$(printf '%s' "$("$CLI" skill_revision get-latest "$S1" 2>/dev/null)" | jget id)
    R2=$(printf '%s' "$("$CLI" skill_revision get-latest "$S2" 2>/dev/null)" | jget id)
    say "revisions" "model_rev=$MREV skill_rev(plain)=$R1 skill_rev(schema)=$R2"
    isint "$MREV" && isint "$R1" && isint "$R2" || { echo "FAIL: get-latest did not return revision ids" >&2; exit 1; }

    E1=$(echo "$("$CLI" exec create --id_only --json "{\"context_id\":$CID,\"skill_revision_id\":$R1,\"model_revision_id\":$MREV}" 2>/dev/null)" | tr -d '[:space:]')
    E2=$(echo "$("$CLI" exec create --id_only --json "{\"context_id\":$CID,\"skill_revision_id\":$R2,\"model_revision_id\":$MREV}" 2>/dev/null)" | tr -d '[:space:]')
    say "executions" "plain=$E1 output_schema=$E2"
    isint "$E1" && isint "$E2" || { echo "FAIL: exec create did not return ids" >&2; exit 1; }

    say "check $MID (model record)" ""
    CHECKOUT=$("$RUN" check "$MID" 2>&1)
    CHECKRC=$?
    printf '%s\n' "$CHECKOUT" | grep -v '^\[warn\]' | sed 's/^/     /'
    [ "$CHECKRC" = 0 ] || { echo "FAIL: acta_runner check $MID exited $CHECKRC" >&2; exit 1; }

    # A successful run prints NOTHING on stdout (docs/runner_contract.md): the
    # whole audit lands in execution_log, which is what step "audit keys" reads.
    for E in "$E1" "$E2"; do
        RUNOUT=$("$RUN" run "$E" 2>&1); RUNRC=$?
        say "run $E" "exit $RUNRC (a successful run prints nothing: the audit is in execution_log)"
        printf '%s\n' "$RUNOUT" | grep -v '^\[warn\]\|^warning:' | sed 's/^/     /'
        [ "$RUNRC" = 0 ] || { echo "FAIL: acta_runner run $E exited $RUNRC" >&2; exit 1; }
    done

    for E in "$E1" "$E2"; do
        ST=$("$CLI" exec get "$E" --fields status 2>/dev/null | jget status)
        say "exec $E status" "$ST"
        [ "$ST" = completed ] || { echo "FAIL: exec $E status is '$ST', expected completed" >&2; exit 1; }
    done

    say "exec $E2 raw_response" ""
    "$CLI" exec get "$E2" --raw_out raw_response 2>/dev/null | python3 -c '
import json,sys
raw=sys.stdin.read().strip()
try: d=json.loads(raw)
except Exception as e: print("  FAIL: raw_response is not JSON: %r (%s)" % (raw[:160],e)); sys.exit(1)
if not isinstance(d,dict): print("  FAIL: raw_response is not a JSON object: %r" % (d,)); sys.exit(1)
miss=[k for k in ("label","confidence") if k not in d]
if miss: print("  FAIL: raw_response misses %s" % ",".join(miss)); sys.exit(1)
if not isinstance(d["label"],str): print("  FAIL: label is not a string: %r" % (d["label"],)); sys.exit(1)
if isinstance(d["confidence"],bool) or not isinstance(d["confidence"],(int,float)): print("  FAIL: confidence is not a number: %r" % (d["confidence"],)); sys.exit(1)
print("  raw_response satisfies the output_schema: %s" % json.dumps(d)[:160])
' || { echo "FAIL: exec $E2 raw_response does not satisfy the schema" >&2
          echo "   This server accepts the $DIALECT envelope but does NOT enforce the schema" >&2
          echo "   (no strict:true, unlike Strata): the model's reply deviated even though" >&2
          echo "   the skill prompt names the required fields. Model non-compliance," >&2
          echo "   not a backend refusal — re-run or register a stricter server." >&2
          exit 1; }

    # ---- the audit trail: which dialect produced these rows ----
    "$CLI" log list "$E2" 2>/dev/null > "$LOGF"
    echo "   audit keys of execution $E2 (log list, metadata is a STRING containing JSON):"
    python3 -c '
import json,sys
dialect=sys.argv[1]
logs=json.load(open(sys.argv[2]))
meta={}
for e in logs:
    m=e.get("metadata") or ""
    if isinstance(m,str):
        try: m=json.loads(m)
        except Exception: m={}
    meta.setdefault(e.get("event"),m)
fail=[]
pf=meta.get("preflight_passed")
if not isinstance(pf,dict): fail.append("no preflight_passed event in the log")
else:
    mc=pf.get("max_context")
    if not isinstance(mc,(int,float)) or mc<=0: fail.append("preflight_passed.max_context missing or <= 0 (got %r)" % (mc,))
    if not pf.get("max_context_source"): fail.append("preflight_passed.max_context_source missing")
    if pf.get("dialect")!=dialect: fail.append("preflight_passed.dialect=%r expected %r" % (pf.get("dialect"),dialect))
    if dialect=="openai" and pf.get("catalog") is not None: fail.append("openai preflight_passed.catalog must be null (got %r)" % (pf.get("catalog"),))
    print("  preflight_passed: max_context=%s source=%s dialect=%s catalog=%s" % (
        pf.get("max_context"), pf.get("max_context_source"), pf.get("dialect"),
        "null" if pf.get("catalog") is None else json.dumps(pf.get("catalog"))[:60]))
lr=meta.get("llm_request")
if not isinstance(lr,dict): fail.append("no llm_request event in the log")
else:
    if lr.get("dialect")!=dialect: fail.append("llm_request.dialect=%r expected %r" % (lr.get("dialect"),dialect))
    if lr.get("dialect_source")!="model_revision": fail.append("llm_request.dialect_source=%r expected model_revision" % (lr.get("dialect_source"),))
    if lr.get("model_backend")!=dialect: fail.append("llm_request.model_backend=%r expected %r" % (lr.get("model_backend"),dialect))
    print("  llm_request: dialect=%s source=%s model_backend=%s response_format=%s" % (
        lr.get("dialect"), lr.get("dialect_source"), lr.get("model_backend"), lr.get("response_format")))
for f in fail: print("  AUDIT FAIL: %s" % f)
sys.exit(1 if fail else 0)
' "$DIALECT" "$LOGF" || { echo "FAIL: the audit trail does not name the $DIALECT dialect" >&2; exit 1; }

    # exec 1 is the plain skill: the audit must show NO response_format was sent
    "$CLI" log list "$E1" 2>/dev/null > "$LOGF"
    python3 -c '
import json,sys
logs=json.load(open(sys.argv[1]))
for e in logs:
    if e.get("event")!="llm_request": continue
    m=e.get("metadata") or ""
    try: m=json.loads(m)
    except Exception: m={}
    if m.get("response_format") is not False:
        print("  AUDIT FAIL: the plain skill execution sent response_format=%r" % (m.get("response_format"),)); sys.exit(1)
    print("  exec %s llm_request: response_format=false (plain skill, no envelope)" % sys.argv[2]); sys.exit(0)
print("  AUDIT FAIL: no llm_request event for the plain execution"); sys.exit(1)
' "$LOGF" "$E1" || exit 1

    [ "$KEEP_DB" = 1 ] && echo "   scratch db kept: $DB"
    echo "== pipeline verdict: the $DIALECT dialect ran the full pipeline end to end"
fi

exit 0
