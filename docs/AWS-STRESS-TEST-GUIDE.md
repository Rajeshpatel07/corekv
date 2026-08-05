# CoreKV — AWS Stress Testing & Performance Guide

This guide walks you through running a **production-style stress test** of the
CoreKV in-memory key-value store on AWS, producing:

1. **Latency percentiles** (p50 / p95 / p99 / p99.9 / mean) per operation type
2. A **Grafana dashboard** showing live latencies **and** system resource usage
   (CPU, memory, network) for both the server and the load generator
3. A repeatable, containerized setup
4. A results summary you can drop straight into a blog post

Everything is built from the files in this repo:

```
bench/corekv_bench.cpp               custom load generator (CoreKV wire protocol)
infra/docker/server.Dockerfile       builds the server image
infra/docker/bench.Dockerfile        builds the load generator image
docker-compose.server.yml            server stack: corekv + node-exporter + prometheus + grafana
docker-compose.loadgen.yml           loadgen stack: node-exporter (bench runs on demand)
infra/prometheus/prometheus.yml      scrape targets for Prometheus
infra/grafana/...                    auto-provisioned datasource + dashboard
infra/scripts/setup-server.sh        one-shot bootstrap for the server EC2 instance
infra/scripts/setup-loadgen.sh       one-shot bootstrap for the loadgen EC2 instance
infra/scripts/run-stress-test.sh     runs the full stress-test scenario suite
infra/scripts/collect-results.sh     prints a results table from the bench logs
docs/BLOG-POST-TEMPLATE.md           blog post skeleton (fill in your results)
```

---

## 0. Architecture

```
                        ┌─────────────────────────────┐
                        │  SERVER EC2  (e.g. c5.large)│
                        │                             │
  LOADGEN EC2           │  corekv-server  :8000       │
  (e.g. c5.xlarge)      │  node-exporter  :9100       │
                        │  prometheus     :9090       │
  corekv-bench ──tcp──► │  grafana        :3000       │
  :9091 (metrics)       └─────────────────────────────┘
        │                        ▲
        └────── Prometheus scrapes: ──┘
              * bench :9091   (live QPS + latency percentiles)
              * node-exporter :9100 on BOTH instances (CPU, RAM, net)
```

Key design decision: the load generator runs on a **separate instance** so its
CPU usage never distorts the server's resource graphs.

> **Important architectural note for your blog post.** The CoreKV server is a
> **single-threaded `poll()` event loop** (`src/net/server.cpp`). It can only
> use one CPU core. Your stress results will show the server at ~100% on one
> core while the instance is mostly idle overall — that is *expected* and is
> the #1 optimization opportunity (multi-threaded worker threads, epoll, etc.).

---

## 1. Prerequisites

- An **AWS account** with billing enabled.
- `aws` CLI installed locally (optional but handy for SG rules / queries):
  ```bash
  aws --version
  ```
- A computer with **Docker** running for the local dry-run (Step 2).
- Estimated cost: **~$1–3/day** with the instance sizes below (see Step 10 for
  exact numbers). **Terminate instances when done.**

---

## 2. Local full-stack run (do this FIRST, it's free)

Validate the **entire harness including the Grafana dashboard in your browser**
before spending a cent on AWS. Requirements: Docker (running) and a browser.

### 2a. Sanity-check the load generator against a local server

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/corekv-server &          # listens on 127.0.0.1:8000
```

```bash
g++ -O3 -std=c++17 -pthread bench/corekv_bench.cpp -o /tmp/corekv-bench
/tmp/corekv-bench --host 127.0.0.1 --port 8000 \
  --threads 8 --keys 5000 --val-size 64 --mix 50:40:10 \
  --duration 5 --metrics-port 9091
