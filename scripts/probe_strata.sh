#!/usr/bin/env bash
# probe_strata.sh — can a Strata server act as an acta_runner backend?
#
# It checks exactly the surface acta_runner uses (docs/runner_contract.md):
#   1. GET  /health               -> the first preflight GET
#   2. GET  /v1/models            -> the second preflight GET, is the id served
#   3. POST /v1/chat/completions  -> one non-stream completion (the run pipeline)
#   4. POST ... response_format   -> the output-schema path
#   4b. POST ... json_schema      -> the envelope acta_runner REALLY sends for
#                                    the --dialect chosen (openai by default);
#                                    the llama.cpp shape is what ACTA used to
#                                    send unconditionally and what an
#                                    OpenAI-dialect server refuses (issue A)
#   5. the same probe from WSL    -> what a WSL-hosted runner would see
#
# Runs from either side: MSYS2/Windows bash, or a WSL distro (curl.exe is used
# when native curl is absent, and the Windows host IP is derived automatically).
#
# Usage: scripts/probe_strata.sh [--dialect llamacpp|openai] [base_url]
# Env:   ACTA_STRATA_BASE   default: http://127.0.0.1:8080 on Windows,
#                           http://<windows-host-ip>:8080 inside WSL
#        ACTA_STRATA_MODEL  model id to look for (default: read from /health)
#        ACTA_STRATA_DIALECT default dialect for step 4b: openai
#        ACTA_WSL_DISTRO    WSL distro used for the cross-side check
# Exit:  0 usable as a backend | 1 preflight failed | 2 chat call failed
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

# --dialect <llamacpp|openai>: which response_format envelope step 4b sends
# (the one acta_runner would build for a model revision registered that way).
# It does NOT change steps 1-3, which are dialect-independent.
DIALECT="${ACTA_STRATA_DIALECT:-openai}"
POS=
while [ $# -gt 0 ]; do
    case "$1" in
        --dialect)
            DIALECT="${2:-}"
            [ -z "$DIALECT" ] && { echo "--dialect needs a value (llamacpp|openai)" >&2; exit 1; }
            shift 2 ;;
        --dialect=*)
            DIALECT="${1#--dialect=}"; shift ;;
        -h|--help)
            sed -n '2,25p' "$0"; exit 0 ;;
        *)
            [ -z "$POS" ] && POS="$1"; shift ;;
    esac
done
if [ "$DIALECT" != llamacpp ] && [ "$DIALECT" != openai ]; then
    echo "invalid --dialect '$DIALECT' (expected llamacpp or openai)" >&2
    exit 1
fi

BASE="$POS"
[ -z "$BASE" ] && BASE="${ACTA_STRATA_BASE:-$DEFAULT_BASE}"
TMO="${ACTA_STRATA_TIMEOUT:-60}"
KEY="${OPENAI_API_KEY:-}"
AUTH=(); [ -n "$KEY" ] && AUTH=(-H "Authorization: Bearer $KEY")
CURL=$(command -v curl 2>/dev/null || command -v curl.exe 2>/dev/null || echo curl)
fail=0
PORT=$(printf '%s' "$BASE" | sed -E 's#.*://[^/]*:([0-9]+).*#\1#'); [ "$PORT" = "$BASE" ] && PORT=8080
say() { printf '%-32s %s\n' "$1" "$2"; }
get() {   # GET with one retry: the NAT/Hyper-V firewall can drop the first packet
    "$CURL" -sS -m "$TMO" "${AUTH[@]}" "$1" 2>&1 || "$CURL" -sS -m "$TMO" "${AUTH[@]}" "$1" 2>&1
}

# dotted JSON path reader (jq is unreliable on this box)
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

echo "== Strata probe: $BASE"

# --- 1. GET /health ----------------------------------------------------------
H=$(get "$BASE/health"); [ -n "$H" ] || { say "1 /health" "FAIL: no response"; exit 1; }
MODEL="${ACTA_STRATA_MODEL:-$(printf '%s' "$H" | jget model)}"
say "1 /health" "status=$(printf '%s' "$H" | jget status) model=$MODEL max_context=$(printf '%s' "$H" | jget max_context)"
[ "$(printf '%s' "$H" | jget status)" = "ok" ] || { say "" "preflight 1 failed"; exit 1; }

