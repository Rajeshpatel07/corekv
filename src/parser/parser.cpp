#include "parser/parser.h"

#include "net/socket.h"

#include <array>
#include <cerrno>
#include <cstddef>
#include <iostream>
#include <string_view>
#include <sys/socket.h>
#include <system_error>
#include <utility>

namespace corekv::parser {
namespace {

constexpr std::size_t kReadChunkSize = 1024;
constexpr std::string_view kGreeting = "Hello from server!";

} // namespace

IoResult read_from_client(net::ClientObject &client) {
  std::array<char, kReadChunkSize> buffer{};

  while (true) {
    const ssize_t received =
        ::recv(client.socket.fd(), buffer.data(), buffer.size(), 0);

    if (received > 0) {
      std::cout << "Message from client: "
                << std::string_view{buffer.data(),
                                    static_cast<std::size_t>(received)}
                << '\n';
      return IoResult::ok;
    }

    if (received == 0) {
      std::clog << "Client disconnected.\n";
      return IoResult::closed;
    }

    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return IoResult::would_block;
    }
    if (errno == ECONNRESET) {
      return IoResult::closed;
    }

    throw std::system_error(errno, std::generic_category(),
                            "Failed to read from socket");
  }
}

IoResult write_to_client(net::ClientObject &client) {
  while (true) {
    const ssize_t sent = ::send(client.socket.fd(), kGreeting.data(),
                                kGreeting.size(), MSG_NOSIGNAL);

    if (std::cmp_equal(sent, kGreeting.size())) {
      return IoResult::ok;
    }

    if (sent == 0) {
      return IoResult::closed;
    }
    if (sent > 0) {
      return IoResult::closed;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return IoResult::would_block;
    }
    if (errno == EPIPE || errno == ECONNRESET) {
      return IoResult::closed;
    }

    throw std::system_error(errno, std::generic_category(),
                            "Failed to write to socket");
  }
}

} // namespace corekv::parser
