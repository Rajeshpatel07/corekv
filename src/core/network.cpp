#include "core/network.h"
#include "parser/parser.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace Corekv {

Server::Server(int port) : server_port_(port) {
  create_socket();
  set_socket_options();
  bind_and_listen();
  run_epoll();
}

Server::~Server() {
  if (server_fd_ >= 0) {
    close(server_fd_);
    close(epoll_fd_);
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

  set_socket_nonblock(server_fd_);
}

void Server::set_socket_nonblock(int fd) {
  int flags{fcntl(fd, F_GETFL, 0)};
  if (flags < 0) {
    throw std::system_error(errno, std::system_category(),
                            "Failed to get socket flags");
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    throw std::system_error(errno, std::system_category(),
                            "Failed to set socket to non-blocking");
  }
}

void Server::accept_client() {
  sockaddr_in client_addr{};
  socklen_t addr_len = sizeof(client_addr);
  int client_fd =
      accept(server_fd_, reinterpret_cast<sockaddr *>(&client_addr), &addr_len);

  if (client_fd < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to accept client connection");
  }

  set_socket_nonblock(client_fd);

  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = client_fd; // Store fd safely (do NOT mix with ev.data.ptr unless
                          // fd is inside Conn)

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to add fd to epoll watchlist");
  }
}

void Server::run_epoll() {
  if ((epoll_fd_ = epoll_create1(0)) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to create epoll instance");
  }

  epoll_event ev{};
  ev.events = EPOLLIN;
  ev.data.fd = server_fd_;

  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &ev) < 0) {
    throw std::system_error(errno, std::generic_category(),
                            "Failed to add fd to epoll watchlist");
  }

  std::vector<epoll_event> fd_events(MAX_EVENTS);

  std::cout << "Epoll Server started at port " << server_port_ << "\n";
  while (true) {
    int ndfs = epoll_wait(epoll_fd_, fd_events.data(), MAX_EVENTS, -1);
    if (ndfs < 0) {
      if (errno == EINTR)
        continue; // Retry if interrupted by signal
      throw std::system_error(errno, std::generic_category(),
                              "Failed in epoll_wait");
    }

    for (int i = 0; i < ndfs; i++) {
      int curr_fd = fd_events[i].data.fd;
      uint32_t curr_event = fd_events[i].events;

      // Handle errors or hangups first
      if (curr_event & (EPOLLERR | EPOLLHUP)) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, curr_fd, nullptr);
        close(curr_fd);
        continue;
      }

      if (curr_fd == server_fd_) {
        accept_client();
      } else {
        if (curr_event & EPOLLIN) {
          if (!read_from_client(epoll_fd_, fd_events[i])) {
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, curr_fd, nullptr);
            close(curr_fd);
          }
        } else if (curr_event & EPOLLOUT) {
          if (!write_to_client(epoll_fd_, fd_events[i])) {
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, curr_fd, nullptr);
            close(curr_fd);
          }
        }
      }
    }
  }
}

} // namespace Corekv
