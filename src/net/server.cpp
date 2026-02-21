#include "server.hpp"
#include "../protocol/parser.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

namespace corekv {

void Server::start(int port) {
  serverFd = createServerSocket(port);
  if (serverFd < 0) {
    std::exit(1);
  }
  eventLoop();
}

void Server::eventLoop() {
  while (true) {
    pollArgs.clear();

    pollfd serverPfd = {serverFd, POLLIN, 0};
    pollArgs.push_back(serverPfd);

    for (Connection *conn : connections) {
      if (!conn)
        continue;

      pollfd p = {conn->fd, POLLERR, 0};
      if (conn->wantRead) {
        p.events |= POLLIN;
      }
      if (conn->wantWrite) {
        p.events |= POLLOUT;
      }
      pollArgs.push_back(p);
    }

    int pollRes = poll(pollArgs.data(), static_cast<nfds_t>(pollArgs.size()), -1);
    if (pollRes < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("poll");
      std::exit(1);
    }

    if (pollArgs[0].revents & POLLIN) {
      handleNewConnection();
    }

    for (size_t i = 1; i < pollArgs.size(); ++i) {
      processConnection(i);
    }
  }
}

void Server::handleNewConnection() {
  if (Connection *conn = acceptConnection(serverFd)) {
    if (connections.size() <= static_cast<size_t>(conn->fd)) {
      connections.resize(conn->fd + 1, nullptr);
    }
    connections[conn->fd] = conn;
  }
}

void Server::processConnection(size_t idx) {
  short ready = pollArgs[idx].revents;
  Connection *conn = connections[pollArgs[idx].fd];

  if (!conn) {
    return;
  }

  if (ready & POLLIN) {
    handleRead(conn);
  }
  if (ready & POLLOUT) {
    handleWrite(conn);
  }

  if ((ready & POLLERR) || conn->wantClose) {
    cleanupConnection(conn);
  }
}

void Server::cleanupConnection(Connection *conn) {
  close(conn->fd);
  connections[conn->fd] = nullptr;
  delete conn;
}

} // namespace corekv
