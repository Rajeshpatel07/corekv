#pragma once

#include "net/epoll.h"
#include "net/socket.h"
namespace corekv::server {

class Server {
public:
  Server();
  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;
  Server(Server &&other) noexcept;
  Server &operator=(Server &&other) noexcept;
  ~Server() = default;

  void start_eventloop();
  void handle_client_events(epoll_event &ev);
  void handle_read();
  void handle_write();

private:
  net::Epoll epoll_;
  net::TcpListener listener_;
};

} // namespace corekv::server
