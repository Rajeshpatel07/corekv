#pragma once

#include <cstdint>
#include <fcntl.h>
#include <vector>

namespace corekv {

class Connection {
public:
  int fd = -1;
  bool wantRead = false;
  bool wantWrite = false;
  bool wantClose = false;
  std::vector<uint8_t> incoming;
  std::vector<uint8_t> outgoing;
};

void setSocketNonBlocking(int fd);

Connection *acceptConnection(int fd);

int createServerSocket(int port);

} // namespace corekv
