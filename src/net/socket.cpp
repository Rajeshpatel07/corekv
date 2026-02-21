#include "socket.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <netinet/in.h>
#include <unistd.h>

namespace corekv {

void setSocketNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    perror("fcntl F_GETFL");
    return;
  }
  flags |= O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) < 0) {
    perror("fcntl F_SETFL");
  }
}

Connection *acceptConnection(int fd) {
  sockaddr_in client{};
  socklen_t len = sizeof(client);
  int clientFd = accept(fd, reinterpret_cast<sockaddr *>(&client), &len);

  if (clientFd < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      perror("accept");
    }
    return nullptr;
  }

  char ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client.sin_addr, ip, sizeof(ip));
  std::cout << "New client: " << ip << ":" << ntohs(client.sin_port)
            << std::endl;

  setSocketNonBlocking(clientFd);

  auto *conn = new Connection();
  conn->fd = clientFd;
  conn->wantRead = true;
  return conn;
}

int createServerSocket(int port) {
  int serverFd = socket(AF_INET, SOCK_STREAM, 0);
  if (serverFd < 0) {
    perror("socket");
    return -1;
  }

  int opt = 1;
  if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("setsockopt SO_REUSEADDR");
    close(serverFd);
    return -1;
  }

  int keepalive = 1;
  setsockopt(serverFd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));

  sockaddr_in serverAddr{};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(static_cast<uint16_t>(port));
  serverAddr.sin_addr.s_addr = INADDR_ANY;

  if (bind(serverFd, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr)) < 0) {
    perror("bind");
    close(serverFd);
    return -1;
  }

  if (listen(serverFd, SOMAXCONN) < 0) {
    perror("listen");
    close(serverFd);
    return -1;
  }

  setSocketNonBlocking(serverFd);
  std::cout << "Server listening on port " << port << "..." << std::endl;
  return serverFd;
}

} // namespace corekv
