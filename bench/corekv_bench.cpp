// corekv-bench: a load generator for the CoreKV binary protocol.
//
// Request framing (same as the CoreKV client):
//   [u32 BE total_payload_size][u32 BE word_len][word]...
// Response framing (produced by the server):
//   [u32 BE total_len][tag byte][payload]
//
// The tool:
//   * opens one TCP connection per worker thread
//   * pre-populates the key space (SET warmup) so GETs mostly hit
//   * runs a mixed SET/GET/DEL workload for a fixed duration
//   * records round-trip latency into power-of-two-bucket histograms
//     (one per operation type) and reports p50/p95/p99/p99.9/mean
//   * optionally exposes live metrics at :metrics-port in Prometheus
//     text format (a "summary" with quantiles) for Grafana dashboards

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <thread>
#include <vector>

using Clock = std::chrono::steady_clock;

// ---------------------------------------------------------------------------
// Power-of-two-bucket histogram. Bucket i covers [2^i ns, 2^(i+1) ns).
// ---------------------------------------------------------------------------
static constexpr int kHistBuckets = 64;

struct Histogram {
  uint64_t b[kHistBuckets] = {0};
  uint64_t count = 0;

  void add(uint64_t ns) {
    if (ns == 0)
      ns = 1;
    int idx = 63 - __builtin_clzll(ns);
    ++b[idx];
    ++count;
  }

  void merge(const Histogram &o) {
    for (int i = 0; i < kHistBuckets; ++i)
      b[i] += o.b[i];
    count += o.count;
  }

  static uint64_t lowOf(int i) { return 1ull << i; }
  static uint64_t highOf(int i) { return ((1ull << i) - 1) | (1ull << i); }

  // Value at percentile p (0 < p <= 1) in nanoseconds, linear interpolation
  // within the bucket. Returns 0 when empty.
  uint64_t percentile(double p) const {
    if (count == 0)
      return 0;
    uint64_t target = static_cast<uint64_t>(std::ceil(p * static_cast<double>(count)));
    if (target == 0)
      target = 1;
    uint64_t cum = 0;
    for (int i = 0; i < kHistBuckets; ++i) {
      if (b[i] == 0)
        continue;
      uint64_t before = cum;
      cum += b[i];
      if (cum >= target) {
        uint64_t posInBucket = target - before; // 1..b[i]
        long double width = static_cast<long double>(highOf(i)) - lowOf(i) + 1;
        long double frac = static_cast<long double>(posInBucket) /
                           static_cast<long double>(b[i]);
        return static_cast<uint64_t>(static_cast<long double>(lowOf(i)) + frac * width);
      }
    }
    return highOf(kHistBuckets - 1);
  }

  uint64_t meanNanos() const {
    if (count == 0)
      return 0;
    __int128 total = 0;
    for (int i = 0; i < kHistBuckets; ++i) {
      if (b[i] == 0)
        continue;
      uint64_t mid = lowOf(i) + ((highOf(i) - lowOf(i)) / 2);
      total += static_cast<__int128>(mid) * b[i];
    }
    return static_cast<uint64_t>(total / static_cast<__int128>(count));
  }

  // Sum of recorded latencies in nanoseconds (for Prometheus _sum metric).
  __int128 sumNanos() const {
    __int128 total = 0;
    for (int i = 0; i < kHistBuckets; ++i) {
      if (b[i] == 0)
        continue;
      uint64_t mid = lowOf(i) + ((highOf(i) - lowOf(i)) / 2);
      total += static_cast<__int128>(mid) * b[i];
    }
    return total;
  }
};

// ---------------------------------------------------------------------------
// Config / global state
// ---------------------------------------------------------------------------
struct Config {
  std::string host = "127.0.0.1";
  int port = 8000;
  int threads = 50;
  int keys = 100000;
  int valSize = 64;
  int durationSec = 60;
  double mixSet = 0.5, mixGet = 0.4, mixDel = 0.1;
  uint64_t qps = 0; // 0 = run as fast as possible
  int metricsPort = 9091;
  int reportInterval = 5;
};