# --- 2. GET /v1/models -------------------------------------------------------
M=$(get "$BASE/v1/models"); [ -n "$M" ] || { say "2 /v1/models" "FAIL: no response"; exit 1; }
IDS=$(printf '%s' "$M" | jids)
say "2 /v1/models" "served: $(printf '%s' "$IDS" | tr '\n' ' ')"
if [ -n "$MODEL" ] && printf '%s\n' "$IDS" | grep -qx "$MODEL"; then
    say "  model id present" "$MODEL"
else
    say "  model id present" "NO -> model_identifier must be one of: $IDS"; fail=$((fail+1))
fi

# --- 3. POST /v1/chat/completions -------------------------------------------
ID="${MODEL:-strata}"
C=$("$CURL" -sS -m "$TMO" -H "Content-Type: application/json" "${AUTH[@]}" \
    -d '{"model":"'"$ID"'","messages":[{"role":"system","content":"Answer with one short sentence."},{"role":"user","content":"Say hello."}],"temperature":0,"max_tokens":64,"stream":false}' \
    "$BASE/v1/chat/completions" 2>&1) || { say "3 chat/completions" "FAIL: $C"; exit 2; }
REPLY=$(printf '%s' "$C" | jget choices.0.message.content)
[ -z "$REPLY" ] && REPLY=$(printf '%s' "$C" | jget error.message)
say "3 chat/completions" "${REPLY:0:60}"
[ -n "$REPLY" ] || { say "" "chat call failed"; printf '%s\n' "$C" | head -c 400; exit 2; }

# --- 4. response_format (ACTA output schema path) ---------------------------
S=$("$CURL" -sS -m "$TMO" -H "Content-Type: application/json" "${AUTH[@]}" \
    -d '{"model":"'"$ID"'","messages":[{"role":"user","content":"Classify the sentiment. JSON only: {\"label\":\"positive|negative\",\"confidence\":number}"}],"response_format":{"type":"json_object"},"temperature":0,"max_tokens":400,"stream":false}' \
    "$BASE/v1/chat/completions" 2>&1)
say "4 response_format json_object" "$(printf '%s' "$S" | jget choices.0.message.content | tr -d '\n' | cut -c1-60) [finish=$(printf '%s' "$S" | jget choices.0.finish_reason)]"

# --- 4b. response_format json_schema — the envelope acta_runner sends ----
# The dialect decides the shape (docs/archive/backend_dialects.md §2):
#   openai    {"type":"json_schema","json_schema":{"name":"acta_output",
#              "strict":true,"schema":{…}}}   <- what a strict OpenAI-compatible
#                                              server (Strata) requires
#   llamacpp  {"type":"json_schema","schema":{…}}  <- what acta_runner used to
#              send unconditionally, and what an OpenAI-dialect server refuses
#              with "response_format.json_schema needs a schema object"
#              (docs/archive/session_strata_backend.md issue A)
# A refusal of the llama.cpp shape is therefore the EXPECTED answer on an
# openai-dialect server: reported, not counted as a probe failure. A refusal
# of the --dialect envelope IS a failure — it is exactly what blocks a skill
# with output_schema (docs/archive/backend_dialects.md §6 criterion 2).
SCHEMA='{"type":"object","required":["label","confidence"],"properties":{"label":{"type":"string"},"confidence":{"type":"number"}}}'
if [ "$DIALECT" = openai ]; then
    RF='{"type":"json_schema","json_schema":{"name":"acta_output","strict":true,"schema":'"$SCHEMA"'}}'
else
    RF='{"type":"json_schema","schema":'"$SCHEMA"'}'