```

You should see a live progress report and a final `RESULT`/`RESULT_LAT` block
with percentiles in microseconds. Stop the bare server (`kill %1`) before
starting the Docker stack below — the stack runs its own server on :8000.

### 2b. Bring up the monitoring stack + dashboard

1. Render the Prometheus config for local use (Prometheus does **not** expand
   env vars in `scrape_configs`, so IPs must be baked in):
   ```bash
   bash infra/scripts/render-prometheus-config.sh
   # defaults to 127.0.0.1 for both nodes
   ```
2. Build and start the full stack (server, node-exporter, Prometheus, Grafana):
   ```bash
   GRAFANA_ADMIN_PASSWORD=change-me docker compose -f docker-compose.server.yml up -d --build
   docker compose -f docker-compose.server.yml ps   # corekv-server should say (healthy)
   ```
3. **Open the dashboard in your browser:** http://localhost:3000
   Login with `admin` / `<GRAFANA_ADMIN_PASSWORD>`. The **CoreKV Stress Test**
   dashboard is auto-provisioned. With nothing running it shows empty latency
   panels — that's expected; latency data only exists while the load generator
   runs.
4. Generate live traffic and watch the dashboard populate:
   ```bash
   docker build -f infra/docker/bench.Dockerfile -t corekv-bench .
   docker run --rm --network host corekv-bench \
     --host 127.0.0.1 --port 8000 --threads 50 --keys 100000 \
     --val-size 64 --mix 50:40:10 --duration 60 --metrics-port 9091
   ```
   While it runs, the dashboard shows QPS, p50/p95/p99/p99.9 latencies, and
   CPU/memory/network for the server. For the blog, screenshot the dashboard
   *during* a run (`Shift+Ctrl+E` → "View as PNG" in Grafana exports a clean
   image).
5. Confirm Prometheus sees everything:
   ```bash
   curl -s http://127.0.0.1:9090/api/v1/targets | grep '"health"'
   # node exporters: up, corekv-bench: up while step 4 runs
   ```

Everything you just did is exactly what the AWS instances will do, just on
bigger machines. When you're done, `docker compose -f docker-compose.server.yml down`.

---

## 3. AWS networking: security groups

Use your **default VPC** to keep this simple. Create **two security groups**
in the same region:

### `corekv-server-sg` (attach to the SERVER instance)

| Type | Protocol | Port | Source | Purpose |
|------|----------|------|--------|---------|
| SSH | TCP | 22 | your-IP/32 | admin |
| Custom TCP | TCP | 8000 | `corekv-loadgen-sg` | CoreKV traffic (only from loadgen) |
| Custom TCP | TCP | 3000 | your-IP/32 | Grafana UI |
| Custom TCP | TCP | 9090 | your-IP/32 | Prometheus UI (optional) |

### `corekv-loadgen-sg` (attach to the LOADGEN instance)

| Type | Protocol | Port | Source | Purpose |
|------|----------|------|--------|---------|
| SSH | TCP | 22 | your-IP/32 | admin |
| Custom TCP | TCP | 9091 | `corekv-server-sg` | Prometheus scrapes bench metrics |
| Custom TCP | TCP | 9100 | `corekv-server-sg` | Prometheus scrapes node metrics |

> **Security note.** Do NOT open 8000/3000/9090/9091 to `0.0.0.0/0`. Prometheus
> and Grafana contain no auth for the former and a weak default password for the
> latter. Keep everything scoped to the security groups + your IP. The two
> instances talk to each other over their **private** IPs.

CLI one-liner (adjust for your region/account):
```bash
AWS_REGION=us-east-1
MY_IP=$(curl -s ifconfig.me)

# Server SG
SG_SERVER=$(aws ec2 create-security-group --region $AWS_REGION \
  --group-name corekv-server-sg --description "corekv server" \
  --query 'GroupId' --output text)
# Loadgen SG
SG_LOADGEN=$(aws ec2 create-security-group --region $AWS_REGION \
  --group-name corekv-loadgen-sg --description "corekv loadgen" \
  --query 'GroupId' --output text)

