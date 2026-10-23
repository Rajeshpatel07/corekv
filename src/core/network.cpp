#include "core/network.h"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

namespace Corekv {

Server::Server(int port) : server_port_(port) {
  create_socket();
  set_socket_options();
  bind_and_listen();
  start_server();
}

Server::~Server() {
  if (server_fd_ >= 0) {
    close(server_fd_);
  }
}

Server::Server(Server &&other) noexcept
    : server_fd_(other.server_fd_), server_port_(other.server_port_) {
  other.server_fd_ = -1;
}

Server &Server::operator=(Server &&other) noexcept {
  if (this != &other) {
    if (server_fd_ >= 0) {
      close(server_fd_);
    }
    server_fd_ = other.server_fd_;
    server_port_ = other.server_port_;
    other.server_fd_ = -1;
  }
  return *this;
}

void Server::create_socket() {
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to create socket");
  }
}

void Server::set_socket_options() {
  int opt_reuseaddr = 1;
  if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt_reuseaddr,
                 sizeof(opt_reuseaddr)) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to set SO_REUSEADDR");
  }

  int opt_keepalive = 1;
  if (setsockopt(server_fd_, SOL_SOCKET, SO_KEEPALIVE, &opt_keepalive,
                 sizeof(opt_keepalive)) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to set SO_KEEPALIVE");
  }
}

void Server::bind_and_listen() {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(server_port_);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to bind socket");
  }

  if (listen(server_fd_, SOMAXCONN) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to listen on socket");
  }
}

[[nodiscard]] int Server::accept_client() {
  sockaddr_in client_addr{};
  socklen_t addr_len = sizeof(client_addr);
  int r_val =
      accept(server_fd_, reinterpret_cast<sockaddr *>(&client_addr), &addr_len);

  if (r_val < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to accept client connection");
  }

  char ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
  std::cout << "New client: " << ip << ":" << ntohs(client_addr.sin_port)
            << std::endl;

  return r_val;
}

void Server::start_server() {
  while (true) {
    int client_fd{accept_client()};

    char buffer[1024];
    int read_val = read(client_fd, buffer, sizeof(buffer));
    if (read_val <= 0) {
      std::cerr << "Error reading data from client\n";
    }
    std::cout << "client: " << buffer << "\n";

    std::string response = "Hello from Server!";
    int write_val = write(client_fd, response.data(), response.size());
    if (write_val < 0) {
      std::cerr << "Error writing data to client\n";
    }

    close(client_fd);
  }
}

} // namespace Corekv
