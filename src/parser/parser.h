#pragma once

#include <cstdint>

namespace corekv::net {
struct ClientObject;
}

namespace corekv::parser {

enum class IoResult : std::uint8_t {
  ok,
  would_block,
  closed,
};

[[nodiscard]] IoResult read_from_client(net::ClientObject &client);
[[nodiscard]] IoResult write_to_client(net::ClientObject &client);

} // namespace corekv::parser
