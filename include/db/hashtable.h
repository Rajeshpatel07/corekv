#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Corekv {

#define CONTAINER_OF(ptr, T, member)                                           \
  ((T *)((char *)(ptr) - offsetof(T, member)))

struct HashNode {
  HashNode *next{nullptr};
  uint64_t h_code{};
};

struct Entry {
  HashNode node;
  std::string key;
  std::string val;
};

[[nodiscard]] bool match_keys(HashNode *left, HashNode *right);

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

} // namespace Corekv
