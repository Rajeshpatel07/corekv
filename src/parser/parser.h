#pragma once

#include "net/socket.h"
#include <cstdint>
#include <string>
#include <vector>

namespace corekv::net {
struct ClientObject;
}

namespace corekv::parser {

enum class IoResult : std::uint8_t {
  ok,
  would_block,
  closed,
};

void write_to_buffer(uint8_t *src, std::vector<uint8_t> &dist,
                     const uint64_t size);
void read_from_buffer(std::vector<uint8_t> &src, uint8_t *dist, uint64_t size);

void parser_handler(net::ClientObject &client);

std::string decode_simple_string(net::ClientObject &client);

// NOTE: implement all the function below
// std::string decode_bulk_string(net::ClientObject &client);
// int decode_integer(net::ClientObject &client);
// std::string decode_simple_error(net::ClientObject &client);
// std::vector<std::string> decode_array(net::ClientObject &client);

[[nodiscard]] IoResult read_from_client(net::ClientObject &client);
[[nodiscard]] IoResult write_to_client(net::ClientObject &client);

} // namespace corekv::parser
