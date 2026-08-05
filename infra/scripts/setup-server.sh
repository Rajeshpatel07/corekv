#!/usr/bin/env bash
# Bootstrap script for the CoreKV SERVER EC2 instance.
#
# Installs Docker, clones the repo, then starts:
#   corekv-server + node-exporter + prometheus + grafana
#
# usage: bash setup-server.sh [LOADGEN_PRIVATE_IP]
set -euo pipefail

LOADGEN_PRIVATE_IP="${1:-${LOADGEN_PRIVATE_IP:-}}"
COREKV_REPO_URL="${COREKV_REPO_URL:-https://github.com/Rajeshpatel07/corekv.git}"

SERVER_PRIVATE_IP="$(curl -s http://169.254.169.254/latest/meta-data/local-ipv4)"
SERVER_PUBLIC_IP="$(curl -s http://169.254.169.254/latest/meta-data/public-ipv4 || echo '<unknown>')"

if [ -z "$LOADGEN_PRIVATE_IP" ]; then
  echo "WARNING: LOADGEN_PRIVATE_IP is empty; Prometheus will scrape 127.0.0.1:9100 for it."
  echo "         Pass it as the first argument or export LOADGEN_PRIVATE_IP."
fi

echo "==> Installing Docker + git"
if command -v dnf >/dev/null 2>&1; then
  sudo dnf install -y docker git
  sudo systemctl enable --now docker
elif command -v apt-get >/dev/null 2>&1; then
  sudo apt-get update -y
  sudo apt-get install -y docker.io git
  sudo systemctl enable --now docker
else
  echo "Unsupported package manager" >&2
  exit 1
fi

echo "==> Cloning CoreKV"
sudo mkdir -p /opt
if [ ! -d /opt/corekv/.git ]; then
  sudo git clone "$COREKV_REPO_URL" /opt/corekv
else
  sudo git -C /opt/corekv pull --ff-only || echo "    (could not pull, using existing checkout)"
fi

echo "==> Rendering the Prometheus config with real IPs"
sudo SERVER_PRIVATE_IP="$SERVER_PRIVATE_IP" LOADGEN_PRIVATE_IP="$LOADGEN_PRIVATE_IP" \
  bash /opt/corekv/infra/scripts/render-prometheus-config.sh

echo "==> Starting the CoreKV server + monitoring stack"
sudo docker compose -f /opt/corekv/docker-compose.server.yml up -d --build

echo "==> Waiting for healthchecks"
sleep 10
sudo docker compose -f /opt/corekv/docker-compose.server.yml ps

echo ""
echo "===== DONE ====="
echo "CoreKV server : tcp://$SERVER_PRIVATE_IP:8000"
echo "Prometheus    : http://$SERVER_PUBLIC_IP:9090 (restricted by SG)"
echo "Grafana       : http://$SERVER_PUBLIC_IP:3000"
echo ""
echo "Next: set SERVER_NODE=$SERVER_PRIVATE_IP:9100 and LOADGEN_NODE=<loadgen ip>:9100"
echo "      in the CoreKV Stress Test dashboard variables, then run run-stress-test.sh"
echo "      on the load generator instance."
