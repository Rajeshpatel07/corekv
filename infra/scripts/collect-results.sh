#!/usr/bin/env bash
# Summarizes the corekv-bench result logs into a compact table.
#
# usage: bash collect-results.sh [RESULTS_DIR]
set -euo pipefail
RESULTS_DIR="${1:-./results}"

field() {
  local line="$1"
  local label="$2"
  local next="$3"
  if [ -z "$next" ]; then
    echo "$line" | sed -E "s/.*${label}=([0-9.]+).*/\1/"
  else
    echo "$line" | sed -E "s/.*${label}=([0-9.]+)${next}.*/\1/"
  fi
}

if [ ! -d "$RESULTS_DIR" ]; then
  echo "No results directory: $RESULTS_DIR" >&2
  exit 1
fi

echo "=== COREEV STRESS TEST SUMMARY ==="
printf "%-16s %12s %10s %12s | %8s %8s %8s %8s %8s\n" \
  "scenario" "total_ops" "errors" "qps" "get_p50" "get_p95" "get_p99" "get_p999" "get_mean"
printf "%-16s %12s %10s %12s | %8s %8s %8s %8s %8s\n" \
  "--------" "---------" "------" "---" "-------" "-------" "-------" "--------" "--------"

for f in "$RESULTS_DIR"/*.log; do
  [ -f "$f" ] || continue
  name="$(basename "$f" .log)"
  line_result="$(grep -m1 '^RESULT ' "$f" || true)"
  line_get="$(grep -m1 '^RESULT_LAT get ' "$f" || true)"
  line_set="$(grep -m1 '^RESULT_LAT set ' "$f" || true)"
  line_del="$(grep -m1 '^RESULT_LAT del ' "$f" || true)"

  if [ -z "$line_result" ]; then
    echo "    $name : no RESULT line (scenario may have failed)"
    continue
  fi

  total_ops="$(field "$line_result" "total_ops" " errors")"
  errors="$(field "$line_result" "errors" " elapsed")"
  qps="$(field "$line_result" "qps" "$")"

  g50="$(field "$line_get" "p50" " p95")"
  g95="$(field "$line_get" "p95" " p99")"
  g99="$(field "$line_get" "p99" " p999")"
  g999="$(field "$line_get" "p999" " mean")"
  gmean="$(field "$line_get" "mean" "")"

  printf "%-16s %12s %10s %12s | %8s %8s %8s %8s %8s\n" \
    "$name" "$total_ops" "$errors" "$qps" "$g50" "$g95" "$g99" "$g999" "$gmean"
done

echo ""
echo "All latency values are in microseconds (µs). See individual .log files for"
echo "set/del breakdowns and full percentile reports."
