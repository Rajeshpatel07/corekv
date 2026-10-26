#pragma once

#include <cstdint>
#include <vector>
namespace Corekv {

constexpr int MAX_EVENTS = 64;

struct Conn {
  std::vector<uint8_t> read_buffer;
  std::vector<uint8_t> write_buffer;
};

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
  int epoll_fd_{-1};
  int server_port_{8080};

  void create_socket();
  void set_socket_options();
  void set_socket_nonblock(int fd);
  void bind_and_listen();
  void accept_client();

  void run_epoll();

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
