#pragma once

namespace Corekv {

/*
 * Represents a TCP network server for the CoreKV
 * -> void
 *
 * parameters:
 * - int port
 */

class Server {
private:
  int server_fd_{-1};
  int server_port_{8080};

  void create_socket();
  void set_socket_options();
  void bind_and_listen();
  [[nodiscard]] int accept_client();
  void start_server();

public:
  Server() = default;
  explicit Server(int port);
  ~Server();

  // Rule of Zero/Five: explicitly manage resources
  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;

  Server(Server &&other) noexcept;
  Server &operator=(Server &&other) noexcept;
};

} // namespace Corekv