struct Worker {
  Config cfg;
  int id = 0;
  int fd = -1;
  std::mt19937_64 rng;
  Histogram hSet, hGet, hDel;
  uint64_t nSet = 0, nGet = 0, nDel = 0, nErr = 0;
  std::atomic<uint64_t> *pacing = nullptr; // global next-op counter
  Clock::time_point start;
  std::string valueBuf;
  char keyBuf[32];

  bool connectSocket();
  void warmup(uint64_t firstKey, uint64_t lastKey);
  bool roundTrip(const std::string &payload, uint64_t *latencyNs);
  void run();
};

static std::atomic<uint64_t> gOps[4]; // 0=set 1=get 2=del 3=error
static std::atomic<bool> gDone{false};

// ---------------------------------------------------------------------------
// Socket helpers
// ---------------------------------------------------------------------------
static bool readExactly(int fd, void *buf, size_t n) {
  char *ptr = static_cast<char *>(buf);
  while (n > 0) {
    ssize_t rv = read(fd, ptr, n);
    if (rv <= 0)
      return false;
    ptr += rv;
    n -= static_cast<size_t>(rv);
  }
  return true;
}

static bool writeExactly(int fd, const void *buf, size_t n) {
  const char *ptr = static_cast<const char *>(buf);
  while (n > 0) {
    ssize_t rv = write(fd, ptr, n);
    if (rv <= 0)
      return false;
    ptr += rv;
    n -= static_cast<size_t>(rv);
  }
  return true;
}

static void appendU32(std::string &out, uint32_t v) {
  uint32_t net = htonl(v);
  out.append(reinterpret_cast<const char *>(&net), 4);
}

static uint64_t nowNanos() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          Clock::now().time_since_epoch()).count());
}

bool Worker::connectSocket() {
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return false;
  int one = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(cfg.port));
  if (inet_pton(AF_INET, cfg.host.c_str(), &addr.sin_addr) != 1)
    return false;
  if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
    return false;
  return true;
}

bool Worker::roundTrip(const std::string &payload, uint64_t *latencyNs) {
  uint64_t t0 = nowNanos();
  if (!writeExactly(fd, payload.data(), payload.size()))
    return false;

  uint32_t hdr;
  if (!readExactly(fd, &hdr, 4))
    return false;
  hdr = ntohl(hdr);
  if (hdr > (32u << 20)) // cap at 32 MB, matches server's max message size
    return false;

  static thread_local std::vector<uint8_t> body;
  body.resize(hdr);
  if (!readExactly(fd, body.data(), hdr))
    return false;

  *latencyNs = nowNanos() - t0;

  // First byte after the header is the response tag. TAG_ERR == 1.
  if (hdr >= 1 && body[0] == 1)
    return false;
  return true;
}

void Worker::warmup(uint64_t firstKey, uint64_t lastKey) {
  for (uint64_t k = firstKey; k < lastKey; ++k) {
    snprintf(keyBuf, sizeof(keyBuf), "key_%010llu",
             static_cast<unsigned long long>(k));
    std::string key(keyBuf);
    std::string val;
    val.reserve(static_cast<size_t>(cfg.valSize));
    for (int i = 0; i < cfg.valSize; ++i)
      val.push_back(static_cast<char>(rng() & 0xff));

    std::string payload;
    const char *cmd = "set";
    payload.reserve(4 + 4 + 3 + 4 + key.size() + 4 + val.size());
    appendU32(payload, static_cast<uint32_t>(4 + 3 + 4 + key.size() + 4 + val.size()));
    appendU32(payload, 3);
    payload.append(cmd);
    appendU32(payload, static_cast<uint32_t>(key.size()));
    payload.append(key);
    appendU32(payload, static_cast<uint32_t>(val.size()));
    payload.append(val);

    uint64_t lat = 0;
    if (!roundTrip(payload, &lat))
      ++nErr;
  }
}

