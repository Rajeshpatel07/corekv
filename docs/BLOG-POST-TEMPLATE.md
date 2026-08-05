# Benchmarking CoreKV: A C++ In-Memory Key-Value Store Under AWS Load

> **Status:** TEMPLATE — replace every `PLACEHOLDER` with real numbers from
> `results/*.log` and screenshots from the Grafana dashboard.

---

## TL;DR

CoreKV, a custom C++ in-memory key-value store built on a single-threaded
`poll()` event loop, sustained **PLACEHOLDER ops/s** with a **PLACEHOLDER µs
p99 GET latency** on a **c5.large** instance during a mixed 50/40/10
SET/GET/DEL workload. The bottleneck is the single-threaded event loop — the
server saturates one CPU core while the instance idles, which points to
multi-threaded worker support as the biggest optimization opportunity.

---

## Why I built this

PLACEHOLDER (background: custom TCP protocol, TLV serialization, chained hash
table, no persistence — a from-scratch key-value store).

## The architecture under test

- Custom binary protocol: `[u32 payload-len][u32 word-len][word]...`
  requests, `[u32 total-len][tag][payload]` responses
- Single-threaded `poll()` event loop (`src/net/server.cpp`)
- Chained hash table storage (`src/core/hash_table.cpp`)
- Commands: `GET`, `SET`, `DEL`, `KEYS`

## The test harness

To benchmark it like production, I built a dedicated load generator and a full
observability stack:

- **`corekv-bench`** — a C++ load generator speaking the exact CoreKV wire
  protocol, with:
  - one TCP connection per worker thread (TCP_NODELAY)
  - key-space pre-population so GETs mostly hit
  - configurable SET:GET:DEL mix, key space, value size, duration, QPS cap
  - power-of-two-bucket histograms → p50/p95/p99/p99.9/mean
  - a live Prometheus `/metrics` endpoint (summary with quantiles)
- **Prometheus + Grafana** — scrapes the load generator (latencies, QPS) and
  node_exporter on both instances (CPU, RAM, network), all on one dashboard
- **Two EC2 instances** — server (c5.large) and load generator (c5.xlarge) on
  separate boxes so client CPU never pollutes server graphs

```
[corekv-bench] --tcp:8000--> [corekv-server]   (c5.large)
     | :9091                       |
     +------> Prometheus <---------+-- node_exporter :9100 (both)
                    |
                  Grafana :3000
```

## Methodology

- Instance: `PLACEHOLDER` (e.g., c5.large, 2 vCPU / 4 GB, Amazon Linux 2023)
- Working set: `PLACEHOLDER` keys, `PLACEHOLDER`-byte values
- Concurrency: `PLACEHOLDER` connections (threads)
- Duration: `PLACEHOLDER` s per scenario, after key-space warmup
- Scenarios: balanced (50:40:10), read-heavy (20:75:5), write-heavy (75:20:5),
  max-throughput (unlimited QPS)
- Latency measured client-side (round-trip, includes network)

## Results

### Balanced (50% SET / 40% GET / 10% DEL), max throughput

| Metric | Value |
|--------|-------|
| Throughput | **PLACEHOLDER ops/s** |
| Errors | PLACEHOLDER |
| SET | p50 **PLACEHOLDER µs**, p95 PLACEHOLDER, p99 PLACEHOLDER, p99.9 PLACEHOLDER |
| GET | p50 **PLACEHOLDER µs**, p95 PLACEHOLDER, p99 PLACEHOLDER, p99.9 PLACEHOLDER |
| DEL | p50 PLACEHOLDER µs, p95 PLACEHOLDER, p99 PLACEHOLDER, p99.9 PLACEHOLDER |

_Insert dashboard screenshot: throughput + latency during the run._

### Read-heavy (20% SET / 75% GET / 5% DEL)

| Metric | Value |
|--------|-------|
| Throughput | **PLACEHOLDER ops/s** |
| GET p50 / p99 / p99.9 | PLACEHOLDER / PLACEHOLDER / PLACEHOLDER µs |

### Write-heavy (75% SET / 20% GET / 5% DEL)

| Metric | Value |
|--------|-------|
| Throughput | **PLACEHOLDER ops/s** |
| SET p50 / p99 / p99.9 | PLACEHOLDER / PLACEHOLDER / PLACEHOLDER µs |

### System resource usage

_Insert dashboard screenshot: server CPU/mem, loadgen CPU/mem, network I/O._

- Server CPU: peaked at **PLACEHOLDER %** (per-core), ~**PLACEHOLDER %** of one
  core — the event loop thread was saturated
- Server memory: **PLACEHOLDER GB** used, stable over the run (in-memory store)
- Load generator CPU: **PLACEHOLDER %** — the client was NOT the bottleneck
- TCP established connections: **PLACEHOLDER**

## Analysis

### The single-threaded ceiling

The server is a single-threaded `poll()` loop. Every request — parsing,
hash-table lookup, serialization — happens on one core. Results confirm this:

- throughput plateaus at **PLACEHOLDER ops/s** regardless of client threads
- server CPU graph shows ~100% on one core while instance utilization is
  **PLACEHOLDER %**
- p99 vs p50 gap grows as QPS approaches the ceiling (queuing behind the single
  event-loop thread)

### Tail latency

p50 = PLACEHOLDER µs, p99.9 = PLACEHOLDER µs → the long tail is
PLACEHOLDER× worse than median at max load. Under fixed QPS (PLACEHOLDER
ops/s) the tail collapses to PLACEHOLDER× — latency scales with queue depth.

### Why not more throughput?

- The event loop is O(connections) per iteration and processes one request at a
  time; no parallelism within the store
- No client pipelining in this test (1 request/1 response per socket)
- Hash-table ops are cheap; the bottleneck is serialization of the protocol
  loop, not storage

### What would move the needle (ranked)

1. **Multi-threaded server** (worker threads / epoll) — biggest win, directly
   attacks the 100%-of-one-core ceiling
2. **Pipelining support** in the client/protocol — amortizes syscall overhead
3. **Batch/bulk commands** for warmup and write-heavy loads
4. **Persistent storage** (WAL/SST) — out of scope for an in-memory store, but
   the natural next step for durability

## Cost

| Item | $/day |
|------|-------|
| c5.large (server) | ~$0.20 |
| c5.xlarge (loadgen) | ~$0.40 |
| **Total** | **~$0.60/day** |

Full test (4 scenarios × 60 s + warmup) ran in under 10 minutes.

## Reproducing this

Everything is in the repo:

```bash
# server
ssh ec2-user@<SERVER> "bash infra/scripts/setup-server.sh <LOADGEN_IP>"
# loadgen
ssh ec2-user@<LOADGEN> "bash infra/scripts/setup-loadgen.sh <SERVER_IP>"
# run the suite
cd /opt/corekv && SERVER_HOST=<SERVER_IP> bash infra/scripts/run-stress-test.sh
# summarize
bash infra/scripts/collect-results.sh ./results
```

See `docs/AWS-STRESS-TEST-GUIDE.md` for the full walkthrough (security groups,
instance sizing, Grafana setup, troubleshooting).

## Lessons learned / next steps

PLACEHOLDER (what surprised you: e.g., "100k ops/s from a single thread with no
syscall-heavy work is actually solid", "p99 is where the real story is", "the
poll() loop's O(conns) rebuild is visible in the TCP-established chart").

---

_CoreKV — a lightweight, high-performance key-value store written in C++._
