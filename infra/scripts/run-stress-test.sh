#!/usr/bin/env bash
# Runs the CoreKV stress test from the LOAD GENERATOR instance.
#
# Runs several scenarios sequentially; each one is a fresh corekv-bench
# process exposing live metrics on :9091 for Prometheus/Grafana.
#
# usage: bash run-stress-test.sh [SERVER_HOST]
#
# Optional env overrides:
#   THREADS      concurrent benchmark connections   (default 100)
#   KEYS         key-space size, pre-populated      (default 200000)
#   VAL_SIZE     value size in bytes                (default 64)
#   DURATION     per-scenario seconds               (default 60)
#   QPS_UNLIMITED  1 = run max-throughput (default) 0 = fixed QPS
#   QPS          target aggregate QPS when QPS_UNLIMITED=0
set -euo pipefail

SERVER_HOST="${1:-${SERVER_HOST:?set SERVER_HOST to the corekv server private IP}}"
THREADS="${THREADS:-100}"
KEYS="${KEYS:-200000}"
VAL_SIZE="${VAL_SIZE:-64}"
DURATION="${DURATION:-60}"
QPS_UNLIMITED="${QPS_UNLIMITED:-1}"
QPS="${QPS:-50000}"
RESULTS_DIR="${RESULTS_DIR:-./results}"

mkdir -p "$RESULTS_DIR"
echo "==> Target server: $SERVER_HOST:8000 (threads=$THREADS keys=$KEYS val=$VAL_SIZE dur=${DURATION}s)"

echo "==> Waiting for the CoreKV server"
up=0
for i in $(seq 1 60); do
  if timeout 2 bash -c "exec 3<>/dev/tcp/$SERVER_HOST/8000" 2>/dev/null; then
    up=1
    break
  fi
  sleep 2
done
if [ "$up" != "1" ]; then
  echo "ERROR: server not reachable at $SERVER_HOST:8000" >&2
  exit 1
fi
echo "    server is up"

echo "==> Sanity check (single thread, 1s, 500 qps)"
docker run --rm --network host corekv-bench \
  --host "$SERVER_HOST" --port 8000 --threads 1 --keys 100 --val-size 16 \
  --mix 100:0:0 --duration 1 --qps 500 --metrics-port 0 --report-interval 0

run_scenario() {
  local name="$1"
  shift
  echo "==> Scenario: $name"
  docker run --rm --network host corekv-bench "$@" \
    | tee "$RESULTS_DIR/$name.log"
}

run_scenario balanced \
  --host "$SERVER_HOST" --threads "$THREADS" --keys "$KEYS" --val-size "$VAL_SIZE" \
  --mix 50:40:10 --duration "$DURATION" --metrics-port 9091 --report-interval 5

run_scenario read-heavy \
  --host "$SERVER_HOST" --threads "$THREADS" --keys "$KEYS" --val-size "$VAL_SIZE" \
  --mix 20:75:5 --duration "$DURATION" --metrics-port 9091 --report-interval 5

run_scenario write-heavy \
  --host "$SERVER_HOST" --threads "$THREADS" --keys "$KEYS" --val-size "$VAL_SIZE" \
  --mix 75:20:5 --duration "$DURATION" --metrics-port 9091 --report-interval 5

if [ "$QPS_UNLIMITED" = "1" ]; then
  run_scenario max-throughput \
    --host "$SERVER_HOST" --threads "$THREADS" --keys "$KEYS" --val-size "$VAL_SIZE" \
    --mix 50:40:10 --duration "$DURATION" --metrics-port 9091 --report-interval 5
else
  run_scenario fixed-qps \
    --host "$SERVER_HOST" --threads "$THREADS" --keys "$KEYS" --val-size "$VAL_SIZE" \
    --mix 50:40:10 --duration "$DURATION" --qps "$QPS" --metrics-port 9091 --report-interval 5
fi

echo ""
echo "==> Summary"
bash "$(dirname "$0")/collect-results.sh" "$RESULTS_DIR"
echo ""
echo "==> Done. Results in $RESULTS_DIR/. Full metrics were collected live by"
echo "    Prometheus during each run - grab screenshots from the Grafana dashboard."
