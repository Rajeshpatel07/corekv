#!/usr/bin/env bash
# Renders infra/prometheus/prometheus.yml from prometheus.template.yml with the
# real private IPs baked in. Must run BEFORE `docker compose up` whenever the
# IPs change (Prometheus does NOT expand env vars in scrape_configs).
#
# usage: bash render-prometheus-config.sh
#   SERVER_PRIVATE_IP  server EC2 private IP  (default 127.0.0.1)
#   LOADGEN_PRIVATE_IP loadgen EC2 private IP (default 127.0.0.1)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/infra/prometheus/prometheus.template.yml"
OUT="$ROOT/infra/prometheus/prometheus.yml"
SERVER_IP="${SERVER_PRIVATE_IP:-127.0.0.1}"
LOADGEN_IP="${LOADGEN_PRIVATE_IP:-127.0.0.1}"

sed -e "s/__SERVER_PRIVATE_IP__/${SERVER_IP}/g" \
    -e "s/__LOADGEN_PRIVATE_IP__/${LOADGEN_IP}/g" \
    "$SRC" > "$OUT"

echo "rendered $OUT"
echo "  server-node  = $SERVER_IP:9100"
echo "  loadgen-node = $LOADGEN_IP:9100"
echo "  bench        = $LOADGEN_IP:9091"
