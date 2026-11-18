#pragma once

#include "db/hashtable.h"
#include <vector>

namespace corekv::db {

constexpr uint64_t kFnvPrime64 = 1099511628211ULL;
constexpr uint64_t kFnvOffsetBasis64 = 14695981039346656037ULL;

[[nodiscard]] inline uint64_t fnv1a_hash(const uint8_t *data, size_t len) {
  uint64_t h = kFnvOffsetBasis64;
  const auto *bytes = static_cast<const unsigned char *>(data);

  for (size_t i = 0; i < len; ++i) {
    h ^= bytes[i];
    h *= kFnvPrime64;
  }
  return h;
}

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
