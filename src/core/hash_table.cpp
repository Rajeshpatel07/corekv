#include "hash_table.hpp"
#include <cstdlib>

namespace corekv {

uint64_t hash(const uint8_t *data, size_t len) {
  uint64_t h = kFnvOffsetBasis64;
  const auto *bytes = static_cast<const unsigned char *>(data);

  for (size_t i = 0; i < len; ++i) {
    h ^= bytes[i];
    h *= kFnvPrime64;
  }

  return h;
}

bool matchKeys(HNode *lhs, HNode *rhs) {
  Entry *l = CONTAINER_OF(lhs, Entry, node);
  Entry *r = CONTAINER_OF(rhs, Entry, node);
  return l->key == r->key;
}

void hInit(HTab *htab, size_t size) {
  htab->tab = static_cast<HNode **>(std::calloc(size, sizeof(HNode *)));
  htab->mask = size - 1;
  htab->size = 0;
}

void hInsert(HTab *htab, HNode *node) {
  uint64_t pos = node->hcode & htab->mask;

  HNode *next = htab->tab[pos];
  node->next = next;
  htab->tab[pos] = node;
  htab->size++;
}

HNode **hLookup(HTab *htab, HNode *node) {
  if (!htab || !htab->tab) {
    return nullptr;
  }

  size_t pos = node->hcode & htab->mask;
  HNode **entry = &htab->tab[pos];

  for (HNode *cur; (cur = *entry) != nullptr; entry = &cur->next) {
    if (cur->hcode == node->hcode && matchKeys(cur, node)) {
      return entry;
    }
  }

  return nullptr;
}

HNode *hDetach(HTab *htab, HNode **from) {
  HNode *node = *from;

  *from = node->next;
  htab->size--;
  return node;
}

void hmapTriggerRehash(HMap *hmap) {
  hmap->older = hmap->newer;

  hInit(&hmap->newer, (hmap->newer.mask + 1) * 2);
  hmap->migrate_pos = 0;
}

void hmapRehash(HMap *hmap) {
  size_t nwork = 0;
  while (nwork < kRehashingWork && hmap->older.size > 0) {
    if (hmap->migrate_pos > hmap->older.mask) {
      break;
    }

    HNode **from = &hmap->older.tab[hmap->migrate_pos];
    if (!*from) {
      hmap->migrate_pos++;
      continue;
    }

    hInsert(&hmap->newer, hDetach(&hmap->older, from));
    nwork++;
  }

  if (hmap->older.size == 0 && hmap->older.tab) {
    std::free(hmap->older.tab);
    hmap->older = HTab{};
  }
}

HNode *hmapLookup(HMap *hmap, HNode *key) {
  HNode **from = hLookup(&hmap->newer, key);

  if (!from) {
    from = hLookup(&hmap->older, key);
  }

  return from ? *from : nullptr;
}

HNode *hmapDelete(HMap *hmap, HNode *key) {
  if (HNode **from = hLookup(&hmap->newer, key)) {
    return hDetach(&hmap->newer, from);
  }

  if (HNode **from = hLookup(&hmap->older, key)) {
    return hDetach(&hmap->older, from);
  }

  return nullptr;
}

void hmapInsert(HMap *hmap, HNode *node) {
  if (!hmap->newer.tab) {
    hInit(&hmap->newer, 4);
  }

  hInsert(&hmap->newer, node);

  if (!hmap->older.tab) {
    size_t loadfactor = (hmap->newer.mask + 1) * kMaxLoadFactor;
    if (hmap->newer.size >= loadfactor) {
      hmapTriggerRehash(hmap);
    }
  }

  hmapRehash(hmap);
}

} // namespace corekv
