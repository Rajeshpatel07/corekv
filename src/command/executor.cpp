#include "command/executor.h"

#include "common/common.h"
#include "db/db.h"
#include "db/hashtable.h"
#include "net/socket.h"
#include "parser/resp.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>

namespace corekv::executor {
namespace {

[[nodiscard]] bool equals_ignore_case(std::string_view left,
                                      std::string_view right) {
  return std::ranges::equal(left, right, [](char lhs, char rhs) {
    return std::tolower(static_cast<unsigned char>(lhs)) ==
           std::tolower(static_cast<unsigned char>(rhs));
  });
}

[[nodiscard]] std::uint64_t hash_key(const std::string &key) {
  return fnv1a_hash(reinterpret_cast<const std::uint8_t *>(key.data()),
                    key.size());
}

[[nodiscard]] db::Entry *find_entry(const std::string &key) {
  db::Entry probe;
  probe.key = key;
  probe.node.h_code = hash_key(key);

  db::HashNode *node = db::database.lookup(&probe.node);
  if (node == nullptr) {
    return nullptr;
  }

  return CONTAINER_OF(node, db::Entry, node);
}

void encode_wrong_arity(net::ClientObject &client, std::string_view command) {
  const std::string message = "ERR wrong number of arguments for '" +
                              std::string{command} + "' command";
  RESP::encode_simple_error(client, message);
}

void handle_ping(net::ClientObject &client) {
  RESP::encode_simple_string(client, "pong");
}

void handle_get(net::ClientObject &client, const RespArray &args) {
  if (args.size() != 2) {
    encode_wrong_arity(client, "get");
    return;
  }
  if (!args[1].is_string()) {
    RESP::encode_simple_error(client, "ERR key must be a string");
    return;
  }

  const db::Entry *entry = find_entry(args[1].get_string());
  if (entry == nullptr) {
    RESP::encode_null_bulk_string(client);
    return;
  }

  RESP::encode_handler(client, entry->val);
}

void handle_set(net::ClientObject &client, RespArray &args) {
  if (args.size() != 3) {
    encode_wrong_arity(client, "set");
    return;
  }
  if (!args[1].is_string()) {
    RESP::encode_simple_error(client, "ERR key must be a string");
    return;
  }

  const std::string &key = args[1].get_string();

  if (db::Entry *entry = find_entry(key)) {
    entry->val = std::move(args[2]);
  } else {
    auto new_entry = std::make_unique<db::Entry>();
    new_entry->key = key;
    new_entry->val = std::move(args[2]);
    new_entry->node.h_code = hash_key(key);
    db::database.insert(&new_entry->node);
    std::ignore = new_entry.release();
  }

  RESP::encode_simple_string(client, "OK");
}

void handle_del(net::ClientObject &client, const RespArray &args) {
  if (args.size() != 2) {
    encode_wrong_arity(client, "del");
    return;
  }
  if (!args[1].is_string()) {
    RESP::encode_simple_error(client, "ERR key must be a string");
    return;
  }

  db::Entry *entry = find_entry(args[1].get_string());
  if (entry == nullptr) {
    RESP::encode_integer(client, 0);
    return;
  }

  if (db::database.remove(&entry->node) == nullptr) {
    RESP::encode_simple_error(client, "ERR failed to delete key");
    return;
  }

  delete entry;
  RESP::encode_integer(client, 1);
}

} // namespace

void execute_command(net::ClientObject &client, RespValue &command) {
  if (!command.is_array()) {
    RESP::encode_simple_error(client, "ERR command must be an array");
    return;
  }

  RespArray &args = command.get_array();
  if (args.empty()) {
    RESP::encode_simple_error(client, "ERR empty command");
    return;
  }
  if (!args[0].is_string()) {
    RESP::encode_simple_error(client, "ERR command name must be a string");
    return;
  }

  const std::string &name = args[0].get_string();
  if (equals_ignore_case(name, "ping")) {
    handle_ping(client);
  } else if (equals_ignore_case(name, "get")) {
    handle_get(client, args);
  } else if (equals_ignore_case(name, "set")) {
    handle_set(client, args);
  } else if (equals_ignore_case(name, "del")) {
    handle_del(client, args);
  } else {
    RESP::encode_simple_error(client, "ERR unknown command '" + name + "'");
  }
}

} // namespace corekv::executor
