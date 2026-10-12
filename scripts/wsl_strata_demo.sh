#!/usr/bin/env bash
# wsl_strata_demo.sh — run one ACTA execution from a WSL build against Strata
# on the Windows host (WSL2 NAT mode: reach the host through its adapter IP).
#
# Run inside WSL, from the repo root:   scripts/wsl_strata_demo.sh
# Env: ACTA_STRATA_BASE   default http://<windows-host-ip>:8080
#      ACTA_DB            default ./acta_wsl.db
set -u
cd "$(dirname "$0")/.."

wsl_host_ip() {
    local gw
    gw=$(awk '$2=="00000000" {print $3}' /proc/net/route | head -1)
    [ -n "$gw" ] && printf '%d.%d.%d.%d' 0x${gw:6:2} 0x${gw:4:2} 0x${gw:2:2} 0x${gw:0:2}
}

HOST=$(wsl_host_ip)
BASE="${ACTA_STRATA_BASE:-http://${HOST:-127.0.0.1}:8080}"
export ACTA_DB="${ACTA_DB:-$PWD/acta_wsl.db}"
export OPENAI_API_KEY="${OPENAI_API_KEY-}"
CLI=acta_cli/acta_cli
RUN=acta_runner/acta_runner

echo "== build check"
[ -x "$CLI" ] && [ -x "$RUN" ] || { echo "build first: make all"; exit 1; }
"$CLI" --version 2>/dev/null | head -1
"$RUN" --version 2>/dev/null | head -1
echo "== base_url: $BASE   db: $ACTA_DB"

rm -f "$ACTA_DB"
"$CLI" db init >/dev/null

echo "== 1. model"
"$CLI" model create --json "{\"name\":\"strata-nat\",\"backend\":\"openai\",\"base_url\":\"$BASE\",\"model_identifier\":\"qwen3.8-flash-next-iq3_xxs\"}" 2>/dev/null
echo "== 2. skills: plain (no schema) + schema"
"$CLI" skill create --json '{"name":"summarize","prompt_template":"Summarize the input in one short sentence."}' 2>/dev/null
"$CLI" skill create --json '{"name":"sentiment","prompt_template":"Classify the sentiment of the input. Reply with JSON only.","output_schema":"{\"type\":\"object\",\"required\":[\"label\",\"confidence\"],\"properties\":{\"label\":{\"type\":\"string\"},\"confidence\":{\"type\":\"number\"}}}"}' 2>/dev/null
echo "== 3. context"
"$CLI" context create --json '{"type":"text","content":"The build system shipped on time and the release went smoothly."}' 2>/dev/null
echo "== 4. executions: 1 = plain skill, 2 = skill with output_schema"
"$CLI" exec create --json '{"context_id":1,"skill_revision_id":1,"model_revision_id":1}' 2>/dev/null
"$CLI" exec create --json '{"context_id":1,"skill_revision_id":2,"model_revision_id":1}' 2>/dev/null

echo "== 5. check (model record 1)"
"$RUN" check 1 2>/dev/null
echo "== 6. run 1 (plain: no response_format)"
"$RUN" run 1 2>&1 | grep -v "^\[warn\]\|^warning:" | tail -2
echo "== 7. run 2 (output_schema -> response_format json_schema)"
"$RUN" run 2 2>&1 | grep -v "^\[warn\]\|^warning:" | tail -2

echo "== 8. results"
"$CLI" exec get 1 2>/dev/null
"$CLI" exec get 2 2>/dev/null
echo "== 9. log of execution 1"
"$CLI" log list 1 2>/dev/null | python3 -c '
import json,sys
for e in json.load(sys.stdin):
    print("  %-18s %s" % (e["event"], (e.get("metadata") or "")[:110]))
'
