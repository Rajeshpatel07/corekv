#pragma once

#include "socket.hpp"
#include <poll.h>
#include <vector>

namespace corekv {

class Server {
  int serverFd = -1;
  std::vector<Connection *> connections;
  std::vector<pollfd> pollArgs;

public:
  void start(int port);

private:
  void eventLoop();
  void handleNewConnection();
  void processConnection(size_t idx);
  void cleanupConnection(Connection *conn);
};

} // namespace corekv
