#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace corekv::net {

inline constexpr std::uint16_t kDefaultTcpPort = 8080;

class Socket {
public:
  Socket() noexcept = default;
  explicit Socket(int fd) noexcept;

  Socket(const Socket &) = delete;
  Socket &operator=(const Socket &) = delete;

  Socket(Socket &&other) noexcept;
  Socket &operator=(Socket &&other) noexcept;

  ~Socket();

  void close() noexcept;
  [[nodiscard]] int fd() const noexcept;
  void set_nonblocking() const;

private:
  int fd_{-1};
};

struct ClientObject {
  explicit ClientObject(Socket owned_socket) noexcept
      : socket(std::move(owned_socket)) {}

  ClientObject(const ClientObject &) = delete;
  ClientObject &operator=(const ClientObject &) = delete;
  ClientObject(ClientObject &&) = delete;
  ClientObject &operator=(ClientObject &&) = delete;

  std::vector<std::uint8_t> read_buffer;
  std::vector<std::uint8_t> write_buffer;
  std::size_t read_offset{0};
  std::size_t write_offset{0};
  Socket socket;
};

class TcpListener {
public:
  TcpListener();
  ~TcpListener() = default;

  TcpListener(const TcpListener &) = delete;
  TcpListener &operator=(const TcpListener &) = delete;

  TcpListener(TcpListener &&other) noexcept = default;
  TcpListener &operator=(TcpListener &&other) noexcept = default;

  [[nodiscard]] int fd() const noexcept;

  [[nodiscard]] std::unique_ptr<ClientObject> try_accept();

private:
  void open();

  Socket socket_;
};

} // namespace corekv::net
