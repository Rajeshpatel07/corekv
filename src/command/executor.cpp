#include "command/executor.h"

#include "net/socket.h"
#include "parser/resp.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace corekv::executor {
namespace {

[[nodiscard]] bool equals_ignore_case(std::string_view lhs,
                                      std::string_view rhs) {
  return std::ranges::equal(lhs, rhs, [](char left, char right) {
    return std::tolower(static_cast<unsigned char>(left)) ==
           std::tolower(static_cast<unsigned char>(right));
  });
}

void handle_ping(net::ClientObject &client) {
  RESP::encode_simple_string(client, std::string{"pong"});
}

} // namespace

void execute_command(net::ClientObject &client,
                     const RESP::RespValue &command) {
  if (!command.is_array()) {
    RESP::encode_simple_error(client, "ERR command must be an array");
    return;
  }

  const RESP::RespArray &arguments = command.get_array();
  if (arguments.empty() || !arguments.front().is_string()) {
    RESP::encode_simple_error(client, "ERR invalid command");
    return;
  }

  const std::string &name = arguments.front().get_string();
  if (equals_ignore_case(name, "ping")) {
    handle_ping(client);
    return;
  }

  RESP::encode_simple_error(client, "ERR unknown command '" + name + "'");
}

} // namespace corekv::executor