fi
J=$("$CURL" -sS -m "$TMO" -H "Content-Type: application/json" "${AUTH[@]}" -w '\n%{http_code}' \
    -d '{"model":"'"$ID"'","messages":[{"role":"user","content":"Classify the sentiment of \"The food was excellent.\" Reply with JSON only."}],"response_format":'"$RF"',"temperature":0,"max_tokens":400,"stream":false}' \
    "$BASE/v1/chat/completions" 2>&1)
CODE=$(printf '%s' "$J" | tail -1)
BODY=$(printf '%s' "$J" | sed '$d')
JC=$(printf '%s' "$BODY" | jget choices.0.message.content | tr -d '\n' | cut -c1-60)
JE=$(printf '%s' "$BODY" | jget error.message)
JF=$(printf '%s' "$BODY" | jget choices.0.finish_reason)
if [ "$CODE" = 200 ] && [ -n "$JC" ]; then
    say "4b json_schema $DIALECT" "HTTP 200 $JC"
elif [ "$CODE" = 400 ] && [ "$DIALECT" = llamacpp ]; then
    say "4b json_schema llamacpp" "HTTP 400 ${JE:-<no error message>} <- expected on an OpenAI-dialect server (issue A): register backend=openai"
else
    say "4b json_schema $DIALECT" "HTTP $CODE ${JE:-${BODY:0:80}} [finish=$JF] -> the $DIALECT envelope was refused"
    fail=$((fail+1))
fi

# --- 5. cross-side check ------------------------------------------------------
if [ "$IN_WSL" = 1 ]; then
    echo "== step 5: running inside WSL (${WSL_DISTRO_NAME:-?}); using the Windows host IP"
    say "   windows host" "$(wsl_host_ip)  (Strata binds 0.0.0.0, so the host IP works)"
elif command -v wsl.exe >/dev/null 2>&1; then
D="${ACTA_WSL_DISTRO:-$(wsl.exe -l -q 2>/dev/null | tr -d '\000' | head -1 | tr -d '\r')}"
if [ -n "$D" ]; then
    echo "== step 5: $BASE/health from WSL ($D)"
    wsl.exe -d "$D" -e bash -lc '
        PORT="$1"
        P=$(command -v python3 || echo /usr/bin/python3)
        GW=$(awk "\$2==\"00000000\" {print \$3}" /proc/net/route | head -1)
        HOST=$(printf "%d.%d.%d.%d" 0x${GW:6:2} 0x${GW:4:2} 0x${GW:2:2} 0x${GW:0:2})
        echo "   wsl default route -> windows host: $HOST (probing port $PORT)"
        for t in 127.0.0.1 "$HOST"; do
            $P -c "import socket,sys
try:
    socket.create_connection((sys.argv[1],int(sys.argv[2])),3); print(\"   REACHABLE  \",sys.argv[1])
except Exception as e: print(\"   unreachable\",sys.argv[1],type(e).__name__)" "$t" "$PORT"
        done' strata "$PORT" 2>&1 | tr -d '\r'
    # second attempt: the first packet across the NAT/Hyper-V firewall can be dropped
    echo "   (retry)"
    wsl.exe -d "$D" -e bash -lc '
        PORT="$1"; P=$(command -v python3 || echo /usr/bin/python3)
        GW=$(awk "\$2==\"00000000\" {print \$3}" /proc/net/route | head -1)
        HOST=$(printf "%d.%d.%d.%d" 0x${GW:6:2} 0x${GW:4:2} 0x${GW:2:2} 0x${GW:0:2})
        $P -c "import socket,sys
try:
    socket.create_connection((sys.argv[1],int(sys.argv[2])),4); print(\"   REACHABLE  \",sys.argv[1])
except Exception as e: print(\"   unreachable\",sys.argv[1],type(e).__name__)" "$HOST" "$PORT"' strata "$PORT" 2>&1 | tr -d '\r'
fi
else
    echo "== step 5: skipped (no wsl.exe on PATH)"
fi

echo "== verdict"
echo "   model to register: backend=$DIALECT base_url=$BASE model_identifier=${ID}"
[ "$fail" -eq 0 ] && exit 0 || exit 1
