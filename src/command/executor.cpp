#include "command/executor.h"

#include "db/db.h"
#include "db/hashtable.h"
#include "net/socket.h"
#include "parser/resp.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
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

[[nodiscard]] std::uint64_t hash_key(std::string_view key) {
  return db::fnv1a_hash(reinterpret_cast<const std::uint8_t *>(key.data()),
                        key.size());
}

[[nodiscard]] bool has_canonical_integer_syntax(std::string_view value) {
  if (value.empty()) {
    return false;
  }

  const bool negative = value.front() == '-';
  if (negative) {
    value.remove_prefix(1);
    if (value.empty()) {
      return false;
    }
  }

  if (value.front() == '0') {
    return !negative && value.size() == 1;
  }
  return true;
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

void insert_entry(std::string_view key, std::string_view val) {
  auto new_entry = std::make_unique<db::Entry>();
  new_entry->key = key;
  new_entry->val = val;
  new_entry->node.h_code = hash_key(key);
  db::database.insert(&new_entry->node);
  std::ignore = new_entry.release();
}

[[nodiscard]] int64_t increment_int64(std::string_view num, int64_t addend) {
  if (!has_canonical_integer_syntax(num)) {
    throw std::invalid_argument("not a valid integer");
  }

  int64_t parsed = 0;
  const auto [ptr, ec] =
      std::from_chars(num.data(), num.data() + num.size(), parsed);

  if (ec == std::errc::result_out_of_range) {
    throw std::out_of_range("value out of range");
  }
  if (ec != std::errc() || ptr != num.data() + num.size()) {
    throw std::invalid_argument("not a valid integer");
  }

  if (addend > 0 && parsed > std::numeric_limits<int64_t>::max() - addend) {
    throw std::overflow_error("increment would overflow");
  }
  if (addend < 0 && parsed < std::numeric_limits<int64_t>::min() - addend) {
    throw std::overflow_error("decrement would underflow");
  }

  return parsed + addend;
}

void handle_ping(net::ClientObject &client) {
  RESP::encode_simple_string(client, "pong");
}

void handle_get(net::ClientObject &client, const RESP::DataBuffer &args) {
  if (args.size() != 2) {
    encode_wrong_arity(client, "get");
    return;
  }

  const db::Entry *entry = find_entry(args[1]);
  if (entry == nullptr) {
    RESP::encode_null_bulk_string(client);
    return;
  }

  RESP::encode_bulk_string(client, entry->val);
}

void handle_set(net::ClientObject &client, const RESP::DataBuffer &args) {
  if (args.size() != 3) {
    encode_wrong_arity(client, "set");
    return;
  }

  const std::string &key = args[1];
  const std::string &val = args[2];

  if (db::Entry *entry = find_entry(key)) {
    entry->val = val;
  } else {
    insert_entry(key, val);
  }

  RESP::encode_simple_string(client, "OK");
}

void handle_del(net::ClientObject &client, const RESP::DataBuffer &args) {
  if (args.size() != 2) {
    encode_wrong_arity(client, "del");
    return;
  }

  db::Entry *entry = find_entry(args[1]);
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

void handle_incr(net::ClientObject &client, const RESP::DataBuffer &args) {
  if (args.size() != 2) {
    encode_wrong_arity(client, "incr");
    return;
  }

  const std::string &key = args[1];
  db::Entry *entry = find_entry(key);
  if (entry == nullptr) {
    insert_entry(key, "1");
    RESP::encode_integer(client, 1);
    return;
  }

  try {
    const int64_t next = increment_int64(entry->val, 1);
    entry->val = std::to_string(next);
    RESP::encode_integer(client, next);
  } catch (const std::invalid_argument &) {
    RESP::encode_simple_error(client,
                              "ERR value is not an integer or out of range");
  } catch (const std::out_of_range &) {
    RESP::encode_simple_error(client,
                              "ERR value is not an integer or out of range");
  } catch (const std::overflow_error &) {
    RESP::encode_simple_error(client,
                              "ERR increment or decrement would overflow");
  }
}

void handle_decr(net::ClientObject &client, const RESP::DataBuffer &args) {
  if (args.size() != 2) {
    encode_wrong_arity(client, "decr");
    return;
  }

  const std::string &key = args[1];
  db::Entry *entry = find_entry(key);
  if (entry == nullptr) {
    insert_entry(key, "-1");
    RESP::encode_integer(client, -1);
    return;
  }

  try {
    const int64_t next = increment_int64(entry->val, -1);
    entry->val = std::to_string(next);
    RESP::encode_integer(client, next);
  } catch (const std::invalid_argument &) {
    RESP::encode_simple_error(client,
                              "ERR value is not an integer or out of range");
  } catch (const std::out_of_range &) {
    RESP::encode_simple_error(client,
                              "ERR value is not an integer or out of range");
  } catch (const std::overflow_error &) {
    RESP::encode_simple_error(client,
                              "ERR increment or decrement would overflow");
  }
}

void handle_mget(net::ClientObject &client, const RESP::DataBuffer &args) {
  if (args.size() < 2) {
    encode_wrong_arity(client, "mget");
    return;
  }

  std::vector<std::optional<std::string>> values;
  values.reserve(args.size() - 1);

  for (size_t i = 1; i < args.size(); i++) {
    const db::Entry *entry = find_entry(args[i]);
    if (entry == nullptr) {
      values.emplace_back(std::nullopt);
    } else {
      values.emplace_back(entry->val);
    }
  }

  RESP::encode_array(client, values);
}

void handle_exists(net::ClientObject &client, const RESP::DataBuffer &args) {
  if (args.size() < 2) {
    encode_wrong_arity(client, "exists");
    return;
  }

  int64_t exist_cnt{0};

  for (size_t i = 1; i < args.size(); i++) {
    const db::Entry *entry = find_entry(args[i]);
    if (entry != nullptr) {
      exist_cnt++;
    }
  }

  RESP::encode_integer(client, exist_cnt);
}

} // namespace

void execute_command(net::ClientObject &client, const RESP::DataBuffer &args) {
  if (args.empty()) {
    RESP::encode_simple_error(client, "ERR empty command");
    return;
  }

  const std::string &cmd = args[0];
  if (equals_ignore_case(cmd, "ping")) {
    handle_ping(client);
  } else if (equals_ignore_case(cmd, "get")) {
    handle_get(client, args);
  } else if (equals_ignore_case(cmd, "set")) {
    handle_set(client, args);
  } else if (equals_ignore_case(cmd, "del")) {
    handle_del(client, args);
  } else if (equals_ignore_case(cmd, "incr")) {
    handle_incr(client, args);
  } else if (equals_ignore_case(cmd, "decr")) {
    handle_decr(client, args);
  } else if (equals_ignore_case(cmd, "mget")) {
    handle_mget(client, args);
  } else if (equals_ignore_case(cmd, "exists")) {
    handle_exists(client, args);
  } else {
    RESP::encode_simple_error(client, "ERR unknown command '" + cmd + "'");
  }
}

} // namespace corekv::executor
