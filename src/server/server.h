#pragma once

#include "net/epoll.h"
#include "net/socket.h"

#include <memory>

namespace corekv::server {

class Server {
public:
  Server() = default;
  ~Server() = default;

  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;

  Server(Server &&) = delete;
  Server &operator=(Server &&) = delete;

  void run();

private:
  void accept_clients();
  void register_client(std::unique_ptr<net::ClientObject> client);
  void disconnect_client(net::ClientObject &client);
  void handle_client_event(epoll_event event);

  net::Epoll epoll_;
  net::TcpListener listener_;
};

} // namespace corekv::server