void Worker::run() {
  uint64_t firstKey = (static_cast<uint64_t>(id) * cfg.keys) / cfg.threads;
  uint64_t lastKey = (static_cast<uint64_t>(id + 1) * cfg.keys) / cfg.threads;

  if (!connectSocket()) {
    fprintf(stderr, "[worker %d] failed to connect to %s:%d\n", id,
            cfg.host.c_str(), cfg.port);
    return;
  }

  warmup(firstKey, lastKey);

  std::string payload;
  payload.reserve(4 + 4 + 24 + 4 + static_cast<size_t>(cfg.valSize));

  while (true) {
    uint64_t n = pacing->fetch_add(1);
    if (cfg.qps > 0) {
      long double seconds = static_cast<long double>(n) / cfg.qps;
      auto target = start + std::chrono::duration_cast<Clock::duration>(
                                std::chrono::duration<long double>(seconds));
      std::this_thread::sleep_until(target);
    }

    if (Clock::now() - start >= std::chrono::seconds(cfg.durationSec))
      break;

    // Pick an operation from the mix.
    double r = static_cast<double>(rng() >> 11) * (1.0 / 9007199254740992.0);
    int op;
    if (r < cfg.mixSet)
      op = 0;
    else if (r < cfg.mixSet + cfg.mixGet)
      op = 1;
    else
      op = 2;

    uint64_t idx = rng() % static_cast<uint64_t>(cfg.keys);
    snprintf(keyBuf, sizeof(keyBuf), "key_%010llu",
             static_cast<unsigned long long>(idx));
    std::string key(keyBuf);

    if (op == 0) { // SET: fill value buffer with random bytes
      valueBuf.clear();
      valueBuf.reserve(static_cast<size_t>(cfg.valSize));
      for (int i = 0; i < cfg.valSize; ++i)
        valueBuf.push_back(static_cast<char>(rng() & 0xff));
    }

    payload.clear();
    {
      const char *cmd = op == 0 ? "set" : (op == 1 ? "get" : "del");
      uint32_t cmdLen = static_cast<uint32_t>(strlen(cmd));
      uint32_t words = 4 + cmdLen + 4 + static_cast<uint32_t>(key.size()) +
                       (op == 0 ? 4 + static_cast<uint32_t>(valueBuf.size()) : 0);
      appendU32(payload, words);
      appendU32(payload, cmdLen);
      payload.append(cmd);
      appendU32(payload, static_cast<uint32_t>(key.size()));
      payload.append(key);
      if (op == 0) {
        appendU32(payload, static_cast<uint32_t>(valueBuf.size()));
        payload.append(valueBuf);
      }
    }

    uint64_t lat = 0;
    bool ok = roundTrip(payload, &lat);
    if (op == 0) {
      ++nSet;
      if (ok)
        hSet.add(lat);
    } else if (op == 1) {
      ++nGet;
      if (ok)
        hGet.add(lat);
    } else {
      ++nDel;
      if (ok)
        hDel.add(lat);
    }
    if (!ok)
      ++nErr;

    gOps[op].fetch_add(1);
    if (!ok)
      gOps[3].fetch_add(1);
  }

  close(fd);
  fd = -1;
}

// ---------------------------------------------------------------------------
// Aggregate worker state
// ---------------------------------------------------------------------------
struct Aggregate {
  Histogram hSet, hGet, hDel;
  uint64_t nSet = 0, nGet = 0, nDel = 0, nErr = 0;
};

static void aggregateWorkers(const std::vector<Worker> &workers, Aggregate &agg) {
  for (const Worker &w : workers) {
    agg.hSet.merge(w.hSet);
    agg.hGet.merge(w.hGet);
    agg.hDel.merge(w.hDel);
    agg.nSet += w.nSet;
    agg.nGet += w.nGet;
    agg.nDel += w.nDel;
    agg.nErr += w.nErr;
  }
}

// ---------------------------------------------------------------------------
// Minimal Prometheus text-format HTTP endpoint
// ---------------------------------------------------------------------------
static void formatLatency(std::string &out, const char *op,
                          const Histogram &h) {
  const double quantiles[] = {0.5, 0.9, 0.95, 0.99, 0.999};
  char line[128];
  for (double q : quantiles) {
    double v = static_cast<double>(h.percentile(q)) / 1e9;
    snprintf(line, sizeof(line),
             "corekv_bench_duration_seconds{op=\"%s\",quantile=\"%g\"} %.9g\n",
             op, q, v);
    out.append(line);
  }
  snprintf(line, sizeof(line),
           "corekv_bench_duration_seconds_sum{op=\"%s\"} %.9g\n", op,
           static_cast<double>(h.sumNanos()) / 1e9);
  out.append(line);
  snprintf(line, sizeof(line),
           "corekv_bench_duration_seconds_count{op=\"%s\"} %llu\n", op,
           static_cast<unsigned long long>(h.count));
  out.append(line);
}

