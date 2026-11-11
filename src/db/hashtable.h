#pragma once

#include "common/common.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace corekv::db {

#define CONTAINER_OF(ptr, T, member)                                           \
  (reinterpret_cast<T *>(reinterpret_cast<char *>(ptr) - offsetof(T, member)))

// NOTE: Move all of these into a constants.h file for single place to modify
// everything.
constexpr size_t kMaxLoadFactor = 8;
constexpr size_t kRehashingWork = 128;

struct HashNode {
  HashNode *next{nullptr};
  uint64_t h_code{};
};

struct Entry {
  HashNode node;
  std::string key;
  RespValue val;
};

class HashTable {
public:
  std::vector<HashNode *> buckets;
  uint64_t size_{0};

  HashTable() = default;
  HashTable(const HashTable &) = delete;
  HashTable &operator=(const HashTable &) = delete;
  HashTable(HashTable &&other) noexcept;
  HashTable &operator=(HashTable &&other) noexcept;
  ~HashTable();

  [[nodiscard]] uint64_t mask() const noexcept;
  void initialize(uint64_t size);
  void insert(HashNode *node);
  [[nodiscard]] HashNode **lookup(HashNode *node,
                                  bool (*match_keys)(HashNode *l, HashNode *r));
  HashNode *detach(HashNode **from);
  void destroy();
};

class HashMap {
public:
  HashMap() = default;
  HashMap(const HashMap &) = delete;
  HashMap &operator=(const HashMap &) = delete;
  HashMap(HashMap &&other) noexcept;
  HashMap &operator=(HashMap &&other) noexcept;
  ~HashMap() = default;

  [[nodiscard]] uint64_t size() const noexcept;

  [[nodiscard]] HashNode *lookup(HashNode *node);
  void insert(HashNode *node);
  // It's the caller responsibility to delete the memory
  [[nodiscard]] HashNode *remove(HashNode *node);

private:
  HashTable newer_{};
  HashTable older_{};
  uint64_t migration_pos{0};

  static bool match_keys(HashNode *left, HashNode *right);
  void trigger_rehash();
  // Move data from one HashTable to another
  void rehash();
};

} // namespace corekv::db