aws ec2 authorize-security-group-ingress --region $AWS_REGION --group-id $SG_SERVER \
  --ip-permissions \
    "IpProtocol=tcp,FromPort=22,ToPort=22,IpRanges=[{CidrIp=$MY_IP/32}]" \
    "IpProtocol=tcp,FromPort=8000,ToPort=8000,UserIdGroupPairs=[{GroupId=$SG_LOADGEN}]" \
    "IpProtocol=tcp,FromPort=3000,ToPort=3000,IpRanges=[{CidrIp=$MY_IP/32}]" \
    "IpProtocol=tcp,FromPort=9090,ToPort=9090,IpRanges=[{CidrIp=$MY_IP/32}]"

aws ec2 authorize-security-group-ingress --region $AWS_REGION --group-id $SG_LOADGEN \
  --ip-permissions \
    "IpProtocol=tcp,FromPort=22,ToPort=22,IpRanges=[{CidrIp=$MY_IP/32}]" \
    "IpProtocol=tcp,FromPort=9091,ToPort=9091,UserIdGroupPairs=[{GroupId=$SG_SERVER}]" \
    "IpProtocol=tcp,FromPort=9100,ToPort=9100,UserIdGroupPairs=[{GroupId=$SG_SERVER}]"
```

---

## 4. Key pair

```bash
aws ec2 create-key-pair --region $AWS_REGION --key-name corekv-test \
  --query 'KeyMaterial' --output text > corekv-test.pem
chmod 400 corekv-test.pem
```

---

## 5. Launch the SERVER instance

Since CoreKV is **single-threaded**, you do **not** need a big instance — you
need **RAM for the key space** and **one fast core**.

### Choosing the instance type & size

- **CPU is not the dimension that grows** — one core saturates regardless of
  instance size, so bigger vCPU counts only help the small overhead of the OS +
  Prometheus + Grafana sharing the box.
- **Pick RAM based on your working set** (approx, in-memory store, plus
  overhead): `keyspace × value-size × ~2.5` bytes. E.g. 1M keys × 128 B values
  ≈ `1e6 × 128 × 2.5` ≈ **320 MB** of data.
- A 30–60 s test holds only `keys ` entry records + their values in RAM, so
  4 GB is plenty for any realistic keyspace.

Recommended starting points (us-east-1, x86_64):

| Your test size | Server type | vCPU | RAM | Notes |
|----------------|-------------|------|-----|-------|
| small (≤ 200k keys) | `c5.large` | 2 | 4 GB | default in these instructions |
| larger keyspace | `c5.xlarge` | 4 | 8 GB | more headroom for the monitor stack |
| many instances / demo | `t3.large` | 2 | 8 GB | cheaper burstable (watch CPU credit burndown!) |

| Attribute | Value |
|-----------|-------|
| AMI | **Amazon Linux 2023** (x86_64, free tier friendly) |
| Type | `c5.large` (2 vCPU, 4 GB) — start here |
| Storage | 20 GB gp3 |
| Security group | `corekv-server-sg` |
| Key | `corekv-test` |

CLI:
```bash
AMI=$(aws ec2 describe-images --region $AWS_REGION --owners amazon \
  --filters "Name=name,Values=al2023-ami-2023.*x86_64" \
  --query 'sort_by(Images,&CreationDate)[-1].ImageId' --output text)

aws ec2 run-instances --region $AWS_REGION \
  --image-id $AMI --instance-type c5.large \
  --key-name corekv-test \
  --security-group-ids $SG_SERVER \
  --block-device-mappings 'DeviceName=/dev/xvda,Ebs={VolumeSize=20,VolumeType=gp3}' \
  --tag-specifications 'ResourceType=instance,Tags=[{Key=Name,Value=corekv-server}]'
