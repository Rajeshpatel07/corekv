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

> **Status:** work in progress. Networking, RESP2 request parsing, pipelining,
> the string command set (`PING`, `SET`, `MSET`, `GET`, `DEL`, `RENAME`,
> `INCR`, `DECR`, `MGET`, `EXISTS`), and the sharded in-memory hash map are
> implemented. Persistence, expiration, and the remaining Redis data types are
> not — see [Roadmap](#roadmap).

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
redis-cli -p 8080 mset first one second two
redis-cli -p 8080 rename first renamed
redis-cli -p 8080 mget renamed second missing
redis-cli -p 8080 set counter 41
redis-cli -p 8080 incr counter
redis-cli -p 8080 exists greeting counter missing
redis-cli -p 8080 mget greeting counter missing
redis-cli -p 8080 del greeting counter missing renamed second
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
| `PING` | `+PONG` | Liveness check. |
| `SET key value` | `+OK` | Stores `value` as a string, replacing any existing value. |
| `MSET key value [key value ...]` | `+OK` | Sets all key/value pairs; later duplicates overwrite earlier values. |
| `GET key` | bulk string or `$-1` | Returns the stored value, or null when the key is absent. |
| `DEL key [key ...]` | integer | Deletes every named key and returns how many were removed. |
| `RENAME key newkey` | `+OK` | Renames a key; replaces `newkey` if it exists, or returns `ERR no such key` when the source is missing. |
| `INCR key` | integer | Treats the value as a signed 64-bit integer and increments it; missing keys start at `1`. |
| `DECR key` | integer | Treats the value as a signed 64-bit integer and decrements it; missing keys start at `-1`. |
| `MGET key [key ...]` | array | Returns the value or null for each key, preserving argument order. |
| `EXISTS key [key ...]` | integer | Counts the keys that exist; repeated keys are counted each time. |

Command names are case-insensitive.

### Protocol notes

CoreKV implements RESP2 requests as arrays of bulk strings. All keys and values
are opaque byte strings.

- Requests must be RESP arrays of non-null bulk strings (`$`). Simple strings
  (`+`), integers (`:`), nested arrays (`*`), and null bulk strings (`$-1`)
  inside a command are protocol errors.
- `GET` returns a bulk string or a null bulk string (`$-1`). `MGET` returns an
  array of bulk strings and null bulk strings.
- `PING` replies `+PONG`.
- `INCR` and `DECR` operate on canonical signed 64-bit decimal integers and
  return a Redis-style error when the stored value is not an integer or the
  operation would overflow.
- Unknown commands reply `-ERR unknown command '<name>'`. Newer `redis-cli`
  versions send `CLIENT SETINFO` when they connect. CoreKV does not implement
  it yet, so the server answers with an error for that one command.
- Malformed requests reply `-ERR Protocol error: ...` and clear the buffered
  input. The connection stays open.

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

- RESP2 request decoding for arrays of bulk strings, producing a flat
  `RESP::DataBuffer` (`std::vector<std::string>`) for the command layer.
- Reply encoding for simple strings, errors, integers, bulk strings, null bulk
  strings, and arrays.
- **Request pipelining**: every complete command in a single read is executed,
  and all replies are batched into one write.
- **Partial frame handling**: a frame that is split across TCP segments is
  buffered and retried once the remainder arrives. This is not treated as a
  protocol error.
- Malformed frames produce `-ERR Protocol error: ...`, discard the buffered
  input, and keep the connection open.
- `std::from_chars` for numeric parsing in the protocol and in `INCR`/`DECR`.

**Storage**

- Intrusive hash map with **incremental rehashing**: migration is capped per
  insert, so a large resize never stalls the event loop.
- **Sharded** keyspace: a 64-bit FNV-1a hash distributes keys across shards.
- Keys and values are stored as `std::string`; every value is an opaque byte
  string, with no variant or type tag in the storage layer.
- `DEL` detaches a key from the hash table before freeing its entry.
- Bucket counts stay powers of two; the table uses `hash & (bucket_count - 1)`.

---

## Project Structure

```text
src/
├── command/    command dispatch and per-command handlers
├── db/         intrusive hash map, sharded database, FNV-1a hashing
├── net/        RAII socket, listener, and epoll wrappers
├── parser/     RESP decoding/encoding and connection I/O
├── server/     epoll event loop and client lifecycle
└── main.cpp    application entry point
```

---

## Design rules

- Command requests are RESP arrays of bulk strings, decoded into
  `RESP::DataBuffer = std::vector<std::string>`.
- Every file descriptor is owned by an RAII type. Copying is deleted; moving
  transfers ownership.
- Each accepted connection is a heap-allocated `ClientObject`; epoll stores its
  address in `epoll_event::data.ptr`. The object owns its `Socket`, so deleting
  the client closes the fd and frees the buffers. There is no server-side
  client registry, which keeps separate event loops independent.
- Bucket counts in the hash table must stay powers of two.
- Incomplete input is never a protocol error. Only genuinely malformed frames
  are; those clear the buffered input and reply with a protocol error without
  closing the connection.
- The server runs on one thread. Per-shard locking is deferred until
  multi-threaded workers are added.

---

## Roadmap

- **Expiration**: `EXPIRE`, `TTL`, `PERSIST`, with lazy plus active expiry.
- **Keyspace commands**: `TYPE`, `DBSIZE`, `KEYS`, `SCAN`, `UNLINK`.
- **More data types**: list, hash, set, sorted set.
- **Persistence**: RDB snapshots and an append-only file.
- **Replication**: `PSYNC` and the replication stream.
- **Transactions**: `MULTI`/`EXEC`/`DISCARD`/`WATCH`.
- **RESP3**: `HELLO` negotiation, map replies, push semantics.
- **Multi-threading**: per-shard locking.

---

## License

This project is provided as-is for learning purposes.
