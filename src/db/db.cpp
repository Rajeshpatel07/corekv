#include "db/db.h"
#include "db/hashtable.h"
#include <thread>

namespace corekv::db {

Database::Database()
    : shards(std::max<std::size_t>(1, std::thread::hardware_concurrency())) {}

[[nodiscard]] uint64_t
Database::calculate_shard(const HashNode *node) const noexcept {
  return node->h_code % shards.size();
}

[[nodiscard]] HashNode *Database::lookup(HashNode *node) {
  return shards[calculate_shard(node)].lookup(node);
}

void Database::insert(HashNode *node) {
  shards[calculate_shard(node)].insert(node);
}

[[nodiscard]] HashNode *Database::remove(HashNode *node) {
  return shards[calculate_shard(node)].remove(node);
}

Database database;
} // namespace corekv::db
