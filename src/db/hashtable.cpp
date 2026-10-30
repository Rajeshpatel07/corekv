#include "db/hashtable.h"

namespace corekv::db {

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

HashMap::HashMap(HashMap &&other) noexcept
    : newer_(std::move(other.newer_)), older_(std::move(other.older_)),
      migration_pos(other.migration_pos) {
  other.migration_pos = 0;
}

HashMap &HashMap::operator=(HashMap &&other) noexcept {
  if (this != &other) {
    newer_ = std::move(other.newer_);
    older_ = std::move(other.older_);
    migration_pos = other.migration_pos;
    other.migration_pos = 0;
  }
  return *this;
}

[[nodiscard]] bool HashMap::match_keys(HashNode *left, HashNode *right) {
  Entry *l = CONTAINER_OF(left, Entry, node);
  Entry *r = CONTAINER_OF(right, Entry, node);
  return l->key == r->key;
}

void HashMap::trigger_rehash() {
  older_ = std::move(newer_);
  newer_.initialize((older_.mask() + 1) * 2);
  migration_pos = 0;
}

void HashMap::rehash() {
  uint64_t no_rehashed{0};

  while (no_rehashed < kRehashingWork && older_.size_ > 0) {
    if (migration_pos > older_.mask()) {
      break;
    }

    HashNode **from = &older_.buckets[migration_pos];
    if (!*from) {
      migration_pos++;
      continue;
    }

    newer_.insert(older_.detach(from));
    no_rehashed++;
  }

  if (older_.size_ == 0 && !older_.buckets.empty()) {
    older_.buckets.clear();
  }
}

[[nodiscard]] uint64_t HashMap::size() const noexcept {
  return newer_.size_ + older_.size_;
}

[[nodiscard]] uint64_t HashMap::hash(const uint8_t *data, size_t len) {
  uint64_t h = kFnvOffsetBasis64;
  const auto *bytes = static_cast<const unsigned char *>(data);

  for (size_t i = 0; i < len; ++i) {
    h ^= bytes[i];
    h *= kFnvPrime64;
  }
  return h;
}

[[nodiscard]] HashNode *HashMap::lookup(HashNode *node) {
  HashNode **from{newer_.lookup(node, match_keys)};
  if (!from) {
    from = older_.lookup(node, match_keys);
  }

  return from ? *from : nullptr;
}

void HashMap::insert(HashNode *node) {
  if (newer_.buckets.empty()) {
    newer_.initialize(4);
  }

  newer_.insert(node);
  if (older_.buckets.empty()) {
    uint64_t load_factor_thresold = (newer_.mask() + 1) * kMaxLoadFactor;
    if (newer_.size_ >= load_factor_thresold) {
      trigger_rehash();
    }
  }

  rehash();
}

HashNode *HashMap::remove(HashNode *node) {
  if (HashNode **from = newer_.lookup(node, match_keys)) {
    return newer_.detach(from);
  }

  if (HashNode **from = older_.lookup(node, match_keys)) {
    return older_.detach(from);
  }

  return nullptr;
}

} // namespace corekv::db
