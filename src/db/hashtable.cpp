#include "db/hashtable.h"

namespace Corekv {

bool match_keys(HashNode *left, HashNode *right) {
  Entry *l = CONTAINER_OF(left, Entry, node);
  Entry *r = CONTAINER_OF(right, Entry, node);
  return l->key == r->key;
}

HashTable::HashTable(HashTable &&other) noexcept
    : buckets(std::move(other.buckets)), size_(other.size_) {
  other.size_ = 0;
}

HashTable &HashTable::operator=(HashTable &&other) noexcept {
  if (this != &other) {
    destroy();
    buckets = std::move(other.buckets);
    size_ = other.size_;
    other.size_ = 0;
  }
  return *this;
}

HashTable::~HashTable() { destroy(); }

[[nodiscard]] uint64_t HashTable::mask() const noexcept {
  return buckets.empty() ? 0 : buckets.size() - 1;
}

void HashTable::initialize(uint64_t size) {
  if (size == 0) {
    size = 4;
  }

  buckets.assign(size, nullptr);
  size_ = 0;
}

void HashTable::insert(HashNode *node) {
  uint64_t pos{node->h_code & mask()};
  node->next = buckets[pos];
  buckets[pos] = node;
  size_++;
}

[[nodiscard]] HashNode **HashTable::lookup(HashNode *node,
                                           bool (*match_keys)(HashNode *l,
                                                              HashNode *r)) {
  if (buckets.empty()) {
    return nullptr;
  }

  uint64_t pos = node->h_code & mask();
  HashNode **next = &buckets[pos];

  for (HashNode *cur{nullptr}; (cur = *next) != nullptr; next = &cur->next) {
    if (cur->h_code == node->h_code && match_keys(node, cur)) {
      return next;
    }
  }

  return nullptr;
}

HashNode *HashTable::detach(HashNode **from) {
  HashNode *node = *from;
  *from = node->next;
  size_--;
  return node;
}

void HashTable::destroy() {
  if (buckets.empty()) {
    return;
  }

  for (size_t i = 0; i < buckets.size(); i++) {
    HashNode *head = buckets[i];
    while (head) {
      HashNode *tmp = head;
      head = head->next;
      Entry *entry = CONTAINER_OF(tmp, Entry, node);
      delete entry;
    }
  }

  buckets.clear();
  size_ = 0;
}

} // namespace Corekv