static std::string renderMetrics(const Aggregate &agg,
                                 uint64_t lastTotal, Clock::time_point lastT) {
  std::string out;
  char line[256];

  uint64_t now = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          Clock::now().time_since_epoch()).count());
  uint64_t total = gOps[0].load() + gOps[1].load() + gOps[2].load();
  double elapsedMs = 1.0;
  uint64_t lastMs = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          lastT.time_since_epoch()).count());
  if (now > lastMs)
    elapsedMs = static_cast<double>(now - lastMs);
  double qps = static_cast<double>(total - lastTotal) / (elapsedMs / 1000.0);

  out.append("# HELP corekv_bench_ops_total Total operations completed by type.\n");
  out.append("# TYPE corekv_bench_ops_total counter\n");
  snprintf(line, sizeof(line), "corekv_bench_ops_total{op=\"set\"} %llu\n",
           static_cast<unsigned long long>(gOps[0].load()));
  out.append(line);
  snprintf(line, sizeof(line), "corekv_bench_ops_total{op=\"get\"} %llu\n",
           static_cast<unsigned long long>(gOps[1].load()));
  out.append(line);
  snprintf(line, sizeof(line), "corekv_bench_ops_total{op=\"del\"} %llu\n",
           static_cast<unsigned long long>(gOps[2].load()));
  out.append(line);
  snprintf(line, sizeof(line), "corekv_bench_ops_total{op=\"error\"} %llu\n",
           static_cast<unsigned long long>(gOps[3].load()));
  out.append(line);

  out.append("# HELP corekv_bench_duration_seconds Per-op round-trip latency.\n");
  out.append("# TYPE corekv_bench_duration_seconds summary\n");
  formatLatency(out, "set", agg.hSet);
  formatLatency(out, "get", agg.hGet);
  formatLatency(out, "del", agg.hDel);

  out.append("# HELP corekv_bench_qps Instantaneous throughput.\n");
  out.append("# TYPE corekv_bench_qps gauge\n");
  snprintf(line, sizeof(line), "corekv_bench_qps %g\n", qps);
  out.append(line);

  return out;
}

static void metricsServer(int port, const std::vector<Worker> &workers) {
  int listenFd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenFd < 0)
    return;
  int one = 1;
  setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (bind(listenFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    fprintf(stderr, "metrics: bind on port %d failed\n", port);
    close(listenFd);
    return;
  }
  listen(listenFd, 16);

  uint64_t lastTotal = 0;
  Clock::time_point lastT = Clock::now();

  while (!gDone.load()) {
    pollfd pfd = {listenFd, POLLIN, 0};
    int rv = poll(&pfd, 1, 200);
    if (rv <= 0)
      continue;
    int clientFd = accept(listenFd, nullptr, nullptr);
    if (clientFd < 0)
      continue;

    char req[1024];
    ssize_t n = read(clientFd, req, sizeof(req) - 1);
    if (n > 0) {
      Aggregate agg;
      aggregateWorkers(workers, agg);
      std::string body = renderMetrics(agg, lastTotal, lastT);
      lastTotal = gOps[0].load() + gOps[1].load() + gOps[2].load();
      lastT = Clock::now();

      char head[256];
      snprintf(head, sizeof(head),
               "HTTP/1.1 200 OK\r\nContent-Type: text/plain; version=0.0.4; charset=utf-8\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
               body.size());
      write(clientFd, head, strlen(head));
      writeExactly(clientFd, body.data(), body.size());
    }
    close(clientFd);
  }

  close(listenFd);
}

