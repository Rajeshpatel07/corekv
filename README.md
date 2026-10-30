# corekv

A small Linux TCP server written in C++20. The project is a work in progress:
the networking/event-loop layer is functional, while the in-memory hash table
in `src/db` is not wired into the request path yet.

## Build and run

```sh
cmake -S . -B build
cmake --build build
./build/corekv
```

The server listens on TCP port 8000. It currently logs incoming data and
replies with a greeting.

## Layout

```text
src/
  db/       intrusive hash table (not used by the server yet)
  net/      RAII wrappers for sockets and epoll
  parser/   connection I/O callbacks; protocol parsing will live here
  server/   event loop and connection ownership
  main.cpp  application entry point
```

## Design rules

- Every file descriptor is owned by an RAII type (`Socket`, `Epoll`,
  `TcpListener`). Copying is deleted; moving transfers ownership.
- Each accepted connection is a heap-allocated `ClientObject`; epoll stores its
  address in `epoll_event::data.ptr`. The object owns its `Socket`, so
  `delete client` closes the fd and frees the buffers. There is no server-side
  client registry, which keeps separate event loops independent.
- Epoll status bits (`EPOLLERR`, `EPOLLHUP`) must be tested with bitwise masks,
  never with `==`.
- Bucket counts in the hash table must stay powers of two; the table uses
  `hash & (bucket_count - 1)` instead of modulo.