```

Write down the **public IP** (for SSH/Grafana) and **private IP** (for the load
generator and dashboard variables).

> **Tip for `t3`/`t4g` burstable instances:** they throttle CPU when burst
> credits run out, which will look like a sudden throughput cliff in your
> dashboard and can invalidate latency numbers. Use `c5`/`c6i`/`m7i` (or
> dedicated) for clean measurements.

---

## 6. Launch the LOAD GENERATOR instance

The load generator needs more cores so its own CPU usage never becomes the
bottleneck. As a rule of thumb it should have roughly **2–4× the vCPU of the
single-threaded server**, because the client burns CPU for every connection and
may be the first to saturate.

| Test size | Loadgen type | vCPU | RAM | Notes |
|-----------|--------------|------|-----|-------|
| default | `c5.xlarge` | 4 | 8 GB | recommended |
| tabletop (few hundred K qps) | `c5.large` | 2 | 4 GB | fine at ≤ ~100k ops/s |
| very high concurrency | `c5.2xlarge` | 8 | 16 GB | 200+ connections |

The load generator needs to serve:
- `THREADS` benchmark connections to the server (this is the main CPU user)
- one port 9091 (live metrics) and node-exporter on 9100 — trivial

| Attribute | Value |
|-----------|-------|
| Type | `c5.xlarge` (4 vCPU, 8 GB) |
| Security group | `corekv-loadgen-sg` |
| Key | `corekv-test` |

```bash
aws ec2 run-instances --region $AWS_REGION \
  --image-id $AMI --instance-type c5.xlarge \
  --key-name corekv-test \
  --security-group-ids $SG_LOADGEN \
  --block-device-mappings 'DeviceName=/dev/xvda,Ebs={VolumeSize=20,VolumeType=gp3}' \
  --tag-specifications 'ResourceType=instance,Tags=[{Key=Name,Value=corekv-loadgen}]'
```

> **Why two instances?** A benchmark client is CPU-hungry. If it shares the box
> with the server, its CPU usage shows up on the server graphs and you can't
> tell which side is the bottleneck.

---

## 7. Bootstrap the SERVER instance

SSH in and run the setup script. It installs Docker, clones the repo, and
starts `corekv-server + node-exporter + prometheus + grafana`.

```bash
ssh -i corekv-test.pem ec2-user@<SERVER_PUBLIC_IP>
sudo bash -c 'echo "dns=none" > /etc/docker/daemon.json' # optional: faster image pulls w/o DNS races
# from the instance:
bash -c "$(curl -fsSL https://raw.githubusercontent.com/Rajeshpatel07/corekv/main/infra/scripts/setup-server.sh)" -- <LOADGEN_PRIVATE_IP>
```

If you prefer to copy the repo directly instead of cloning from GitHub:
```bash
# from your laptop
rsync -avz --exclude build --exclude .git . ec2-user@<SERVER_PUBLIC_IP>:/tmp/corekv
ssh ec2-user@<SERVER_PUBLIC_IP> \
  'sudo mkdir -p /opt && sudo cp -r /tmp/corekv /opt/ && cd /opt/corekv && sudo SERVER_PRIVATE_IP=... bash infra/scripts/setup-server.sh'
```

When it finishes it prints the endpoints. Verify each:

```bash
# from your laptop
curl -s http://<SERVER_PUBLIC_IP>:9090/-/ready          # Prometheus: "Prometheus is Ready"
curl -s http://<SERVER_PUBLIC_IP>:9090/api/v1/targets   # check all targets are UP
```

On the instance, confirm the stack:
```bash
sudo docker compose -f /opt/corekv/docker-compose.server.yml ps
```

---

## 8. Bootstrap the LOAD GENERATOR instance

```bash
ssh -i corekv-test.pem ec2-user@<LOADGEN_PUBLIC_IP>
bash -c "$(curl -fsSL https://raw.githubusercontent.com/Rajeshpatel07/corekv/main/infra/scripts/setup-loadgen.sh)" -- <SERVER_PRIVATE_IP>
```

This installs Docker, starts node-exporter, and builds the `corekv-bench`
image.

Verify connectivity from the load generator to the server:
```bash
nc -zv <SERVER_PRIVATE_IP> 8000
docker run --rm --network host corekv-bench --help
```

---

## 9. Open Grafana and point the dashboard at your instances

1. Browse to `http://<SERVER_PUBLIC_IP>:3000`
2. Login: `admin` / `corekv-admin` (override with `GRAFANA_ADMIN_PASSWORD` env
   when you run compose, **change this from the default**).
