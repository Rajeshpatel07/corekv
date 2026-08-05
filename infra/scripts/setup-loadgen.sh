#!/usr/bin/env bash
# Bootstrap script for the CoreKV LOAD GENERATOR EC2 instance.
#
# Installs Docker, clones the repo, starts node-exporter, and builds the
# corekv-bench image. The actual stress test is run with run-stress-test.sh.
#
# usage: bash setup-loadgen.sh [SERVER_PRIVATE_IP]
set -euo pipefail

SERVER_PRIVATE_IP="${1:-${SERVER_PRIVATE_IP:-}}"
COREKV_REPO_URL="${COREKV_REPO_URL:-https://github.com/Rajeshpatel07/corekv.git}"

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

echo "==> Starting node-exporter"
sudo docker compose -f /opt/corekv/docker-compose.loadgen.yml up -d

echo "==> Building the corekv-bench image"
sudo docker build -f /opt/corekv/infra/docker/bench.Dockerfile -t corekv-bench /opt/corekv

echo ""
echo "===== DONE ====="
echo "Run the stress test with:"
echo "  cd /opt/corekv && SERVER_HOST=${SERVER_PRIVATE_IP:-<server-private-ip>} \\"
echo "    bash infra/scripts/run-stress-test.sh"
