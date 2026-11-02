#include "parser/parser.h"
#include "resp.h"

#include "net/socket.h"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
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

inline void write_to_buffer(const uint8_t *src, std::vector<uint8_t> &dist,
                            const uint64_t size) {
  if (size == 0)
    return;
  dist.insert(dist.end(), src, src + size);
}

void read_from_buffer(std::vector<uint8_t> &src, uint8_t *dist, uint64_t size,
                      size_t &read_offset) {
  if (read_offset + size > src.size()) {
    throw std::out_of_range("Attempted to read past buffer bounds");
  }

  std::memcpy(dist, src.data() + read_offset, size);
  read_offset += size;

  if (read_offset >= src.size()) {
    src.clear();
    read_offset = 0;
  }
}

IoResult read_from_client(net::ClientObject &client) {
  std::array<char, kReadChunkSize> buffer{};
  bool received_any_data = false;

  while (true) {
    const ssize_t received =
        ::recv(client.socket.fd(), buffer.data(), buffer.size(), 0);

    if (received == 0) {
      return IoResult::closed;
    }

    if (received < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }

      std::clog << "Client read error.\n";
      return IoResult::closed;
    }

    write_to_buffer(reinterpret_cast<const uint8_t *>(buffer.data()),
                    client.read_buffer, static_cast<uint64_t>(received));
    received_any_data = true;
  }

  if (received_any_data && client.read_offset < client.read_buffer.size()) {
    parser_handler(client);
  }

  return IoResult::ok;
}

void parser_handler(net::ClientObject &client) {
  if (client.read_offset >= client.read_buffer.size()) {
    return;
  }
  uint8_t type = client.read_buffer[client.read_offset];

  switch (type) {
  case '+':
    std::cout << "simple string: " << RESP::decode_simple_string(client)
              << "\n";
    break;
  case '$':
    std::cout << "bulk string: " << RESP::decode_bulk_string(client) << "\n";
    break;
  case ':':
    std::cout << "integer: " << RESP::decode_integer(client) << "\n";
    break;
  case '(':
    std::cout << "Big number: " << RESP::decode_big_numbers(client) << "\n";
    break;
  case '*':
    RESP::decode_array(client);
    break;
  default:
    break;
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
