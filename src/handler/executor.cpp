#include "executor.hpp"
#include "../core/db.hpp"
#include "../core/hash_table.hpp"
#include "../protocol/serializer.hpp"
#include <iostream>

namespace corekv {

static void doGet(const std::vector<std::string> &cmds,
                  std::vector<uint8_t> &dest) {
  Entry dummy;
  dummy.key = cmds[1];
  dummy.node.hcode = hash(reinterpret_cast<const uint8_t *>(dummy.key.data()),
                          dummy.key.size());

  HNode *node = hmapLookup(&db.store, &dummy.node);
  if (!node) {
    const std::string res = "nil";
    addTagStr(dest, reinterpret_cast<const uint8_t *>(res.data()), res.size());
    return;
  }

  Entry *data = CONTAINER_OF(node, Entry, node);
  addTagStr(dest, reinterpret_cast<const uint8_t *>(data->val.data()),
            data->val.size());
  // return data->val;
}

static void doSet(const std::vector<std::string> &cmds,
                  std::vector<uint8_t> &dest) {
  Entry dummy;
  dummy.key = cmds[1];
  dummy.node.hcode = hash(reinterpret_cast<const uint8_t *>(dummy.key.data()),
                          dummy.key.size());

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

  addTagNil(dest);
}

static void doDel(const std::vector<std::string> &cmds,
                  std::vector<uint8_t> &dest) {
  Entry dummy;
  dummy.key = cmds[1];
  dummy.node.hcode = hash(reinterpret_cast<const uint8_t *>(dummy.key.data()),
                          dummy.key.size());

  HNode *node = hmapDelete(&db.store, &dummy.node);
  if (node) {
    delete CONTAINER_OF(node, Entry, node);
  }

  addTagNil(dest);
}

static void doKeys(std::vector<uint8_t> &dest) {
  addTagArr(dest, hmapSize(&db.store));
  hmapKeys(&db.store, dest);
}

void executeCommand(std::vector<std::string> &cmds, Connection *conn) {
  if (cmds.size() == 2 && cmds[0] == "get") {
    doGet(cmds, conn->outgoing);
  } else if (cmds.size() == 3 && cmds[0] == "set") {
    doSet(cmds, conn->outgoing);
  } else if (cmds.size() == 2 && cmds[0] == "del") {
    doDel(cmds, conn->outgoing);
  } else if (cmds.size() == 1 && cmds[0] == "keys") {
    doKeys(conn->outgoing);
  } else {
    addTagErr(conn->outgoing, "invalid command");
  }
}

} // namespace corekv