3. The datasource (`Prometheus`) and the **CoreKV Stress Test** dashboard are
   auto-provisioned.
4. Open the dashboard, click **Dashboard settings → Variables**, and set:
   - `SERVER_NODE` = `<SERVER_PRIVATE_IP>:9100`
   - `LOADGEN_NODE` = `<LOADGEN_PRIVATE_IP>:9100`
   (These are the `instance` labels Prometheus assigns to the node-exporter
   targets. The defaults are `127.0.0.1:9100` for the local dry-run.)

The dashboard shows (live, 2 s scrape interval):
- **Stats:** total QPS, errors/sec, GET p99, server CPU
- **Throughput:** QPS by op, cumulative ops
- **Latency:** p50 / p95 / p99 / p99.9 per op (µs) + GET vs SET comparison
- **System (server):** CPU & memory %, network I/O, TCP connections
- **System (loadgen):** CPU & memory %, network I/O

---

## 10. Run the stress test

On the **load generator** instance:

```bash
cd /opt/corekv
SERVER_HOST=<SERVER_PRIVATE_IP> bash infra/scripts/run-stress-test.sh
```

Defaults: 100 connections, 200k-key working set, 64 B values, 60 s per
scenario. It runs:

| Scenario | Mix (set:get:del) | What it shows |
|----------|-------------------|----------------|
| `balanced` | 50:40:10 | steady-state mixed workload |
| `read-heavy` | 20:75:5 | read-dominated workload (often the highest QPS) |
| `write-heavy` | 75:20:5 | write-dominated workload |
| `max-throughput` | 50:40:10, no QPS cap | absolute ceiling of the server |

Each scenario:
- pre-populates the 200k key space (so GETs hit)
- runs at max QPS (or a fixed QPS via `QPS_UNLIMITED=0 QPS=50000`)
- exposes live metrics on `:9091` → visible in Grafana during the run
- writes a full percentile report to `./results/<scenario>.log`

**Watch Grafana while it runs** — screenshot the dashboard for your blog post.

Tune with env vars:
```bash
THREADS=200 KEYS=500000 VAL_SIZE=256 DURATION=120 \
  SERVER_HOST=<SERVER_PRIVATE_IP> bash infra/scripts/run-stress-test.sh
```

---

## 11. Collect and read the results

On the load generator:

```bash
cd /opt/corekv
bash infra/scripts/collect-results.sh ./results
```

Example output:

```
scenario       total_ops     errors          qps | get_p50 get_p95 get_p99 get_p999 get_mean
balanced         439687          0     109746.8 |      64     125     139      862       75
read-heavy       100000          5       1666.7 |      45      85     110      750       50
...
```

Every `.log` file also contains the full breakdown (set / get / del with
p50/p95/p99/p99.9/mean) plus the live progress lines.

**Pull the logs to your laptop** for the blog post:
```bash
rsync -avz ec2-user@<LOADGEN_PUBLIC_IP>:/opt/corekv/results/ ./results/
```

For time-series figures (throughput over time, CPU during load), query
Prometheus directly and export the panel data from Grafana:

```bash
curl -g "http://<SERVER_PUBLIC_IP>:9090/api/v1/query?query=sum(rate(corekv_bench_ops_total%5B60s%5D))"
```

---

## 12. What to look for / how to read the results

- **Server CPU ≈ 100% on one core, but instance shows <50%**: confirms the
  single-threaded event loop is the bottleneck. Throughput will plateau no
  matter how many loadgen threads you add. This is your headline finding.
- **p99 latency climbs as QPS approaches the plateau**: classic single-threaded
  queueing. The gap between p50 and p99.9 at max throughput is the "tail
  latency" story.
- **Errors**: should be ~0. Non-zero errors mean the server dropped/closed
  connections under load (check `corekv_bench_ops_total{op="error"}`).
- **Memory**: stays flat (in-memory store; grows with key space and value
  sizes). A "memory leak" would show as a steady climb during the test.
