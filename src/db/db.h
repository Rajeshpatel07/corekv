#pragma once

#include "db/hashtable.h"
#include <vector>

namespace corekv::db {

// NOTE: Single threaded Sharding DB now. When multi-threading workers are
// implemented mutexs and locks will be added
class Database {
public:
  Database();
  Database(const Database &) = delete;
  Database &operator=(const Database &) = delete;
  Database(Database &&) = delete;
  Database &operator=(Database &&) = delete;
  ~Database() = default;

  [[nodiscard]] HashNode *lookup(HashNode *node);
  void insert(HashNode *node);
  [[nodiscard]] HashNode *remove(HashNode *node);

private:
  std::vector<HashMap> shards;

  [[nodiscard]] uint64_t calculate_shard(const HashNode *node) const noexcept;
};

extern Database database;
} // namespace corekv::db
