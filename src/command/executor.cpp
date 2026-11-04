#include "command/executor.h"
#include "db/db.h"
#include "db/hashtable.h"
#include "net/socket.h"
#include "parser/resp.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>
#include <string_view>

namespace corekv::executor {

[[nodiscard]] bool is_ping(std::string_view lhs) {
  const std::string rhs{"ping"};
  return std::ranges::equal(lhs, rhs, [](char left, char right) {
    return std::tolower(static_cast<unsigned char>(left)) ==
           std::tolower(static_cast<unsigned char>(right));
  });
}

void handle_ping(net::ClientObject &client) {
  RESP::encode_simple_string(client, "pong");
}

void handle_get(net::ClientObject &client, const RESP::RespArray &cmds) {

  db::Entry dummy;
  dummy.key = cmds[1].get_string();
  dummy.node.h_code = db::db.h_map.hash(
      reinterpret_cast<const uint8_t *>(dummy.key.data()), dummy.key.size());

  db::HashNode *node{db::db.h_map.lookup(&dummy.node)};
  if (!node) {
    RESP::encode_simple_string(client, "nil");
    return;
  }

  db::Entry *data = CONTAINER_OF(&db::db.h_map, db::Entry, node);
  RESP::encode_simple_string(client, data->val);
}

void handle_set(net::ClientObject &client, const RESP::RespArray &cmds) {

  db::Entry dummy;
  dummy.key = cmds[1].get_string();
  dummy.node.h_code = db::db.h_map.hash(
      reinterpret_cast<const uint8_t *>(dummy.key.data()), dummy.key.size());

  db::HashNode *node{db::db.h_map.lookup(&dummy.node)};
  if (node) {
    db::Entry *entry{CONTAINER_OF(&db::db.h_map, db::Entry, node)};
    entry->val = cmds[2].get_string();
  } else {
    db::Entry *entry = new db::Entry();
    entry->key = dummy.key;
    entry->val = cmds[2].get_string();
    entry->node.h_code = dummy.node.h_code;
    db::db.h_map.insert(&entry->node);
  }

  RESP::encode_simple_string(client, "OK");
}

void handle_del(net::ClientObject &client, const RESP::RespArray &cmds) {
  db::Entry dummy;
  dummy.key = cmds[1].get_string();
  dummy.node.h_code = db::db.h_map.hash(
      reinterpret_cast<const uint8_t *>(dummy.key.data()), dummy.key.size());
  db::HashNode *node{db::db.h_map.lookup(&dummy.node)};

  if (node) {
    delete CONTAINER_OF(&db::db.h_map, db::Entry, node);
  }

  RESP::encode_simple_string(client, "OK");
}

void execution_handler(net::ClientObject &client,
                       const RESP::RespValue &command) {
  if (!command.is_array()) {
    RESP::encode_simple_error(client, "ERR command must be an array");
    return;
  }

  const RESP::RespArray &args = command.get_array();
  if (args.empty()) {
    RESP::encode_simple_error(client, "ERR Invalid arguments");
    return;
  }

  if (!args[0].is_string()) {
    RESP::encode_simple_error(client, "ERR Unknown Command");
    return;
  }

  const std::string cmd_type{args[0].get_string()};

  if (is_ping(cmd_type)) {
    handle_ping(client);
  } else if (cmd_type == "get") {
    handle_get(client, args);
  } else if (cmd_type == "set") {
    handle_set(client, args);
  } else if (cmd_type == "del") {
    handle_del(client, args);
  } else {
    RESP::encode_simple_error(client, "ERR Unknown Command '" + cmd_type + "'");
  }
}

} // namespace corekv::executor
