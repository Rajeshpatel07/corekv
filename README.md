<p align="center">
  <img src="./assets/corekv.png" alt="CoreKV Logo" width="500">
</p>

<h1 align="center">CoreKV</h1>

<p align="center">A lightweight, high-performance key-value store written in C++</p>

---

## Requirements

- **C++ Compiler**: GCC 9+ or Clang 10+ (C++17 support)
- **CMake**: 3.31+
- **Make** or **Ninja**

---

## Setup

```bash
git clone https://github.com/Rajeshpatel07/corekv.git
cd corekv
```

---

## Build

### Production
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Debug/Dev
```bash
cmake -B build
cmake --build build
```

---


### Run server
```bash
./build/bin/corekv-server
```
### Run client
```bash
./build/bin/corekv-client
```

Listens on port **8000**.

---

## Load testing & stress testing

`corekv-bench` (in `bench/`) is a load generator that speaks the CoreKV wire
protocol. It reports p50/p95/p99/p99.9/mean latencies per operation and exposes
live Prometheus metrics for Grafana.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build

# 50 connections, 100k-key working set, balanced mix, 60s, metrics on :9091
./build/bin/corekv-bench --host 127.0.0.1 --port 8000 \
  --threads 50 --keys 100000 --val-size 64 --mix 50:40:10 \
  --duration 60 --metrics-port 9091
```

For a full AWS stress-test setup (Prometheus + Grafana dashboard + system
metrics + step-by-step guide + blog post template), see:

- `docs/AWS-STRESS-TEST-GUIDE.md` — the complete walkthrough
- `infra/` — Dockerfiles, docker-compose stacks, Prometheus & Grafana config
- `infra/scripts/` — one-shot bootstrap and test orchestration scripts

---

## Project Structure

```
src/
├── core/
├── net/
├── protocol/
├── handler/
├─ server.cpp
└─ client.cpp
```
