#pragma once

#include <cstdint>
#include <string>

const uint64_t FNV_PRIME_64 = 1099511628211ULL;
const uint64_t FNV_OFFSET_BASIS_64 = 14695981039346656037ULL;
const uint64_t k_max_load_factor = 8;
const uint64_t k_rehashing_work = 128;

#define container_of(ptr, T, member) ((T *)((char *)ptr - offsetof(T, member)));

class HNode {
public:
  HNode *next;
  uint64_t hcode = 0; // Hash key
};

class HTab {
public:
  HNode **tab = nullptr; // Array of HNode similar to vector<HNode *>
  std::size_t mask = 0;  // value for getting the slot in the array.
  std::size_t size = 0;  // Total no of elements in the array.
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

uint64_t hash(const uint8_t *data, std::size_t len);

void h_init(HTab *htab, std::size_t size);

void h_insert(HTab *htab, HNode *node);

HNode **h_lookup(HTab *htab, HNode *node);

HNode *h_detach(HTab *htab, HNode **from);

bool matchkeys(HNode *lhs, HNode *rhs);

void hmap_trigger_rehash(HMap *hmap);

HNode *hmap_delete(HMap *hmap, HNode *key);
HNode *hmap_lookup(HMap *hmap, HNode *key);

void hmap_insert(HMap *hmap, HNode *node);

void hmap_rehash(HMap *hmap);
