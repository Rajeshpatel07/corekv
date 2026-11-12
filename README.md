<p align="center">
  <img src="./assets/corekv.png" alt="CoreKV Logo" width="500">
</p>

<h1 align="center">CoreKV</h1>

<p align="center">A lightweight, high-performance in-memory key-value store written in C++20, speaking the Redis wire protocol</p>

---

## About

CoreKV is an in-memory key-value store that speaks the [RESP](https://redis.io/docs/latest/develop/reference/protocol-spec/)
(Redis Serialization Protocol) wire format, so it can be driven by `redis-cli` or
any standard Redis client library.

It is written from scratch in C++20 on top of `epoll`, with no external
dependencies beyond the C++ standard library. The server is single threaded
and fully event driven, so it never blocks on a client.

> **Status:** work in progress. The networking, RESP parsing, pipelining, and
> the sharded in-memory hash map are implemented. Persistence, expiration, and
> the remaining Redis data structures are not — see [Roadmap](#roadmap).

---

## Requirements

- **C++ Compiler**: GCC 10+ or Clang 13+ (C++20 support)
- **CMake**: 3.20+
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

The server binary is written to `build/bin/corekv`.

---

## Run

```bash
./build/bin/corekv
```

Listens on TCP port **8080**.

```bash
redis-cli -p 8080 ping
redis-cli -p 8080 set greeting "hello"
redis-cli -p 8080 get greeting
redis-cli -p 8080 del greeting
```

A raw session works too. Keep the connection open long enough to read the
reply back:

```bash
python3 -c "
import socket
s = socket.create_connection(('127.0.0.1', 8080))
s.sendall(b'*3\r\n\$3\r\nSET\r\n\$8\r\ngreeting\r\n\$5\r\nhello\r\n')
s.sendall(b'*2\r\n\$3\r\nGET\r\n\$8\r\ngreeting\r\n')
print(s.recv(1024).decode())
"
```

```text
+OK
$5
hello
```

The two commands above are sent as a single batch, which is pipelining: both
are executed and both replies are returned together.

---

## Supported commands

| Command | Reply | Notes |
|---|---|---|
| `PING` | `+pong` | Liveness check. |
| `SET key value` | `+OK` | Value may be a bulk string, integer, or array. |
| `GET key` | value or `$-1` | Returns the stored value with its original RESP type. |
| `DEL key` | `:1` / `:0` | `:0` when the key does not exist. |

Command names are case-insensitive.

### Protocol notes

CoreKV implements a subset of RESP2, so standard clients work, but a few
details differ from Redis:

- `PING` replies `+pong` (lowercase). Redis replies `+PONG`.
- `SET` accepts a typed value. A client that sends the value as a bulk string
  stores a string; a client that sends `:42` or an array stores an integer or
  an array, and `GET` returns it with that same type. Redis always stores
  bytes and always replies to `GET` with a bulk string.
- Unknown commands reply `-ERR unknown command '<name>'`. Newer `redis-cli`
  versions send `CLIENT SETINFO` when they connect. CoreKV does not implement
  it yet, so the server answers with an error for that one command.

---

## Features

**Networking**

- Non-blocking `epoll` event loop with level-triggered readiness.
- One read buffer and one write buffer per connection.
- RAII ownership of every file descriptor (`Socket`, `Epoll`, `TcpListener`).
  Copying is deleted; moving transfers ownership.
- Epoll status bits (`EPOLLERR`, `EPOLLHUP`) are tested with bitwise masks,
  never with `==`.

**RESP**

- RESP2 decoding for arrays, bulk strings, simple strings, and integers.
- **Request pipelining**: every complete command in a single read is executed,
  and all replies are batched into one write.
- **Partial frame handling**: a frame that is split across TCP segments is
  buffered and retried once the remainder arrives. This is not treated as a
  protocol error.
- Malformed frames produce `-ERR Protocol error: ...` and discard the input.
- `std::from_chars` for integer parsing, so no allocation and no exceptions
  escaping from the numeric layer.

**Storage**

- Intrusive hash map with **incremental rehashing**: migration is capped per
  operation, so a large resize never stalls the event loop.
- **Sharded** keyspace: keys are distributed across shards by hash code.
- Values are stored as a `std::variant` of string, 64-bit integer, and array,
  so a key can hold more than a flat string.
- Bucket counts stay powers of two; the table uses `hash & (bucket_count - 1)`.

---

## Project Structure

```text
src/
├── command/    command dispatch and per-command handlers
├── common/     shared value types and hashing helpers
├── db/         hash map, sharded database
├── net/        RAII socket and epoll wrappers
├── parser/     RESP decoding/encoding and connection I/O
├── server/     event loop and connection ownership
└── main.cpp    application entry point
```

---

## Design rules

- Every file descriptor is owned by an RAII type. Copying is deleted; moving
  transfers ownership.
- Each accepted connection is a heap-allocated `ClientObject`; epoll stores its
  address in `epoll_event::data.ptr`. The object owns its `Socket`, so deleting
  the client closes the fd and frees the buffers. There is no server-side
  client registry, which keeps separate event loops independent.
- Bucket counts in the hash table must stay powers of two.
- Incomplete input is never a protocol error. Only genuinely malformed frames
  are.

---

## Roadmap

- **Expiration**: `EXPIRE`, `TTL`, `PERSIST`, with lazy plus active expiry.
- **Keyspace commands**: `EXISTS`, `TYPE`, `DBSIZE`, `KEYS`, `SCAN`,
  `RENAME`, `MSET`/`MGET`.
- **More data types**: list, hash, set, sorted set.
- **Persistence**: RDB snapshots and an append-only file.
- **Replication**: `PSYNC` and the replication stream.
- **Transactions**: `MULTI`/`EXEC`/`DISCARD`/`WATCH`.
- **RESP3**: `HELLO` negotiation, map replies, push semantics.
- **Multi-threading**: per-shard locking.

---

## License

This project is provided as-is for learning purposes.
