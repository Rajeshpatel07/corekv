#include "executor.hpp"
#include "../core/db.hpp"
#include "../core/hash_table.hpp"
#include "../protocol/serializer.hpp"

namespace corekv {

static std::string doGet(const std::vector<std::string> &cmds) {
  Entry dummy;
  dummy.key = cmds[1];
  dummy.node.hcode = hash(reinterpret_cast<const uint8_t *>(dummy.key.data()), dummy.key.size());

  HNode *node = hmapLookup(&db.store, &dummy.node);
  if (!node) {
    return "nil";
  }

  Entry *data = CONTAINER_OF(node, Entry, node);
  return data->val;
}

static void doSet(const std::vector<std::string> &cmds) {
  Entry dummy;
  dummy.key = cmds[1];
  dummy.node.hcode = hash(reinterpret_cast<const uint8_t *>(dummy.key.data()), dummy.key.size());

  HNode *node = hmapLookup(&db.store, &dummy.node);

  if (node) {
    Entry *data = CONTAINER_OF(node, Entry, node);
    data->val = cmds[2];
  } else {
    auto *newEntry = new Entry();
    newEntry->key = dummy.key;
    newEntry->val = cmds[2];
    newEntry->node.hcode = dummy.node.hcode;
    hmapInsert(&db.store, &newEntry->node);
  }
}

static void doDel(const std::vector<std::string> &cmds) {
  Entry dummy;
  dummy.key = cmds[1];
  dummy.node.hcode = hash(reinterpret_cast<const uint8_t *>(dummy.key.data()), dummy.key.size());

  HNode *node = hmapDelete(&db.store, &dummy.node);
  if (node) {
    delete CONTAINER_OF(node, Entry, node);
  }
}

void executeCommand(std::vector<std::string> &cmds, Connection *conn) {
  std::string res;

  if (cmds.size() == 2 && cmds[0] == "get") {
    res = doGet(cmds);
  } else if (cmds.size() == 3 && cmds[0] == "set") {
    doSet(cmds);
    res = "ok";
  } else if (cmds.size() == 2 && cmds[0] == "del") {
    doDel(cmds);
    res = "ok";
  } else {
    res = "invalid command";
  }

  generateResponse(conn->outgoing, res);
}

} // namespace corekv
