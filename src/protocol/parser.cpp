#include "parser.hpp"
#include "../handler/executor.hpp"
#include "serializer.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace corekv {

constexpr int kMaxMessageSize = 32 << 22; // 128 MB

void handleRead(Connection *conn) {
  char buffer[64 * 1024];

  ssize_t rv = read(conn->fd, buffer, sizeof(buffer));

  if (rv <= 0) {
    if (rv < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return;
    }

    conn->wantClose = true;
    if (rv < 0) {
      perror("read");
    }
    return;
  }

  appendToBuffer(conn->incoming, reinterpret_cast<const uint8_t *>(buffer),
                 static_cast<int>(rv));

  while (parseMessage(conn)) {
  }

  if (!conn->outgoing.empty()) {
    conn->wantRead = false;
    conn->wantWrite = true;
  }
}

void handleWrite(Connection *conn) {
  ssize_t wt = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());

  if (wt <= 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }
    conn->wantClose = true;
    return;
  }

  removeFromBuffer(conn->outgoing, static_cast<int>(wt));

  if (conn->outgoing.empty()) {
    conn->wantWrite = false;
    conn->wantRead = true;
  }
}

bool parseMessage(Connection *conn) {
  if (conn->incoming.size() < 4) {
    return false;
  }

  uint32_t header;
  readUint32(conn->incoming, header);

  if (header > static_cast<uint32_t>(kMaxMessageSize)) {
    conn->wantClose = true;
    return false;
  }

  if (conn->incoming.size() < 4 + header) {
    return false;
  }

  std::vector<std::string> cmds;

  removeFromBuffer(conn->incoming, 4);

  conn->outgoing.resize(4); // reserving 4 bytes for the length;

  parseCommands(conn->incoming, cmds, static_cast<int>(header));
  executeCommand(cmds, conn);

  uint32_t len = conn->outgoing.size();
  std::memcpy(&conn->outgoing[0], &len, 4);

  return true;
}

} // namespace corekv
