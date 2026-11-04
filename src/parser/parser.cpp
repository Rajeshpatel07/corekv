#include "parser/parser.h"

#include "command/executor.h"
#include "net/socket.h"
#include "parser/resp.h"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <system_error>

namespace corekv::parser {
namespace {

constexpr std::size_t kReadChunkSize = 1024;

} // namespace

void append_to_buffer(const uint8_t *src, std::vector<uint8_t> &dist,
                      const uint64_t size) {
  if (size == 0) {
    return;
  }
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

    append_to_buffer(reinterpret_cast<const uint8_t *>(buffer.data()),
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
  const std::uint8_t type = client.read_buffer[client.read_offset];

  try {
    switch (type) {
    case '*': {
      const RESP::RespValue command = RESP::decode_array(client);
      executor::execution_handler(client, command);
      break;
    }
    default: {
      std::clog << "Unknown RESP type: " << static_cast<unsigned int>(type)
                << '\n';
      client.read_buffer.clear();
      client.read_offset = 0;
      RESP::encode_simple_error(client, "ERR unknown RESP type");
      break;
    }
    }
  } catch (const std::exception &error) {
    std::clog << "RESP decode error: " << error.what() << '\n';
    client.read_buffer.clear();
    client.read_offset = 0;
    RESP::encode_simple_error(client, std::string{"ERR Protocol error: "} +
                                          error.what());
  }
}

IoResult write_to_client(net::ClientObject &client) {
  while (client.write_offset < client.write_buffer.size()) {
    const std::size_t remaining =
        client.write_buffer.size() - client.write_offset;
    const ssize_t sent = ::send(
        client.socket.fd(), client.write_buffer.data() + client.write_offset,
        remaining, MSG_NOSIGNAL);

    if (sent > 0) {
      client.write_offset += static_cast<std::size_t>(sent);
      continue;
    }
    if (sent == 0) {
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

  client.write_buffer.clear();
  client.write_offset = 0;
  return IoResult::ok;
}

} // namespace corekv::parser
