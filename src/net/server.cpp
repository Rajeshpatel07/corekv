#include "server.hpp"
#include "../core/db.hpp"
#include "../protocol/parser.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

namespace corekv {

Server::Server() : serverFd(-1), running(false) {
}

Server::~Server() {
  stop();
}

void Server::stop() {
  if (!running) {
    return;
  }
  running = false;

  // Close all client connections
  for (Connection *conn : connections) {
    if (conn) {
      close(conn->fd);
      delete conn;
    }
  }
  connections.clear();

  // Close server socket
  if (serverFd >= 0) {
    close(serverFd);
    serverFd = -1;
  }

  // Free all hash table memory (fixes memory leak)
  hmapDestroy(&db.store);
}

void Server::start(int port) {
  serverFd = createServerSocket(port);
  if (serverFd < 0) {
    std::exit(1);
  }
  running = true;
  eventLoop();
}

void Server::eventLoop() {
  while (running) {
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
      running = false;
      break;
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