// ---------------------------------------------------------------------------
// Periodic reporter
// ---------------------------------------------------------------------------
static void reporter(const Config &cfg, const std::vector<Worker> &workers) {
  if (cfg.reportInterval <= 0)
    return;
  Clock::time_point start = Clock::now();
  Aggregate prev;
  aggregateWorkers(workers, prev);
  Clock::time_point lastT = Clock::now();
  uint64_t lastTotal = prev.nSet + prev.nGet + prev.nDel;

  while (!gDone.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (Clock::now() - lastT < std::chrono::seconds(cfg.reportInterval))
      continue;
    Aggregate cur;
    aggregateWorkers(workers, cur);
    double dt = std::chrono::duration<double>(Clock::now() - lastT).count();
    uint64_t total = cur.nSet + cur.nGet + cur.nDel;
    double qps = static_cast<double>(total - lastTotal) / dt;
    double runSec = std::chrono::duration<double>(Clock::now() - start).count();

    printf("[t=+%6.1fs] qps=%10.0f ops=%llu errors=%llu\n", runSec, qps,
           static_cast<unsigned long long>(total),
           static_cast<unsigned long long>(cur.nErr));
    printf("    set  p50=%.0fus p95=%.0fus p99=%.0fus p99.9=%.0fus\n",
           cur.hSet.percentile(0.5) / 1000.0, cur.hSet.percentile(0.95) / 1000.0,
           cur.hSet.percentile(0.99) / 1000.0, cur.hSet.percentile(0.999) / 1000.0);
    printf("    get  p50=%.0fus p95=%.0fus p99=%.0fus p99.9=%.0fus\n",
           cur.hGet.percentile(0.5) / 1000.0, cur.hGet.percentile(0.95) / 1000.0,
           cur.hGet.percentile(0.99) / 1000.0, cur.hGet.percentile(0.999) / 1000.0);
    printf("    del  p50=%.0fus p95=%.0fus p99=%.0fus p99.9=%.0fus\n",
           cur.hDel.percentile(0.5) / 1000.0, cur.hDel.percentile(0.95) / 1000.0,
           cur.hDel.percentile(0.99) / 1000.0, cur.hDel.percentile(0.999) / 1000.0);
    fflush(stdout);

    prev = cur;
    lastTotal = total;
    lastT = Clock::now();
  }
}

// ---------------------------------------------------------------------------
// Argument parsing
// ---------------------------------------------------------------------------
static void printHelp(const char *prog) {
  printf("usage: %s [options]\n"
         "  --host HOST        corekv server address (default 127.0.0.1)\n"
         "  --port PORT        corekv server port (default 8000)\n"
         "  --threads N        number of worker connections (default 50)\n"
         "  --keys N           key-space size, pre-populated during warmup (default 100000)\n"
         "  --val-size N       value size in bytes (default 64)\n"
         "  --duration SEC     test duration in seconds (default 60)\n"
         "  --mix S:G:D        operation mix as percentages (default 50:40:10)\n"
         "  --qps N            target aggregate throughput; 0 = max (default 0)\n"
         "  --metrics-port N   port for the Prometheus /metrics endpoint (default 9091, 0 = off)\n"
         "  --report-interval N  seconds between console progress reports (default 5, 0 = off)\n"
         "  --help             show this help\n",
         prog);
}