- **TCP established**: should stay ≈ number of bench connections; sudden drops
  indicate the server couldn't keep up with new connections.

---

## 13. Costs & cleanup

Rough daily cost (us-east-1, on-demand, ~24 h):

| Instance | Type | ~$/day |
|----------|------|--------|
| server | c5.large | $0.20 |
| loadgen | c5.xlarge | $0.40 |
| **Total** | | **~$0.60/day** |

**Terminate when done** — this is the most important step:
```bash
aws ec2 terminate-instances --region $AWS_REGION \
  --instance-ids $(aws ec2 describe-instances --region $AWS_REGION \
    --filters "Name=tag:Name,Values=corekv-server,corekv-loadgen" \
    --query 'Reservations[].Instances[].InstanceId' --output text)
```

Optionally delete the security groups afterward. If you're just done forever,
nuke the default VPC additions you made.

---

## 14. Troubleshooting

| Symptom | Fix |
|---------|-----|
| `Failed to connect` from bench | Check SG rule for 8000 (must reference loadgen SG), confirm server on :8000 (`sudo docker compose ps`), test `nc -zv <ip> 8000` |
| Prometheus targets `DOWN` / URL shows literal `__SERVER_PRIVATE_IP__` | Prometheus does **not** expand env vars in `scrape_configs`. Re-run `infra/scripts/render-prometheus-config.sh` with `SERVER_PRIVATE_IP`/`LOADGEN_PRIVATE_IP` set, then `docker compose up -d`. Never edit `infra/prometheus/prometheus.yml` by hand (it's generated + gitignored). |
| Docker build fails: `ERROR: externally-managed-environment` | PIP blocks pip installs on some bases; `server.Dockerfile` uses `--break-system-packages`. If you see it, add that flag to your pip install. |
| `./corekv-server: /usr/lib/x86_64-linux-gnu/libstdc++.so.6: version GLIBCXX_3.4.32 not found` | The build image shipped a newer libstdc++ than the runtime base. The Dockerfiles build on `gcc:13` but run on **`ubuntu:24.04`** which has libstdc++ 6.0.32 (GLIBCXX_3.4.32). Don't use `debian:bookworm-slim` as the runtime base. |
| Compose errors: `services.prometheus` missing / wrong nesting | Ensure every service is indented under `services:` (a common paste bug). |
| Grafana shows "No data" for latency | The bench must be *running* (metrics only exist during a run). Run `run-stress-test.sh` and watch. |
| Grafana shows "No data" for CPU | Update `SERVER_NODE` / `LOADGEN_NODE` dashboard variables to `<private-ip>:9100` |
| Wrong percentiles in panel | Panels read `corekv_bench_duration_seconds{quantile=...}` from the live bench; final authoritative numbers are the `RESULT_LAT` lines in `results/*.log` |
| Grafana admin login fails | Password is `corekv-admin` by default or whatever you set via `GRAFANA_ADMIN_PASSWORD` |
| Build too slow in Docker | Build once, tag the image, and push to ECR; or skip Docker for the server and run `./build/bin/corekv-server` directly |

---

## 15. Going further (production hardening ideas for the blog)

- **Server instrumentation:** add a `/metrics` endpoint to the server itself
  (Prometheus C++ client or a hand-rolled text endpoint) to measure
  server-side latency and internal queue depth, separate from client-side RTT.
- **Multi-threading:** the current design is a single-threaded event loop;
  splitting work across worker threads or an epoll-based loop is the next big
  lever. Re-run this exact harness after the change to show the delta.
- **Pipelining:** the bench is request/response. Adding command pipelining in
  the client would show network-bound vs CPU-bound behavior.
- **Alerting:** add Prometheus alert rules (e.g., `error rate > 0` for 1 min)
  and wire them into Grafana alerting.

---

## 16. Now write the blog post

Open `docs/BLOG-POST-TEMPLATE.md`, paste your `results/*.log` numbers and
Grafana screenshots into the `PLACEHOLDER` slots, and fill in the analysis
using the "What to look for" section above.
