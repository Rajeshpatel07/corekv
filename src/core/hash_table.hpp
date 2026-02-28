#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace corekv {

constexpr uint64_t kFnvPrime64 = 1099511628211ULL;
constexpr uint64_t kFnvOffsetBasis64 = 14695981039346656037ULL;
constexpr uint64_t kMaxLoadFactor = 8;
constexpr uint64_t kRehashingWork = 128;

#define CONTAINER_OF(ptr, T, member) ((T *)((char *)ptr - offsetof(T, member)))

class HNode {
public:
  HNode *next = nullptr;
  uint64_t hcode = 0;
};

class HTab {
public:
  HNode **tab = nullptr;
  size_t mask = 0;
  size_t size = 0;
};

struct HMap {
  HTab newer;
  HTab older;
  size_t migrate_pos = 0;
};

class Entry {
public:
  HNode node;
  std::string key;
  std::string val;
};

uint64_t hash(const uint8_t *data, size_t len);

void hInit(HTab *htab, size_t size);

void hInsert(HTab *htab, HNode *node);

HNode **hLookup(HTab *htab, HNode *node);

HNode *hDetach(HTab *htab, HNode **from);

bool matchKeys(HNode *lhs, HNode *rhs);

void hmapTriggerRehash(HMap *hmap);

HNode *hmapDelete(HMap *hmap, HNode *key);

HNode *hmapLookup(HMap *hmap, HNode *key);

void hmapInsert(HMap *hmap, HNode *node);

void hmapRehash(HMap *hmap);

size_t hmapSize(HMap *hmap);

void hmapKeys(HMap *hmap, std::vector<uint8_t> &dest);

// Destroy hash table and free all allocated memory
void hmapDestroy(HMap *hmap);

} // namespace corekv