static bool parseMix(const std::string &mix, double *set, double *get, double *del) {
  int s = 50, g = 40, d = 10;
  if (sscanf(mix.c_str(), "%d:%d:%d", &s, &g, &d) != 3) {
    fprintf(stderr, "invalid --mix value: %s (expected S:G:D)\n", mix.c_str());
    return false;
  }
  if (s < 0 || g < 0 || d < 0 || s + g + d == 0) {
    fprintf(stderr, "invalid --mix values\n");
    return false;
  }
  *set = static_cast<double>(s) / (s + g + d);
  *get = static_cast<double>(g) / (s + g + d);
  *del = static_cast<double>(d) / (s + g + d);
  return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char **argv) {
  Config cfg;

  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&](const char *name) -> const char * {
      if (i + 1 >= argc) {
        fprintf(stderr, "missing value for %s\n", name);
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--host")
      cfg.host = next("--host");
    else if (a == "--port")
      cfg.port = atoi(next("--port"));
    else if (a == "--threads")
      cfg.threads = atoi(next("--threads"));
    else if (a == "--keys")
      cfg.keys = atoi(next("--keys"));
    else if (a == "--val-size")
      cfg.valSize = atoi(next("--val-size"));
    else if (a == "--duration")
      cfg.durationSec = atoi(next("--duration"));
    else if (a == "--mix") {
      if (!parseMix(next("--mix"), &cfg.mixSet, &cfg.mixGet, &cfg.mixDel))
        return 2;
    } else if (a == "--qps")
      cfg.qps = static_cast<uint64_t>(atoll(next("--qps")));
    else if (a == "--metrics-port")
      cfg.metricsPort = atoi(next("--metrics-port"));
    else if (a == "--report-interval")
      cfg.reportInterval = atoi(next("--report-interval"));
    else if (a == "--help") {
      printHelp(argv[0]);
      return 0;
    } else {
      fprintf(stderr, "unknown option: %s\n", a.c_str());
      printHelp(argv[0]);
      return 2;
    }
  }

  if (cfg.threads < 1)
    cfg.threads = 1;
  if (cfg.keys < static_cast<int>(cfg.threads))
    cfg.keys = cfg.threads;

  printf("corekv-bench: host=%s:%d threads=%d keys=%d val=%dB dur=%ds mix=%.0f:%.0f:%.0f qps=%llu metrics=:%d\n",
         cfg.host.c_str(), cfg.port, cfg.threads, cfg.keys, cfg.valSize,
         cfg.durationSec, cfg.mixSet * 100, cfg.mixGet * 100, cfg.mixDel * 100,
         static_cast<unsigned long long>(cfg.qps), cfg.metricsPort);
  fflush(stdout);

  std::atomic<uint64_t> pacing{0};
  Clock::time_point start = Clock::now();

  std::vector<Worker> workers(static_cast<size_t>(cfg.threads));
  std::vector<std::thread> threads;
  for (int i = 0; i < cfg.threads; ++i) {
    workers[static_cast<size_t>(i)].cfg = cfg;
    workers[static_cast<size_t>(i)].id = i;
    workers[static_cast<size_t>(i)].rng.seed(0xC0FFEEull ^ static_cast<uint64_t>(i) ^ 1);
    workers[static_cast<size_t>(i)].pacing = &pacing;
    workers[static_cast<size_t>(i)].start = start;
    threads.emplace_back(&Worker::run, &workers[static_cast<size_t>(i)]);
  }

  std::thread metricThread;
  if (cfg.metricsPort > 0)
    metricThread = std::thread(metricsServer, cfg.metricsPort, std::cref(workers));
  std::thread reportThread;
  if (cfg.reportInterval > 0)
    reportThread = std::thread(reporter, cfg, std::cref(workers));

  for (auto &t : threads)
    t.join();

  gDone.store(true);
  if (metricThread.joinable())
    metricThread.join();
  if (reportThread.joinable())
    reportThread.join();

  Aggregate agg;
  aggregateWorkers(workers, agg);
  double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
  uint64_t total = agg.nSet + agg.nGet + agg.nDel;

  printf("\n===== RESULTS =====\n");
  printf("RESULT total_ops=%llu errors=%llu elapsed=%.3fs qps=%.1f\n",
         static_cast<unsigned long long>(total),
         static_cast<unsigned long long>(agg.nErr), elapsed,
         elapsed > 0 ? static_cast<double>(total) / elapsed : 0.0);
  const char *names[] = {"set", "get", "del"};
  const Histogram *hs[] = {&agg.hSet, &agg.hGet, &agg.hDel};
  const uint64_t *ns[] = {&agg.nSet, &agg.nGet, &agg.nDel};
  for (int i = 0; i < 3; ++i) {
    printf("RESULT_LAT %s ops=%llu p50=%.0f p95=%.0f p99=%.0f p999=%.0f mean=%.0f (us)\n",
           names[i], static_cast<unsigned long long>(*ns[i]),
           hs[i]->percentile(0.5) / 1000.0, hs[i]->percentile(0.95) / 1000.0,
           hs[i]->percentile(0.99) / 1000.0, hs[i]->percentile(0.999) / 1000.0,
           hs[i]->meanNanos() / 1000.0);
  }
  return 0;
}
